import argparse
import ctypes
import io
import os
from pathlib import Path
import re
import stat
import sys
import tempfile
import time


COMMAND_FILE_BYTES = 64 * 1024
MAX_COMMAND_LINE_BYTES = 126
MAX_SEQUENCE = 2**64 - 1


BUTTONS = {
    "l3": 0x2, "r3": 0x4, "options": 0x8,
    "up": 0x10, "right": 0x20, "down": 0x40, "left": 0x80,
    "l2": 0x100, "r2": 0x200, "l1": 0x400, "r1": 0x800,
    "triangle": 0x1000, "circle": 0x2000, "cross": 0x4000,
    "square": 0x8000, "touchpad": 0x100000,
    "touch-left": 0x200000, "touch-right": 0x400000, "release": 0,
}


def trim_command_line(line):
    line = line.removesuffix(b"\n").removesuffix(b"\r")
    if len(line) > MAX_COMMAND_LINE_BYTES:
        return None
    return line.split(b"\0", 1)[0]


def timeline_sequence(command, header):
    if os.name == "nt" and b"\x1a" in header:
        return 0
    header = trim_command_line(header)
    if header is None:
        return 0
    fields = re.fullmatch(rb"\s*APS5_PAD_TIMELINE\s+(\+?[0-9]+)\s+(\+?[0-9]+)\s+(\+?[0-9]+)\s*", header)
    if fields is None:
        return 0
    version, sequence, count = [int(token) for token in fields.groups()]
    if version != 1 or not 1 <= sequence <= MAX_SEQUENCE or not 1 <= count <= 4096:
        return 0
    previous_end = 0
    for _ in range(count):
        line = command.readline(129)
        if os.name == "nt" and b"\x1a" in line:
            line = line.split(b"\x1a", 1)[0]
            command.seek(0, os.SEEK_END)
        if not line:
            return 0
        line = trim_command_line(line)
        if line is None:
            return 0
        fields = re.fullmatch(rb"\s*(\+?[0-9]+)(?:\s+|(?=\+))(\+?[0-9]+)(?:\s+|(?=\+))(\+?[0-9]+)((?:(?:\s+|(?=\+))\+?[0-9]+){4})?\s*", line)
        if fields is None:
            return 0
        start, duration, buttons = [int(token) for token in fields.groups()[:3]]
        if buttons & ~0x70fffe or not 1 <= duration <= 5000 or start < previous_end or start + duration > 3600000:
            return 0
        if fields.group(4) and any(int(axis) > 255 for axis in re.findall(rb"\+?[0-9]+", fields.group(4))):
            return 0
        previous_end = start + duration
    while tail := command.read(4096):
        if os.name == "nt" and b"\x1a" in tail:
            return sequence if not tail.split(b"\x1a", 1)[0].strip() else 0
        if tail.strip():
            return 0
    return sequence


def previous_sequence(path):
    nonblocking = getattr(os, "O_NONBLOCK", 0)
    try:
        with open(path, "rb", opener=lambda name, flags: os.open(name, flags | nonblocking)) as source:
            if not stat.S_ISREG(os.fstat(source.fileno()).st_mode):
                raise ValueError(f"controller command path must be a regular file: {path}")
            contents = source.read(COMMAND_FILE_BYTES + 1)
    except FileNotFoundError:
        return 0
    if len(contents) > COMMAND_FILE_BYTES:
        return 0
    if os.name == "nt":
        contents = contents.split(b"\x1a", 1)[0]
    if not contents:
        return 0
    command = io.BytesIO(contents)
    raw = command.readline(129)
    line = trim_command_line(raw)
    if line is None:
        return 0
    tokens = line.split()
    if tokens[:1] == [b"APS5_PAD_TIMELINE"]:
        return timeline_sequence(command, raw)
    if len(tokens) in (4, 8):
        touch = re.fullmatch(rb"touch\((\+?[0-9]+),(\+?[0-9]+),(\+?[0-9]+),(\+?[0-9]+)\)", tokens.pop())
        if touch is None or any(int(value) > limit for value, limit in zip(touch.groups(), (1919, 1079, 127, 1))):
            return 0
    if len(tokens) not in (3, 7):
        return 0
    if any(re.fullmatch(rb"\+?[0-9]+", token) is None for token in tokens):
        return 0
    values = [int(token) for token in tokens]
    if any(value > 2**64 - 1 for value in values):
        return 0
    if values[0] == 0 or values[1] & ~0x70fffe or not 1 <= values[2] <= 5000:
        return 0
    if any(axis > 255 for axis in values[3:]):
        return 0
    return values[0]


def next_sequence(path):
    sequence = max(time.monotonic_ns(), previous_sequence(path) + 1)
    if sequence > MAX_SEQUENCE:
        raise ValueError("controller command sequence is exhausted")
    return sequence


def replace_command(temporary, path):
    try:
        os.replace(temporary, path)
    except PermissionError as error:
        if os.name != "nt" or error.winerror not in (5, 32):
            raise
        replace = ctypes.WinDLL("kernel32", use_last_error=True).ReplaceFileW
        replace.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_wchar_p,
                            ctypes.c_uint32, ctypes.c_void_p, ctypes.c_void_p]
        replace.restype = ctypes.c_int
        if not replace(str(path), str(temporary), None, 0, None, None):
            raise


def button_mask(value):
    names = value.split("+")
    if any(name not in BUTTONS for name in names) or ("release" in names and len(names) > 1) or len(set(names)) != len(names):
        raise argparse.ArgumentTypeError(f"invalid button {value!r}: use one of {', '.join(BUTTONS)}, or several joined by '+'")
    mask = 0
    for name in names:
        mask |= BUTTONS[name]
    return mask


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Queue a controller pulse for APS5_PAD_INPUT_FILE.")
    parser.add_argument("path", type=Path)
    parser.add_argument("button", help=f"a button name ({', '.join(BUTTONS)}), or several joined by '+' to press them together (for example l1+r1); release must be used alone")
    parser.add_argument("--hold-ms", type=int, default=300)
    parser.add_argument("--left-stick", type=int, nargs=2, metavar=("X", "Y"))
    parser.add_argument("--right-stick", type=int, nargs=2, metavar=("X", "Y"))
    parser.add_argument("--touch", type=int, nargs=4, metavar=("X", "Y", "ID", "ACTIVE"),
                        help="touch contact: X 0..1919, Y 0..1079, ID 0..127, ACTIVE 0 or 1; keep ID while moving, use 0 to lift")
    args = parser.parse_args(argv)
    try:
        mask = button_mask(args.button)
    except argparse.ArgumentTypeError as error:
        parser.error(str(error))
    if args.path.is_dir():
        parser.error(f"{args.path} is a directory")
    if not args.path.parent.is_dir():
        parser.error(f"directory does not exist: {args.path.parent}")
    if not 1 <= args.hold_ms <= 5000:
        parser.error("--hold-ms must be between 1 and 5000")
    touch = ""
    if args.touch is not None:
        if any(not 0 <= value <= limit for value, limit in zip(args.touch, (1919, 1079, 127, 1))):
            parser.error("--touch requires X 0..1919, Y 0..1079, ID 0..127, ACTIVE 0 or 1")
        touch = " touch(" + ",".join(str(value) for value in args.touch) + ")"
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
            output.write(f"{sequence} {mask} {args.hold_ms}{sticks}{touch}\n")
        replace_command(temporary, args.path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    print(f"Queued {args.button}{' with sticks' + sticks if sticks else ''} for {args.hold_ms} ms in {args.path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
