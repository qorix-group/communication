/*******************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#include "score/mw/com/test/fields/set_and_notifier/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/fields/set_and_notifier/datatypes/notifier_only_enabled_field.h"
#include "score/mw/com/test/fields/set_and_notifier/datatypes/set_and_notifier_enabled_field.h"
#include "score/mw/com/test/fields/set_and_notifier/test_constants.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace score::mw::com::test
{
namespace
{

template <typename ProxyFieldType>
void CallGetAndCheckValue(ProxyFieldType& proxy_field, const std::int32_t expected_value)
{
    const auto get_result = proxy_field.Get();
    if (!get_result.has_value())
    {
        FailTest("Consumer: Get() failed: ", get_result.error());
    }
    if (*(get_result.value()) != expected_value)
    {
        FailTest("Consumer: Get() returned ", *(get_result.value()), " but expected ", expected_value);
    }
    std::cout << "\nConsumer: Get() returned expected value " << expected_value << std::endl;
}

template <typename ProxyFieldType>
void CallSetAndCheckReturnValue(ProxyFieldType& proxy_field,
                                const std::int32_t set_request_value,
                                const std::int32_t expected_accepted_value)
{
    const auto set_result = proxy_field.Set(set_request_value);
    if (!set_result.has_value())
    {
        FailTest("Consumer: Set() failed: ", set_result.error());
    }
    const std::int32_t accepted_value = *(set_result.value());
    if (accepted_value != expected_accepted_value)
    {
        FailTest("Consumer: Set() returned accepted value ", accepted_value, " but expected ", expected_accepted_value);
    }
    std::cout << "\nConsumer: Set() returned expected accepted value " << accepted_value << std::endl;
}

void run_notifier_consumer(const score::cpp::stop_token& stop_token)
{
    auto done_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create done ProcessSynchronizer");
    }
    ExitFunctionGuard exit_guard{[&done_synchronizer_result]() {
        done_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<NotifierOnlyEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "notifier");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Register unified receive handler that verifies samples arrive in the expected order
    std::cout << "\nConsumer: Step 2 - Register unified receive handler" << std::endl;
    ProxyEventReceiver field_receiver{proxy.notifier_only_enabled_field};

    // Step 3. Register state change handler
    std::cout << "\nConsumer: Step 3 - Register state change handler" << std::endl;
    ProxyEventStateChangeNotifier subscription_notifier{proxy.notifier_only_enabled_field};

    // Step 4. Subscribe to field with enough buffer for all samples the provider will send
    std::cout << "\nConsumer: Step 4 - Subscribe to field" << std::endl;
    const auto subscribe_result = proxy.notifier_only_enabled_field.Subscribe(kTotalNumValuesToSend);
    if (!subscribe_result.has_value())
    {
        FailTest("Consumer: Subscribe failed in notifier scenario: ", subscribe_result.error());
    }

    // Step 5. Wait for subscription
    std::cout << "\nConsumer: Step 5 - Wait for subscription" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in notifier scenario");
    }

    // Step 6. Wait for all expected samples
    std::cout << "\nConsumer: Step 6 - Wait for all expected samples" << std::endl;
    const std::vector<std::int32_t> values_to_receive = {kInitialValue, 20, 30, 35};
    if (!field_receiver.WaitForSamples(stop_token, values_to_receive))
    {
        FailTest("Consumer: Did not receive all expected samples in notifier scenario");
    }

    proxy.notifier_only_enabled_field.Unsubscribe();
}

void run_set_and_notifier_consumer(const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerDoneShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create ProcessSynchronizer");
    }
    auto set_done_synchronizer_result = ProcessSynchronizer::Create(kSetDoneShmPath);
    if (!set_done_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create set-done ProcessSynchronizer");
    }
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result, &set_done_synchronizer_result]() {
        process_synchronizer_result->Notify();
        set_done_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    ProxyContainer<SetAndNotifierEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(kInstanceSpecifier, "set_and_notifier");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Register unified receive handler that verifies samples arrive in the expected order
    std::cout << "\nConsumer: Step 2 - Register unified receive handler" << std::endl;
    ProxyEventReceiver field_receiver{proxy.set_and_notifier_enabled_field};

    // Step 3. Register state change handler
    std::cout << "\nConsumer: Step 3 - Register state change handler" << std::endl;
    ProxyEventStateChangeNotifier subscription_notifier{proxy.set_and_notifier_enabled_field};

    // Step 4. Subscribe to field with enough buffer for all samples the provider will send
    std::cout << "\nConsumer: Step 4 - Subscribe to field" << std::endl;
    std::ignore = proxy.set_and_notifier_enabled_field.Subscribe(kTotalNumValuesToSend);

    // Step 5. Wait for subscription
    std::cout << "\nConsumer: Step 5 - Wait for subscription and verify initial value" << std::endl;
    if (!subscription_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("Consumer: Subscription failed in set scenario");
    }

    // Step 6. Wait for all expected samples
    std::cout << "\nConsumer: Step 6 - Wait for all expected samples" << std::endl;
    {
        const std::vector<std::int32_t> values_to_receive = {kInitialValue, 20, 30, 35};
        if (!field_receiver.WaitForSamples(stop_token, values_to_receive))
        {
            FailTest("Consumer: Did not receive all expected samples in notifier scenario");
        }
    }

    // Step 7. Set new field value and verify accepted value matches expected transformed value
    std::cout << "\nConsumer: Step 7 - Set field value and verify accepted value" << std::endl;
    const auto expected_accepted_set_value = (kSetRequestValue * 2) + 1;
    CallSetAndCheckReturnValue(proxy.set_and_notifier_enabled_field, kSetRequestValue, expected_accepted_set_value);

    // Step 8. Notify provider that Set() was called
    std::cout << "\nConsumer: Step 8 - Notify provider that Set() was verified" << std::endl;
    set_done_synchronizer_result->Notify();

    // Step 9. Wait for additional samples sent after the provider receives the set-complete signal
    std::cout << "\nConsumer: Step 9 - Wait for all expected samples" << std::endl;
    const std::vector<std::int32_t> values_to_receive = {expected_accepted_set_value, 10, 100};
    if (!field_receiver.WaitForSamples(stop_token, values_to_receive))
    {
        FailTest("Consumer: Did not receive all expected samples in notifier scenario");
    }

    proxy.set_and_notifier_enabled_field.Unsubscribe();
}

}  // namespace

void run_consumer(const score::cpp::stop_token& stop_token, TestMode mode)
{
    switch (mode)
    {
        case TestMode::kNotifier:
            run_notifier_consumer(stop_token);
            return;
        case TestMode::kSetAndNotifier:
            run_set_and_notifier_consumer(stop_token);
            return;
    }
}

}  // namespace score::mw::com::test
