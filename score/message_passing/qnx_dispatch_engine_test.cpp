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

#include <unordered_map>

namespace score
{
namespace message_passing
{
namespace
{

using namespace ::testing;

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
        message_context_t context;
        resmgr_iomsgs_t message;
        context.msg = &message;

        if (std::holds_alternative<PulseMessage>(current_message_))
        {
            auto& pulse = std::get<PulseMessage>(current_message_);
            message.pulse.value.sival_int = pulse.value;

            auto& handler = pulse_handlers_[pulse.code];
            score::cpp::ignore = handler.first(&context, pulse.code, 0, handler.second);
        }
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

    using QueueMessage = std::variant<std::monostate, ErrnoPseudoMessage, PulseMessage>;

    std::unordered_map<std::int32_t, std::pair<pulse_handler_t, void*>> pulse_handlers_{};

    constexpr static std::size_t kMaxQueueLength{5};
    score::concurrency::SynchronizedQueue<QueueMessage> message_queue_{kMaxQueueLength};
    QueueMessage current_message_{};
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
        EXPECT_CALL(*dispatch_, resmgr_attach).Times(1).WillOnce(Return(kFakeResmgrId));
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
    constexpr static std::int32_t kFakeResmgrId{0};
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
    EXPECT_CALL(*dispatch_, resmgr_attach).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

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
    EXPECT_CALL(*dispatch_, resmgr_attach).Times(AnyNumber()).WillOnce(Return(kFakeResmgrId));
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

}  // namespace
}  // namespace message_passing
}  // namespace score
