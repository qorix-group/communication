# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************
load("@score-baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/performance_benchmarks/macro_benchmark/config_generator:config_generator_rule.bzl", "make_configs")

alias(
    name = "logging.json",
    actual = "//score/mw/com/performance_benchmarks/macro_benchmark/config:logging_json",
    visibility = [
        "//score/mw/com/performance_benchmarks/macro_benchmark:__pkg__",
        "//platform/aas/test/pas/perf_tests/tools/artificial_load/apps/lola_ipc:__pkg__",
    ],
)

alias(
    name = "mw_com_config.json",
    actual = "//score/mw/com/performance_benchmarks/macro_benchmark/config:mw_com_config_json",
    visibility = [
        "//score/mw/com/performance_benchmarks/macro_benchmark:__pkg__",
        "//platform/aas/test/pas/perf_tests/tools/artificial_load/apps/lola_ipc:__pkg__",
    ],
)

alias(
    name = "joined_config.json",
    actual = "//score/mw/com/performance_benchmarks/macro_benchmark/config:joined_benchmark_config_json",
)

cc_library(
    name = "lola_interface",
    srcs = ["lola_interface.cpp"],
    hdrs = ["lola_interface.h"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__pkg__"],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "common_resources",
    srcs = ["common_resources.cpp"],
    hdrs = ["common_resources.h"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__pkg__"],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:shared_memory_object_creator",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "@score-baselibs//score/mw/log",
        "@boost.program_options",
    ],
)

cc_library(
    name = "config_parser",
    srcs = ["config_parser.cpp"],
    hdrs = ["config_parser.h"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__pkg__"],
    deps = [
        ":common_resources",
        "@score-baselibs//score/json",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:shared_memory_object_creator",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "@score-baselibs//score/mw/log",
    ],
)

make_configs(
    name = "make_configs",
    config_json_path = ":joined_config.json",
    mw_com_config_path = ":mw_com_config.json",
    out_dir = "gen_config",
)

cc_binary(
    name = "lola_benchmarking_service",
    srcs = [
        "lola_benchmarking_service.cpp",
    ],
    args = [
        "$(location :make_configs)/service_benchmark_config.json",
        "$(location :make_configs)/service_mw_com_config.json",
    ],
    data = [
        ":logging.json",
        ":make_configs",
    ],
    env = {"MW_LOG_CONFIG_FILE": "$(location logging.json)"},
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = [
        "//score/mw/com/performance_benchmarks/macro_benchmark:__pkg__",
        "//platform/aas/test/pas/perf_tests/tools/artificial_load/apps/lola_ipc:__pkg__",
    ],
    deps = [
        "config_parser",
        ":common_resources",
        ":lola_interface",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:shared_memory_object_creator",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        "@score-baselibs//score/mw/log",
    ],
)

cc_binary(
    name = "lola_benchmarking_client",
    srcs = [
        "lola_benchmarking_client.cpp",
    ],
    args = [
        "$(location :make_configs)/client_benchmark_config.json",
        "$(location :make_configs)/client_mw_com_config.json",
    ],
    data = [
        ":logging.json",
        ":make_configs",
    ],
    env = {"MW_LOG_CONFIG_FILE": "$(location logging.json)"},
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = [
        "//score/mw/com/performance_benchmarks/macro_benchmark:__pkg__",
        "//platform/aas/test/pas/perf_tests/tools/artificial_load/apps/lola_ipc:__pkg__",
    ],
    deps = [
        ":common_resources",
        ":config_parser",
        ":lola_interface",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:shared_memory_object_creator",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        "@score-baselibs//score/mw/log",
    ],
)

py_binary(
    name = "perf_run",
    srcs = ["perf_run.py"],
    args = [
        "$(location logging.json)",
        "gen_config",
    ],
    data = [
        ":logging.json",
        ":lola_benchmarking_client",
        ":lola_benchmarking_service",
        ":make_configs",
    ],
    env = {"MW_LOG_CONFIG_FILE": "$(location logging.json)"},
)
