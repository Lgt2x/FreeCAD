# SPDX-License-Identifier: LGPL-2.1-or-later

import io
import sys
import types
import unittest
from contextlib import redirect_stdout
from unittest import mock

from CTestTests import discover, load_tests as _load_ctest_tests


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

    def test_selected_cases_use_module_not_package_attributes(self):
        # CAM exports classes whose names shadow their modules. Resolve case IDs
        # relative to the imported module, not by traversing package attributes.
        name = self.module.__name__ + ".child"
        child = types.ModuleType(name)
        child.SelectedCase = self.module.SelectedCase
        child.SelectedCase.__module__ = name
        self.module.child = child.SelectedCase
        case_id = name + ".SelectedCase.test_selected"
        with (
            mock.patch.dict(sys.modules, {name: child}),
            mock.patch.dict(
                "os.environ", {"FREECAD_CTEST_MODULE": name, "FREECAD_CTEST_CASES": case_id}
            ),
        ):
            suite = _load_ctest_tests(unittest.TestLoader(), None, None)
        self.assertEqual(suite.countTestCases(), 1)
        self.assertEqual(next(iter(next(iter(suite)))).id(), case_id)

    def _run_selected_case(self):
        name = self.module.__name__
        with mock.patch.dict(
            "os.environ",
            {
                "FREECAD_CTEST_MODULE": name,
                "FREECAD_CTEST_CASES": name + ".SelectedCase.test_selected",
            },
        ):
            suite = _load_ctest_tests(unittest.TestLoader(), None, None)
        result = unittest.TestResult()
        output = io.StringIO()
        with redirect_stdout(output):
            suite.run(result)
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
