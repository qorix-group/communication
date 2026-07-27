#!/usr/bin/env python3
# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
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

import importlib.util
import pathlib
import tempfile
import unittest
from unittest import mock


_MODULE_PATH = pathlib.Path(__file__).with_name("codeql_lint.py")
_SPEC = importlib.util.spec_from_file_location("codeql_lint", _MODULE_PATH)
assert _SPEC is not None and _SPEC.loader is not None
codeql_lint = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(codeql_lint)


class CodeQlLintTest(unittest.TestCase):
    def test_recategorize_sarif_invokes_helper_and_replaces_input(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = pathlib.Path(tmp_dir)
            config_path = tmp_path / "coding-standards.yaml"
            sarif_path = tmp_path / "codeql.sarif"
            recategorized_path = pathlib.Path(str(sarif_path) + ".recategorized")
            coding_standards_schema_target = tmp_path / "coding-standards-schema.json"
            sarif_schema_target = tmp_path / "sarif-schema.json"
            coding_standards_schema_link = tmp_path / "coding-standards-schema.link.json"
            sarif_schema_link = tmp_path / "sarif-schema.link.json"
            config_path.write_text("guideline-recategorizations: []\n", encoding="utf-8")
            sarif_path.write_text("{}", encoding="utf-8")
            coding_standards_schema_target.write_text("{}", encoding="utf-8")
            sarif_schema_target.write_text("{}", encoding="utf-8")
            coding_standards_schema_link.symlink_to(coding_standards_schema_target)
            sarif_schema_link.symlink_to(sarif_schema_target)

            calls = []

            def fake_run(cmd, check):
                calls.append(cmd)
                recategorized_path.write_text('{"version":"2.1.0","runs":[]}', encoding="utf-8")

            with mock.patch.object(
                codeql_lint,
                "_find_recategorization_schema_paths",
                return_value=(
                    str(coding_standards_schema_link),
                    str(sarif_schema_link),
                ),
            ), mock.patch.object(codeql_lint.subprocess, "run", side_effect=fake_run), \
                 mock.patch.object(codeql_lint.os, "replace") as replace:
                result = codeql_lint.recategorize_sarif(
                    "/tmp/recategorize",
                    str(config_path),
                    str(sarif_path),
                )

            self.assertEqual(result, str(sarif_path))
            self.assertEqual(
                calls,
                [[
                    "/tmp/recategorize",
                    "--coding-standards-schema-file",
                    str(coding_standards_schema_target),
                    "--sarif-schema-file",
                    str(sarif_schema_target),
                    str(config_path),
                    str(sarif_path),
                    str(recategorized_path),
                ]],
            )
            replace.assert_called_once_with(str(recategorized_path), str(sarif_path))


if __name__ == "__main__":
    unittest.main()
