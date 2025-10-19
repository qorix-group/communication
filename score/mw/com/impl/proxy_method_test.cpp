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

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{
using testing::Return;

const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
const ServiceTypeDeployment kEmptyTypeDeployment{score::cpp::blank{}};
const ServiceIdentifierType kFooService{make_ServiceIdentifierType("foo")};
const ServiceInstanceDeployment kEmptyInstanceDeployment{kFooService,
                                                         LolaServiceInstanceDeployment{LolaServiceInstanceId{10U}},
                                                         QualityType::kASIL_QM,
                                                         kInstanceSpecifier};
ProxyBase kEmptyProxy(std::make_unique<mock_binding::Proxy>(),
                      make_HandleType(make_InstanceIdentifier(kEmptyInstanceDeployment, kEmptyTypeDeployment)));
const auto kMethodName{"DummyMethod"};

TEST(ProxyMethodTest, Construct)
{
    auto proxy_method_binding_mock = std::make_unique<mock_binding::ProxyMethod>();
    // auto* proxy_method_binding_mock_ptr = proxy_method_binding_mock.get();

    auto unit = ProxyMethod<void(int, double, char)>{kEmptyProxy, std::move(proxy_method_binding_mock), kMethodName};
}

TEST(ProxyMethodTest, Allocate)
{
    auto proxy_method_binding_mock = std::make_unique<mock_binding::ProxyMethod>();
    auto* proxy_method_binding_mock_ptr = proxy_method_binding_mock.get();
    alignas(8) std::array<std::byte, 1024> method_in_args_buffer{};

    // Given a ProxyMethod with a return type of void and three arguments: int, double, char
    auto unit = ProxyMethod<void(int, double, char)>{kEmptyProxy, std::move(proxy_method_binding_mock), kMethodName};

    // Expect that AllocateInArgs is called once for queue position 0 on the binding mock and returns a pointer to our
    // buffer
    EXPECT_CALL(*proxy_method_binding_mock_ptr, AllocateInArgs(0))
        .WillOnce(Return(score::Result<void*>{method_in_args_buffer.data()}));

    // when Allocate is called on the ProxyMethod
    auto method_in_arg_ptr_tuple = unit.Allocate();

    // then no error is returned and the returned MethodInArgPtr relate all to queue position 0 and point to the correct
    // locations in the buffer
    auto& pointer1 = std::get<0>(method_in_arg_ptr_tuple.value());
    EXPECT_EQ(pointer1.GetQueuePosition(), 0);
    EXPECT_EQ(reinterpret_cast<std::byte*>(pointer1.get()), &(method_in_args_buffer[0]));
    auto& pointer2 = std::get<1>(method_in_arg_ptr_tuple.value());
    EXPECT_EQ(pointer2.GetQueuePosition(), 0);
    EXPECT_GT(reinterpret_cast<std::byte*>(pointer2.get()), reinterpret_cast<std::byte*>(pointer1.get()));
    auto& pointer3 = std::get<2>(method_in_arg_ptr_tuple.value());
    EXPECT_EQ(pointer3.GetQueuePosition(), 0);
}

TEST(ProxyMethodTest, Allocate_QueueFullError)
{
    auto proxy_method_binding_mock = std::make_unique<mock_binding::ProxyMethod>();
    auto* proxy_method_binding_mock_ptr = proxy_method_binding_mock.get();
    alignas(8) std::array<std::byte, 1024> method_in_args_buffer{};

    // Given a ProxyMethod with a return type of void and three arguments: int, double, char
    auto unit = ProxyMethod<void(int, double, char)>{kEmptyProxy, std::move(proxy_method_binding_mock), kMethodName};

    // Expect that AllocateInArgs is called once for queue position 0 on the binding mock and returns a pointer to our
    // buffer
    EXPECT_CALL(*proxy_method_binding_mock_ptr, AllocateInArgs(0))
        .WillOnce(Return(score::Result<void*>{method_in_args_buffer.data()}));

    // when Allocate is called on the ProxyMethod
    auto method_in_arg_ptr_tuple = unit.Allocate();

    EXPECT_TRUE(method_in_arg_ptr_tuple.has_value());

    // when Allocate is called a 2nd time on the ProxyMethod
    auto method_in_arg_ptr_tuple_2 = unit.Allocate();

    EXPECT_FALSE(method_in_arg_ptr_tuple_2.has_value());
    EXPECT_EQ(method_in_arg_ptr_tuple_2.error(), ComErrc::kCallQueueFull);
}

TEST(ProxyMethodDeathTest, Allocate_BindingError)
{
    auto proxy_method_binding_mock = std::make_unique<mock_binding::ProxyMethod>();
    auto* proxy_method_binding_mock_ptr = proxy_method_binding_mock.get();

    // Given a ProxyMethod with a return type of void and three arguments: int, double, char
    auto unit = ProxyMethod<void(int, double, char)>{kEmptyProxy, std::move(proxy_method_binding_mock), kMethodName};

    auto allocate_with_binding_error = [&unit, proxy_method_binding_mock_ptr]() {
        // Expect that AllocateInArgs is called once for queue position 0 on the binding mock and returns an error.
        EXPECT_CALL(*proxy_method_binding_mock_ptr, AllocateInArgs(0))
            .WillOnce(Return(MakeUnexpected(ComErrc::kBindingFailure)));
        unit.Allocate();
    };

    // expect that the program terminates, because of the SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE in ProxyMethodBinding::Allocate().
    // An allocation failure from the binding is unexpected here as we already checked for available queue slots on
    // binding independent level.
    EXPECT_DEATH(allocate_with_binding_error(), ".*");
}

TEST(ProxyMethodTest, CallOperator_WithCopy)
{
    auto proxy_method_binding_mock = std::make_unique<mock_binding::ProxyMethod>();
    auto* proxy_method_binding_mock_ptr = proxy_method_binding_mock.get();
    alignas(8) std::array<std::byte, 1024> method_in_args_buffer{};

    // Given a ProxyMethod with a return type of void and three arguments: int, double, char
    auto unit = ProxyMethod<void(int, double, char)>{kEmptyProxy, std::move(proxy_method_binding_mock), kMethodName};

    // Expect that AllocateInArgs is called once for queue position 0 on the binding mock and returns a pointer to our
    // buffer
    EXPECT_CALL(*proxy_method_binding_mock_ptr, AllocateInArgs(0))
        .WillOnce(Return(score::Result<void*>{method_in_args_buffer.data()}));

    // when call operator is called on the ProxyMethod
    int arg1 = 42;
    double arg2 = 3.14;
    char arg3 = 'a';
    auto call_result = unit(arg1, arg2, arg3);
    EXPECT_TRUE(call_result.has_value());
}

}  // namespace
}  // namespace score::mw::com::impl
