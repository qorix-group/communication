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

load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_test")

rust_binary(
    name = "com-api-example",
    testonly = True,
    srcs = ["basic-consumer-producer.rs"],
    visibility = ["//visibility:public"],
    deps = [
        "//score/mw/com/example/com-api-example/com-api-gen:com-api-gen-mock-variant",
        "//score/mw/com/impl/rust/com-api/com-api:com-api-mock",
    ],
)

rust_test(
    name = "com-api-example-test",
    crate = ":com-api-example",
    tags = ["manual"],
    deps = [
        "//score/mw/com/impl/rust/com-api",
        "//score/mw/com/impl/rust/com-api-gen",
        "//score/mw/com/impl/rust/com-api-runtime",
        "@crate_index//:tokio",
    ],
)
