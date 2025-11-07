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

load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "proxy_instance_identifier",
    srcs = ["proxy_instance_identifier.cpp"],
    hdrs = ["proxy_instance_identifier.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        "//score/mw/com/impl/configuration",
    ],
)

cc_library(
    name = "skeleton_instance_identifier",
    srcs = ["skeleton_instance_identifier.cpp"],
    hdrs = ["skeleton_instance_identifier.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        "//score/mw/com/impl/configuration",
    ],
)

cc_gtest_unit_test(
    name = "proxy_instance_identifier_test",
    srcs = ["proxy_instance_identifier_test.cpp"],
    deps = [
        ":proxy_instance_identifier",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_instance_identifier_test",
    srcs = ["skeleton_instance_identifier_test.cpp"],
    deps = [
        ":skeleton_instance_identifier",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":proxy_instance_identifier_test",
        ":skeleton_instance_identifier_test",
    ],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
    ],
)
