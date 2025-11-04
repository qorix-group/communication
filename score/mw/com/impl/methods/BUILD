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

load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "method_signature_element_ptr",
    srcs = ["method_signature_element_ptr.cpp"],
    hdrs = ["method_signature_element_ptr.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
    ],
)

cc_library(
    name = "proxy_method_base",
    srcs = ["proxy_method_base.cpp"],
    hdrs = ["proxy_method_base.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":method_signature_element_ptr",
        ":proxy_method_binding",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:proxy_base",
        "//score/mw/com/impl/util:type_erased_storage",
        "@score_baselibs//score/containers:dynamic_array",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory/shared:data_type_size_info",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "proxy_method",
    srcs = ["proxy_method.cpp"],
    hdrs = ["proxy_method.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":method_signature_element_ptr",
        ":proxy_method_base",
        ":proxy_method_binding",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:proxy_base",
        "//score/mw/com/impl/util:type_erased_storage",
        "@score_baselibs//score/containers:dynamic_array",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "proxy_method_binding",
    srcs = [
        "proxy_method_binding.cpp",
    ],
    hdrs = [
        "proxy_method_binding.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/mock_binding:__pkg__",
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl/util:type_erased_storage",
        "@score_baselibs//score/memory/shared:data_type_size_info",
        "@score_baselibs//score/result",
    ],
)

cc_gtest_unit_test(
    name = "method_signature_element_ptr_test",
    srcs = ["method_signature_element_ptr_test.cpp"],
    deps = [
        ":method_signature_element_ptr",
    ],
)

cc_unit_test(
    name = "proxy_method_test",
    srcs = ["proxy_method_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        ":proxy_method",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":method_signature_element_ptr_test",
        ":proxy_method_test",
    ],
    visibility = ["//score/mw/com:__pkg__"],
)
