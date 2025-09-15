#include "score/json/json_parser.h"
#include "score/mw/com/performance_benchmarks/macro_benchmark/common_resources.h"
#include "score/mw/log/logging.h"

#include <string_view>
namespace score::mw::com::test
{

constexpr static std::string_view kJsonParserLogContext{"Blib"};
score::json::Any parse_json_from_file(std::string_view path);

std::optional<json::Object::const_iterator> find_json_key(std::string_view key, const score::json::Any& json_root);

template <typename ElementType>
ElementType cast_json_any_to_type(const score::json::Any& value_as_any)
{
    auto value_result = value_as_any.As<ElementType>();

    if (!value_result.has_value())
    {
        score::mw::log::LogError(kJsonParserLogContext) << "key: could not be interpreted as the provided type.";
        score::mw::log::LogError(kJsonParserLogContext) << value_result.error();
        score::mw::com::test::test_failure("failed during json parsing.", kJsonParserLogContext);
    }
    return value_result.value();
}

template <typename ElementType>
ElementType parse_json_key(std::string_view key, const score::json::Any& json_root)
{
    const auto search_res_optional = find_json_key(key, json_root);
    if (!search_res_optional.has_value())
    {
        score::mw::log::LogError(kJsonParserLogContext) << "key: " << key << " could not be found";
        score::mw::com::test::test_failure("failed during json parsing.", kJsonParserLogContext);
    }

    const auto& value_as_any = search_res_optional.value()->second;
    return cast_json_any_to_type<ElementType>(value_as_any);
}

}  // namespace score::mw::com::test
