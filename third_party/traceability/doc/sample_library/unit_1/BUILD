cc_library(
    name = "unit_1",
    srcs = ["foo.cpp"],
    hdrs = ["foo.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "unit_1_test",
    srcs = ["foo_test.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":unit_1",
        "@googletest//:gtest_main",
    ],
)
