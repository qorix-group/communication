# *******************************************************************************
# Copyright (c) 2024 Contributors to the Eclipse Foundation
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

load("@//third_party/traceability/bazel:safety_analysis.bzl", "safety_analysis")
load("@lobster//:lobster.bzl", "lobster_trlc")
load("@trlc//:trlc.bzl", "trlc_requirements", "trlc_requirements_test")
load("//third_party/traceability/tools/source_code_linker:collect_source_files.bzl", "parse_source_files_for_links")

# Failures Modes
trlc_requirements(
    name = "failure_modes",
    srcs = [
        "failure_modes.trlc",
    ],
    spec = [
        "@//third_party/traceability/config/trlc/score_model:requirement_metamodel",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "failure_modes_test",
    reqs = [
        ":failure_modes",
    ],
    visibility = ["//visibility:public"],
)

lobster_trlc(
    name = "failure_modes_lobster",
    config = "@//third_party/traceability/config/lobster:safety_analysis_conf",
    requirements = [
        ":failure_modes",
    ],
    visibility = ["//visibility:public"],
)

# Root Causes
filegroup(
    name = "root_causes",
    srcs = [
        "@//score/mw/com/requirements/safety_analysis/root_causes:general",
        "@//score/mw/com/requirements/safety_analysis/root_causes/proxy:root_causes_proxy",
        "@//score/mw/com/requirements/safety_analysis/root_causes/skeleton:root_causes_skeleton",
        "@//score/mw/com/requirements/safety_analysis/root_causes/smart_pointer_sample_allocate_ptr:smart_pointer_sample_allocate_ptr",
    ],
    visibility = ["//visibility:public"],
)

parse_source_files_for_links(
    name = "root_causes_tags",
    srcs_and_deps = [":root_causes"],
)

# Control Measures
# AoUs
trlc_requirements(
    name = "control_measures",
    srcs = [
        "aou.trlc",
    ],
    spec = [
        "@//third_party/traceability/config/trlc/score_model:requirement_metamodel",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "control_measures_test",
    reqs = [
        ":control_measures",
    ],
    visibility = ["//visibility:public"],
)

lobster_trlc(
    name = "control_measures_lobster",
    config = "@//third_party/traceability/config/lobster:safety_analysis_conf",
    requirements = [
        ":control_measures",
    ],
    visibility = ["//visibility:public"],
)

safety_analysis(
    name = "mw_com_safety_analysis",
    controlmeasures = [":control_measures"],
    failuremodes = [":failure_modes"],
    fta = [":root_causes_tags"],
    public_api = ["@//third_party/traceability/doc/sample_library/design:public_api"],
)
