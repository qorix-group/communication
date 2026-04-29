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
#include "sample_method_caller.h"

#include "score/mw/com/impl/generic_proxy.h"
//#include "score/mw/com/impl/generic_proxy_method.h"
#include "score/mw/com/impl/handle_type.h"

#include "score/mw/com/impl/bindings/lola/shm_path_builder.h"  // ADD THIS
#include "score/memory/shared/shared_memory_factory.h"  //Add

#include <score/assert.hpp>
#include <score/hash.hpp>
#include <score/optional.hpp>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

using namespace std::chrono_literals;

namespace score::mw::com
{

namespace
{

/// \brief Helper class to format output messages
std::ostream& operator<<(std::ostream& stream, const InstanceSpecifier& instance_specifier)
{
    stream << instance_specifier.ToString();
    return stream;
}

/// \brief Variadic template helper for string formatting
template <typename T>
void ToStringImpl(std::ostream& o, T t)
{
    o << t;
}

template <typename T, typename... Args>
void ToStringImpl(std::ostream& o, T t, Args... args)
{
    ToStringImpl(o, t);
    ToStringImpl(o, args...);
}

template <typename... Args>
std::string ToString(Args... args)
{
    std::ostringstream oss;
    ToStringImpl(oss, args...);
    return oss.str();
}

/// \brief Compute hash of route information
std::size_t ComputeRouteHash(const RouteInformation& route_info)
{
    constexpr std::size_t START_HASH = 64738U;
    std::size_t hash_value = START_HASH;

    // Hash the segments array
    const std::ptrdiff_t buffer_size = reinterpret_cast<const std::uint8_t*>(&*route_info.route_segments.cend()) -
                                       reinterpret_cast<const std::uint8_t*>(&*route_info.route_segments.cbegin());
    hash_value = score::cpp::hash_bytes_fnv1a(static_cast<const void*>(route_info.route_segments.data()),
                                              static_cast<std::size_t>(buffer_size),
                                              hash_value);

    return hash_value;
}

#if 0
std::string GetMethodShmPathName(std::uint32_t service_id, 
                                  std::uint32_t instance_id,
                                  std::uint32_t proxy_counter)
{
    // Format: /lola-methods-{service_id:016x}-{instance_id:05x}-{proxy_counter:05x}-00000
    std::ostringstream oss;
    oss << "/lola-methods-" 
        << std::hex << std::setw(16) << std::setfill('0') << service_id
        << "-" << std::hex << std::setw(5) << std::setfill('0') << instance_id
        << "-" << std::hex << std::setw(5) << std::setfill('0') << proxy_counter
        << "-00000";
    return oss.str();
}
#endif

#if 0
/// \brief Helper class to format output messages
std::ostream& operator<<(std::ostream& stream, const InstanceSpecifier& instance_specifier)
{
    stream << instance_specifier.ToString();
    return stream;
}
#endif 


/// \brief Find service handle from instance specifier
template <typename ProxyType = IpcBridgeProxy>
score::Result<impl::HandleType> GetHandleFromSpecifier(const InstanceSpecifier& instance_specifier)
{
    std::cout << ToString(instance_specifier, ": Looking for service provider\n");
    ServiceHandleContainer<impl::HandleType> handles{};
    do
    {
        auto handles_result = ProxyType::FindService(instance_specifier);
        if (!handles_result.has_value())
        {
            return MakeUnexpected<impl::HandleType>(std::move(handles_result.error()));
        }
        handles = std::move(handles_result).value();
        if (handles.size() == 0)
        {
            std::this_thread::sleep_for(500ms);
        }
    } while (handles.size() == 0);

    std::cout << ToString(instance_specifier, ": Found service provider\n");
    return handles.front();
}

}  // namespace

template <typename ProxyType>
int MethodCaller::RunAsServiceConsumer(const score::mw::com::InstanceSpecifier& instance_specifier,
                                       const std::chrono::milliseconds cycle_time,
                                       const std::size_t num_cycles,
                                       bool try_writing_to_shared_memory)
{

    auto handle_result = GetHandleFromSpecifier(instance_specifier);
    //const auto method_shm_path = ProxyType::GetMethodChannelShmName(instance_specifier);
   // memory::shared::SharedMemoryFactory::RemoveStaleArtefacts(method_shm_path);
    if (!handle_result.has_value())
    {
        std::cerr << "Unable to find service: " << instance_specifier
                  << ". Failed with error: " << handle_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }



    auto handle = handle_result.value();
    std::vector<std::string_view> enabledMethod{"calculate_route_hash"};
    auto proxy_result = ProxyType::Create(std::move(handle), enabledMethod);


    if (!proxy_result.has_value())
    {
        std::cerr << "Unable to construct proxy: " << proxy_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }

    auto& proxy = proxy_result.value();
    proxy.calculate_route_hash.Allocate();
    //proxy.MarkSubscribed();

    std::cout << ToString(instance_specifier, ": Starting method calls\n");

    for (std::size_t cycle = 0U; cycle < num_cycles; ++cycle)
    {
        const auto cycle_start_time = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(cycle_time);

        // Call method with input arguments and expect return value
        std::cout << ToString(instance_specifier, ": Calling method with input parameters (cycle ", cycle, ")\n");

        auto route_result = proxy.calculate_route_hash(static_cast<std::uint32_t>(cycle));
        if (!route_result.has_value())
        {
            std::cerr << ToString(instance_specifier, ": Method call failed: ", route_result.error(), "\n");
            return EXIT_FAILURE;
        }

        const auto returned_hash = *route_result.value();
        std::cout << ToString(instance_specifier,
                              ": Method returned hash value: ",
                              returned_hash,
                              " for cycle ",
                              cycle,
                              "\n");

        const auto cycle_duration = std::chrono::steady_clock::now() - cycle_start_time;
        std::cout << ToString(instance_specifier,
                              ": Cycle duration ",
                              std::chrono::duration_cast<std::chrono::milliseconds>(cycle_duration).count(),
                              "ms\n");
    }

    std::cout << ToString(instance_specifier, ": Completed all method calls\n");
    return EXIT_SUCCESS;
}

int MethodCaller::RunAsServiceProvider(const score::mw::com::InstanceSpecifier& instance_specifier,
                                       const std::chrono::milliseconds cycle_time,
                                       const std::size_t num_cycles)
{
    std::cout << __func__ << __LINE__ << " : " << " IN" << std::endl;
    auto create_result = IpcBridgeSkeleton::Create(instance_specifier);
    std::cout << __func__ << __LINE__ << " : " << " after Create" << std::endl;
    if (!create_result.has_value())
    {
        std::cerr << "Unable to construct skeleton: " << create_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }
    auto& skeleton = create_result.value();
    std::cout << __func__ << __LINE__ << " : " << " after create_result.value()" << std::endl;

    // Register method handler for calculate_route_hash
    // This handler will be called when a consumer invokes this method
    const auto register_result = skeleton.calculate_route_hash.RegisterHandler(
        [&instance_specifier](std::uint32_t segment_id) -> std::uint64_t {
            std::cout << ToString(instance_specifier, ": Method handler called with segment_id: ", segment_id, "\n");

            // Simulate route hash calculation based on segment_id
            // In a real application, this would calculate the actual hash of route data
            std::uint64_t calculated_hash = 64738U;
            calculated_hash = score::cpp::hash_bytes_fnv1a(
                static_cast<const void*>(&segment_id), sizeof(segment_id), calculated_hash);

            std::cout << ToString(instance_specifier, ": Calculated hash: ", calculated_hash, "\n");
            return calculated_hash;
        });

    if (!register_result.has_value())
    {
        std::cerr << "Unable to register method handler: " << register_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }

    const auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer service for skeleton: " << offer_result.error() << ", bailing!\n";
        return EXIT_FAILURE;
    }

    std::cout << ToString(instance_specifier, ": Service offered, ready to handle method calls\n");

    // Keep the service provider running for the specified number of cycles
    for (std::size_t cycle = 0U; cycle < num_cycles || num_cycles == 0U; ++cycle)
    {
        std::this_thread::sleep_for(cycle_time);

        {
            std::lock_guard lock{method_handler_mutex_};
            method_ready_ = true;
        }

        if (num_cycles != 0U && cycle % 10 == 0)
        {
            std::cout << ToString(instance_specifier, ": Service provider cycle ", cycle, "\n");
        }
    }

    std::cout << ToString(instance_specifier, ": Stopping service...\n");
    skeleton.StopOfferService();
    std::cout << ToString(instance_specifier, ": Service stopped, terminating\n");

    return EXIT_SUCCESS;
}

template int MethodCaller::RunAsServiceConsumer<IpcBridgeProxy>(
    const score::mw::com::InstanceSpecifier&,
    const std::chrono::milliseconds,
    const std::size_t,
    bool);

}  // namespace score::mw::com
