cc_library(
    name = "unit_2",
    srcs = ["bar.cpp"],
    hdrs = ["bar.h"],
    visibility = ["//visibility:public"],
    deps = ["//third_party/traceability/doc/sample_library/unit_1"],
)

cc_test(
    name = "unit_2_test",
    srcs = ["bar_test.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":unit_2",
        "@googletest//:gtest_main",
    ],
)
