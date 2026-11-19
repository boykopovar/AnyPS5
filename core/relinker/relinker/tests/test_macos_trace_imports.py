"""--trace-imports passes each macOS function import through libc.prx's TraceImport_nid_no_patch before the guest entry."""

import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile

from guest_import_fixtures import executable


PROVIDER = "#include <unistd.h>\nint shared(void) { _exit(11); }\n"
HOOK = ("#include <string.h>\n"
        "#include <unistd.h>\n"
        "static int replacement(void) { _exit(77); }\n"
        "void* TraceImport_nid_no_patch(void* target, const char* name, const char* library) {\n"
        "    if (((unsigned long)__builtin_frame_address(0) & 15) != 0) _exit(13);\n"
        "    return strcmp(name, \"shared\") == 0 && strcmp(library, \"a.prx\") == 0 ? (void*)replacement : target;\n"
        "}\n")
PLAIN = "int Unrelated(void) { return 0; }\n"
START = "void Aps5StartGuest_nid_no_patch(void (*entry)(void)) { entry(); }\n"


def relink(relinker, source, output, *options):
    return subprocess.run([str(relinker), "--macos", "--skip-sce-module", *options, str(source), str(output)],
                          capture_output=True, text=True, timeout=30)


def library(work, name, code):
    source = work / (name + ".c")
    source.write_text(code)
    output = work / (name + ".dylib")
    subprocess.run(["xcrun", "clang", "-arch", "x86_64", "-dynamiclib", str(source),
                    "-Wl,-install_name,@rpath/" + name + ".prx", "-o", str(output)],
                   check=True, capture_output=True, timeout=20)
    return output


def run_binary(output):
    command = (["/usr/bin/arch", "-x86_64"] if platform.machine() == "arm64" else []) + [str(output)]
    return subprocess.run(command, capture_output=True, text=True, timeout=30)


def main():
    relinker = Path(sys.argv[1]).resolve()
    runnable = sys.platform == "darwin" and platform.machine() in ("x86_64", "arm64")
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-trace-imports-") as directory:
        work = Path(directory)
        libraries = {}
        if runnable:
            libraries = {name: library(work, name, code) for name, code in (
                ("a", PROVIDER), ("hook", HOOK), ("plain", PLAIN), ("libkernel", START))}

        for name, options, libc, symbol_info, expected in (
                ("untraced", (), "hook", 0x12, 11),
                ("traced", ("--trace-imports",), "hook", 0x12, 77),
                ("fixed-base", ("--trace-imports", "--macos-fixed-base", "0x400000"), "hook", 0x12, 77),
                ("data", ("--trace-imports",), "hook", 0x11, 11),
                ("weak", ("--trace-imports",), "hook", 0x22, 11),
                ("unsupported", ("--trace-imports",), "plain", 0x12, None)):
            case = work / name
            case.mkdir()
            image = executable("a.prx", extra_dependencies=("libc.prx",))
            image[0x4898 + 4] = symbol_info
            source = case / "input.elf"
            source.write_bytes(image)
            output = case / "output"
            result = relink(relinker, source, output, *options)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            if not runnable:
                continue
            (case / "libs").mkdir()
            os.link(libraries["a"], case / "libs" / "a.prx")
            os.link(libraries[libc], case / "libs" / "libc.prx")
            os.link(libraries["libkernel"], case / "libs" / "libkernel.prx")
            output.chmod(0o755)
            run = run_binary(output)
            if run.returncode != expected and "Bad CPU type in executable" in run.stderr:
                assert os.environ.get("GITHUB_ACTIONS") != "true", "macOS CI must execute the x86_64 import trace"
                runnable = False
                continue
            if expected is None:
                assert run.returncode != 0 and "TraceImport_nid_no_patch" in run.stderr, (name, run.returncode, run.stdout, run.stderr)
            else:
                assert run.returncode == expected, (name, run.returncode, run.stdout, run.stderr)

        case = work / "without-libc"
        case.mkdir()
        source = case / "input.elf"
        source.write_bytes(executable("a.prx"))
        result = relink(relinker, source, case / "output", "--trace-imports")
        assert result.returncode == 2 and "--trace-imports requires libc.prx" in result.stderr, (result.returncode, result.stderr)
        assert not (case / "output").exists()
        result = relink(relinker, source, case / "output", "--trace-imports", "--lazy-binding")
        assert result.returncode != 0 and "--trace-imports conflicts with --lazy-binding" in result.stderr, (result.returncode, result.stderr)

    print("macOS import tracing tests passed")


if __name__ == "__main__":
    main()
