import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTCOMES = ("relink-failed", "crashed", "exited", "stalled", "blank", "rendering")
TITLE_FIELDS = {"name": str, "relink": list, "fixed_base": str, "executables": list, "seconds": int, "env": dict, "input": list}
FIXED_BASE_FLAGS = {"windows": "--windows-fixed-base", "linux": "--linux-fixed-base"}


def load_titles(path):
    titles = json.loads(Path(path).read_text(encoding="utf-8"))
    for title_id, entry in titles.items():
        if not re.fullmatch(r"[A-Z]{4}[0-9]{5}", title_id):
            raise ValueError(f"{path}: {title_id} is not a title ID")
        for key, value in entry.items():
            if key not in TITLE_FIELDS:
                raise ValueError(f"{path}: {title_id}: unknown field {key}")
            if not isinstance(value, TITLE_FIELDS[key]):
                raise ValueError(f"{path}: {title_id}: {key} must be {TITLE_FIELDS[key].__name__}")
    return titles


def relink_args(entry, platform):
    args = ["--windows"] if platform == "windows" else []
    args += entry.get("relink", [])
    if "fixed_base" in entry:
        if platform not in FIXED_BASE_FLAGS:
            raise ValueError(f"fixed_base is not supported on {platform}")
        args += [FIXED_BASE_FLAGS[platform], entry["fixed_base"]]
    for executable in entry.get("executables", []):
        args += ["--executable", executable]
    return args


ERROR = re.compile(r"what\(\):|FATAL|not implemented|unhandled exception")
WARNINGS = ("destroyed with GPU results pending", "guest memory changed while GPU results were pending")
STALL_SECONDS = 20


def first_error(text):
    return next((line.strip() for line in text.splitlines() if ERROR.search(line)), "")


def warning_counts(text):
    return {warning: text.count(warning) for warning in WARNINGS}


def read_summary(path):
    lines = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()] if path.is_file() else []
    presents = sum(line.get("presents", 0) for line in lines)
    seconds = sum(line.get("window", {}).get("ms", 0) for line in lines) / 1000
    last = lines[-1] if lines else {}
    return {"presents": presents, "fps": round(presents / seconds, 1) if seconds else 0.0,
            "gpu": last.get("gpu", ""), "driver": last.get("driver", ""), "os": last.get("os", "")}


def frame_is_uniform(path, tolerance=8):
    data = path.read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    channels = struct.unpack_from("<H", data, 28)[0] // 8
    stride = (width * channels + 3) & ~3
    first = data[offset:offset + 3]
    for row in range(abs(height)):
        pixels = data[offset + row * stride:offset + row * stride + width * channels]
        for index in range(0, len(pixels), channels):
            if max(abs(pixels[index + channel] - first[channel]) for channel in range(3)) > tolerance:
                return False
    return True


def classify(relinked, exit_code, summary_age, last_frame_uniform):
    if not relinked:
        return "relink-failed"
    if exit_code is not None:
        return "crashed" if exit_code != 0 else "exited"
    if summary_age is None or summary_age > STALL_SECONDS:
        return "stalled"
    if last_frame_uniform:
        return "blank"
    return "rendering"
