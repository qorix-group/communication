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
#include "score/mw/com/performance_benchmarks/macro_benchmark/config_parser.h"
#include "score/json/internal/model/any.h"
#include "score/mw/com/performance_benchmarks/macro_benchmark/common_resources.h"
#include "score/mw/log/logging.h"
#include <exception>
#include <map>
#include <optional>
#include <string_view>
#include <utility>

namespace
{
std::string_view gLogContext;

std::optional<score::mw::com::test::ServiceFinderMode> ParseServiceFinderModeFromString(std::string_view mode)
{

    if (mode == "POLLING")
    {
        return score::mw::com::test::ServiceFinderMode::POLLING;
    }
    if (mode == "ASYNC")
    {
        return score::mw::com::test::ServiceFinderMode::ASYNC;
    }

    score::mw::log::LogError(gLogContext) << "Unidentified ServiceFinderMode " << mode;
    score::mw::log::LogError(gLogContext) << "only allowed strings are POLLING and ASYNC" << mode;
    return std::nullopt;
}

score::json::Any parse_json_from_file(std::string_view path)
{
    score::json::JsonParser json_parser;

    auto json_res = json_parser.FromFile(path);
    if (!json_res.has_value())
    {
        score::mw::log::LogError(gLogContext) << "Can not create a json from: " << path;
        score::mw::log::LogError(gLogContext) << json_res.error();
        std::exit(EXIT_FAILURE);
    }

    return std::move(json_res).value();
}

auto find_json_key(std::string_view key, const score::json::Any& json_root)
{
    const auto& top_level_object = json_root.As<score::json::Object>().value().get();

    const auto value_it = top_level_object.find(key);
    return std::make_pair(value_it, value_it != top_level_object.end());
}

template <typename ElementType>
ElementType cast_json_any_to_type(const score::json::Any& value_as_any)
{
    auto value_result = value_as_any.As<ElementType>();

    if (!value_result.has_value())
    {
        score::mw::log::LogError(gLogContext) << "key: could not be interpreted as the provided type.";
        score::mw::log::LogError(gLogContext) << value_result.error();
        score::mw::com::test::test_failure("failed during json parsing.", gLogContext);
    }
    return value_result.value();
}

template <typename ElementType>
ElementType parse_json_key(std::string_view key, const score::json::Any& json_root)
{
    const auto [search_res, search_success] = find_json_key(key, json_root);
    if (!search_success)
    {
        score::mw::log::LogError(gLogContext) << "key: " << key << " could not be found";
        score::mw::com::test::test_failure("failed during json parsing.", gLogContext);
    }

    const auto& value_as_any = search_res->second;
    return cast_json_any_to_type<ElementType>(value_as_any);
}

score::mw::com::test::DurationUnit ParseDurationUnitFromString(std::string_view unit_sv)
{
    if (unit_sv == "ms")
    {
        return score::mw::com::test::DurationUnit::ms;
    }
    if (unit_sv == "s")
    {
        return score::mw::com::test::DurationUnit::s;
    }
    if (unit_sv == "sample_count")
    {
        return score::mw::com::test::DurationUnit::sample_count;
    }
    score::mw::com::test::test_failure(
        "could not parse run_duration unit, not one of allowed values, ms, s, sample_count", gLogContext);
    std::terminate();
}

}  // namespace

namespace score::mw::com::test
{
ClientConfig ParseClientConfig(std::string_view path, std::string_view log_context)
{
    ::gLogContext = log_context;
    auto json_root = parse_json_from_file(path);

    auto number_of_clients = parse_json_key<unsigned int>("number_of_clients", json_root);
    auto read_cycle_time_ms = parse_json_key<int>("read_cycle_time_ms", json_root);
    auto max_num_samples = parse_json_key<unsigned int>("max_num_samples", json_root);

    auto service_finder_mode_str = parse_json_key<std::string_view>("service_finder_mode", json_root);
    auto service_finder_mode_maybe = ParseServiceFinderModeFromString(service_finder_mode_str);
    if (!service_finder_mode_maybe.has_value())
    {
        score::mw::com::test::test_failure("failed during json parsing.", log_context);
    }

    const auto& [run_time_limit_it, run_time_limit_found] = find_json_key("run_time_limit", json_root);
    std::optional<ClientConfig::RunTimeLimit> run_time_limit = std::nullopt;
    if (run_time_limit_found)
    {
        const auto& [duration_it, duration_found] = find_json_key("duration", run_time_limit_it->second);
        if (!duration_found)
        {
            score::mw::com::test::test_failure("failed during json parsing.", log_context);
        }
        const auto duration = cast_json_any_to_type<unsigned int>(duration_it->second);

        const auto& [duration_unit_it, duration_unit_found] = find_json_key("unit", run_time_limit_it->second);
        if (!duration_unit_found)
        {
            score::mw::com::test::test_failure("failed during json parsing.", log_context);
        }
        const auto duration_unit_str = cast_json_any_to_type<std::string_view>(duration_unit_it->second);

        const auto duration_unit = ParseDurationUnitFromString(duration_unit_str);
        run_time_limit = {duration, duration_unit};
    }

    return ClientConfig{
        read_cycle_time_ms, number_of_clients, max_num_samples, service_finder_mode_maybe.value(), run_time_limit};
}

ServiceConfig ParseServiceConfig(std::string_view path, std::string_view log_context)
{
    ::gLogContext = log_context;
    auto json_root = parse_json_from_file(path);

    auto number_of_clients = parse_json_key<unsigned int>("number_of_clients", json_root);
    auto send_cycle_time_ms = parse_json_key<unsigned int>("send_cycle_time_ms", json_root);

    return ServiceConfig{
        send_cycle_time_ms,
        number_of_clients,
    };
}
}  // namespace score::mw::com::test
