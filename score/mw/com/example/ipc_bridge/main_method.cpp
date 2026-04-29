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
#include "assert_handler.h"
#include "sample_method_caller.h"

#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>

#include "score/mw/com/runtime.h"

using namespace std::chrono_literals;

struct MethodParams
{
    score::cpp::optional<std::string> mode;
    score::cpp::optional<std::string> instance_manifest;
    score::cpp::optional<std::chrono::milliseconds> cycle_time;
    score::cpp::optional<unsigned long> cycle_num;
};

/// \brief Extract optional value from command line arguments
template <typename ParsedType, typename SavedType = ParsedType>
score::cpp::optional<SavedType> GetMethodValueIfProvided(const boost::program_options::variables_map& args,
                                                         std::string arg_string)
{
    return (args.count(arg_string) > 0U) ? static_cast<SavedType>(args[arg_string].as<ParsedType>())
                                         : score::cpp::optional<SavedType>();
}

/// \brief Parse command line arguments for method example
MethodParams ParseMethodCommandLineArguments(const int argc, const char** argv)
{
    namespace po = boost::program_options;

    po::options_description options;

    options.add_options()("help,h", "Display the help message");
    options.add_options()(
        "num-cycles,n",
        po::value<std::size_t>()->default_value(10U),
        "Number of method calls to make. 0 indicates no limit for provider.");
    options.add_options()("mode,m",
                          po::value<std::string>(),
                          "Set to either 'provider' or 'consumer' to determine the role of the process");
    options.add_options()("cycle-time,t",
                          po::value<std::size_t>(),
                          "Cycle time in milliseconds (required for method example)");
    options.add_options()(
        "service_instance_manifest,s", po::value<std::string>(), "Path to the com configuration file");

    po::variables_map args;
    const auto parsed_args =
        po::command_line_parser{argc, argv}
            .options(options)
            .style(po::command_line_style::unix_style | po::command_line_style::allow_long_disguise)
            .run();
    po::store(parsed_args, args);

    if (args.count("help") > 0U)
    {
        std::cerr << "Method-based Communication Example\n\n";
        std::cerr << "This example demonstrates synchronous method-based communication,\n"
                  << "contrasting with the event-based asynchronous communication.\n\n";
        std::cerr << options << std::endl;
        std::exit(EXIT_SUCCESS);
    }

    return {GetMethodValueIfProvided<std::string>(args, "mode"),
            GetMethodValueIfProvided<std::string>(args, "service_instance_manifest"),
            GetMethodValueIfProvided<std::size_t, std::chrono::milliseconds>(args, "cycle-time"),
            GetMethodValueIfProvided<std::size_t>(args, "num-cycles")};
}

int main(const int argc, const char** argv)
{
    score::mw::com::SetupAssertHandler();
    MethodParams params = ParseMethodCommandLineArguments(argc, argv);

    if (!params.mode.has_value() || !params.cycle_time.has_value())
    {
        std::cerr << "Mode and cycle time must be specified\n";
        std::cerr << "Usage: " << argv[0] << " --mode [provider|consumer] --cycle-time <ms> [--num-cycles <n>]\n";
        return EXIT_FAILURE;
    }

    if (params.instance_manifest.has_value())
    {
        const std::string& manifest_path = params.instance_manifest.value();
        score::StringLiteral runtime_args[2u] = {"-service_instance_manifest", manifest_path.c_str()};
        score::mw::com::runtime::InitializeRuntime(2, runtime_args);
    }
    else
    {
        score::mw::com::runtime::InitializeRuntime(0, nullptr);
    }

    const std::string& mode = params.mode.value();
    const std::chrono::milliseconds cycle_time = params.cycle_time.value();
    const std::size_t cycle_num = params.cycle_num.has_value() ? params.cycle_num.value() : 10U;

    // Default instance specifier for method example
    const score::mw::com::InstanceSpecifier instance_specifier =
        score::mw::com::InstanceSpecifier::Create(std::string{"example/ipc_bridge/methods"}).value();

    score::mw::com::MethodCaller method_caller;

    if (mode == "provider")
    {
        std::cout << "Starting Method-based Service Provider...\n";
        return method_caller.RunAsServiceProvider(instance_specifier, cycle_time, cycle_num);
    }
    else if (mode == "consumer")
    {
        std::cout << "Starting Method-based Service Consumer...\n";
        return method_caller.RunAsServiceConsumer(instance_specifier, cycle_time, cycle_num);
    }
    else
    {
        std::cerr << "Invalid mode: " << mode << ". Use 'provider' or 'consumer'\n";
        return EXIT_FAILURE;
    }
}
