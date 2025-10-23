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
load("@score-baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "test_type_factories",
    testonly = True,
    srcs = ["test_type_factories.cpp"],
    hdrs = ["test_type_factories.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl/configuration",
    ],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:handle_type",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "//score/mw/com/impl/plumbing:sample_ptr",
    ],
)

cc_library(
    name = "proxy_event_mock",
    srcs = ["proxy_event_mock.cpp"],
    hdrs = ["proxy_event_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:event_receive_handler",
        "//score/mw/com/impl:subscription_state",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "@score-baselibs//score/language/futurecpp",
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "proxy_event_mock_impl",
    testonly = True,
    srcs = ["proxy_event_mock_impl.cpp"],
    hdrs = ["proxy_event_mock_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":proxy_event_mock",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "proxy_field_mock",
    srcs = ["proxy_field_mock.cpp"],
    hdrs = ["proxy_field_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:event_receive_handler",
        "//score/mw/com/impl:subscription_state",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "@score-baselibs//score/language/futurecpp",
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "proxy_field_mock_impl",
    testonly = True,
    srcs = ["proxy_field_mock_impl.cpp"],
    hdrs = ["proxy_field_mock_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":proxy_field_mock",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "skeleton_mock",
    srcs = ["skeleton_mock.cpp"],
    hdrs = ["skeleton_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "@score-baselibs//score/language/futurecpp",
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "skeleton_mock_impl",
    testonly = True,
    srcs = ["skeleton_mock_impl.cpp"],
    hdrs = ["skeleton_mock_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":skeleton_mock",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "skeleton_event_mock",
    srcs = ["skeleton_event_mock.cpp"],
    hdrs = ["skeleton_event_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "@score-baselibs//score/language/futurecpp",
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "skeleton_event_mock_impl",
    testonly = True,
    srcs = ["skeleton_event_mock_impl.cpp"],
    hdrs = ["skeleton_event_mock_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":skeleton_event_mock",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "skeleton_field_mock",
    srcs = ["skeleton_field_mock.cpp"],
    hdrs = ["skeleton_field_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "@score-baselibs//score/language/futurecpp",
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "skeleton_field_mock_impl",
    testonly = True,
    srcs = ["skeleton_field_mock_impl.cpp"],
    hdrs = ["skeleton_field_mock_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "skeleton_field_mock",
        "@googletest//:gtest",
    ],
)

cc_gtest_unit_test(
    name = "test_type_factories_test",
    srcs = ["test_type_factories_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":test_type_factories",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_mock_test",
    srcs = ["skeleton_mock_test.cpp"],
    deps = [
        ":skeleton_mock_impl",
        ":test_type_factories",
        "//score/mw/com/impl:skeleton_base",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":skeleton_mock_test",
        ":test_type_factories_test",
    ],
    test_suites_from_sub_packages = [],
    visibility = ["//score/mw/com/impl:__pkg__"],
)
