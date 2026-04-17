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
#include "score/mw/com/test/generic_skeleton_method/generic_consumer.h"

#include "score/mw/com/impl/generic_proxy.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"

#include "score/memory/data_type_size_info.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <array>
#include <iostream>
#include <memory>
#include <mutex>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/generic_skeleton_method_interprocess_notification"};

const impl::InstanceSpecifier kInstanceSpecifier =
    impl::InstanceSpecifier::Create(std::string{"test/generic_skeleton_method/TestMethods"}).value();

constexpr std::string_view kMethodName{"with_in_args_and_return"};
constexpr std::string_view kEventName{"counter_value"};

constexpr std::int32_t kTestValueA = 42;
constexpr std::int32_t kTestValueB = 17;

}  // namespace

int run_generic_consumer()
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        std::cerr << "GenericConsumer: Could not create ProcessSynchronizer" << std::endl;
        return EXIT_FAILURE;
    }

    // Step 1. Find the service via async discovery
    std::cout << "GenericConsumer: Starting service discovery..." << std::endl;

    struct FindState
    {
        std::unique_ptr<impl::GenericProxy::HandleType> handle;
        std::mutex mtx;
        std::condition_variable cv;
        bool ready{false};
    };
    FindState find_state;

    auto find_result = impl::GenericProxy::StartFindService(
        [&find_state](impl::ServiceHandleContainer<impl::GenericProxy::HandleType> handles,
                      impl::FindServiceHandle find_handle) noexcept {
            std::lock_guard lock{find_state.mtx};
            if (handles.size() == 1U)
            {
                find_state.handle = std::make_unique<impl::GenericProxy::HandleType>(handles[0]);
                score::cpp::ignore = impl::GenericProxy::StopFindService(find_handle);
            }
            else
            {
                std::cerr << "GenericConsumer: Expected 1 handle, got " << handles.size() << std::endl;
            }
            find_state.ready = true;
            find_state.cv.notify_all();
        },
        kInstanceSpecifier);

    if (!find_result.has_value())
    {
        std::cerr << "GenericConsumer: StartFindService failed: " << find_result.error() << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    {
        std::unique_lock lock{find_state.mtx};
        const bool found = find_state.cv.wait_for(lock, std::chrono::seconds(10), [&find_state] {
            return find_state.ready;
        });
        if (!found || find_state.handle == nullptr)
        {
            std::cerr << "GenericConsumer: Service not found within timeout" << std::endl;
            process_synchronizer_result->Notify();
            return EXIT_FAILURE;
        }
    }

    std::cout << "GenericConsumer: Service found" << std::endl;

    // Step 2. Create GenericProxy with method size info for int32_t(int32_t, int32_t)
    // Two int32_t in-args packed consecutively: size=8, alignment=4
    // One int32_t return value: size=4, alignment=4
    const impl::GenericProxyMethodInfo method_info{
        kMethodName,
        score::memory::DataTypeSizeInfo{sizeof(std::int32_t) * 2U, alignof(std::int32_t)},
        score::memory::DataTypeSizeInfo{sizeof(std::int32_t), alignof(std::int32_t)}};

    const std::array<impl::GenericProxyMethodInfo, 1U> methods{method_info};
    auto proxy_result = impl::GenericProxy::Create(*find_state.handle, {methods.data(), methods.size()});
    if (!proxy_result.has_value())
    {
        std::cerr << "GenericConsumer: GenericProxy::Create failed: " << proxy_result.error() << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }
    auto proxy = std::move(proxy_result).value();
    std::cout << "GenericConsumer: GenericProxy created" << std::endl;

    // Step 3. Subscribe to the event (before calling method, so the sent sample is available)
    auto event_it = proxy.GetEvents().find(kEventName);
    if (event_it == proxy.GetEvents().cend())
    {
        std::cerr << "GenericConsumer: Event '" << kEventName << "' not found in proxy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }
    const auto subscribe_result = event_it->second.Subscribe(1U);
    if (!subscribe_result.has_value())
    {
        std::cerr << "GenericConsumer: Subscribe failed: " << subscribe_result.error() << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }
    std::cout << "GenericConsumer: Subscribed to event '" << kEventName << "'" << std::endl;

    // Step 4. Look up the method and call it
    auto method_it = proxy.GetMethods().find(kMethodName);
    if (method_it == proxy.GetMethods().cend())
    {
        std::cerr << "GenericConsumer: Method '" << kMethodName << "' not found in proxy" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    // Step 4a. Allocate in-arg storage directly in the binding's call-queue buffer (zero-copy)
    constexpr std::size_t kQueuePos{0U};
    auto in_args_result = method_it->second.AllocateInArgs(kQueuePos);
    if (!in_args_result.has_value())
    {
        std::cerr << "GenericConsumer: AllocateInArgs failed: " << in_args_result.error() << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }
    // NOLINTNEXTLINE(score-banned-function)
    std::memcpy(in_args_result.value().data(), &kTestValueA, sizeof(std::int32_t));
    // NOLINTNEXTLINE(score-banned-function)
    std::memcpy(in_args_result.value().data() + sizeof(std::int32_t), &kTestValueB, sizeof(std::int32_t));

    // Step 4b. Allocate return-type storage at the same queue slot; read from it after Call()
    auto return_type_result = method_it->second.AllocateReturnType(kQueuePos);
    if (!return_type_result.has_value())
    {
        std::cerr << "GenericConsumer: AllocateReturnType failed: " << return_type_result.error() << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }
    auto return_buf = return_type_result.value();

    std::cout << "GenericConsumer: Calling method with a=" << kTestValueA << ", b=" << kTestValueB << std::endl;
    const auto call_result = method_it->second.Call(kQueuePos);
    if (!call_result.has_value())
    {
        std::cerr << "GenericConsumer: Call failed: " << call_result.error() << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    std::int32_t received{};
    // NOLINTNEXTLINE(score-banned-function)
    std::memcpy(&received, return_buf.data(), sizeof(std::int32_t));

    const std::int32_t expected = kTestValueA + kTestValueB;
    if (received != expected)
    {
        std::cerr << "GenericConsumer: Expected " << expected << " but got " << received << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    std::cout << "GenericConsumer: Method returned correct result: " << received << std::endl;

    // Step 5. Read the event sample that was sent by the provider inside the method handler
    std::int32_t event_value{};
    bool event_received{false};
    const auto get_samples_result =
        event_it->second.GetNewSamples([&event_value, &event_received](impl::SamplePtr<void> sample) noexcept {
            // NOLINTNEXTLINE(score-banned-function)
            std::memcpy(&event_value, sample.Get(), sizeof(std::int32_t));
            event_received = true;
        },
                                       1U);

    if (!get_samples_result.has_value())
    {
        std::cerr << "GenericConsumer: GetNewSamples failed: " << get_samples_result.error() << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    if (!event_received)
    {
        std::cerr << "GenericConsumer: No event samples received" << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    if (event_value != expected)
    {
        std::cerr << "GenericConsumer: Event value: expected " << expected << " but got " << event_value << std::endl;
        process_synchronizer_result->Notify();
        return EXIT_FAILURE;
    }

    std::cout << "GenericConsumer: Event delivered correct value: " << event_value << std::endl;

    process_synchronizer_result->Notify();
    return EXIT_SUCCESS;
}

}  // namespace score::mw::com::test
