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

cc_gtest_unit_test(
    name = "test_type_factories_test",
    srcs = ["test_type_factories_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":test_type_factories",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":test_type_factories_test",
    ],
    test_suites_from_sub_packages = [],
    visibility = ["//score/mw/com/impl:__pkg__"],
)
