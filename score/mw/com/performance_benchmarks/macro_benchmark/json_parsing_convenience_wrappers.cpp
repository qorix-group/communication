#include "score/mw/com/performance_benchmarks/macro_benchmark/json_parsing_convenience_wrappers.h"

#include <optional>
#include <string_view>
#include <utility>
namespace score::mw::com::test
{

score::json::Any parse_json_from_file(std::string_view path)
{
    score::json::JsonParser json_parser;

    auto json_res = json_parser.FromFile(path);
    if (!json_res.has_value())
    {
        score::mw::log::LogError(kJsonParserLogContext) << "Can not create a json from: " << path;
        score::mw::log::LogError(kJsonParserLogContext) << json_res.error();
        std::exit(EXIT_FAILURE);
    }

    return std::move(json_res).value();
}

std::optional<json::Object::const_iterator> find_json_key(std::string_view key, const score::json::Any& json_root)
{
    const auto& top_level_object = json_root.As<score::json::Object>();
    if (!top_level_object.has_value())
    {
        return std::nullopt;
    }
    const auto& const_reference_wrapper_to_the_parsed_root = top_level_object.value();
    const auto& parsed_root = const_reference_wrapper_to_the_parsed_root.get();

    json::Object::const_iterator value_it = parsed_root.find(key);
    if (value_it == parsed_root.end())
    {
        return std::nullopt;
    }
    return value_it;
}

}  // namespace score::mw::com::test
