load("@score-baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_binary(
    name = "lola_public_api_benchmarks",
    srcs = [
        "lola_public_api_benchmarks.cpp",
    ],
    data = [
        "//score/mw/com/performance_benchmarks/api_microbenchmarks/config",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["benchmark"],
    deps = [
        "//score/mw/com",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
        "@benchmark//:benchmark_main",
    ],
)
