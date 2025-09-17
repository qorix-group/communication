/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include <gtest/gtest.h>

#include "score/message_passing/qnx_dispatch/qnx_dispatch_engine.h"

#include "score/concurrency/synchronized_queue.h"

#include "score/os/mocklib/fcntl_mock.h"
#include "score/os/mocklib/qnx/mock_channel.h"
#include "score/os/mocklib/qnx/mock_dispatch.h"
#include "score/os/mocklib/qnx/mock_iofunc.h"
#include "score/os/mocklib/qnx/mock_timer.h"
#include "score/os/mocklib/sys_uio_mock.h"
#include "score/os/mocklib/unistdmock.h"

#include <future>
#include <unordered_map>

namespace score
{
namespace message_passing
{
namespace
{

using namespace ::testing;

class ResourceManagerServerMock : public QnxDispatchEngine::ResourceManagerServer
{
  public:
    ResourceManagerServerMock(std::shared_ptr<QnxDispatchEngine> engine)
        : QnxDispatchEngine::ResourceManagerServer(std::move(engine))
    {
    }

    MOCK_METHOD(std::int32_t, ProcessConnect, (resmgr_context_t*, io_open_t*), (noexcept, override));
};

class ResourceManagerConnectionMock : public QnxDispatchEngine::ResourceManagerConnection
{
  public:
    MOCK_METHOD(bool, ProcessInput, (std::uint8_t code, score::cpp::span<const std::uint8_t>), (noexcept, override));
    MOCK_METHOD(void, ProcessDisconnect, (), (noexcept, override));
    MOCK_METHOD(bool, HasSomethingToRead, (), (noexcept, override));
    MOCK_METHOD(std::int32_t, ProcessReadRequest, (resmgr_context_t*), (noexcept, override));
};

class ResourceMockHelper
{
  public:
    using pulse_handler_t = std::int32_t (*)(message_context_t* ctp,
                                             std::int32_t code,
                                             std::uint32_t flags,
                                             void* handle) noexcept;

    score::cpp::expected<std::int32_t, score::os::Error> pulse_attach(dispatch_t* const /*dpp*/,
                                                             const std::int32_t /*flags*/,
                                                             const std::int32_t code,
                                                             const pulse_handler_t func,
                                                             void* const handle) noexcept
    {
        pulse_handlers_.emplace(code, std::make_pair(func, handle));
        return code;
    }

    score::cpp::expected<std::int32_t, score::os::Error> resmgr_attach(dispatch_t* const dpp,
                                                              resmgr_attr_t* const attr,
                                                              const char* const path,
                                                              const enum _file_type file_type,
                                                              const std::uint32_t flags,
                                                              const resmgr_connect_funcs_t* const connect_funcs,
                                                              const resmgr_io_funcs_t* const io_funcs,
                                                              RESMGR_HANDLE_T* const handle) noexcept
    {
        connect_funcs_ = connect_funcs;
        io_funcs_ = io_funcs;
        handle_ = handle;
        return kFakeResmgrServerId;
    }

    score::cpp::expected_blank<score::os::Error> dispatch_block(dispatch_context_t* const /*ctp*/) noexcept
    {
        const auto max_wait = std::chrono::milliseconds(1000);

        auto result = message_queue_.Pop(max_wait, score::cpp::stop_token{});
        if (!result.has_value())
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(ETIMEDOUT));
        }
        if (std::holds_alternative<ErrnoPseudoMessage>(*result))
        {
            return score::cpp::make_unexpected(score::os::Error::createFromErrno(std::get<ErrnoPseudoMessage>(*result).error));
        }
        current_message_ = std::move(*result);
        return {};
    }

    score::cpp::expected_blank<std::int32_t> dispatch_handler(dispatch_context_t* const /*ctp*/) noexcept
    {
        if (std::holds_alternative<PulseMessage>(current_message_))
        {
            auto& pulse = std::get<PulseMessage>(current_message_);

            message_context_t context{};
            resmgr_iomsgs_t message{};
            context.msg = &message;
            message.pulse.value.sival_int = pulse.value;

            auto& handler = pulse_handlers_[pulse.code];
            score::cpp::ignore = handler.first(&context, pulse.code, 0, handler.second);
        }
        else if (std::holds_alternative<IoOpenMessage>(current_message_))
        {
            auto& io_open = std::get<IoOpenMessage>(current_message_);
            status_to_return_ = io_open.iofunc_open_status;

            resmgr_context_t context{};
            io_open_t message{};

            score::cpp::ignore = (*connect_funcs_->open)(&context, &message, handle_, nullptr);
            promises_.open.set_value();
        }
        return {};
    }

    score::cpp::expected_blank<std::int32_t> iofunc_open(resmgr_context_t* const /*ctp*/,
                                                  io_open_t* const /*msg*/,
                                                  iofunc_attr_t* const /*attr*/,
                                                  iofunc_attr_t* const /*dattr*/,
                                                  struct _client_info* const /*info*/) noexcept
    {
        if (status_to_return_ != EOK)
        {
            return score::cpp::make_unexpected(status_to_return_);
        }
        return {};
    }

    score::cpp::expected_blank<std::int32_t> iofunc_attr_lock(iofunc_attr_t* const attr) noexcept
    {
        ++lock_count_;
        return {};
    }

    score::cpp::expected_blank<std::int32_t> iofunc_attr_unlock(iofunc_attr_t* const attr) noexcept
    {
        --lock_count_;
        return {};
    }

    score::cpp::expected_blank<score::os::Error> MsgSendPulse(const std::int32_t coid,
                                                     const std::int32_t priority,
                                                     const std::int32_t code,
                                                     const std::int32_t value) noexcept
    {
        message_queue_.CreateSender().push(PulseMessage{code, value});
        return {};
    }

    void HelperInsertDispatchBlockError(std::int32_t error)
    {
        message_queue_.CreateSender().push(ErrnoPseudoMessage{error});
    }

    void HelperInsertIoOpen(std::int32_t iofunc_open_status)
    {
        message_queue_.CreateSender().push(IoOpenMessage{iofunc_open_status});
    }

    bool HelperIsLocked() const
    {
        return lock_count_ != 0;
    }

    constexpr static std::int32_t kFakeResmgrServerId{1};

    struct Promises
    {
        std::promise<void> open;
    };

    Promises promises_{};

  private:
    struct PulseMessage
    {
        std::int32_t code;
        std::int32_t value;
    };

    struct ErrnoPseudoMessage
    {
        std::int32_t error;
    };

    struct IoOpenMessage
    {
        std::int32_t iofunc_open_status;
    };

    using QueueMessage = std::variant<std::monostate, ErrnoPseudoMessage, PulseMessage, IoOpenMessage>;

    std::unordered_map<std::int32_t, std::pair<pulse_handler_t, void*>> pulse_handlers_{};
    const resmgr_connect_funcs_t* connect_funcs_{};
    const resmgr_io_funcs_t* io_funcs_{};
    RESMGR_HANDLE_T* handle_{};

    constexpr static std::size_t kMaxQueueLength{5};
    score::concurrency::SynchronizedQueue<QueueMessage> message_queue_{kMaxQueueLength};
    QueueMessage current_message_{};

    std::int32_t status_to_return_{EOK};
    std::int32_t lock_count_{0};
};

class QnxDispatchEngineTestFixture : public ::testing::Test
{
  protected:
    template <typename Mock>
    static void SetupResource(score::cpp::pmr::unique_ptr<Mock>& mock_ptr)
    {
        mock_ptr = score::cpp::pmr::make_unique<Mock>(score::cpp::pmr::get_default_resource());
    }

    void SetUp() override
    {
        SetupResource(channel_);
        SetupResource(dispatch_);
        SetupResource(fcntl_);
        SetupResource(iofunc_);
        SetupResource(timer_);
        SetupResource(sysuio_);
        SetupResource(unistd_);
    }

    void TearDown() override {}

    QnxDispatchEngine::OsResources MoveMockOsResources() noexcept
    {
        return {std::move(channel_),
                std::move(dispatch_),
                std::move(fcntl_),
                std::move(iofunc_),
                std::move(timer_),
                std::move(sysuio_),
                std::move(unistd_)};
    }

    void ExpectEngineConstructed()
    {
        EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(1).WillOnce(Return(kFakeDispatchPtr));
        EXPECT_CALL(*dispatch_, pulse_attach)
            .Times(2)
            .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::pulse_attach));
        EXPECT_CALL(*dispatch_, message_connect).Times(1).WillOnce(Return(kFakeCoid));
        EXPECT_CALL(*dispatch_, resmgr_attach(_, _, IsNull(), _, _, _, _, _))
            .Times(1)
            .WillOnce(Return(kFakeResmgrEmptyId));
        EXPECT_CALL(*dispatch_, dispatch_context_alloc).Times(1).WillOnce(Return(kFakeContextPtr));
        EXPECT_CALL(*timer_, TimerCreate).Times(1).WillOnce(Return(kFakeTimerId));
        EXPECT_CALL(*iofunc_, iofunc_func_init).Times(1);
    }

    void ExpectEngineThreadRunning()
    {
        EXPECT_CALL(*dispatch_, dispatch_block)
            .Times(AnyNumber())
            .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::dispatch_block));
        EXPECT_CALL(*dispatch_, dispatch_handler)
            .Times(AnyNumber())
            .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::dispatch_handler));
    }

    void ExpectEngineDestructed()
    {
        EXPECT_CALL(*channel_, MsgSendPulse)
            .Times(1)
            .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::MsgSendPulse));

        EXPECT_CALL(*timer_, TimerDestroy).Times(1);
        EXPECT_CALL(*channel_, ConnectDetach).Times(1);
        EXPECT_CALL(*dispatch_, pulse_detach).Times(2);
        EXPECT_CALL(*dispatch_, dispatch_destroy).Times(1);
        EXPECT_CALL(*dispatch_, dispatch_context_free).Times(1);
    }

    void ExpectServerAttached()
    {
        EXPECT_CALL(*dispatch_, resmgr_attach(_, _, NotNull(), _, _, _, _, _))
            .Times(1)
            .WillOnce(Invoke(&helper_, &ResourceMockHelper::resmgr_attach));
        EXPECT_CALL(*iofunc_, iofunc_attr_init).Times(1);
    }

    void ExpectServerDetached()
    {
        EXPECT_CALL(*dispatch_,
                    resmgr_detach(_, kFakeResmgrServerId, static_cast<std::uint32_t>(_RESMGR_DETACH_CLOSE)));
    }

    void ExpectConnectionOpen()
    {
        InSequence is;
        EXPECT_CALL(*iofunc_, iofunc_attr_lock)
            .Times(1)
            .WillOnce(Invoke(&helper_, &ResourceMockHelper::iofunc_attr_lock));
        EXPECT_CALL(*iofunc_, iofunc_open).Times(1).WillOnce(Invoke(&helper_, &ResourceMockHelper::iofunc_open));
        EXPECT_CALL(*iofunc_, iofunc_attr_unlock)
            .Times(1)
            .WillOnce(Invoke(&helper_, &ResourceMockHelper::iofunc_attr_unlock));
    }

    template <typename Mock>
    using mock_ptr = score::cpp::pmr::unique_ptr<StrictMock<Mock>>;

    mock_ptr<score::os::MockChannel> channel_;
    mock_ptr<score::os::MockDispatch> dispatch_;
    mock_ptr<score::os::FcntlMock> fcntl_;
    mock_ptr<score::os::MockIoFunc> iofunc_;
    mock_ptr<score::os::qnx::MockTimer> timer_;
    mock_ptr<score::os::SysUioMock> sysuio_;
    mock_ptr<score::os::UnistdMock> unistd_;

    ResourceMockHelper helper_;

    constexpr static dispatch_t* kFakeDispatchPtr{nullptr};
    constexpr static dispatch_context_t* kFakeContextPtr{nullptr};
    constexpr static std::int32_t kFakeCoid{0};
    constexpr static std::int32_t kFakeTimerId{0};
    constexpr static std::int32_t kFakeResmgrEmptyId{0};
    constexpr static std::int32_t kFakeResmgrServerId{ResourceMockHelper::kFakeResmgrServerId};
    const score::cpp::unexpected<score::os::Error> kFakeOsError{score::os::Error::createFromErrno(EINVAL)};
};

using QnxDispatchEngineDeathTest = QnxDispatchEngineTestFixture;

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_CreateChannel)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to allocate dispatch handle");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_AttachTimerPulse)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to attach timer pulse code");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_AttachEventPulse)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach)
        .Times(AnyNumber())
        .WillOnce(Invoke(&helper_, &ResourceMockHelper::pulse_attach))
        .WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to attach event pulse code");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_ConnectSideChannel)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::pulse_attach));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to create side channel");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_CreateTimer)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::pulse_attach));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(kFakeCoid));
    EXPECT_CALL(*timer_, TimerCreate).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()), "Unable to create timer");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_SetUpResourceManager)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::pulse_attach));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(kFakeCoid));
    EXPECT_CALL(*timer_, TimerCreate).Times(AnyNumber()).WillOnce(Return(kFakeTimerId));
    EXPECT_CALL(*dispatch_, resmgr_attach(_, _, IsNull(), _, _, _, _, _))
        .Times(AnyNumber())
        .WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to set up resource manager operations");
}

TEST_F(QnxDispatchEngineDeathTest, EngineCreation_AllocateContext)
{
    // Times(AnyNumber()) to satisfy death test logic
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeDispatchPtr));
    EXPECT_CALL(*dispatch_, pulse_attach)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper_, &ResourceMockHelper::pulse_attach));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(kFakeCoid));
    EXPECT_CALL(*timer_, TimerCreate).Times(AnyNumber()).WillOnce(Return(kFakeTimerId));
    EXPECT_CALL(*dispatch_, resmgr_attach(_, _, IsNull(), _, _, _, _, _))
        .Times(AnyNumber())
        .WillOnce(Return(kFakeResmgrEmptyId));
    EXPECT_CALL(*dispatch_, dispatch_context_alloc).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    std::optional<QnxDispatchEngine> engine;
    EXPECT_DEATH(engine.emplace(score::cpp::pmr::get_default_resource(), MoveMockOsResources()),
                 "Unable to allocate context pointer");
}

TEST_F(QnxDispatchEngineTestFixture, EngineCreationAndDestruction)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectEngineDestructed();

    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());

    // Check that dispatch_block error does not break dispatch loop
    helper_.HelperInsertDispatchBlockError(ENOMEM);
}

TEST_F(QnxDispatchEngineDeathTest, PosixEndpoint_RegisterNotOnCallbackThread)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    // For non-death fork:
    ExpectEngineDestructed();

    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    ISharedResourceEngine::PosixEndpointEntry posix_endpoint;
    EXPECT_DEATH(engine.RegisterPosixEndpoint(posix_endpoint), "");
}

TEST_F(QnxDispatchEngineDeathTest, PosixEndpoint_UnregisterNotOnCallbackThread)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    // For non-death fork:
    ExpectEngineDestructed();

    QnxDispatchEngine engine(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    ISharedResourceEngine::PosixEndpointEntry posix_endpoint;
    EXPECT_DEATH(engine.UnregisterPosixEndpoint(posix_endpoint), "");
}

TEST_F(QnxDispatchEngineTestFixture, ServerStart_Unsuccessful)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    EXPECT_CALL(*dispatch_, resmgr_attach(_, _, NotNull(), _, _, _, _, _)).Times(1).WillOnce(Return(kFakeOsError));
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};

    EXPECT_FALSE(server.Start(path));
}

TEST_F(QnxDispatchEngineTestFixture, ServerStartStop)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    {
        InSequence is;
        EXPECT_CALL(*dispatch_, resmgr_attach(_, _, NotNull(), _, _, _, _, _))
            .Times(1)
            .WillOnce(Return(kFakeResmgrServerId));
        EXPECT_CALL(*iofunc_, iofunc_attr_init).Times(1);

        EXPECT_CALL(*dispatch_,
                    resmgr_detach(_, kFakeResmgrServerId, static_cast<std::uint32_t>(_RESMGR_DETACH_CLOSE)));
    }
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};

    // shall be ignored by engine
    server.Stop();

    EXPECT_TRUE(server.Start(path));
    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerOpenCheckFailure)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};
    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(ENOMEM);
    helper_.promises_.open.get_future().get();
    EXPECT_FALSE(helper_.HelperIsLocked());

    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerOpenCheckSuccess)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};

    EXPECT_CALL(server, ProcessConnect).Times(1).WillOnce([this](auto&&...) {
        EXPECT_TRUE(helper_.HelperIsLocked());
        return EOK;
    });

    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(EOK);
    helper_.promises_.open.get_future().get();
    EXPECT_FALSE(helper_.HelperIsLocked());

    server.Stop();
}

TEST_F(QnxDispatchEngineTestFixture, ServerOpenCheckSuccessConnectionAttached)
{
    ExpectEngineConstructed();
    ExpectEngineThreadRunning();
    ExpectServerAttached();
    ExpectConnectionOpen();
    EXPECT_CALL(*iofunc_, iofunc_ocb_attach).Times(1);

    ExpectServerDetached();
    ExpectEngineDestructed();

    auto engine = std::make_shared<QnxDispatchEngine>(score::cpp::pmr::get_default_resource(), MoveMockOsResources());
    StrictMock<ResourceManagerServerMock> server{engine};
    QnxDispatchEngine::QnxResourcePath path{"fake_path"};

    EXPECT_CALL(server, ProcessConnect)
        .Times(1)
        .WillOnce([this, &server, &engine](resmgr_context_t* const ctp, io_open_t* const msg) {
            EXPECT_TRUE(helper_.HelperIsLocked());
            StrictMock<ResourceManagerConnectionMock> connection;
            score::cpp::ignore = engine->AttachConnection(ctp, msg, server, connection);
            return EOK;
        });

    EXPECT_TRUE(server.Start(path));

    helper_.HelperInsertIoOpen(EOK);
    helper_.promises_.open.get_future().get();
    EXPECT_FALSE(helper_.HelperIsLocked());

    server.Stop();
}

}  // namespace
}  // namespace message_passing
}  // namespace score
