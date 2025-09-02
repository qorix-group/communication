load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")

filegroup(
    name = "service_benchmark_config",
    srcs = ["service_benchmark_config.json"],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__subpackages__"],
)

filegroup(
    name = "client_benchmark_config",
    srcs = ["client_benchmark_config.json"],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__subpackages__"],
)

filegroup(
    name = "joined_benchmark_config_json",
    srcs = ["joined_benchmark_config.json"],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__subpackages__"],
)

validate_json_schema_test(
    name = "validate_lola_schema",
    json = "//score/mw/com/performance_benchmarks/macro_benchmark/config:mw_com_config_json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "validate_joined_benchmark_config_schema",
    json = "joined_benchmark_config.json",
    schema = "joined_benchmark_config_schema.json",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "validate_client_benchmark_config_schema",
    json = "client_benchmark_config.json",
    schema = "client_benchmark_config_schema.json",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "validate_service_benchmark_config_schema",
    json = "service_benchmark_config.json",
    schema = "service_benchmark_config_schema.json",
    tags = ["lint"],
)

filegroup(
    name = "mw_com_config_json",
    srcs = ["mw_com_config.json"],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__subpackages__"],
)

filegroup(
    name = "logging_json",
    srcs = ["logging.json"],
    visibility = ["//score/mw/com/performance_benchmarks/macro_benchmark:__subpackages__"],
)
