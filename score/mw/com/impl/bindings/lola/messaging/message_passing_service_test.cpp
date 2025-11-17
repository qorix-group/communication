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
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service.h"

#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance_factory_mock.h"
#include "score/mw/com/impl/bindings/lola/messaging/message_passing_service_instance_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>

#include <memory>

namespace score::mw::com::impl::lola::test
{

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

MATCHER_P(MatchesAsilSpecificConfig, cfg, "")
{
    return arg.message_queue_rx_size_ == cfg.message_queue_rx_size_ && arg.allowed_user_ids_ == cfg.allowed_user_ids_;
}

class MessagePassingServiceTest : public ::testing::Test
{
  protected:
    MessagePassingServiceTest& WithAsilBAndQmInstance()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG_MESSAGE(asil_qm_message_passing_service_instance_mock_ != nullptr &&
                                   asil_b_message_passing_service_instance_mock_ != nullptr,
                               "Dependencies invalid");

        ON_CALL(*factory_, Create(ClientQualityType::kASIL_B, MatchesAsilSpecificConfig(asil_b_cfg_), _, _, _))
            .WillByDefault(Return(ByMove(std::move(asil_b_message_passing_service_instance_mock_))));
        ON_CALL(*factory_, Create(ClientQualityType::kASIL_QMfromB, MatchesAsilSpecificConfig(asil_qm_cfg_), _, _, _))
            .WillByDefault(Return(ByMove(std::move(asil_qm_message_passing_service_instance_mock_))));
        return *this;
    }

    MessagePassingServiceTest& WithAsilQmInstance()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG_MESSAGE(asil_qm_message_passing_service_instance_mock_ != nullptr, "Dependencies invalid");

        ON_CALL(*factory_, Create(ClientQualityType::kASIL_QM, MatchesAsilSpecificConfig(asil_qm_cfg_), _, _, _))
            .WillByDefault(Return(ByMove(std::move(asil_qm_message_passing_service_instance_mock_))));

        return *this;
    }

    AsilSpecificCfg asil_qm_cfg_{1, {}};
    AsilSpecificCfg asil_b_cfg_{1, {}};
    std::unique_ptr<MessagePassingServiceInstanceMock> asil_b_message_passing_service_instance_mock_ =
        std::make_unique<MessagePassingServiceInstanceMock>();
    std::unique_ptr<MessagePassingServiceInstanceMock> asil_qm_message_passing_service_instance_mock_ =
        std::make_unique<MessagePassingServiceInstanceMock>();

    std::unique_ptr<MessagePassingServiceInstanceFactoryMock> factory_ =
        std::make_unique<MessagePassingServiceInstanceFactoryMock>();
};

TEST_F(MessagePassingServiceTest, CreatesQMInstanceForQMOnlyService)
{
    // Given a unit with no dependencies to inject
    WithAsilQmInstance();

    // Expecting a construction of an ASIL-QM instance and none for an ASIL-B instance
    EXPECT_CALL(*factory_, Create(ClientQualityType::kASIL_B, _, _, _, _)).Times(0);
    EXPECT_CALL(*factory_, Create(ClientQualityType::kASIL_QM, MatchesAsilSpecificConfig(asil_qm_cfg_), _, _, _))
        .Times(1);

    // When constructing the unit
    const MessagePassingService unit{asil_qm_cfg_, std::nullopt, std::move(factory_)};
}

TEST_F(MessagePassingServiceTest, NotifyEventDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to NotifyEvent of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, NotifyEvent(event_id)).Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, NotifyEvent(_)).Times(0);

    // When calling NotifyEvent
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.NotifyEvent(QualityType::kASIL_QM, event_id);
}

TEST_F(MessagePassingServiceTest, NotifyEventDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to NotifyEvent of ASIL-B mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, NotifyEvent(_)).Times(0);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, NotifyEvent(event_id)).Times(1);

    // When calling NotifyEvent
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.NotifyEvent(QualityType::kASIL_B, event_id);
}

TEST_F(MessagePassingServiceTest, DeathTestNotifyEventAbortsWithInvalidAsilLevel)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // When calling NotifyEvent with an invalid ASIL level THEN it terminates
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};

    // Then we expect it to have died
    EXPECT_DEATH(unit.NotifyEvent(QualityType::kInvalid, event_id), "");
}

MATCHER_P(MatchesWeakPtr, ptr, "")
{
    return arg.lock() == ptr.lock();
}

TEST_F(MessagePassingServiceTest, RegisterEventNotificationDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    const auto callback = std::make_shared<ScopedEventReceiveHandler>();
    const std::weak_ptr<ScopedEventReceiveHandler> weak_callback = callback;
    constexpr pid_t pid{5};
    constexpr IMessagePassingService::HandlerRegistrationNoType handler_no{3};

    // Expecting a call to RegisterEventNotification of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterEventNotification(event_id, MatchesWeakPtr(weak_callback), pid))
        .WillOnce(Return(handler_no));
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, RegisterEventNotification(_, _, _)).Times(0);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    const auto result = unit.RegisterEventNotification(QualityType::kASIL_QM, event_id, weak_callback, pid);

    // Then the return value matches the injected handler_no
    EXPECT_EQ(result, handler_no);
}

TEST_F(MessagePassingServiceTest, RegisterEventNotificationDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    const auto callback = std::make_shared<ScopedEventReceiveHandler>();
    const std::weak_ptr<ScopedEventReceiveHandler> weak_callback = callback;
    constexpr pid_t pid{5};
    constexpr IMessagePassingService::HandlerRegistrationNoType handler_no{3};

    // Expecting a call to RegisterEventNotification of ASIL-B mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, RegisterEventNotification(_, _, _)).Times(0);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_,
                RegisterEventNotification(event_id, MatchesWeakPtr(weak_callback), pid))
        .WillOnce(Return(handler_no));

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    const auto result = unit.RegisterEventNotification(QualityType::kASIL_B, event_id, weak_callback, pid);

    // Then the return value matches the injected handler_no
    EXPECT_EQ(result, handler_no);
}

using MessagePassingServiceDeathTest = MessagePassingServiceTest;
TEST_F(MessagePassingServiceDeathTest, RegisterEventNotificationAbortsWithInvalidAsilLevel)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    const auto callback = std::make_shared<ScopedEventReceiveHandler>();
    const std::weak_ptr<ScopedEventReceiveHandler> weak_callback = callback;
    constexpr pid_t pid{5};

    // When calling RegisterEventNotification with an invalid ASIL level THEN it terminates
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    EXPECT_DEATH(unit.RegisterEventNotification(QualityType::kInvalid, event_id, weak_callback, pid), "");
}

TEST_F(MessagePassingServiceTest, ReregisterEventNotificationDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr pid_t pid{5};

    // Expecting a call to RegisterEventNotification of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, ReregisterEventNotification(event_id, pid)).Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, ReregisterEventNotification(_, _)).Times(0);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.ReregisterEventNotification(QualityType::kASIL_QM, event_id, pid);
}

TEST_F(MessagePassingServiceTest, ReregisterEventNotificationDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr pid_t pid{5};

    // Expecting a call to RegisterEventNotification of ASIL-B mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, ReregisterEventNotification(_, _)).Times(0);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, ReregisterEventNotification(event_id, pid)).Times(1);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.ReregisterEventNotification(QualityType::kASIL_B, event_id, pid);
}

TEST_F(MessagePassingServiceDeathTest, ReregisterEventNotificationAbortsWithInvalidAsilLevel)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr pid_t pid{5};

    // When calling ReregisterEventNotification with an invalid ASIL level THEN it terminates
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    EXPECT_DEATH(unit.ReregisterEventNotification(QualityType::kInvalid, event_id, pid), "");
}

TEST_F(MessagePassingServiceTest, UnregisterEventNotificationDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr IMessagePassingService::HandlerRegistrationNoType handler_no{3};
    constexpr pid_t pid{5};

    // Expecting a call to RegisterEventNotification of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, UnregisterEventNotification(event_id, handler_no, pid))
        .Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, UnregisterEventNotification(_, _, _)).Times(0);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.UnregisterEventNotification(QualityType::kASIL_QM, event_id, handler_no, pid);
}

TEST_F(MessagePassingServiceTest, UnregisterEventNotificationDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr IMessagePassingService::HandlerRegistrationNoType handler_no{3};
    constexpr pid_t pid{5};

    // Expecting a call to RegisterEventNotification of ASIL-B mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, UnregisterEventNotification(_, _, _)).Times(0);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, UnregisterEventNotification(event_id, handler_no, pid))
        .Times(1);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.UnregisterEventNotification(QualityType::kASIL_B, event_id, handler_no, pid);
}

TEST_F(MessagePassingServiceDeathTest, UnregisterEventNotificationAbortsWithInvalidAsilLevel)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};
    constexpr IMessagePassingService::HandlerRegistrationNoType handler_no{3};
    constexpr pid_t pid{5};

    // When calling UnregisterEventNotification with an invalid ASIL level THEN it terminates
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    EXPECT_DEATH(unit.UnregisterEventNotification(QualityType::kInvalid, event_id, handler_no, pid), "");
}

TEST_F(MessagePassingServiceTest, NotifyOutdatedNodeIdDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    constexpr pid_t pid{5};
    constexpr pid_t old_pid{4};

    // Expecting a call to RegisterEventNotification of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, NotifyOutdatedNodeId(pid, old_pid)).Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, NotifyOutdatedNodeId(_, _)).Times(0);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.NotifyOutdatedNodeId(QualityType::kASIL_QM, pid, old_pid);
}

TEST_F(MessagePassingServiceTest, NotifyOutdatedNodeIdDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    constexpr pid_t pid{5};
    constexpr pid_t old_pid{4};

    // Expecting a call to RegisterEventNotification of ASIL-B mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, NotifyOutdatedNodeId(_, _)).Times(0);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, NotifyOutdatedNodeId(pid, old_pid)).Times(1);

    // When calling RegisterEventNotification
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.NotifyOutdatedNodeId(QualityType::kASIL_B, pid, old_pid);
}

TEST_F(MessagePassingServiceDeathTest, NotifyOutdatedNodeIdAbortsWithInvalidAsilLevel)
{
    // Given some input parameters to the tested function call
    constexpr pid_t pid{5};
    constexpr pid_t old_pid{4};

    // When calling NotifyOutdatedNodeId with an invalid ASIL level THEN it terminates
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    EXPECT_DEATH(unit.NotifyOutdatedNodeId(QualityType::kInvalid, pid, old_pid), "");
}

TEST_F(MessagePassingServiceTest, RegisterEventNotificationExistenceChangedCallbackDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to RegisterEventNotificationExistenceChangedCallback of ASIL-B mock instance
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_,
                RegisterEventNotificationExistenceChangedCallback(event_id, _))
        .Times(1);
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterEventNotificationExistenceChangedCallback(_, _))
        .Times(0);

    // When calling RegisterEventNotificationExistenceChangedCallback
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.RegisterEventNotificationExistenceChangedCallback(QualityType::kASIL_B, event_id, {});
}

TEST_F(MessagePassingServiceTest, RegisterEventNotificationExistenceChangedCallbackDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to RegisterEventNotificationExistenceChangedCallback of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                RegisterEventNotificationExistenceChangedCallback(event_id, _))
        .Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, RegisterEventNotificationExistenceChangedCallback(_, _))
        .Times(0);

    // When calling RegisterEventNotificationExistenceChangedCallback
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.RegisterEventNotificationExistenceChangedCallback(QualityType::kASIL_QM, event_id, {});
}

TEST_F(MessagePassingServiceTest, UnregisterEventNotificationExistenceChangedCallbackDispatchesToAsilBInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to UnregisterEventNotificationExistenceChangedCallback of ASIL-B mock instance
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_,
                UnregisterEventNotificationExistenceChangedCallback(event_id))
        .Times(1);
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_, UnregisterEventNotificationExistenceChangedCallback(_))
        .Times(0);

    // When calling UnregisterEventNotificationExistenceChangedCallback
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.UnregisterEventNotificationExistenceChangedCallback(QualityType::kASIL_B, event_id);
}

TEST_F(MessagePassingServiceTest, UnregisterEventNotificationExistenceChangedCallbackDispatchesToAsilQMInstance)
{
    // Given some input parameters to the tested function call
    const ElementFqId event_id{2U, 4U, 3U, ServiceElementType::EVENT};

    // Expecting a call to UnregisterEventNotificationExistenceChangedCallback of ASIL-QM mock instance
    EXPECT_CALL(*asil_qm_message_passing_service_instance_mock_,
                UnregisterEventNotificationExistenceChangedCallback(event_id))
        .Times(1);
    EXPECT_CALL(*asil_b_message_passing_service_instance_mock_, UnregisterEventNotificationExistenceChangedCallback(_))
        .Times(0);

    // When calling UnregisterEventNotificationExistenceChangedCallback
    WithAsilBAndQmInstance();
    MessagePassingService unit{asil_qm_cfg_, asil_qm_cfg_, std::move(factory_)};
    unit.UnregisterEventNotificationExistenceChangedCallback(QualityType::kASIL_QM, event_id);
}

}  // namespace score::mw::com::impl::lola::test
