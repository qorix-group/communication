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


def test_generic_proxy_method_end_to_end(sut):
    """
    End-to-end test using generic APIs on both provider and consumer sides.

    The provider creates a GenericSkeleton with a TypeErasedHandler for
    "with_in_args_and_return" that returns the sum of two int32_t arguments.

    The consumer creates a GenericProxy with GenericProxyMethodInfo describing
    the in-args (8 bytes / align 4) and return type (4 bytes / align 4), then
    calls GenericProxyMethod::Call with raw byte spans and verifies the result.

    Test is successful if both processes exit with code 0 and no crashes occur.
    """
    with sut.start_process(
        "./bin/main_provider", cwd="/opt/GenericSkeletonMethodProviderApp/"
    ) as provider:
        with sut.start_process(
            "./bin/main_generic_consumer",
            cwd="/opt/GenericSkeletonMethodGenericConsumerApp/",
        ) as consumer:
            assert consumer.wait_for_exit(timeout=120) == 0
            assert provider.wait_for_exit(timeout=120) == 0
