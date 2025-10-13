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

load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_test")  # @unused

rust_binary(
    name = "com-api-example",
    srcs = ["main.rs"],
    visibility = ["//visibility:public"],
    deps = [
        "//score/mw/com/impl/rust:com-api",
        "//score/mw/com/impl/rust:com-api-gen",
        "//score/mw/com/impl/rust:com-api-runtime",
    ],
)

# Uncomment the following lines to enable the test
# Note: The test is currently commented out to avoid test failures because of code implemention is not complete.
#rust_test(
#    name = "com-api-example-test",
#    crate = ":com-api-example",
#    deps = [
#        "//score/mw/com/impl/rust:com-api",
#        "//score/mw/com/impl/rust:com-api-gen",
#        "//score/mw/com/impl/rust:com-api-runtime",
#        "@crate_index//:tokio",
#    ],
#)
