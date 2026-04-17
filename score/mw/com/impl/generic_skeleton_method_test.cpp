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
#include "score/mw/com/impl/generic_skeleton_method.h"

#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_method.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include "score/result/result.h"

#include <score/span.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class EmptySkeleton final : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;
};

class GenericSkeletonMethodTest : public ::testing::Test
{
  public:
    GenericSkeletonMethodTest()
        : skeleton_{std::make_unique<NiceMock<mock_binding::Skeleton>>(),
                    identifier_builder_.CreateValidLolaInstanceIdentifier()}
        , method_{std::make_unique<GenericSkeletonMethod>(
              skeleton_, "dummy_method", std::make_unique<mock_binding::SkeletonMethodFacade>(binding_mock_))}
    {
    }

    DummyInstanceIdentifierBuilder identifier_builder_{};
    NiceMock<mock_binding::SkeletonMethod> binding_mock_{};
    EmptySkeleton skeleton_;
    std::unique_ptr<GenericSkeletonMethod> method_;
};

TEST(GenericSkeletonMethodTraitsTests, IsNotCopyable)
{
    static_assert(!std::is_copy_constructible_v<GenericSkeletonMethod>, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable_v<GenericSkeletonMethod>, "Is wrongly copyable");
}

TEST(GenericSkeletonMethodTraitsTests, IsMoveable)
{
    static_assert(std::is_move_constructible_v<GenericSkeletonMethod>, "Is not move constructible");
    static_assert(std::is_move_assignable_v<GenericSkeletonMethod>, "Is not move assignable");
}

TEST_F(GenericSkeletonMethodTest, ConstructorRegistersInSkeletonBase)
{
    RecordProperty("Description", "Constructor registers the method name in SkeletonBase::methods_.");
    RecordProperty("TestType", "Requirements-based test");

    const auto& methods = SkeletonBaseView{skeleton_}.GetMethods();
    ASSERT_EQ(methods.size(), 1U);
    EXPECT_NE(methods.find("dummy_method"), methods.cend());
}

TEST_F(GenericSkeletonMethodTest, MoveConstructionUpdatesRegistrationInSkeletonBase)
{
    RecordProperty("Description",
                   "Move-constructing a GenericSkeletonMethod updates the SkeletonBase map to point to the new object.");
    RecordProperty("TestType", "Requirements-based test");

    GenericSkeletonMethod moved{std::move(*method_)};

    const auto& methods = SkeletonBaseView{skeleton_}.GetMethods();
    ASSERT_EQ(methods.size(), 1U);
    EXPECT_EQ(&methods.at("dummy_method").get(), static_cast<SkeletonMethodBase*>(&moved));
}

TEST_F(GenericSkeletonMethodTest, RegisterHandlerForwardsToBinding)
{
    RecordProperty("Description",
                   "RegisterHandler passes the TypeErasedHandler to the underlying binding verbatim.");
    RecordProperty("TestType", "Requirements-based test");

    bool handler_invoked = false;
    SkeletonMethodBinding::TypeErasedHandler handler =
        [&handler_invoked](std::optional<score::cpp::span<std::byte>>,
                           std::optional<score::cpp::span<std::byte>>) { handler_invoked = true; };

    EXPECT_CALL(binding_mock_, RegisterHandler(_)).WillOnce(Return(ResultBlank{}));

    const auto result = method_->RegisterHandler(std::move(handler));
    EXPECT_TRUE(result.has_value());
}

TEST_F(GenericSkeletonMethodTest, RegisterHandlerPropagatesBindingError)
{
    RecordProperty("Description", "RegisterHandler propagates the error returned by the binding.");
    RecordProperty("TestType", "Requirements-based test");

    SkeletonMethodBinding::TypeErasedHandler handler =
        [](std::optional<score::cpp::span<std::byte>>, std::optional<score::cpp::span<std::byte>>) {};

    EXPECT_CALL(binding_mock_, RegisterHandler(_))
        .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));

    const auto result = method_->RegisterHandler(std::move(handler));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ComErrc::kBindingFailure);
}

}  // namespace
}  // namespace score::mw::com::impl
