import argparse
import hashlib
import importlib.util
import json
import os
import operator
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
from functools import cache
from pathlib import Path

HERE = Path(__file__).resolve().parent
CACHE = Path(os.environ.get("HW_ORACLE_CACHE") or Path(os.environ.get("XDG_CACHE_HOME") or Path.home() / ".cache") / "anyps5-hw-oracle")
ROWS_PER_DISPATCH = 1024
DEFAULT_LDS = 4096
MAX_LDS = 65536
RUNTIMES = ("hsa", "hip")


def runtime():
    selected = os.environ.get("HW_ORACLE_RUNTIME") or ("hip" if os.name == "nt" else "hsa")
    if selected not in RUNTIMES:
        raise ValueError(f"HW_ORACLE_RUNTIME must be one of {', '.join(RUNTIMES)}")
    return selected


@cache
def hip_runtime():
    spec = importlib.util.spec_from_file_location("oracle_hip", HERE / "oracle_hip.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def rocm_root():
    for root in (os.environ.get("ROCM_PATH"), "/opt/rocm"):
        if root and (Path(root) / "include" / "hsa" / "hsa.h").exists():
            return Path(root)
    return None


def tool(name):
    found = shutil.which(name)
    if found:
        return found
    root = rocm_root()
    if root and (root / "llvm" / "bin" / name).exists():
        return str(root / "llvm" / "bin" / name)
    sys.exit(f"{name} not found: install LLVM with the AMDGPU target or set ROCM_PATH")


def oracle():
    source = HERE / "oracle.c"
    compiler = shlex.split(os.environ.get("CC", "cc"))
    if not compiler:
        raise ValueError("CC must name a compiler command")
    root = rocm_root()
    if root is not None:
        root = root.resolve()
    executable = shutil.which(compiler[0])
    selected = str(Path(executable).resolve()) if executable is not None else None
    configuration = repr((compiler, selected, str(root))).encode("utf-8")
    key = hashlib.sha256(source.read_bytes() + configuration).hexdigest()
    directory = CACHE / key
    binary = directory / "oracle"
    if binary.is_file():
        return binary
    directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=directory) as tmp:
        output = Path(tmp) / "oracle"
        command = [*compiler, "-O1", str(source), "-o", str(output)]
        if root:
            lib = root / "lib"
            command += [f"-I{root / 'include'}", f"-L{lib}", f"-Wl,-rpath,{lib}"]
        subprocess.run(command + ["-lhsa-runtime64"], check=True, stdout=sys.stderr)
        output.replace(binary)
    return binary




@cache
def detected_target(binary, visible_devices):
    environment = dict(os.environ)
    if visible_devices is None:
        environment.pop("ROCR_VISIBLE_DEVICES", None)
    else:
        environment["ROCR_VISIBLE_DEVICES"] = visible_devices
    return subprocess.run([binary, "--target"], capture_output=True, text=True, check=True,
                          env=environment).stdout.strip()


def target():
    if os.environ.get("HW_ORACLE_TARGET"):
        return os.environ["HW_ORACLE_TARGET"]
    if runtime() == "hip":
        return hip_runtime().target()
    return detected_target(oracle(), os.environ.get("ROCR_VISIBLE_DEVICES"))


def assemble(body, work, wave64, *, ieee, denorm32, denorm16, dx10_clamp, round32, round16, fp16_overflow, lds=DEFAULT_LDS):
    modes = (("ieee", ieee, 2), ("denorm32", denorm32, 4), ("denorm16", denorm16, 4),
             ("dx10_clamp", dx10_clamp, 2), ("round32", round32, 4), ("round16", round16, 4),
             ("fp16_overflow", fp16_overflow, 2))
    for name, value, limit in modes:
        if not isinstance(value, int) or value not in range(limit):
            raise ValueError(f"{name} must be an integer from 0 to {limit - 1}")
    if not isinstance(lds, int) or lds not in range(MAX_LDS + 1):
        raise ValueError(f"lds must be an integer from 0 to {MAX_LDS}")
    text = (HERE / "template.s").read_text(encoding="utf-8")
    for key, value in (("@TARGET@", target()), ("@WAVE32@", "0" if wave64 else "1"), ("@WAVESIZE@", "64" if wave64 else "32"),
                       ("@DENORM@", str(int(denorm32))), ("@DENORM16@", str(int(denorm16))), ("@IEEE@", str(int(ieee))),
                       ("@DX10_CLAMP@", str(int(dx10_clamp))), ("@ROUND32@", str(int(round32))), ("@ROUND16@", str(int(round16))),
                       ("@FP16_OVERFLOW@", str(int(fp16_overflow))), ("@LDS@", str(lds)), ("@BODY@", body)):
        text = text.replace(key, value)
    (work / "k.s").write_text(text, encoding="utf-8")
    if runtime() == "hip":
        (work / "k.co").write_bytes(hip_runtime().assemble(text.encode("utf-8"), target(), wave64))
        return work / "k.co"
    subprocess.run([tool("clang"), "-x", "assembler", "-target", "amdgcn-amd-amdhsa", f"-mcpu={target()}", *(["-mwavefrontsize64"] if wave64 else []), "-c", str(work / "k.s"), "-o", str(work / "k.o")], check=True, stdout=sys.stderr)
    subprocess.run([tool("ld.lld"), "-m", "elf64_amdgpu", "-shared", str(work / "k.o"), "-o", str(work / "k.co")], check=True, stdout=sys.stderr)
    return work / "k.co"


def validated_rows(rows):
    validated = []
    for index, row in enumerate(rows, 1):
        try:
            values = tuple(operator.index(value) for value in row)
        except TypeError as error:
            raise ValueError(f"row {index} needs 4 unsigned 32-bit integers") from error
        if len(values) != 4 or any(value < 0 or value > 0xffffffff for value in values):
            raise ValueError(f"row {index} needs 4 unsigned 32-bit integers")
        validated.append(values)
    return validated


def run(body, rows, extra=b"", wave64=False, coarse=False, *, ieee, denorm32, denorm16, dx10_clamp, round32, round16, fp16_overflow, lds=DEFAULT_LDS):
    rows = validated_rows(rows)
    wave = 64 if wave64 else 32
    padded = rows + [(0, 0, 0, 0)] * (-len(rows) % wave)
    env = dict(os.environ)
    env.pop("COARSE", None)
    if coarse:
        env["COARSE"] = "1"
    out = []
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        code = assemble(body, work, wave64, ieee=ieee, denorm32=denorm32, denorm16=denorm16,
                        dx10_clamp=dx10_clamp, round32=round32, round16=round16, fp16_overflow=fp16_overflow, lds=lds)
        rows_per_dispatch = ROWS_PER_DISPATCH // 2 if wave64 else ROWS_PER_DISPATCH
        for start in range(0, len(padded), rows_per_dispatch):
            chunk = padded[start:start + rows_per_dispatch]
            payload = b"".join(struct.pack("<4I", *r) for r in chunk) + extra
            if runtime() == "hip":
                data = hip_runtime().dispatch(Path(code).read_bytes(), len(chunk), payload, 16, coarse)
            else:
                (work / "in.bin").write_bytes(payload)
                (work / "out.bin").unlink(missing_ok=True)
                subprocess.run([oracle(), str(code), str(len(chunk)), str(work / "in.bin"), str(work / "out.bin"), "16"], check=True, env=env, stdout=sys.stderr)
                data = (work / "out.bin").read_bytes()
            expected = len(chunk) * 64
            if len(data) != expected:
                raise ValueError(f"Oracle output size: expected {expected} bytes, got {len(data)}")
            out += [struct.unpack_from("<16I", data, lane * 64) for lane in range(len(chunk))]
    return out[:len(rows)]


def parse_word(value):
    try:
        return int(value, 0)
    except ValueError:
        return int(value, 10)


def main():
    parser = argparse.ArgumentParser(description="Run an RDNA kernel body on the local AMD GPU, one input row per lane.")
    parser.add_argument("body", type=Path, help="assembly inserted into template.s")
    parser.add_argument("rows", type=Path, help="one row per line: 4 u32 values (decimal or 0x hex)")
    parser.add_argument("--wave64", action="store_true")
    parser.add_argument("--ieee", type=int, choices=(0, 1), required=True)
    parser.add_argument("--denorm32", type=int, choices=range(4), required=True)
    parser.add_argument("--denorm16", type=int, choices=range(4), required=True)
    parser.add_argument("--dx10-clamp", type=int, choices=(0, 1), required=True)
    parser.add_argument("--round32", type=int, choices=range(4), required=True)
    parser.add_argument("--round16", type=int, choices=range(4), required=True)
    parser.add_argument("--fp16-overflow", type=int, choices=(0, 1), required=True)
    parser.add_argument("--coarse", action="store_true", help="input and output in coarse-grained GPU memory")
    parser.add_argument("--lds", type=int, default=DEFAULT_LDS, metavar="BYTES", help=f"group segment size, 0 to {MAX_LDS} (default {DEFAULT_LDS})")
    parser.add_argument("--extra", type=Path, help="bytes appended after the rows")
    parser.add_argument("--outs", type=int, choices=range(1, 17), default=16, metavar="N", help="print v10..v(10+N-1)")
    parser.add_argument("--json", action="store_true", help="emit inputs, results and run settings as JSON; target is the assembly target, not a GPU model or driver")
    parser.add_argument("--validate-only", action="store_true",
                        help="check input rows and report dispatch dimensions without assembling or executing the kernel")
    args = parser.parse_args()
    if args.lds not in range(MAX_LDS + 1):
        parser.error(f"lds must be an integer from 0 to {MAX_LDS}")
    try:
        rows = validated_rows(tuple(parse_word(v) for v in line.split())
                              for line in args.rows.read_text(encoding="utf-8").splitlines() if line.strip())
        if not rows:
            parser.error("rows file must contain at least one input row")
        extra = args.extra.read_bytes() if args.extra else b""
        body = args.body.read_text(encoding="utf-8")
    except (OSError, ValueError) as error:
        parser.error(str(error))
    modes = {name: getattr(args, name) for name in ("ieee", "denorm32", "denorm16", "dx10_clamp", "round32", "round16", "fp16_overflow")}
    if args.validate_only:
        wave = 64 if args.wave64 else 32
        padded = (len(rows) + wave - 1) // wave * wave
        dispatch = ROWS_PER_DISPATCH // 2 if args.wave64 else ROWS_PER_DISPATCH
        report = {"validation_only": True, "rows": len(rows), "padded_rows": padded,
                  "dispatches": (padded + dispatch - 1) // dispatch, "wave_size": wave,
                  "floating_point_modes": modes, "lds_bytes": args.lds,
                  "memory": "coarse" if args.coarse else "fine", "extra_bytes": len(extra)}
        print(json.dumps(report, indent=2) if args.json else
              f"Validated {len(rows)} input rows, {padded} padded rows, {report['dispatches']} dispatches. "
              "Assembly and GPU execution were not checked.")
        return 0
    results = run(body, rows, extra, args.wave64, args.coarse, **modes, lds=args.lds)
    if args.json:
        report = {
            "schema_version": 1,
            "assembly_target": target(),
            "wave_size": 64 if args.wave64 else 32,
            "floating_point_modes": modes,
            "lds_bytes": args.lds,
            "memory": "coarse" if args.coarse else "fine",
            "body": body,
            "extra_bytes": len(extra),
            "extra_sha256": hashlib.sha256(extra).hexdigest(),
            "output_registers": [f"v{10 + index}" for index in range(args.outs)],
            "rows": [{"input": row, "output": result[:args.outs]} for row, result in zip(rows, results)],
        }
        print(json.dumps(report, indent=2))
    else:
        for result in results:
            print(" ".join(f"{v:08x}" for v in result[:args.outs]))


if __name__ == "__main__":
    main()
