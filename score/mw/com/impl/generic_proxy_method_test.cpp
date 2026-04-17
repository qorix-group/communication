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
#include <cstdint>
#include <cstring>
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

TEST_F(GenericProxyMethodTest, CallForwardsInArgsToBinding)
{
    RecordProperty("Description", "Call writes in-args into the binding's AllocateInArgs buffer and invokes DoCall.");
    RecordProperty("TestType", "Requirements-based test");

    std::array<std::byte, 4U> shm_in_args{};
    std::array<std::byte, 4U> shm_return{};
    const std::int32_t input_val{42};
    const std::int32_t return_val{99};
    // NOLINTNEXTLINE(score-banned-function)
    std::memcpy(shm_return.data(), &return_val, sizeof(return_val));

    EXPECT_CALL(binding_mock_, AllocateInArgs(0U))
        .WillOnce(Return(score::Result<score::cpp::span<std::byte>>{shm_in_args}));
    EXPECT_CALL(binding_mock_, AllocateReturnType(0U))
        .WillOnce(Return(score::Result<score::cpp::span<std::byte>>{shm_return}));
    EXPECT_CALL(binding_mock_, DoCall(0U)).WillOnce(Return(ResultBlank{}));

    std::array<std::byte, 4U> in_arg_buf{};
    std::array<std::byte, 4U> return_buf{};
    // NOLINTNEXTLINE(score-banned-function)
    std::memcpy(in_arg_buf.data(), &input_val, sizeof(input_val));

    const auto result = method_->Call(in_arg_buf, return_buf);
    EXPECT_TRUE(result.has_value());

    std::int32_t received{};
    // NOLINTNEXTLINE(score-banned-function)
    std::memcpy(&received, return_buf.data(), sizeof(received));
    EXPECT_EQ(received, return_val);
}

TEST_F(GenericProxyMethodTest, CallPropagatesAllocateInArgsError)
{
    RecordProperty("Description", "Call returns the error from AllocateInArgs without calling DoCall.");
    RecordProperty("TestType", "Requirements-based test");

    EXPECT_CALL(binding_mock_, AllocateInArgs(0U))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));
    EXPECT_CALL(binding_mock_, DoCall(_)).Times(0);

    std::array<std::byte, 4U> in_buf{};
    std::array<std::byte, 4U> ret_buf{};
    const auto result = method_->Call(in_buf, ret_buf);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericProxyMethodTest, CallPropagatesDoCallError)
{
    RecordProperty("Description", "Call returns the error from DoCall.");
    RecordProperty("TestType", "Requirements-based test");

    std::array<std::byte, 4U> shm_in{};
    std::array<std::byte, 4U> shm_ret{};
    EXPECT_CALL(binding_mock_, AllocateInArgs(0U)).WillOnce(Return(score::Result<score::cpp::span<std::byte>>{shm_in}));
    EXPECT_CALL(binding_mock_, AllocateReturnType(0U))
        .WillOnce(Return(score::Result<score::cpp::span<std::byte>>{shm_ret}));
    EXPECT_CALL(binding_mock_, DoCall(0U)).WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    std::array<std::byte, 4U> in_buf{};
    std::array<std::byte, 4U> ret_buf{};
    const auto result = method_->Call(in_buf, ret_buf);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

}  // namespace
}  // namespace score::mw::com::impl
