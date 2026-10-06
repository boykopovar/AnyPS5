import argparse
import ctypes
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("provider", type=Path)
    args = parser.parse_args()
    native_flags = 0
    if os.name == "nt":
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
        native_flags = subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS
    modes = ("lifecycle", "preloaded", "shared", "missing", "invalid-image", "unknown",
             "unknown-query", "unknown-unload", "with-arguments")
    if os.name == "nt":
        modes += ("failed-attach",)
    cases = ((module_id, module_name, mode)
             for module_id, module_name in ((6, "libSceFiber"), (0x125, "libSceTextToSpeech2"))
             for mode in modes)
    for module_id, module_name, mode in cases:
        with tempfile.TemporaryDirectory(prefix="anyps5-sysmodule-") as directory:
            root = Path(directory)
            executable = root / args.executable.name
            shutil.copyfile(args.executable, executable)
            provider = root / "libs" / (module_name + ".prx")
            provider.parent.mkdir()
            if mode == "invalid-image":
                provider.write_bytes(b"not a native provider image\n")
            elif mode != "missing":
                shutil.copyfile(args.provider, provider)
            trace = root / "trace.bin"
            environment = os.environ.copy()
            environment["ANYPS5_SYSMODULE_TRACE"] = str(trace)
            environment["ANYPS5_SYSMODULE_TEST_ID"] = str(module_id)
            environment.pop("ANYPS5_SYSMODULE_FAIL_ATTACH", None)
            if mode == "failed-attach":
                environment["ANYPS5_SYSMODULE_FAIL_ATTACH"] = "1"
            result = subprocess.run([str(executable), mode, str(provider), str(module_id)], cwd=root, env=environment,
                                    capture_output=True, timeout=15,
                                    creationflags=native_flags)
            stderr = result.stderr.decode("utf-8", errors="replace")
            effects = trace.read_bytes() if trace.exists() else b""
            if mode in ("lifecycle", "preloaded", "shared"):
                if result.returncode != 0 or effects != b"AD":
                    raise AssertionError(f"{mode}: exit={result.returncode:#x}, effects={effects!r}, stderr={stderr}")
            else:
                phrase = ("sceSysmoduleLoadModule: failed to load" if mode in ("missing", "failed-attach", "invalid-image")
                          else "sceSysmoduleLoadModule: unknown id" if mode == "unknown"
                          else "sceSysmoduleIsLoaded: unknown id" if mode == "unknown-query"
                          else "sceSysmoduleUnloadModule: unknown id" if mode == "unknown-unload"
                          else "sceSysmoduleLoadModuleInternalWithArg: unsupported arguments")
                expected_abort = 0xC0000409 if os.name == "nt" else -signal.SIGABRT
                if result.returncode != expected_abort or phrase not in stderr or "unexpected" in stderr:
                    raise AssertionError(f"{mode}: strict rejection missing; exit={result.returncode:#x}, stderr={stderr}")
                if mode == "failed-attach" and effects != b"AD":
                    raise AssertionError(f"failed attach effects: {effects!r}")
                if mode != "failed-attach" and effects:
                    raise AssertionError(f"{mode}: provider attached before rejection: {effects!r}")
            print(f"{module_name}({module_id:#x})/{mode}: PASS (exit={result.returncode:#x}, effects={effects!r})", flush=True)


if __name__ == "__main__":
    main()
