load("@rules_doxygen//doxygen:doxygen.bzl", "doxygen")

doxygen(
    name = "generate_doxygen",
    outs = [
        "html",
        "xml",
    ],
    exclude_patterns = ["*/external/*"],
    extract_all = True,
    generate_html = True,
    generate_xml = True,
    project_name = "com",
    deps = [
        "//score/mw/com",
    ],
)
