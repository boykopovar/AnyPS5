import argparse
import importlib
import inspect
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / "core" / "relinker" / "relinker" / "tests"
BUILDERS = ("fixture", "image", "make_image", "tls_image", "padded_image", "executable_image",
            "empty_tls_image", "make_eof_segment_prefix_image")
SELECTORS = (0, 1, 2, 4)


def without_arguments(function):
    for parameter in inspect.signature(function).parameters.values():
        positional = parameter.kind in (parameter.POSITIONAL_ONLY, parameter.POSITIONAL_OR_KEYWORD)
        if positional and parameter.default is inspect.Parameter.empty:
            return False
    return True


def images():
    sys.path.insert(0, str(TESTS))
    for path in sorted(TESTS.glob("test_*.py")):
        try:
            module = importlib.import_module(path.stem)
        except Exception:
            continue
        for name in BUILDERS:
            function = getattr(module, name, None)
            if not callable(function) or not without_arguments(function):
                continue
            try:
                data = function()
            except Exception:
                continue
            if isinstance(data, (bytes, bytearray)) and len(data) >= 64:
                yield f"{path.stem}-{name}", bytes(data)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Write relinker test fixtures as relinker_conversion_fuzzer seeds")
    parser.add_argument("output", type=Path)
    parser.add_argument("--all-configurations", action="store_true", help="cover every distinct fuzzer selector configuration")
    arguments = parser.parse_args(argv)
    selectors = SELECTORS
    if arguments.all_configurations:
        configurations = {}
        for selector in range(256):
            configurations.setdefault((selector % 3, bool(selector & 4), (selector >> 3) % 3), selector)
        selectors = tuple(configurations.values())
    if arguments.output.is_file():
        parser.error(f"{arguments.output} is an existing file, not a directory")
    try:
        arguments.output.mkdir(parents=True, exist_ok=True)
    except OSError as error:
        parser.error(str(error))
    count = 0
    try:
        for name, data in images():
            for selector in selectors:
                (arguments.output / f"{name}-{selector}").write_bytes(bytes([selector]) + data)
                count += 1
    except OSError as error:
        parser.error(str(error))
    print(f"{count} seeds written to {arguments.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
