#include "score/mw/com/performance_benchmarks/macro_benchmark/config_parser.h"
#include "score/json/internal/model/any.h"
#include "score/mw/log/logging.h"
#include <optional>

namespace
{
std::string_view log_context;

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

    score::mw::log::LogError(log_context) << "Unidentified ServiceFinderMode " << mode;
    score::mw::log::LogError(log_context) << "only allowed strings are POLLING and ASYNC" << mode;
    return std::nullopt;
}

score::json::Any parse_json_from_file(std::string_view path)
{
    score::json::JsonParser json_parser;

    auto json_res = json_parser.FromFile(path);
    if (!json_res.has_value())
    {
        score::mw::log::LogError(log_context) << "Can not create a json from: " << path;
        score::mw::log::LogError(log_context) << json_res.error();
        std::exit(EXIT_FAILURE);
    }

    return std::move(json_res).value();
}

template <typename ElementType>
ElementType parse_json_key(std::string_view key, const score::json::Any& json_root)
{
    const auto& top_level_object = json_root.As<score::json::Object>().value().get();

    const auto& value_it = top_level_object.find(key);
    if (value_it == top_level_object.end())
    {
        score::mw::log::LogError(log_context) << "key: " << key << " could not be found";
        score::mw::com::test::test_failure("failed during json parsing.", log_context);
    }
    const auto& value_as_any = value_it->second;
    auto value_result = value_as_any.As<ElementType>();

    if (!value_result.has_value())
    {
        score::mw::log::LogError(log_context) << "key: " << key << " could not be interpreted as the provided type.";
        score::mw::log::LogError(log_context) << value_result.error();
        score::mw::com::test::test_failure("failed during json parsing.", log_context);
    }
    return value_result.value();
}
}  // namespace

namespace score::mw::com::test
{
ClientConfig ParseClientConfig(std::string_view path, std::string_view log_context)
{
    ::log_context = log_context;
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

    auto number_of_samples_to_receive = parse_json_key<unsigned long>("number_of_samples_to_receive", json_root);

    return ClientConfig{read_cycle_time_ms,
                        number_of_clients,
                        max_num_samples,
                        service_finder_mode_maybe.value(),
                        number_of_samples_to_receive};
}

ServiceConfig ParseServiceConfig(std::string_view path, std::string_view log_context)
{
    ::log_context = log_context;
    auto json_root = parse_json_from_file(path);

    auto number_of_clients = parse_json_key<unsigned int>("number_of_clients", json_root);
    auto send_cycle_time_ms = parse_json_key<unsigned int>("send_cycle_time_ms", json_root);

    return ServiceConfig{
        send_cycle_time_ms,
        number_of_clients,
    };
}
}  // namespace score::mw::com::test
