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
#include "score/mw/com/message_passing/receiver_factory.h"

#include "score/concurrency/thread_pool.h"
#include "score/mw/com/message_passing/receiver_mock.h"

#include <gtest/gtest.h>

namespace score::mw::com::message_passing
{
namespace
{

using ::testing::An;

constexpr auto SOME_VALID_PATH = "foo";

class ReceiverFactoryFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        message_passing::ReceiverFactory::InjectReceiverMock(&receiver_mock_);
    }
    void TearDown() override
    {
        message_passing::ReceiverFactory::InjectReceiverMock(nullptr);
    }

    testing::StrictMock<message_passing::ReceiverMock> receiver_mock_{};
    score::concurrency::ThreadPool thread_pool_{1};
};

TEST_F(ReceiverFactoryFixture, CheckReceiverMock)
{
    auto unit = ReceiverFactory::Create(SOME_VALID_PATH, thread_pool_, {});

    ::testing::InSequence is;

    constexpr MessageId kShortMessageId{1};
    constexpr MessageId kMediumMessageId{2};

    EXPECT_CALL(receiver_mock_, Register(kShortMessageId, An<IReceiver::ShortMessageReceivedCallback>())).Times(1);
    EXPECT_CALL(receiver_mock_, Register(kMediumMessageId, An<IReceiver::MediumMessageReceivedCallback>())).Times(1);
    EXPECT_CALL(receiver_mock_, StartListening()).Times(1);

    unit->Register(kShortMessageId, IReceiver::ShortMessageReceivedCallback{});
    unit->Register(kMediumMessageId, IReceiver::MediumMessageReceivedCallback{});
    EXPECT_TRUE(unit->StartListening());
}

}  // namespace
}  // namespace score::mw::com::message_passing
