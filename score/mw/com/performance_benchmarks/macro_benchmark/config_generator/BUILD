py_binary(
    name = "config_generator",
    srcs = [
        "__init__.py",
        "config_generator.py",
    ],
    main = "config_generator.py",
    visibility = [
        "//platform/aas/test/pas/perf_tests/tools/artificial_load:__subpackages__",
        "//score/mw/com/performance_benchmarks/macro_benchmark:__subpackages__",
    ],
    deps = [
        "//third_party/jsonschema",
    ],
)
