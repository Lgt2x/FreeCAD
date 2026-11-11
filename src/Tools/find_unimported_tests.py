# SPDX-License-Identifier: LGPL-2.1-or-later
"""Find unittest modules lacking static import/registration references.

Usage: python src/Tools/find_unimported_tests.py [source-directory]
Dynamic discovery can still run these candidates; this is not a coverage proof.
"""

import argparse
import ast
from collections import defaultdict
from pathlib import Path
import sys


LOADERS = {"__import__", "import_module", "loadTestsFromName", "loadTestsFromNames"}


def suffix_index(items):
    result = defaultdict(set)
    for name, value in items:
        parts = name.split(".")
        for i in range(len(parts)):
            result[".".join(parts[i:])].add(value)
    return result


def strings(node):
    return {
        n.value for n in ast.walk(node) if isinstance(n, ast.Constant) and isinstance(n.value, str)
    }


def find_unimported(root):
    trees = {}
    for path in sorted(root.rglob("*.py")):
        try:
            trees[path] = ast.parse(path.read_bytes(), filename=str(path))
        except (SyntaxError, UnicodeError) as error:
            print(f"Skipping {path}: {error}", file=sys.stderr)
    modules = {
        path: ".".join(path.relative_to(root).with_suffix("").parts).removesuffix(".__init__")
        for path in trees
    }
    module_names = suffix_index((name, path) for path, name in modules.items())
    references, classes = defaultdict(set), {}
    for path, tree in trees.items():
        module = modules[path]
        package = module if path.name == "__init__.py" else module.rpartition(".")[0]
        aliases = {}
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                for alias in node.names:
                    references[path].add(alias.name)
                    aliases[alias.asname or alias.name.split(".")[0]] = (
                        alias.name if alias.asname else alias.name.split(".")[0]
                    )
            elif isinstance(node, ast.ImportFrom):
                prefix = node.module or ""
                if node.level:
                    parts = package.split(".") if package else []
                    prefix = ".".join(parts[: len(parts) - node.level + 1] + [prefix]).strip(".")
                references[path].add(prefix)
                for alias in node.names:
                    name = f"{prefix}.{alias.name}".strip(".")
                    references[path].add(name)
                    aliases[alias.asname or alias.name] = name
            elif isinstance(node, (ast.Assign, ast.AugAssign)):
                targets = node.targets if isinstance(node, ast.Assign) else [node.target]
                if any(isinstance(t, ast.Attribute) and t.attr == "__unit_test__" for t in targets):
                    references[path].update(strings(node.value))
            elif isinstance(node, ast.Call) and node.args:
                name = ast.unparse(node.func).split(".")[-1]
                if name in LOADERS:
                    references[path].update(strings(node.args[0]))
        for node in ast.walk(tree):
            if isinstance(node, ast.ClassDef):
                bases = []
                for base in node.bases:
                    head, *tail = ast.unparse(base).split(".")
                    name = ".".join([aliases.get(head, head), *tail])
                    bases.append(f"{module}.{name}" if "." not in name else name)
                classes[f"{module}.{node.name}"] = path, node, bases
    class_names = suffix_index((name, name) for name in classes)
    cases = {"unittest.TestCase", "unittest.case.TestCase", "unittest.IsolatedAsyncioTestCase"}
    tests = set()
    while True:
        before = len(cases), len(tests)
        for name, (path, node, bases) in classes.items():
            parents = {parent for base in bases for parent in class_names.get(base, {base})}
            if parents & cases:
                cases.add(name)
                if parents & tests or any(
                    isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))
                    and (n.name.startswith("test") or n.name == "runTest")
                    for n in node.body
                ):
                    tests.add(name)
        if before == (len(cases), len(tests)):
            break
    imported = set()
    for owner, names in references.items():
        for name in names:
            while name:
                imported.update(path for path in module_names.get(name, ()) if path != owner)
                name = name.rpartition(".")[0]
    missing = defaultdict(list)
    for name in sorted(tests):
        path, node, _ = classes[name]
        if path not in imported:
            missing[path].append(node.name)
    return dict(sorted(missing.items()))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", type=Path, default=Path(__file__).resolve().parents[1])
    root = parser.parse_args().root.resolve()
    if not root.is_dir():
        parser.error(f"Not a directory: {root}")
    missing = find_unimported(root)
    for path, names in missing.items():
        print(f"{path}: {', '.join(names)}")
    print(f"{len(missing)} candidate modules; dynamic discovery may cover some.", file=sys.stderr)
