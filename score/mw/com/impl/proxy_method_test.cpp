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
#include "score/mw/com/impl/proxy_method.h"

#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_method.h"
#include "score/mw/com/impl/configuration/test/configuration_store.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{
using testing::Return;

const auto kMethodName{"DummyMethod"};

class ProxyMethodTestFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        proxy_method_binding_mock_ = std::make_unique<mock_binding::ProxyMethod>();
        proxy_method_binding_mock_ptr_ = proxy_method_binding_mock_.get();
        ON_CALL(*proxy_method_binding_mock_ptr_, AllocateInArgs(0))
            .WillByDefault(Return(score::Result<score::cpp::span<std::byte>>{
                score::cpp::span{method_in_args_buffer_.data(), method_in_args_buffer_.size()}}));

        config_store_ = std::make_unique<ConfigurationStore>(
            InstanceSpecifier::Create(std::string{"/my_dummy_instance_specifier"}).value(),
            make_ServiceIdentifierType("foo"),
            QualityType::kASIL_QM,
            LolaServiceTypeDeployment{42U},
            LolaServiceInstanceDeployment{1U});
        proxy_base_ptr_ =
            std::make_unique<ProxyBase>(std::make_unique<mock_binding::Proxy>(), config_store_->GetHandle());
    }

  protected:
    std::unique_ptr<mock_binding::ProxyMethod> proxy_method_binding_mock_;
    mock_binding::ProxyMethod* proxy_method_binding_mock_ptr_;
    alignas(8) std::array<std::byte, 1024> method_in_args_buffer_{};
    std::unique_ptr<ConfigurationStore> config_store_{nullptr};
    std::unique_ptr<ProxyBase> proxy_base_ptr_{nullptr};
};

TEST_F(ProxyMethodTestFixture, Construction_withVoidReturnAndArgsSucceeds)
{
    // Given a method signature with a void return type and three arguments: int, double, char, then we can construct
    // a ProxyMethod instance without errors.
    ProxyMethod<void(int, double, char)> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};
}

TEST_F(ProxyMethodTestFixture, Construction_withNonVoidReturnAndArgsSucceeds)
{
    // Given a method signature with a non-void return type and three arguments: int, double, char, then we can
    // construct a ProxyMethod instance without errors.
    ProxyMethod<bool(int, double, char)> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};
}

TEST_F(ProxyMethodTestFixture, Construction_withVoidReturnAndNoArgsSucceeds)
{
    // Given a method signature with a void return type and no arguments, then we can
    // construct a ProxyMethod instance without errors.
    ProxyMethod<void()> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};
}

TEST_F(ProxyMethodTestFixture, Construction_withNonVoidReturnAndNoArgsSucceeds)
{
    // Given a method signature with a non-void return type and no arguments, then we can
    // construct a ProxyMethod instance without errors.
    ProxyMethod<int()> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};
}

TEST_F(ProxyMethodTestFixture, AllocateInArgs_ReturnsInArgPointersPointingToInArgsAllocatedByBinding)
{
    // Given a ProxyMethod with a return type of void and three arguments: int, double, char
    ProxyMethod<void(int, double, char)> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // Expect that AllocateInArgs is called once for queue position 0 on the binding mock and returns a pointer to our
    // buffer
    EXPECT_CALL(*proxy_method_binding_mock_ptr_, AllocateInArgs(0))
        .WillOnce(Return(score::Result<score::cpp::span<std::byte>>{
            score::cpp::span{method_in_args_buffer_.data(), method_in_args_buffer_.size()}}));

    // when Allocate is called on the ProxyMethod
    auto method_in_arg_ptr_tuple = unit.Allocate();

    // then no error is returned and the returned MethodInArgPtr relate all to queue position 0 and point to the correct
    // locations in the buffer
    auto& pointer1 = std::get<0>(method_in_arg_ptr_tuple.value());
    EXPECT_EQ(pointer1.GetQueuePosition(), 0);
    // then the 2nd argument pointer is after the 1st argument pointer in the buffer (detailed position is not
    // checked here as this is done/verified in TypeErasedStorage tests)
    EXPECT_EQ(reinterpret_cast<std::byte*>(pointer1.get()), &(method_in_args_buffer_[0]));
    auto& pointer2 = std::get<1>(method_in_arg_ptr_tuple.value());
    EXPECT_EQ(pointer2.GetQueuePosition(), 0);
    // and the 3rd argument pointer is after the 1st argument pointer in the buffer (detailed position is not
    // checked here as this is done/verified in TypeErasedStorage tests)
    EXPECT_GT(reinterpret_cast<std::byte*>(pointer2.get()), reinterpret_cast<std::byte*>(pointer1.get()));
    auto& pointer3 = std::get<2>(method_in_arg_ptr_tuple.value());
    EXPECT_EQ(pointer3.GetQueuePosition(), 0);
}

TEST_F(ProxyMethodTestFixture, AllocateInArgs_QueueFullError)
{
    // Given a ProxyMethod with a return type of void and three arguments: int, double, char
    ProxyMethod<void(int, double, char)> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // when Allocate is called on the ProxyMethod for the 1st time
    auto method_in_arg_ptr_tuple = unit.Allocate();

    // expect that no error is returned
    EXPECT_TRUE(method_in_arg_ptr_tuple.has_value());

    // when Allocate is called a 2nd time on the ProxyMethod (while still holding the method in arg pointers from the
    // 1st call)
    auto method_in_arg_ptr_tuple_2 = unit.Allocate();

    // then a CallQueueFull error is returned
    EXPECT_FALSE(method_in_arg_ptr_tuple_2.has_value());
    EXPECT_EQ(method_in_arg_ptr_tuple_2.error(), ComErrc::kCallQueueFull);
}

TEST_F(ProxyMethodTestFixture, AllocateInArgs_BindingError)
{
    // Given a ProxyMethod with a return type of void and three arguments: int, double, char
    auto unit =
        ProxyMethod<void(int, double, char)>{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    auto allocate_with_binding_error = [this, &unit]() {
        // Expect that AllocateInArgs is called once for queue position 0 on the binding mock and returns an error.
        EXPECT_CALL(*proxy_method_binding_mock_ptr_, AllocateInArgs(0))
            .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));
        unit.Allocate();
    };

    // expect that the program terminates, because of the SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE in ProxyMethodBinding::Allocate().
    // An allocation failure from the binding is unexpected here as we already checked for available queue slots on
    // binding independent level.
    EXPECT_DEATH(allocate_with_binding_error(), ".*");
}

TEST_F(ProxyMethodTestFixture, CallOperator_VoidReturn_WithCopy)
{
    // Given a ProxyMethod with a return type of void and three arguments: int, double, char
    ProxyMethod<void(int, double, char)> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // when call operator is called on the ProxyMethod
    int arg1 = 42;
    double arg2 = 3.14;
    char arg3 = 'a';
    auto call_result = unit(arg1, arg2, arg3);
    // then no error is returned
    EXPECT_TRUE(call_result.has_value());
}

TEST_F(ProxyMethodTestFixture, CallOperator_NonVoidReturn_WithCopy)
{
    // Given a ProxyMethod with a return type of int and three arguments: int, double, char
    ProxyMethod<int(int, double, char)> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // when call operator is called on the ProxyMethod
    int arg1 = 42;
    double arg2 = 3.14;
    char arg3 = 'a';
    auto call_result = unit(arg1, arg2, arg3);
    // then no error is returned
    EXPECT_TRUE(call_result.has_value());
}

TEST_F(ProxyMethodTestFixture, CallOperator_VoidReturn_WithCopyTemporary)
{
    // Given a ProxyMethod with a return type of void and one argument: int
    ProxyMethod<void(int)> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // when call operator is called on the ProxyMethod handing the arg over as temporary
    auto call_result = unit(42);
    // expect, that no error is returned
    EXPECT_TRUE(call_result.has_value());
}

TEST_F(ProxyMethodTestFixture, CallOperator_VoidReturn_NoArgs)
{
    // Given a ProxyMethod with a return type of void and no arguments
    ProxyMethod<void()> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // when call operator is called on the ProxyMethod
    auto call_result = unit();
    // then no error is returned
    EXPECT_TRUE(call_result.has_value());
}

TEST_F(ProxyMethodTestFixture, CallOperator_NonVoidReturn_NoArgs)
{
    // Given a ProxyMethod with a return type of non-void and no arguments
    ProxyMethod<int()> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // when call operator is called on the ProxyMethod
    auto call_result = unit();
    // then no error is returned
    EXPECT_TRUE(call_result.has_value());
}

TEST_F(ProxyMethodTestFixture, CallOperator_NonVoidReturn_ZeroCopy)
{
    // Given a ProxyMethod with a return type of int and three arguments: int, double, char
    ProxyMethod<int(int, double, char)> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // when Allocate is called on the ProxyMethod
    auto method_in_arg_ptr_tuple = unit.Allocate();
    // expect that no error is returned
    ASSERT_TRUE(method_in_arg_ptr_tuple.has_value());

    // when filling the allocated argument storage and calling the call operator with the allocated argument pointers
    auto [method_in_arg_ptr_0, method_in_arg_ptr_1, method_in_arg_ptr_2] = std::move(method_in_arg_ptr_tuple.value());
    *method_in_arg_ptr_0 = 42;
    *method_in_arg_ptr_1 = 3.14;
    *method_in_arg_ptr_2 = 'a';
    auto call_result =
        unit(std::move(method_in_arg_ptr_0), std::move(method_in_arg_ptr_1), std::move(method_in_arg_ptr_2));
    // then no error is returned
    EXPECT_TRUE(call_result.has_value());
}

TEST_F(ProxyMethodTestFixture, ProxyMethodView_WithInArgsAndNonVoidReturnType)
{
    // Given a ProxyMethod with a return type of int and three arguments: int, double, char
    ProxyMethod<int(int, double, char)> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // then we can create a view on the ProxyMethod
    ProxyMethodView proxy_method_view{unit};

    // and when we call GetTypeErasedInAgs
    auto type_erased_in_args = proxy_method_view.GetTypeErasedInAgs();
    // then we get a valid TypeErasedDataTypeInfo
    EXPECT_TRUE(type_erased_in_args.has_value());
    // and when we call GetTypeErasedReturnType
    auto type_erased_return_type = proxy_method_view.GetTypeErasedReturnType();
    // then we get a valid TypeErasedDataTypeInfo
    EXPECT_TRUE(type_erased_return_type.has_value());
}

TEST_F(ProxyMethodTestFixture, ProxyMethodView_WithoutInArgs)
{
    // Given a ProxyMethod with a return type of int and no arguments.
    ProxyMethod<int()> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // then we can create a view on the ProxyMethod
    ProxyMethodView proxy_method_view{unit};

    // and when we call GetTypeErasedInAgs
    auto type_erased_in_args = proxy_method_view.GetTypeErasedInAgs();
    // then we get no TypeErasedDataTypeInfo for in-arguments
    EXPECT_FALSE(type_erased_in_args.has_value());
    // and when we call GetTypeErasedReturnType
    auto type_erased_return_type = proxy_method_view.GetTypeErasedReturnType();
    // then we get a valid TypeErasedDataTypeInfo
    EXPECT_TRUE(type_erased_return_type.has_value());
}

TEST_F(ProxyMethodTestFixture, ProxyMethodView_WithVoidReturnType)
{
    // Given a ProxyMethod with a return type of void and three arguments: int, double, char
    ProxyMethod<void(int, double, char)> unit{*proxy_base_ptr_, std::move(proxy_method_binding_mock_), kMethodName};

    // then we can create a view on the ProxyMethod
    ProxyMethodView proxy_method_view{unit};

    // and when we call GetTypeErasedInAgs
    auto type_erased_in_args = proxy_method_view.GetTypeErasedInAgs();
    // then we get a valid TypeErasedDataTypeInfo for in-arguments
    EXPECT_TRUE(type_erased_in_args.has_value());
    // and when we call GetTypeErasedReturnType
    auto type_erased_return_type = proxy_method_view.GetTypeErasedReturnType();
    // then we get no TypeErasedDataTypeInfo for return type
    EXPECT_FALSE(type_erased_return_type.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl
