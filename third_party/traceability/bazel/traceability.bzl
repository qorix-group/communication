load("@lobster//:lobster.bzl", "lobster_gtest", "lobster_test", "lobster_trlc")
load("//bazel/rules:expand_template.bzl", "expand_template")
load("//third_party/traceability/bazel:gtest_as_exec.bzl", "test_as_exec")
load("//third_party/traceability/tools/source_code_linker:collect_source_files.bzl", "parse_source_files_for_needs_links")

def safety_software_unit(
        name,
        reqs = None,
        impl = None,
        design = None,  # @unused
        tests = None):
    COMPONENT_REQUIREMENTS = "{}_component_requirements_lobster".format(name)
    SOURCE_LINKS = "{}_sourcelinks".format(name)
    TEST_LINKS = "{}_gtest_lobster".format(name)

    lobster_trlc(
        name = COMPONENT_REQUIREMENTS,
        config = "//third_party/traceability/config/lobster:comp_reqs_conf",
        requirements = reqs,
    )

    parse_source_files_for_needs_links(
        name = SOURCE_LINKS,
        srcs_and_deps = impl,
    )

    executed_tests = []
    for test in tests:
        label = Label(test)
        executed_test_name = "{}_{}_{}_tests_as_exec".format(name, label.package, label.name)
        test_as_exec(
            name = executed_test_name,
            executable = test,
        )
        executed_tests.append(executed_test_name)

    lobster_gtest(
        name = TEST_LINKS,
        testonly = True,
        tests = executed_tests,
    )

    LOBSTER_CONFIG = "{}_lobster_config".format(name)
    expand_template(
        name = LOBSTER_CONFIG,
        template = "//third_party/traceability/config/lobster:traceability_conf",
        out = "{}_traceability_config".format(name),
        substitutions = {
            "component_requirements_lobster": COMPONENT_REQUIREMENTS,
            "gtest_lobster": TEST_LINKS,
            "sourcelinks": SOURCE_LINKS,
        },
    )

    lobster_test(
        name = name,
        config = LOBSTER_CONFIG,
        inputs = [
            COMPONENT_REQUIREMENTS,
            TEST_LINKS,
            SOURCE_LINKS,
        ],
    )

def safety_software_seooc(
        name,
        reqs = None,  # @unused A list of system requirements / assumptions. Only trlc_* deps allowed
        units = None,  # @unused A list of safety_software_unit
        design = None,  # @unused Represents the Software Architectural Design => puml_* deps allowed
        tests = None,  # @unused e.g. Integration Tests
        analysis = None):  # @unused e.g. ITF Tests, SCTF Test
    # This is just to show the idea, will be filled in another commit.
    pass
