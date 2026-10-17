import argparse
import os
from pathlib import Path
import re
import stat
import tempfile
import time


BUTTONS = {
    "l3": 0x2, "r3": 0x4, "options": 0x8,
    "up": 0x10, "right": 0x20, "down": 0x40, "left": 0x80,
    "l2": 0x100, "r2": 0x200, "l1": 0x400, "r1": 0x800,
    "triangle": 0x1000, "circle": 0x2000, "cross": 0x4000,
    "square": 0x8000, "touchpad": 0x100000, "release": 0,
}


def previous_sequence(path):
    try:
        with path.open("rb") as command:
            line = command.readline(129)
    except (FileNotFoundError, IsADirectoryError, PermissionError):
        return 0
    if os.name == "nt" and line.endswith(b"\r\n"):
        line = line[:-2] + b"\n"
    tokens = line.split()
    if len(line.rstrip(b"\n")) > 127 or len(tokens) not in (3, 7):
        return 0
    if any(re.fullmatch(rb"\+?[0-9]+", token) is None for token in tokens):
        return 0
    values = [int(token) for token in tokens]
    if any(value > 2**64 - 1 for value in values):
        return 0
    if values[0] == 0 or values[1] & ~0x10fffe or not 1 <= values[2] <= 5000:
        return 0
    if any(axis > 255 for axis in values[3:]):
        return 0
    return values[0]


def next_sequence(path):
    sequence = max(time.monotonic_ns(), previous_sequence(path) + 1)
    if sequence > 2**64 - 1:
        raise ValueError("controller command sequence is exhausted")
    return sequence


def main():
    parser = argparse.ArgumentParser(description="Queue a controller pulse for APS5_PAD_INPUT_FILE.")
    parser.add_argument("path", type=Path)
    parser.add_argument("button", choices=BUTTONS)
    parser.add_argument("--hold-ms", type=int, default=300)
    parser.add_argument("--left-stick", type=int, nargs=2, metavar=("X", "Y"))
    parser.add_argument("--right-stick", type=int, nargs=2, metavar=("X", "Y"))
    args = parser.parse_args()
    if args.path.is_dir():
        parser.error(f"{args.path} is a directory")
    if not args.path.parent.is_dir():
        parser.error(f"directory does not exist: {args.path.parent}")
    if not 1 <= args.hold_ms <= 5000:
        parser.error("--hold-ms must be between 1 and 5000")
    sticks = ""
    if args.left_stick is not None or args.right_stick is not None:
        axes = (args.left_stick or [128, 128]) + (args.right_stick or [128, 128])
        if not all(0 <= axis <= 255 for axis in axes):
            parser.error("stick axes must be between 0 and 255")
        sticks = "".join(f" {axis}" for axis in axes)
    try:
        sequence = next_sequence(args.path)
    except ValueError as error:
        parser.error(str(error))
    metadata = None
    if os.name == "posix":
        try:
            metadata = args.path.stat()
        except FileNotFoundError:
            pass
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n", dir=args.path.parent, delete=False) as output:
            temporary = Path(output.name)
            if metadata is not None:
                if os.geteuid() == 0:
                    os.fchown(output.fileno(), metadata.st_uid, metadata.st_gid)
                os.fchmod(output.fileno(), stat.S_IMODE(metadata.st_mode))
            output.write(f"{sequence} {BUTTONS[args.button]} {args.hold_ms}{sticks}\n")
        os.replace(temporary, args.path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    print(f"Queued {args.button}{' with sticks' + sticks if sticks else ''} for {args.hold_ms} ms in {args.path}")


if __name__ == "__main__":
    main()
