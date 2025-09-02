load("@score-baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test")
load("@score-baselibs//third_party/itf:py_unittest_qnx_test.bzl", "py_unittest_qnx_test")

py_unittest_qnx_test(
    name = "unit_tests_qnx",
    test_cases = [
        ":client_connection_test",
        ":qnx_dispatch_test",
        ":unix_domain_test",
    ],
    visibility = ["//score/message_passing:__pkg__"],
)

test_suite(
    name = "unit_tests",
    tests = [
        ":client_connection_test",
        ":unix_domain_test",
    ],
    visibility = ["//score/message_passing:__pkg__"],
)

alias(
    name = "message_passing",
    actual = select({
        "@platforms//os:qnx": ":message_passing_qnx_dispatch",
        "//conditions:default": ":message_passing_unix_domain",
    }),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "common_headers",
    srcs = [
        "common_headers.cpp",
    ],
    hdrs = [
        "i_client_connection.h",
        "i_client_factory.h",
        "i_connection_handler.h",
        "i_server.h",
        "i_server_connection.h",
        "i_server_factory.h",
        "i_shared_resource_engine.h",
        "server_types.h",
        "service_protocol_config.h",
        "timed_command_queue_entry.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@score-baselibs//score/containers:intrusivelist",
        "@score-baselibs//score/language/futurecpp",
        "@score-baselibs//score/os:errno",
    ],
)

cc_library(
    name = "message_passing_common",
    srcs = [
        "client_connection.cpp",
        "timed_command_queue.cpp",
    ],
    hdrs = [
        "client_connection.h",
        "client_server_communication.h",
        "timed_command_queue.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":common_headers",
    ],
)

cc_library(
    name = "message_passing_unix_domain",
    srcs = [
        "unix_domain/unix_domain_client_factory.cpp",
        "unix_domain/unix_domain_engine.cpp",
        "unix_domain/unix_domain_server.cpp",
        "unix_domain/unix_domain_server_factory.cpp",
    ],
    hdrs = [
        "unix_domain/unix_domain_client_factory.h",
        "unix_domain/unix_domain_engine.h",
        "unix_domain/unix_domain_server.h",
        "unix_domain/unix_domain_server_factory.h",
        "unix_domain/unix_domain_socket_address.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":message_passing_common",
        "@score-baselibs//score/os:socket",
        "@score-baselibs//score/os:sys_poll",
        "@score-baselibs//score/os:unistd",
    ],
)

cc_library(
    name = "message_passing_qnx_dispatch",
    srcs = [
        "qnx_dispatch/qnx_dispatch_client_factory.cpp",
        "qnx_dispatch/qnx_dispatch_engine.cpp",
        "qnx_dispatch/qnx_dispatch_server.cpp",
        "qnx_dispatch/qnx_dispatch_server_factory.cpp",
        "qnx_dispatch/qnx_resource_path.cpp",
    ],
    hdrs = [
        "qnx_dispatch/qnx_dispatch_client_factory.h",
        "qnx_dispatch/qnx_dispatch_engine.h",
        "qnx_dispatch/qnx_dispatch_server.h",
        "qnx_dispatch/qnx_dispatch_server_factory.h",
        "qnx_dispatch/qnx_resource_path.h",
        "unix_domain/unix_domain_socket_address.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":message_passing_common",
        # sys_io
        "@score-baselibs//score/os/qnx:channel",
        "@score-baselibs//score/os/qnx:dispatch",
        "@score-baselibs//score/os/qnx:iofunc",
        "@score-baselibs//score/os/qnx:timer",
        "@score-baselibs//score/os:unistd",
        "@score-baselibs//score/os:fcntl",
        "@score-baselibs//score/os:sys_uio",
    ],
)

cc_library(
    name = "mock",
    testonly = True,
    srcs = [
        "mock/mocks.cpp",
    ],
    hdrs = [
        "mock/client_connection_mock.h",
        "mock/client_factory_mock.h",
        "mock/server_connection_mock.h",
        "mock/server_factory_mock.h",
        "mock/server_mock.h",
        "mock/shared_resource_engine_mock.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":common_headers",
        "@googletest//:gtest",
    ],
)

cc_gtest_unit_test(
    name = "client_connection_test",
    srcs = [
        "client_connection_test.cpp",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    deps = [
        ":message_passing_common",
        ":mock",
    ],
)

cc_gtest_unit_test(
    name = "unix_domain_test",
    srcs = [
        "unix_domain_server_test.cpp",
        "unix_domain_server_to_client_test.cpp",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    deps = [
        ":message_passing_unix_domain",
    ],
)

cc_gtest_unit_test(
    name = "qnx_dispatch_test",
    srcs = [
        "qnx_dispatch_server_test.cpp",
        "qnx_dispatch_server_to_client_test.cpp",
    ],
    target_compatible_with = ["@platforms//os:qnx"],
    deps = [
        ":message_passing_qnx_dispatch",
    ],
)
