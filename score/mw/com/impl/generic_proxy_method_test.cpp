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
#include "score/mw/com/impl/generic_proxy_method.h"

#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_method.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include "score/result/result.h"

#include <score/span.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <memory>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class EmptyProxy final : public ProxyBase
{
  public:
    using ProxyBase::ProxyBase;
};

class GenericProxyMethodTest : public ::testing::Test
{
  public:
    GenericProxyMethodTest()
        : proxy_{std::make_unique<NiceMock<mock_binding::Proxy>>(),
                 make_HandleType(identifier_builder_.CreateValidLolaInstanceIdentifier())}
        , method_{std::make_unique<GenericProxyMethod>(
              proxy_, "dummy_method", std::make_unique<mock_binding::ProxyMethodFacade>(binding_mock_))}
    {
    }

    DummyInstanceIdentifierBuilder identifier_builder_{};
    NiceMock<mock_binding::ProxyMethod> binding_mock_{};
    EmptyProxy proxy_;
    std::unique_ptr<GenericProxyMethod> method_;
};

TEST(GenericProxyMethodTraitsTests, IsNotCopyable)
{
    static_assert(!std::is_copy_constructible_v<GenericProxyMethod>, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable_v<GenericProxyMethod>, "Is wrongly copyable");
}

TEST(GenericProxyMethodTraitsTests, IsMoveable)
{
    static_assert(std::is_move_constructible_v<GenericProxyMethod>, "Is not move constructible");
    static_assert(std::is_move_assignable_v<GenericProxyMethod>, "Is not move assignable");
}

TEST_F(GenericProxyMethodTest, ConstructorRegistersInProxyBase)
{
    RecordProperty("Description", "Constructor registers the method name in ProxyBase::methods_.");
    RecordProperty("TestType", "Requirements-based test");

    const auto& methods = ProxyBaseView{proxy_}.GetMethods();
    ASSERT_EQ(methods.size(), 1U);
    EXPECT_NE(methods.find("dummy_method"), methods.cend());
}

TEST_F(GenericProxyMethodTest, MoveConstructionUpdatesRegistrationInProxyBase)
{
    RecordProperty("Description",
                   "Move-constructing a GenericProxyMethod updates the ProxyBase map to point to the new object.");
    RecordProperty("TestType", "Requirements-based test");

    GenericProxyMethod moved{std::move(*method_)};

    const auto& methods = ProxyBaseView{proxy_}.GetMethods();
    ASSERT_EQ(methods.size(), 1U);
    EXPECT_EQ(&methods.at("dummy_method").get(), static_cast<ProxyMethodBase*>(&moved));
}

TEST_F(GenericProxyMethodTest, AllocateInArgsForwardsToBinding)
{
    RecordProperty("Description", "AllocateInArgs forwards queue_position to the binding and returns its span.");
    RecordProperty("TestType", "Requirements-based test");

    std::array<std::byte, 4U> shm_in_args{};
    EXPECT_CALL(binding_mock_, AllocateInArgs(0U))
        .WillOnce(Return(score::Result<score::cpp::span<std::byte>>{shm_in_args}));

    const auto result = method_->AllocateInArgs(0U);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().data(), shm_in_args.data());
}

TEST_F(GenericProxyMethodTest, AllocateReturnTypeForwardsToBinding)
{
    RecordProperty("Description", "AllocateReturnType forwards queue_position to the binding and returns its span.");
    RecordProperty("TestType", "Requirements-based test");

    std::array<std::byte, 4U> shm_return{};
    EXPECT_CALL(binding_mock_, AllocateReturnType(0U))
        .WillOnce(Return(score::Result<score::cpp::span<std::byte>>{shm_return}));

    const auto result = method_->AllocateReturnType(0U);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().data(), shm_return.data());
}

TEST_F(GenericProxyMethodTest, CallForwardsQueuePositionToDoCall)
{
    RecordProperty("Description", "Call forwards queue_position to DoCall on the binding.");
    RecordProperty("TestType", "Requirements-based test");

    EXPECT_CALL(binding_mock_, DoCall(0U)).WillOnce(Return(ResultBlank{}));

    EXPECT_TRUE(method_->Call(0U).has_value());
}

TEST_F(GenericProxyMethodTest, AllocateInArgsPropagatesBindingError)
{
    RecordProperty("Description", "AllocateInArgs propagates errors from the binding.");
    RecordProperty("TestType", "Requirements-based test");

    EXPECT_CALL(binding_mock_, AllocateInArgs(0U))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    const auto result = method_->AllocateInArgs(0U);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericProxyMethodTest, CallPropagatesDoCallError)
{
    RecordProperty("Description", "Call propagates errors from DoCall.");
    RecordProperty("TestType", "Requirements-based test");

    EXPECT_CALL(binding_mock_, DoCall(0U)).WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    const auto result = method_->Call(0U);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

}  // namespace
}  // namespace score::mw::com::impl
