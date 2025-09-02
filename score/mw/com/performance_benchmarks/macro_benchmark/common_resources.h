#ifndef SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MACRO_BENCHMARK_COMMON_RESOURCES_H
#define SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MACRO_BENCHMARK_COMMON_RESOURCES_H

#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"

#include <score/stop_token.hpp>

#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>
namespace score::mw::com::test
{

namespace
{

using CounterType = std::atomic<unsigned int>;

constexpr std::string_view kLoLaBenchmarkInstanceSpecifier = "test/lolabenchmark";
constexpr std::string_view kProxyFinishedFlagShmPath{"benchmark_proxy_finished_flag"};

}  // namespace

struct TestCommanLineArguments
{
    std::string config_path;
    std::string service_instance_manifest;
};

bool GetStopTokenAndSetUpSigTermHandler(score::cpp::stop_source& test_stop_source);

std::optional<SharedMemoryObjectCreator<CounterType>> GetSharedFlag();

void test_failure(std::string_view msg, std::string_view log_context);
void test_success(std::string_view msg, std::string_view log_context);

std::optional<TestCommanLineArguments> ParseCommandLineArgs(int, const char**, std::string_view);

void InitializeRuntime(const std::string& path);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MACRO_BENCHMARK_COMMON_RESOURCES_H
