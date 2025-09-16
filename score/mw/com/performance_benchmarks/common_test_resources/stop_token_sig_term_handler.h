#ifndef SCORE_MW_COM_PERFORMANCE_BENCHMARKS_COMMON_TEST_RESOURCES_SIGTERMHANDLER_H
#define SCORE_MW_COM_PERFORMANCE_BENCHMARKS_COMMON_TEST_RESOURCES_SIGTERMHANDLER_H

#include <score/stop_token.hpp>

namespace score::mw::com
{

bool SetupStopTokenSigTermHandler(score::cpp::stop_source& stop_test);

}  // namespace score::mw::com

#endif  // SCORE_MW_COM_PERFORMANCE_BENCHMARKS_COMMON_TEST_RESOURCES_SIGTERMHANDLER_H
