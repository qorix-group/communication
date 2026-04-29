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
#ifndef SCORE_MW_COM_IPC_BRIDGE_SAMPLE_METHOD_CALLER_H
#define SCORE_MW_COM_IPC_BRIDGE_SAMPLE_METHOD_CALLER_H

#include "datatype.h"

#include <score/optional.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

namespace score::mw::com
{

/// \brief Sample application demonstrating method-based communication in IPC bridge
///
/// This class shows how to use methods for synchronous request-response communication
/// pattern, contrasting with the event-based asynchronous communication in EventSenderReceiver.
///
/// Methods allow:
/// - Service consumer (proxy) to call methods on a service provider (skeleton)
/// - Service provider to register handlers that process method calls and return responses
/// - Both copy-based and zero-copy calling conventions
class MethodCaller
{
  public:
    /// \brief Run as service provider (skeleton) that receives method calls
    ///
    /// \param instance_specifier Identifier for the service instance
    /// \param cycle_time Time between processing method calls
    /// \param num_cycles Number of cycles to run (0 = infinite)
    /// \return EXIT_SUCCESS if successful, EXIT_FAILURE otherwise
    int RunAsServiceProvider(const score::mw::com::InstanceSpecifier& instance_specifier,
                             const std::chrono::milliseconds cycle_time,
                             const std::size_t num_cycles);

    /// \brief Run as service consumer (proxy) that calls methods
    ///
    /// \tparam ProxyType The proxy type to use (default: IpcBridgeProxy)
    /// \param instance_specifier Identifier for the service instance
    /// \param cycle_time Cycle time between method calls
    /// \param num_cycles Number of method calls to make
    /// \param try_writing_to_shared_memory Whether to attempt writing to shared memory (for testing)
    /// \return EXIT_SUCCESS if successful, EXIT_FAILURE otherwise
    template <typename ProxyType = score::mw::com::IpcBridgeProxy>
    int RunAsServiceConsumer(const score::mw::com::InstanceSpecifier& instance_specifier,
                             const std::chrono::milliseconds cycle_time,
                             const std::size_t num_cycles,
                             bool try_writing_to_shared_memory = false);

  private:
    std::mutex method_handler_mutex_{};
    std::atomic<bool> method_ready_{false};
    std::atomic<std::size_t> method_calls_received_{0};
};

}  // namespace score::mw::com

#endif  // SCORE_MW_COM_IPC_BRIDGE_SAMPLE_METHOD_CALLER_H
