import argparse
import sys
from pathlib import Path

from pad_input import button_mask, next_sequence


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Print a controller pulse without modifying its command file")
    parser.add_argument("path", type=Path)
    parser.add_argument("button", type=button_mask)
    parser.add_argument("--hold-ms", type=int, default=300)
    parser.add_argument("--left-stick", type=int, nargs=2, metavar=("X", "Y"))
    parser.add_argument("--right-stick", type=int, nargs=2, metavar=("X", "Y"))
    parser.add_argument("--touch", type=int, nargs=4, metavar=("X", "Y", "ID", "ACTIVE"))
    args = parser.parse_args(argv)
    if not 1 <= args.hold_ms <= 5000:
        parser.error("--hold-ms must be between 1 and 5000")
    sticks = []
    if args.left_stick is not None or args.right_stick is not None:
        sticks = (args.left_stick or [128, 128]) + (args.right_stick or [128, 128])
        if any(not 0 <= value <= 255 for value in sticks):
            parser.error("stick axes must be between 0 and 255")
    touch = ""
    if args.touch is not None:
        if any(not 0 <= value <= limit for value, limit in zip(args.touch, (1919, 942, 127, 1))):
            parser.error("invalid touch coordinates, ID or active flag")
        touch = " touch(" + ",".join(map(str, args.touch)) + ")"
    try:
        sequence = next_sequence(args.path)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(" ".join(map(str, [sequence, args.button, args.hold_ms, *sticks])) + touch)
    return 0


if __name__ == "__main__":
    sys.exit(main())
