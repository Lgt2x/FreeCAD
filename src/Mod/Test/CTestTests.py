# SPDX-License-Identifier: LGPL-2.1-or-later

"""Expose the cases selected by FreeCAD's registered suites to CTest."""

import json
import unittest

import FreeCAD


def _cases(suite):
    for test in suite:
        if isinstance(test, unittest.TestSuite):
            yield from _cases(test)
        else:
            yield test


def discover(names):
    """Group selected case IDs by their full defining module, without running tests."""
    modules = {}
    for name in names:
        loader = unittest.TestLoader()
        suite = loader.loadTestsFromName(name)
        if loader.errors:
            # Keep import failures visible as failing tests, not missing coverage.
            modules[name] = [name]
            continue
        for test in _cases(suite):
            module = type(test).__module__
            ids = modules.setdefault(module, [])
            if test.id() not in ids:
                ids.append(test.id())
    return modules


class _SelectedSuite(unittest.TestSuite):
    def run(self, result, debug=False):
        selected = {test.id() for test in _cases(self)}
        result = super().run(result, debug)
        skipped = {test.id() for test, reason in result.skipped}
        # Skipped subtests must not make a partially-executed module look skipped.
        if result.wasSuccessful() and selected and selected == skipped:
            print("FREECAD_CTEST_ALL_SKIPPED")
        return result


def load_tests(loader, tests, pattern):
    # -t uses unittest's standard load_tests hook, so both CLI and GUI initialization
    # have completed and the same registry as an ordinary test run is available.
    print("FREECAD_CTEST_TESTS=" + json.dumps(discover(FreeCAD.__unit_test__)))
    return unittest.TestSuite()
