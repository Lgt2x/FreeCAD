# Python tests

Core tests live here rather than in the Test workbench's implementation:

- `base/`: Base bindings and utilities.
- `app/`: documents, properties, persistence, and application services.
- `gui/`: common GUI infrastructure, including tests using geometry-module fixtures.
- `integration/`: recompute across modules and manual geometry scripts.
- `performance/`: opt-in profiling and stress tools.
- `tools/`: source-only binding, stub-generator, versioning, and API-tool tests.

The runtime suites and their data are explicitly listed in `CMakeLists.txt`.
They are copied into the build tree and installed under `Mod/Test`, retaining
their existing flat import names and `FreeCAD -t` entry points. Registration
remains in `src/Mod/Test/Init.py` and `InitGui.py`. Test workbench menus, commands,
and runners remain in `src/Mod/Test`.

The manual `integration/testPathArray*.py` scripts remain source-only, as before.

For example, after building `TestSources`:

```sh
build-debug/bin/FreeCADCmd -t Document
xvfb-run -a build-debug/bin/FreeCAD -t GuiDocument
```

Tooling tests need the source checkout, not an installed FreeCAD. CI runs:

```sh
pixi run python -m unittest discover -s tests/python/tools -p "test_*.py"
```

Do not recursively discover every Python file here: runtime suites require
FreeCAD, and the integration and performance directories include opt-in scripts.
