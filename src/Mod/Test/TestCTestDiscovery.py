# SPDX-License-Identifier: LGPL-2.1-or-later

import io
import sys
import types
import unittest
from contextlib import redirect_stdout
from unittest import mock

from CTestTests import discover
from TestApp import TestText as _run_text_test, _load_test


class TestCTestDiscovery(unittest.TestCase):
    def setUp(self):
        self.module = types.ModuleType("_ctest_fixture")
        self.module.SelectedCase = type(
            "SelectedCase",
            (unittest.TestCase,),
            {
                "__module__": self.module.__name__,
                "test_selected": lambda self: None,
                "test_excluded": lambda self: None,
            },
        )
        self.patch = mock.patch.dict(sys.modules, {self.module.__name__: self.module})
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def test_preserves_nested_suite_selection(self):
        case = self.module.SelectedCase("test_selected")
        self.module.load_tests = lambda *args: unittest.TestSuite([unittest.TestSuite([case])])
        with mock.patch.object(case, "run") as run:
            self.assertEqual(discover([self.module.__name__]), {self.module.__name__: [case.id()]})
            run.assert_not_called()

    def test_retains_full_module_name(self):
        self.module.SelectedCase.__module__ = "package.subpackage.tests"
        groups = discover([self.module.__name__])
        self.assertEqual(list(groups), ["package.subpackage.tests"])
        self.assertEqual(len(groups["package.subpackage.tests"]), 2)

    def test_deduplicates_overlapping_registrations(self):
        name = self.module.__name__
        self.assertEqual(discover([name, name]), discover([name]))

    def test_preserves_load_failure(self):
        self.assertEqual(
            discover(["_missing_ctest_fixture"]),
            {"_missing_ctest_fixture": ["_missing_ctest_fixture"]},
        )

    def test_direct_module_uses_module_not_package_attributes(self):
        # CAM exports classes whose names shadow their modules. Resolve case IDs
        # relative to the imported module, not by traversing package attributes.
        name = self.module.__name__ + ".child"
        child = types.ModuleType(name)
        child.SelectedCase = self.module.SelectedCase
        child.SelectedCase.__module__ = name
        self.module.child = child.SelectedCase
        with mock.patch.dict(sys.modules, {name: child}):
            suite = _load_test(name)
        self.assertEqual(suite.countTestCases(), 2)
        self.assertEqual(
            {case.id() for group in suite for case in group},
            {name + ".SelectedCase.test_" + suffix for suffix in ("selected", "excluded")},
        )

    def test_direct_case_name_still_works(self):
        name = self.module.__name__ + ".SelectedCase.test_selected"
        suite = _load_test(name)
        self.assertEqual(suite.countTestCases(), 1)
        self.assertEqual(next(iter(suite)).id(), name)

    def test_direct_module_preserves_load_tests(self):
        case = self.module.SelectedCase("test_selected")
        self.module.load_tests = lambda *args: unittest.TestSuite([case])
        suite = _load_test(self.module.__name__)
        self.assertEqual(list(suite), [case])

    def test_direct_missing_module_fails(self):
        result = unittest.TestResult()
        _load_test("_missing_ctest_fixture").run(result)
        self.assertFalse(result.wasSuccessful())
        self.assertEqual(len(result.errors), 1)

    def _run_selected_case(self):
        case = self.module.SelectedCase("test_selected")
        self.module.load_tests = lambda *args: unittest.TestSuite([case])
        output = io.StringIO()
        with redirect_stdout(output):
            result = _run_text_test(self.module.__name__)
        self.assertTrue(result.wasSuccessful())
        return output.getvalue()

    def test_all_skipped_cases_are_reported_to_ctest(self):
        self.module.SelectedCase = unittest.skip("optional dependency")(self.module.SelectedCase)
        self.assertIn("FREECAD_CTEST_ALL_SKIPPED", self._run_selected_case())

    def test_skipped_subtest_does_not_skip_the_module(self):
        def optional_import(case):
            with case.subTest(module="optional"):
                case.skipTest("optional dependency")

        self.module.SelectedCase.test_selected = optional_import
        self.assertNotIn("FREECAD_CTEST_ALL_SKIPPED", self._run_selected_case())
