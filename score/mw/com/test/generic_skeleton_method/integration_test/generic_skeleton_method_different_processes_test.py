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


def test_generic_skeleton_method_different_processes(sut):
    """
    Test that a GenericSkeleton-based provider can serve method calls to a typed proxy consumer.

    The test starts two processes.
    The first process creates a GenericSkeleton with a single method ("with_in_args_and_return")
    and registers a TypeErasedHandler that returns the sum of two int32_t arguments.

    The second process creates a typed TestMethodProxy and calls the method twice
    (once with copy semantics, once with zero-copy), verifying the returned sum each time.

    Test is successful if both processes exit with code 0 and no crashes occur.
    """
    with sut.start_process(
        "./bin/main_provider", cwd="/opt/GenericSkeletonMethodProviderApp/"
    ) as provider:
        with sut.start_process(
            "./bin/main_consumer", cwd="/opt/GenericSkeletonMethodConsumerApp/"
        ) as consumer:
            assert consumer.wait_for_exit(timeout=120) == 0
            assert provider.wait_for_exit(timeout=120) == 0
