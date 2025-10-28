#include "message_passing_service_instance.h"
#include "score/message_passing/mock/client_connection_mock.h"
#include "score/message_passing/mock/client_factory_mock.h"
#include "score/message_passing/mock/server_connection_mock.h"
#include "score/message_passing/mock/server_factory_mock.h"
#include <gtest/gtest.h>

#include "score/concurrency/executor_mock.h"
#include "score/message_passing/mock/server_mock.h"

#include "score/os/mocklib/unistdmock.h"

namespace score::mw::com::impl::lola
{

class MessagePassingServiceInstanceAttorney
{
  public:
    using MessageType = MessagePassingServiceInstance::MessageType;
    static constexpr auto max_receive_handlers_per_event = MessagePassingServiceInstance::kMaxReceiveHandlersPerEvent;
    static constexpr auto node_id_tmp_buffer_size = MessagePassingServiceInstance::NodeIdTmpBufferSize;
};

namespace
{

using namespace score::message_passing;
using MessageType = MessagePassingServiceInstanceAttorney::MessageType;

class MessagePassingServiceInstanceTest : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(*server_mock_, StartListening(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault([this](ConnectCallback cc,
                                  DisconnectCallback dc,
                                  MessageCallback message_received_cb,
                                  MessageCallback message_received_w_reply_cb) -> score::cpp::expected_blank<score::os::Error> {
                connect_callback_ = std::move(cc);
                disconnect_callback_ = std::move(dc);
                received_send_message_callback_ = std::move(message_received_cb);
                received_send_message_with_reply_callback_ = std::move(message_received_w_reply_cb);

                return {};
            });

        ON_CALL(server_factory_mock_, Create(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(::testing::ByMove(std::move(server_mock_))));

        ON_CALL(*client_connection_mock_, GetState()).WillByDefault(testing::Return(IClientConnection::State::kReady));

        ON_CALL(*server_connection_mock_, GetClientIdentity()).WillByDefault(::testing::ReturnRef(client_identity_));
        ON_CALL(*server_connection_mock_, GetUserData()).WillByDefault(::testing::ReturnRef(user_data_));

        ON_CALL(*unistd_mock_, getpid()).WillByDefault(::testing::Return(local_pid_));

        ON_CALL(executor_mock_, Enqueue(testing::_)).WillByDefault([this](auto&& task) {
            executor_task_ = std::forward<decltype(task)>(task);
        });
    }

    template <typename T>
    score::cpp::span<const uint8_t> Serialize(const T& el_id, MessageType message_type, bool valid = true)
    {
        static std::vector<uint8_t> buffer{};
        if (valid)
        {
            buffer.assign(sizeof(T) + 1, 0);
            buffer[0] = score::cpp::to_underlying(message_type);
            memcpy(buffer.data() + 1, &el_id, sizeof(T));
        }
        else
        {
            // +5 is arbitrary, whatever's != (sizeof(T) + 1) works
            // we don't care about the content since it will be filtered before parsing
            buffer.assign(sizeof(T) + 5, 0);
            buffer[0] = score::cpp::to_underlying(message_type);
        }
        return {buffer.data(), buffer.size()};
    }

    ::testing::NiceMock<ClientFactoryMock> client_factory_mock_{};
    ::testing::NiceMock<ServerFactoryMock> server_factory_mock_{};

    score::cpp::pmr::unique_ptr<testing::NiceMock<ServerMock>> server_mock_{
        score::cpp::pmr::make_unique<testing::NiceMock<ServerMock>>(score::cpp::pmr::get_default_resource())};

    concurrency::testing::ExecutorMock executor_mock_{};
    score::cpp::pmr::unique_ptr<score::concurrency::Task> executor_task_{};
    score::cpp::stop_token stop_token_{};

    MessagePassingServiceInstance::AsilSpecificCfg asil_cfg_{};
    MessagePassingClientCache::ClientQualityType quality_type_{MessagePassingClientCache::ClientQualityType::kASIL_B};

    score::cpp::pmr::unique_ptr<::testing::NiceMock<ClientConnectionMock>> client_connection_mock_{
        score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource())};
    score::cpp::pmr::unique_ptr<::testing::NiceMock<ServerConnectionMock>> server_connection_mock_{
        score::cpp::pmr::make_unique<::testing::NiceMock<ServerConnectionMock>>(score::cpp::pmr::new_delete_resource())};

    ConnectCallback connect_callback_{};
    DisconnectCallback disconnect_callback_{};
    MessageCallback received_send_message_callback_{};
    MessageCallback received_send_message_with_reply_callback_{};

    score::safecpp::Scope<> scope_{};
    os::MockGuard<testing::NiceMock<os::UnistdMock>> unistd_mock_{};

    pid_t local_pid_{1};
    pid_t remote_pid_{2};

    ClientIdentity client_identity_{15u, 25u, 35u};
    UserData user_data_{std::in_place_type<uintptr_t>, 10u};

    ElementFqId event_id_;
};

using MessagePassingServiceInstanceDeathTest = MessagePassingServiceInstanceTest;

TEST_F(MessagePassingServiceInstanceDeathTest, TerminationOnStartListeningFail)
{
    // Given server mock that returns error on StartListening
    auto server = score::cpp::pmr::make_unique<ServerMock>(score::cpp::pmr::get_default_resource());
    ON_CALL(*server, StartListening(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(score::cpp::make_unexpected<score::os::Error>(os::Error::createFromErrno(ENOMEM))));

    // and server factory mock that returns the server mock
    ON_CALL(server_factory_mock_, Create(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(::testing::ByMove(std::move(server))));

    // Expect termination
    EXPECT_DEATH(
        {
            // When MessagePassingServiceInstance is constructed
            MessagePassingServiceInstance(
                quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_);
        },
        ".*");
}

// connect/disconnect
TEST_F(MessagePassingServiceInstanceTest, ConnectCallbackReturnsClientPid)
{
    // Given ServerConnection mock
    // and MessagePassingServiceInstance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // When connect callback is invoked with the mock
    const auto pid_result = connect_callback_(*server_connection_mock_);

    // Expect return value contains user pid
    ASSERT_TRUE(pid_result.has_value());
    EXPECT_EQ(std::get<uintptr_t>(pid_result.value()), client_identity_.pid);
}

TEST_F(MessagePassingServiceInstanceTest, DisconnectCallbackSuccesfullyExecuted)
{
    // Just a placeholder due to disconnect callback being empty
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    disconnect_callback_(*server_connection_mock_);
}

// received_send_message_with_reply
TEST_F(MessagePassingServiceInstanceTest, MessageWithReplyIsSuccesfullyExecutedWhenValidPidIsPassed)
{
    // Given message passing instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect GetUserData to be called twice and return valid pid
    EXPECT_CALL(*server_connection_mock_, GetUserData())
        .WillOnce(::testing::ReturnRef(user_data_))
        .WillOnce(::testing::ReturnRef(user_data_));

    // When received_send_message_with_reply callback is executed
    received_send_message_with_reply_callback_(*server_connection_mock_, {});
}

TEST_F(MessagePassingServiceInstanceDeathTest, MessageWithReplyTerminatesWhenInvalidPidIsPassed)
{
    // Given message passing instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect GetUserData to be called twice and return pid > pid_t::max()
    // we can't set .WillOnce() twice since gtest uses fork() for death tests
    user_data_.emplace<uintptr_t>(static_cast<uintptr_t>(std::numeric_limits<pid_t>::max()) + 1);
    EXPECT_CALL(*server_connection_mock_, GetUserData()).WillRepeatedly(::testing::ReturnRef(user_data_));

    // When received_send_message_with_reply callback is executed
    // Expect termination
    EXPECT_DEATH({ received_send_message_with_reply_callback_(*server_connection_mock_, {}); }, ".*");
}

// received_send_message
TEST_F(MessagePassingServiceInstanceTest, DoesntTerminateUponReceivingAnEmptyMessage)
{
    // Given message passing instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // When message callback is called with an empty message
    // Expect no termination
    received_send_message_callback_(*server_connection_mock_, {});
}

TEST_F(MessagePassingServiceInstanceTest, DoesntTerminateUponReceivingMessageOfInvalidType)
{
    // Given message passing instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // When message callback is called with a message of incorrect type
    // Expect no termination
    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, static_cast<MessageType>(0)));
}

// incorrect length
TEST_F(MessagePassingServiceInstanceTest, DoesntTerminateUponReceivingRegisterEventNotifierWithWrongLengthPayload)
{
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier, false));
}

TEST_F(MessagePassingServiceInstanceTest, DoesntTerminateUponReceivingUnegisterEventNotifierWithWrongLengthPayload)
{
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kUnregisterEventNotifier, false));
}

TEST_F(MessagePassingServiceInstanceTest, DoesntTerminateUponReceivingNotifyEventWithWrongLengthPayload)
{
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, MessageType::kNotifyEvent, false));
}

TEST_F(MessagePassingServiceInstanceTest, DoesntTerminateUponReceivingOutdatedNodeIdWithWrongLengthPayload)
{
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(std::get<uintptr_t>(user_data_), MessageType::kOutdatedNodeId, false));
}

TEST_F(MessagePassingServiceInstanceTest, NotfiyEventLocallyCallsRegisteredHandler)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and an event handler to track whether it was called
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // and handler being registered for event
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // When NotifyEvent is called for the same event
    instance.NotifyEvent(event_id_);
    (*executor_task_)(stop_token_);

    // Expect handler to be called
    EXPECT_TRUE(handler_called);
}

TEST_F(MessagePassingServiceInstanceTest, NotfiyEventLocallyDoesntTerminateUponEncounteringDestroyedHandler)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and an empty weak_ptr being registered for event
    instance.RegisterEventNotification(event_id_, {}, local_pid_);

    // Then we call NotifyEvent for the same event
    instance.NotifyEvent(event_id_);

    // Expect no termination of NotifyEventLocally
    (*executor_task_)(stop_token_);
}

TEST_F(MessagePassingServiceInstanceTest, NotfiyEventLocallyDoesntCallUnregisteredHandler)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and handler being registered for event
    auto handler_registration = instance.RegisterEventNotification(event_id_, {}, local_pid_);

    // and then unregistered
    instance.UnregisterEventNotification(event_id_, handler_registration, local_pid_);

    // When we call NotifyEvent for the same event
    instance.NotifyEvent(event_id_);

    // Then NotifyEventLocally shall not be enqueued
    EXPECT_EQ(executor_task_.get(), nullptr);
}

TEST_F(MessagePassingServiceInstanceTest,
       UnregisterEventNotificationCalledWithNonExistingHandlerRegistrationDoesntAffectExistingRegistrations)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and an event handler with indication whether it was called
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // and handler being registered for event
    auto handler_registration = instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // and then UnregisterEventNotification is called with a different handle
    instance.UnregisterEventNotification(event_id_, ++handler_registration, local_pid_);

    // When we call NotifyEvent for the same event
    instance.NotifyEvent(event_id_);
    (*executor_task_)(stop_token_);

    // Then the handler has been called
    EXPECT_TRUE(handler_called);
}

TEST_F(MessagePassingServiceInstanceTest, NotfiyEventDoesntPostNotifyEventLocallyIfNothingRegisteredForTheEvent)
{
    // Given service instance with no registered handlers
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // When we call NotifyEvent for an event
    instance.NotifyEvent(event_id_);

    // Then NotifyEventLocally has not been enqueued.
    EXPECT_EQ(executor_task_.get(), nullptr);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventLocallyCallsNoMoreThanMaxPossibleHandlersPerEvent)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and handler that increments number of calls
    uint16_t nums_called{0};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&nums_called]() {
            ++nums_called;
        });

    // and handler being registered for event (max_receive_handlers_per_event + 2) times
    for (auto i = 0; i < MessagePassingServiceInstanceAttorney::max_receive_handlers_per_event + 2; ++i)
    {
        instance.RegisterEventNotification(event_id_, handler, local_pid_);
    }

    // When we call NotifyEvent for the same event
    instance.NotifyEvent(event_id_);
    (*executor_task_)(stop_token_);

    // Then handler should be called max_receive_handlers_per_event times
    EXPECT_EQ(nums_called, MessagePassingServiceInstanceAttorney::max_receive_handlers_per_event);
}

TEST_F(MessagePassingServiceInstanceTest, RegisterEventNotificationRemoteSendsRegisterMessageOnFirstRegistration)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When handler being registered with pid != local_pid
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest,
       RegisterEventNotificationRemoteDoesnTermianteWhenFailsToSendRegistrationMessage)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called and return an error
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::make_unexpected<score::os::Error>(os::Error::createFromErrno(ENOMEM))));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When handler being registered with pid != local_pid
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest,
       RegisterEventNotificationRemoteSendsRegistrationMessageOnlyOnInitialRegistration)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called once upon first registration
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When handler being registered with pid != local_pid
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest,
       RegisterEventNotificationRemoteReplacesNodeIdWhenCalledWithDifferentForSameEvent)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client factory mock to be called once for each pid
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::Invoke([](auto&&...) {
            auto client_connection_mock =
                score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource());
            EXPECT_CALL(*client_connection_mock, Send(::testing::_)).Times(1);

            return client_connection_mock;
        }));

    // When handler being registered for event with pid != local_pid
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);
    instance.RegisterEventNotification(event_id_, {}, remote_pid_ + 5);
}

TEST_F(MessagePassingServiceInstanceTest,
       UnregisterEventNotificationRemoteSendsUnregisterMessageUponRemovalOfLastRegistration)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called once upon first registration and once upon unregistration
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // Given handler being registered with pid != local_pid
    auto registration_no = instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When UnregisterEventNotification is called with received registration handler
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest,
       UnregisterEventNotificationRemoteDoesnTerminateUponUnregistrationMessageSendFailure)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called twice: on registration and unregistration
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}))
        .WillOnce(testing::Return(score::cpp::make_unexpected<score::os::Error>(os::Error::createFromErrno(ENOMEM))));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // Given handler being registered for event with pid != local_pid
    auto registration_no = instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When UnregisterEventNotification is called with received registration handler
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest, UnregisterEventNotificationRemoteDoesntUndergisterOnPidMismatch)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called once upon first registration
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // Given handler being registered with pid != local_pid
    auto registration_no = instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When UnregisterEventNotification is called with received registration handler
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_ + 5);
}

TEST_F(MessagePassingServiceInstanceTest,
       UnregisterEventNotificationRemoteDoesntSendMessageUponUnregisteringNonLastHandler)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called once upon first registration
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // Given handler being registered with pid != local_pid twice
    auto registration_no = instance.RegisterEventNotification(event_id_, {}, remote_pid_);
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When UnregisterEventNotification is called with received registration handler
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest, UnregisterEventNotificationRemoteDoesntUnregisterLocalRegistations)  // TODO
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client factory to not be requested to create new connection
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // Given handler being registered with local_pid
    auto registration_no = instance.RegisterEventNotification(event_id_, {}, local_pid_);

    // When UnregisterEventNotification is called for the same event but remote pid
    instance.UnregisterEventNotification(event_id_, registration_no, remote_pid_);
}

// notify remote
TEST_F(MessagePassingServiceInstanceTest, NotfiyEventRemoteNotifiesClients)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client connection Send() to be called
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When NotifyEvent() for the same event is called
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, NotfiyEventRemoteNotifiesClientsRegisteredTwice)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received twice
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client connection Send() to be called
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When NotifyEvent() for the same event is called
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventRemoteDoesntTerminateOnSendFail)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // and client connection that returns an error on Send()
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::make_unexpected<score::os::Error>(os::Error::createFromErrno(ENOMEM))));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When NotifyEvent() for the same event is called
    // Expect it to not terminate
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, NotfiyEventRemoteNotifiesClientsExceedingTmpNodeBuffer)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    uint16_t nums_called{0};
    auto size = MessagePassingServiceInstanceAttorney::node_id_tmp_buffer_size * 2;

    // and (MessagePassingServiceInstanceAttorney::node_id_tmp_buffer_size * 2) registered for the same event
    for (auto i = 0; i < size; ++i)
    {
        received_send_message_callback_(*server_connection_mock_,
                                        Serialize(event_id_, MessageType::kRegisterEventNotifier));
        user_data_.emplace<uintptr_t>(std::get<uintptr_t>(user_data_) + 1);
    }

    // and client factory mock that returns new connections
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .Times(size)
        .WillRepeatedly(::testing::Invoke([&nums_called](auto&&...) {
            auto mock =
                score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource());

            EXPECT_CALL(*mock, Send(::testing::_)).WillOnce(testing::Invoke([&nums_called]() {
                ++nums_called;
                return score::cpp::expected_blank<score::os::Error>{};
            }));

            return mock;
        }));

    // When NotifyEvent() for the same event is called
    instance.NotifyEvent(event_id_);

    // Then all clients should be notified
    EXPECT_EQ(nums_called, size);
}

// unregister message
TEST_F(MessagePassingServiceInstanceTest, NotfiyEventRemoteDoesntNotifyUnregisteredClient)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client factory mock's Create() to not be called
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // When unregister event notifier message is received for the same client
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kUnregisterEventNotifier));

    // Then NotifyEvent() for the same event is called
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest,
       UnregisterMessageForNonExistingRegistrationDoesntAffectTheExistingRegistrations)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client connection Send() to be called
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_)).Times(1);

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When unregister event notifier message is received for different event
    ++event_id_.element_id_;
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kUnregisterEventNotifier));

    // Then NotifyEvent() for the initial event is called
    --event_id_.element_id_;
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, MultipleUnregistrationsForTheSameClientDontLeadToTermination)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect client factory mock to not be requested to create a new client
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // When unregister event notifier message is received for the same client twice
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kUnregisterEventNotifier));
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kUnregisterEventNotifier));

    // Then NotifyEvent() for the same event is called
    // Expect no termination
    instance.NotifyEvent(event_id_);
}

// handle outdated msg
TEST_F(MessagePassingServiceInstanceTest, HandleOutdatedNodeIdMsgRemovesOutdatedPid)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Given register event notifier message is received
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // Expect, that client factory mock is not requested to create a new client
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // When pid which was registered for event marked as outdated
    received_send_message_callback_(
        *server_connection_mock_,
        Serialize(static_cast<pid_t>(std::get<uintptr_t>(user_data_)), MessageType::kOutdatedNodeId));

    // Then NotifyEvent() for the same event is called
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest, HandleOutdatedNodeIdCalledWithUnregisteredPidDoesntAffectOtherRegistrations)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_)).Times(1);

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // Given notification is registered
    received_send_message_callback_(*server_connection_mock_,
                                    Serialize(event_id_, MessageType::kRegisterEventNotifier));

    // When kOutdatedNodeId message received for another pid
    received_send_message_callback_(
        *server_connection_mock_,
        Serialize(static_cast<pid_t>(std::get<uintptr_t>(user_data_) + 1), MessageType::kOutdatedNodeId));

    // Then NotifyEvent is called
    instance.NotifyEvent(event_id_);
}

// notify event message
TEST_F(MessagePassingServiceInstanceTest, NotifyEventMessageCallsRegisteredHandler)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and an event handler to track whether it was called
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // and handler being registered for event
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // When notify event message is received with the same event
    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, MessageType::kNotifyEvent));

    // Than handler should be called
    EXPECT_TRUE(handler_called);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyEventMessageDoesntCallHandlerForDifferentEvent)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and an event handler to track whether it was called
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // and handler being registered for event
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // When notify event message is received for a different event
    ++event_id_.element_id_;
    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, MessageType::kNotifyEvent));

    // Then handler should be called
    EXPECT_FALSE(handler_called);
}

// ReregisterEventNotification()
TEST_F(MessagePassingServiceInstanceTest, ReregisterEventNotificationDoesNotRegisterNewNotification)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and client factory mock's Create() to not be called
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_)).Times(0);

    // When ReregisterEventNotification is called on previously unregistered notification
    instance.ReregisterEventNotification(event_id_, remote_pid_);

    // Then NotifyEvent() for the event is called
    instance.NotifyEvent(event_id_);
}

TEST_F(MessagePassingServiceInstanceTest,
       ReregisterEventNotificationDoesntAffectLocalRegistrationsWhenCalledWithRemotePid)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and event handler
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // and notification being registered locally
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // When ReregisterEventNotification is called for the same event but with remote pid
    instance.ReregisterEventNotification(event_id_, remote_pid_);

    // Given notification message received
    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, MessageType::kNotifyEvent));

    // Then handler should be called
    EXPECT_TRUE(handler_called);
}

TEST_F(MessagePassingServiceInstanceTest,
       ReregisterEventNotificationDoesntAffectLocalRegistrationsWhenCalledWithLocalPid)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // and event handler
    bool handler_called{false};
    std::shared_ptr<ScopedEventReceiveHandler> handler =
        std::make_shared<ScopedEventReceiveHandler>(scope_, [&handler_called]() {
            handler_called = true;
        });

    // and notification being registered locally
    instance.RegisterEventNotification(event_id_, handler, local_pid_);

    // When ReregisterEventNotification is called for the same event/local pid
    instance.ReregisterEventNotification(event_id_, local_pid_);

    // Given notification message received
    received_send_message_callback_(*server_connection_mock_, Serialize(event_id_, MessageType::kNotifyEvent));

    // Then handler should be called
    EXPECT_TRUE(handler_called);
}

TEST_F(MessagePassingServiceInstanceTest, ReregisterEventNotificationForTheSameEventPidCombinationDoesntSendMessage)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called once upon first registration
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // Given event notification being registered for remote pid
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When ReregisterEventNotification() is called with the same event and pid
    instance.ReregisterEventNotification(event_id_, remote_pid_);
}

TEST_F(MessagePassingServiceInstanceTest, ReregisterEventNotificationWithDifferentNodeIdReplacesNodeIdAndSendsMessage)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client factory mock to be requested to create client connection twice
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::Invoke([](auto&&...) {
            auto mock =
                score::cpp::pmr::make_unique<::testing::NiceMock<ClientConnectionMock>>(score::cpp::pmr::new_delete_resource());
            EXPECT_CALL(*mock, Send(::testing::_)).WillOnce(::testing::Return(score::cpp::expected_blank<score::os::Error>{}));

            return mock;
        }));

    // Given event notification is registered
    instance.RegisterEventNotification(event_id_, {}, remote_pid_);

    // When event notification for the same event is reregistered with a different pid
    instance.ReregisterEventNotification(event_id_, remote_pid_ + 5);
}

// NotifyOutdatedNode()
TEST_F(MessagePassingServiceInstanceTest, NotifyOutdatedNodeCreatesClientAndSendsMessage)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::expected_blank<score::os::Error>{}));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When NotifyOutdatedNodeId() is called for a previously unused target_node_id
    instance.NotifyOutdatedNodeId(remote_pid_, remote_pid_ + 2);
}

TEST_F(MessagePassingServiceInstanceTest, NotifyOutdatedNodeDoesntTerminateOnFailedSend)
{
    // Given service instance
    MessagePassingServiceInstance instance{
        quality_type_, asil_cfg_, server_factory_mock_, client_factory_mock_, executor_mock_};

    // Expect client connection Send() to be called
    EXPECT_CALL(*client_connection_mock_, Send(::testing::_))
        .WillOnce(testing::Return(score::cpp::make_unexpected<score::os::Error>(os::Error::createFromErrno(ENOMEM))));

    // and client factory mock to return the client connection mock
    EXPECT_CALL(client_factory_mock_, Create(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(::testing::ByMove(std::move(client_connection_mock_))));

    // When NotifyOutdatedNodeId() is called for a previously unused target_node_id
    instance.NotifyOutdatedNodeId(remote_pid_, remote_pid_ + 2);
}
}  // namespace
}  // namespace score::mw::com::impl::lola
