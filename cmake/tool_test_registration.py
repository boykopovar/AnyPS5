import argparse
import json
import re
import sys
import unicodedata
from pathlib import Path


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_registrations(root, paths=None):
    root = Path(root).resolve()
    if not root.is_dir():
        raise ValueError(f"source root is not a directory: {root}")
    directory = root / "tools" / "tests" / "registrations"
    if directory.is_symlink() or directory.resolve() != directory:
        raise ValueError(f"registration directory must not contain symlinks: {directory}")
    if directory.exists() and not directory.is_dir():
        raise ValueError(f"registration path is not a directory: {directory}")
    if paths is None:
        paths = sorted(directory.glob("*.json"))
    else:
        resolved = []
        for path in paths:
            if not isinstance(path, (str, Path)):
                raise ValueError("registration paths must be strings or Path objects")
            path = Path(path)
            if ".." in path.parts:
                raise ValueError(f"registration path must not traverse directories: {path}")
            resolved.append(path if path.is_absolute() else root / path)
        paths = sorted(resolved)
    records = []
    names = set()
    for path in paths:
        if (
            path.parent != directory
            or path.suffix != ".json"
            or path.is_symlink()
            or path.resolve() != path
            or not path.is_file()
        ):
            raise ValueError(f"invalid registration manifest path: {path}")
        try:
            data = json.loads(path.read_bytes(), object_pairs_hook=unique_object)
            if not isinstance(data, dict) or set(data) != {"version", "tests"}:
                raise ValueError("manifest must contain exactly version and tests")
            if type(data["version"]) is not int or data["version"] != 1:
                raise ValueError("manifest version must be 1")
            if not isinstance(data["tests"], list) or not data["tests"]:
                raise ValueError("manifest tests must be a nonempty array")
            for record in data["tests"]:
                if not isinstance(record, dict) or set(record) != {
                    "name", "script", "arguments", "timeout"
                }:
                    raise ValueError("test must contain exactly name, script, arguments and timeout")
                name = record["name"]
                if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
                    raise ValueError(f"invalid test name: {name!r}")
                if name in names:
                    raise ValueError(f"duplicate test name: {name}")
                script = record["script"]
                if not isinstance(script, str) or not re.fullmatch(r"test_[A-Za-z0-9_]+\.py", script):
                    raise ValueError(f"invalid test script: {script!r}")
                script_path = root / "tools" / "tests" / script
                if (script_path.is_symlink() or script_path.resolve() != script_path
                        or not script_path.is_file()):
                    raise ValueError(f"test script must be an existing nonsymlink file: {script}")
                arguments = record["arguments"]
                if not isinstance(arguments, list):
                    raise ValueError(f"test arguments must be an array: {name}")
                for argument in arguments:
                    if (
                        not isinstance(argument, str)
                        or not argument
                        or any(unicodedata.category(char) in {"Cc", "Cs"} for char in argument)
                        or any(char in argument for char in ";\\[]")
                        or "${" in argument
                        or "$<" in argument
                    ):
                        raise ValueError(f"invalid literal test argument: {argument!r}")
                timeout = record["timeout"]
                if type(timeout) not in (int, float) or not 0 < timeout <= 30:
                    raise ValueError(f"test timeout must be greater than 0 and at most 30: {name}")
                names.add(name)
                records.append(record)
        except ValueError as error:
            raise ValueError(f"{path}: {error}") from error
    return records


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    try:
        records = load_registrations(args.root)
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 2
    print(json.dumps({"version": 1, "tests": records}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
