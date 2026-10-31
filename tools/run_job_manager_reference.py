import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from import_audit import AuditError, read_host_imports
from unwrap_self import Refused, unwrap

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "core/relinker/relinker/tests"))
from guest_import_fixtures import executable

CASES = ("memory", "owned_state", "concurrent_wait", "validation", "dispatch", "ordering", "nested", "limits", "node_pool", "priority", "shutdown", "worker_policy", "worker_wake", "worker_wait", "worker_count", "worker_spin")
PARITY_CASES = tuple(case for case in CASES if case not in ("node_pool", "worker_wait"))


def resolve_cli_path(path):
    path = Path(path)
    try:
        resolved = Path(os.path.realpath(path))
    except OSError as error:
        raise ValueError(str(error)) from error
    if resolved.is_symlink():
        try:
            resolved.stat()
        except OSError as error:
            raise ValueError(f"{path}: path is a symbolic link loop: {error}") from error
        raise ValueError(f"{path}: path is a symbolic link loop")
    return resolved


def resolve_existing_cli_path(path):
    resolved = resolve_cli_path(path)
    if not resolved.exists():
        raise FileNotFoundError(2, "No such file or directory", str(resolved))
    return resolved


def stage_libraries(directory, build, toolchain, dependencies):
    providers = {path.name.lower(): path for path in (build / "core/libs/libs").glob("*.prx")}
    if toolchain is not None:
        providers.update({path.name.lower(): path for path in toolchain.glob("*.dll")})
    pending = list(dependencies)
    staged = set()
    while pending:
        name = pending.pop().lower()
        if name in staged:
            continue
        staged.add(name)
        source = providers.get(name)
        if source is None:
            if name.endswith(".prx"):
                raise RuntimeError(f"Missing patched runtime {name}; build core/libs/libs/{name}")
            continue
        shutil.copy2(source, directory / source.name)
        pending.extend(read_host_imports(source))


def run(args, directory):
    libraries = directory / "libs"
    libraries.mkdir()
    helper = args.build / "tests/job_manager_reference.prx"
    if not helper.is_file():
        raise RuntimeError("Build the job_manager_reference target first")
    shutil.copy2(helper, libraries / "jobref.prx")
    suffix = ".exe" if sys.platform == "win32" else ""
    subprocess.run([str(args.build / f"core/libs/nid_patcher{suffix}"), "jobref", "--preserve-export-list",
                    str(args.build / "core/libs/libc.exports"), str(libraries / "jobref.prx")], check=True, capture_output=True)
    module_hash = None
    if args.module:
        module_data = args.module.read_bytes()
        module_hash = hashlib.sha256(module_data).hexdigest()
        modules = directory / "sce_module"
        modules.mkdir()
        target = modules / "libSceJobManager.prx"
        if module_data[:4] == b"\x7fELF":
            shutil.copy2(args.module, target)
        else:
            unwrap(args.module, target)
    source = directory / "input.elf"
    entry = executable("jobref.prx", symbol="Run_nid_no_patch")
    entry[0x4000:0x400f] = b"\x48\x83\xec\x08\xff\x15" + struct.pack("<i", 0x4a20 - 0x400a) + b"\x48\x83\xc4\x08\xc3"
    source.write_bytes(entry)
    output = directory / f"reference{suffix}"
    options = ["--skip-sce-module"] if args.host else []
    platform = ["--windows"] if sys.platform == "win32" else []
    result = subprocess.run([str(args.relinker), *platform, "--registry", *options, str(source), str(output)],
                            cwd=directory, capture_output=True, timeout=60)
    (directory / "relink.log").write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f"Relinker failed ({result.returncode}); see {directory / 'relink.log'}")
    if sys.platform != "win32":
        output.chmod(output.stat().st_mode | 0o111)
    dependencies = set(read_host_imports(helper)) | {"libc.prx"}
    for registry in directory.glob("*.registry.json"):
        dependencies.update(entry["library"] for entry in json.loads(registry.read_text(encoding="utf-8"))
                            if entry.get("library") not in ("jobref.prx", "libSceJobManager.prx"))
    if args.host:
        dependencies.add("libSceJobManager.prx")
    stage_libraries(libraries, args.build, args.toolchain, dependencies)
    environment = os.environ.copy()
    environment["PATH"] = str(libraries) + os.pathsep + environment.get("PATH", "")
    if sys.platform != "win32":
        environment["LD_LIBRARY_PATH"] = str(libraries) + os.pathsep + environment.get("LD_LIBRARY_PATH", "")
    environment.pop("ANYPS5_JOB_MANAGER_HOST", None)
    if args.host:
        environment["ANYPS5_JOB_MANAGER_HOST"] = "1"
    results = []
    for case in args.cases:
        environment["ANYPS5_JOB_MANAGER_CASE"] = case
        try:
            result = subprocess.run([str(output)], cwd=directory, env=environment,
                                    capture_output=True, timeout=args.timeout)
            log = result.stdout + result.stderr
            status = "passed" if result.returncode == 0 else "failed"
            code = result.returncode
        except subprocess.TimeoutExpired as error:
            log = (error.stdout or b"") + (error.stderr or b"")
            status, code = "timeout", None
        (directory / f"{case}.log").write_bytes(log)
        results.append({"case": case, "status": status, "returncode": code})
        print(json.dumps(results[-1]), flush=True)
    summary = {"provider": "host" if args.host else "guest", "module_sha256": module_hash,
               "selection": getattr(args, "suite", None) or "custom",
               "excluded_cases": [case for case in CASES if case not in args.cases], "cases": results}
    (directory / "results.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    return 0 if all(result["status"] == "passed" for result in results) else 1


def main():
    parser = argparse.ArgumentParser(description="Run the CPU JobManager fixtures through native relinker startup using an owned module or the host fallback.")
    provider = parser.add_mutually_exclusive_group(required=True)
    provider.add_argument("--module", type=Path, help="Owned decrypted SELF or ELF libSceJobManager.prx")
    provider.add_argument("--host", action="store_true", help="Validate the runner against the built host fallback")
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--relinker", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, help="Directory containing the matching compiler runtime DLLs (required on Windows)")
    parser.add_argument("--output", type=Path, help="New directory for staged modules, per-case logs and results.json; defaults to a temporary directory")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--cases", nargs="+", choices=CASES)
    selection.add_argument("--suite", choices=("all", "parity"), help="all runs every case (default); parity excludes original-module node_pool and worker_wait stalls with host kernel services")
    parser.add_argument("--timeout", type=float, default=20)
    args = parser.parse_args()
    if args.cases is None:
        args.suite = args.suite or "all"
        args.cases = list(PARITY_CASES if args.suite == "parity" else CASES)
    if sys.platform not in ("win32", "linux"):
        parser.error("The reference adapter supports Windows and Linux")
    if sys.platform == "win32" and args.toolchain is None:
        parser.error("--toolchain is required on Windows")
    if not 0 < args.timeout <= 30:
        parser.error("--timeout must be greater than zero and at most 30 seconds")
    try:
        for name in ("build", "relinker", "toolchain", "module"):
            value = getattr(args, name)
            if value is not None:
                setattr(args, name, resolve_existing_cli_path(value))
        directory = resolve_cli_path(args.output) if args.output else Path(tempfile.mkdtemp(prefix="anyps5-jobmanager-"))
        if args.output:
            directory.mkdir(parents=True, exist_ok=False)
        print(f"Artifacts: {directory}", flush=True)
        return run(args, directory)
    except (AuditError, OSError, Refused, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError) and error.stderr:
            print(error.stderr.decode(errors="replace"), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
