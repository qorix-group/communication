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
#include "score/mw/com/test/generic_skeleton_method/provider.h"

#include "score/mw/com/impl/generic_skeleton.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"

#include <score/stop_token.hpp>
#include <score/utility.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/generic_skeleton_method_interprocess_notification"};

const impl::InstanceSpecifier kInstanceSpecifier =
    impl::InstanceSpecifier::Create(std::string{"test/generic_skeleton_method/TestMethods"}).value();

constexpr std::string_view kMethodName{"with_in_args_and_return"};
constexpr std::string_view kEventName{"counter_value"};

}  // namespace

bool run_provider(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        std::cerr << "Provider: Could not create ProcessSynchronizer" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 1. Create GenericSkeleton with one event and one method
    const impl::EventInfo event_info{kEventName,
                                     impl::DataTypeMetaInfo{sizeof(std::int32_t), alignof(std::int32_t)}};
    const impl::MethodInfo method_info{kMethodName};
    auto skeleton_result =
        impl::GenericSkeleton::Create(kInstanceSpecifier, {.events = {&event_info, 1}, .methods = {&method_info, 1}});
    if (!skeleton_result.has_value())
    {
        std::cerr << "Provider: Could not create GenericSkeleton: " << skeleton_result.error() << std::endl;
        return EXIT_FAILURE;
    }
    auto skeleton = std::move(skeleton_result).value();

    // Step 2. Register a TypeErasedHandler that computes a + b, writes the result, and sends an event.
    // For int32_t(int32_t, int32_t), the in-args span has two consecutive int32_t values
    // at byte offsets 0 and 4 (no padding needed since alignof(int32_t) == sizeof(int32_t) == 4).
    auto method_it = skeleton.GetMethods().find(kMethodName);
    if (method_it == skeleton.GetMethods().cend())
    {
        std::cerr << "Provider: Method not found in skeleton map" << std::endl;
        return EXIT_FAILURE;
    }

    auto event_it = skeleton.GetEvents().find(kEventName);
    if (event_it == skeleton.GetEvents().cend())
    {
        std::cerr << "Provider: Event not found in skeleton map" << std::endl;
        return EXIT_FAILURE;
    }
    auto& event = event_it->second;

    impl::SkeletonMethodBinding::TypeErasedHandler handler =
        [&event](std::optional<score::cpp::span<std::byte>> in_args,
                 std::optional<score::cpp::span<std::byte>> return_buf) {
            std::cout << "Provider: handler invoked" << std::endl;

            std::int32_t a{};
            std::int32_t b{};
            // NOLINTNEXTLINE(score-banned-function)
            std::memcpy(&a, in_args.value().data(), sizeof(std::int32_t));
            // NOLINTNEXTLINE(score-banned-function)
            std::memcpy(&b, in_args.value().data() + sizeof(std::int32_t), sizeof(std::int32_t));
            std::cout << "Provider:   deserialized a = " << a << ", b = " << b << std::endl;

            std::int32_t result = a + b;
            std::cout << "Provider:   computed result = " << result << std::endl;
            // NOLINTNEXTLINE(score-banned-function)
            std::memcpy(return_buf.value().data(), &result, sizeof(std::int32_t));

            auto alloc_result = event.Allocate();
            if (alloc_result.has_value())
            {
                // NOLINTNEXTLINE(score-banned-function)
                std::memcpy(alloc_result.value().Get(), &result, sizeof(std::int32_t));
                score::cpp::ignore = event.Send(std::move(alloc_result.value()));
                std::cout << "Provider:   event sent with value = " << result << std::endl;
            }
            else
            {
                std::cerr << "Provider:   failed to allocate event sample" << std::endl;
            }
        };

    const auto register_result = method_it->second.RegisterHandler(std::move(handler));
    if (!register_result.has_value())
    {
        std::cerr << "Provider: Failed to register handler: " << register_result.error() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Provider: Handler registered successfully" << std::endl;

    // Step 3. Offer service
    const auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Provider: Could not offer service: " << offer_result.error() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Provider: Service offered successfully" << std::endl;

    // Step 4. Wait for consumer to finish
    std::cout << "Provider: Ready for method calls" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        std::cerr << "Provider: WaitForConsumerToFinish was stopped by stop_token instead of notification" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Provider: Shutting down" << std::endl;
    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
