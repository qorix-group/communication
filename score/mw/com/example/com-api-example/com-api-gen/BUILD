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
load("@rules_rust//rust:defs.bzl", "rust_library")

# Wrapper library that selects between mock and lola implementations
rust_library(
    name = "com-api-gen",
    srcs = ["com_api_gen.rs"],
    crate_features = ["lola"],
    crate_name = "com_api_gen",
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "//score/mw/com/example/com-api-example/com-api-gen-lola",
        "//score/mw/com/example/com-api-example/com-api-gen-mock",
    ],
)

# Mock variant for testing
rust_library(
    name = "com-api-gen-mock-variant",
    srcs = ["com_api_gen.rs"],
    crate_features = ["mock"],
    crate_name = "com_api_gen",
    tags = ["manual"],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "//score/mw/com/example/com-api-example/com-api-gen-lola",
        "//score/mw/com/example/com-api-example/com-api-gen-mock",
    ],
)
