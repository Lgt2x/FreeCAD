# SPDX-License-Identifier: LGPL-2.1-or-later

"""Exercise discovery caching with a small stand-in for the FreeCAD processes."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


CMAKE = os.environ.get("CMAKE_COMMAND") or shutil.which("cmake")


@unittest.skipUnless(CMAKE, "CMake is not available")
class TestCTestCache(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="freecad ctest cache ")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.cache = self.root / "PythonTests-Debug-cache.cmake"
        self.signature = Path(str(self.cache) + ".sha256")
        self.script = self.root / "discovery.cmake"
        self.count = self.root / "calls"
        self.count.write_text("")
        self.env = os.environ.copy()
        for key in ("FREECAD_TEST_REFRESH", "FREECAD_TEST_USE_INSTALLED"):
            self.env.pop(key, None)
        executable = self.root / "bin" / "FakeCmd"
        executable.parent.mkdir()
        executable.write_text("binary")
        (self.root / "CMakeCache.txt").write_text("# Test configuration\n")
        template = Path(__file__).with_name("PythonTests.cmake.in").read_text()
        for key, value in {
            "CMAKE_COMMAND": Path(sys.executable).as_posix(),
            "CMAKE_BINARY_DIR": self.root.as_posix(),
            "PY_EXECUTABLE": executable.as_posix(),
            "GUI_EXECUTABLE": "",
        }.items():
            template = template.replace("@" + key + "@", value)
        self.script.write_text(template)
        # The template invokes '<command> -E env ...'. With Python as the command,
        # -E is Python's ignore-environment option and 'env' is this mock launcher.
        (self.root / "env").write_text(
            "from pathlib import Path\n"
            "import os, sys\n"
            "with Path('calls').open('a') as calls: calls.write('called\\n')\n"
            "if os.environ.get('FREECAD_CACHE_TEST_FAIL'): sys.exit(1)\n"
            'print(\'FREECAD_CTEST_TESTS={"nested.Fixture": ["nested.Fixture.Case.test_one"]}\')\n'
        )
        self.driver = self.root / "driver.cmake"
        self.driver.write_text(
            "cmake_minimum_required(VERSION 3.25)\n"
            "set(CTEST_CONFIGURATION_TYPE Debug)\n"
            'file(WRITE collected "")\n'
            "function(add_test)\n"
            '  file(APPEND collected "${ARGV0}\\n")\n'
            "endfunction()\n"
            "function(set_tests_properties)\n"
            "endfunction()\n"
            "include(discovery.cmake)\n"
        )

    def collect(self, **environment):
        result = subprocess.run(
            [CMAKE, "-P", str(self.driver)],
            cwd=self.root,
            env=self.env | environment,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.root / "collected").read_text(), "FreeCAD-PY-nested.Fixture\n")

    def calls(self):
        return len(self.count.read_text().splitlines())

    def test_reuse_and_explicit_refresh(self):
        self.collect()
        signature = self.signature.read_text()
        self.collect()
        self.assertEqual(self.calls(), 1)
        self.collect(FREECAD_TEST_REFRESH="0")
        self.assertEqual(self.calls(), 1)
        self.collect(FREECAD_TEST_REFRESH="1")
        self.assertEqual(self.calls(), 2)
        self.assertEqual(self.signature.read_text(), signature)
        self.collect()
        self.assertEqual(self.calls(), 2)

    def test_runtime_addition_edit_and_removal(self):
        self.collect()
        for directory, name in (("Mod", "Init.py"), ("lib", "module.so"), ("bin", "program")):
            with self.subTest(directory=directory):
                path = self.root / directory / name
                path.parent.mkdir(exist_ok=True)
                before = self.calls()
                path.write_text("one")
                self.collect()
                self.assertEqual(self.calls(), before + 1)
                path.write_text("two")
                os.utime(path, (path.stat().st_atime, path.stat().st_mtime + 2))
                self.collect()
                self.assertEqual(self.calls(), before + 2)
                path.unlink()
                self.collect()
                self.assertEqual(self.calls(), before + 3)

    def test_bytecode_is_ignored(self):
        self.collect()
        for filename in ("__pycache__/Init.pyc", "Init.pyc", "Init.pyo"):
            path = self.root / "Mod" / filename
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"bytecode")
        self.collect()
        self.assertEqual(self.calls(), 1)

    def test_cmake_inputs_invalidate(self):
        self.collect()
        for path in (self.script, self.root / "CMakeCache.txt"):
            path.write_text(path.read_text() + "\n# Changed\n")
            self.collect()
        self.assertEqual(self.calls(), 3)

    def test_environment_changes_invalidate(self):
        self.collect()
        self.collect(PYTHONPATH=self.root.as_posix())
        self.collect(PYTHONPATH=self.root.as_posix())
        self.assertEqual(self.calls(), 2)

    def test_configurations_have_separate_caches(self):
        self.collect()
        original = self.driver.read_text()
        self.driver.write_text(original.replace("TYPE Debug", "TYPE Release"))
        self.collect()
        self.assertTrue((self.root / "PythonTests-Release-cache.cmake").exists())
        self.driver.write_text(original)
        self.collect()
        self.assertEqual(self.calls(), 2)

    def test_failed_collection_is_not_published(self):
        self.collect()
        previous = self.cache.read_bytes(), self.signature.read_bytes()
        result = subprocess.run(
            [CMAKE, "-P", str(self.driver)],
            cwd=self.root,
            env=self.env | {"FREECAD_TEST_REFRESH": "1", "FREECAD_CACHE_TEST_FAIL": "1"},
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.cache.read_bytes(), self.signature.read_bytes()), previous)

    def test_installed_mode_does_not_use_build_cache(self):
        self.collect()
        previous = self.cache.read_bytes(), self.signature.read_bytes()
        self.collect(FREECAD_TEST_USE_INSTALLED="1")
        self.collect(FREECAD_TEST_USE_INSTALLED="1")
        self.assertEqual(self.calls(), 3)
        self.assertEqual((self.cache.read_bytes(), self.signature.read_bytes()), previous)

    def test_concurrent_collection_publishes_once(self):
        processes = [
            subprocess.Popen(
                [CMAKE, "-P", str(self.driver)],
                cwd=self.root,
                env=self.env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            for _ in range(2)
        ]
        try:
            for process in processes:
                stdout, stderr = process.communicate(timeout=30)
                self.assertEqual(process.returncode, 0, stdout + stderr)
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                    process.communicate()
        self.collect()
        self.assertEqual(self.calls(), 1)


if __name__ == "__main__":
    unittest.main()
