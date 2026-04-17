Install VCPKG, export `VCPKG_ROOT` envvar, then:

```bash
uv venv -p 3.12
uv pip install lark shiboken6 pyside6
source .venv/bin/activate
cmake -Ssource-FreeCAD -Bbuild-vcpkg -GNinja -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DPython3_EXECUTABLE=$PWD/.venv/bin/python -DShiboken6_DIR=$PWD/.venv/lib/python3.12/site-packages/shiboken6/lib/cmake/Shiboken6 -DPySide6_DIR=$PWD/.venv/lib/python3.12/site-packages/PySide6/lib/cmake/PySide6
```

Tested only on Linux, but should work on Windows too.
