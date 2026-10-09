import argparse
import os
from pathlib import Path
import tempfile
import time


BUTTONS = {
    "l3": 0x2, "r3": 0x4, "options": 0x8,
    "up": 0x10, "right": 0x20, "down": 0x40, "left": 0x80,
    "l2": 0x100, "r2": 0x200, "l1": 0x400, "r1": 0x800,
    "triangle": 0x1000, "circle": 0x2000, "cross": 0x4000,
    "square": 0x8000, "touchpad": 0x100000, "release": 0,
}


def main():
    parser = argparse.ArgumentParser(description="Queue a controller pulse for APS5_PAD_INPUT_FILE.")
    parser.add_argument("path", type=Path)
    parser.add_argument("button", choices=BUTTONS)
    parser.add_argument("--hold-ms", type=int, default=300)
    args = parser.parse_args()
    if not 1 <= args.hold_ms <= 5000:
        parser.error("--hold-ms must be between 1 and 5000")
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=args.path.parent, delete=False) as output:
            temporary = Path(output.name)
            output.write(f"{time.monotonic_ns()} {BUTTONS[args.button]} {args.hold_ms}\n")
        os.replace(temporary, args.path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    print(f"Queued {args.button} for {args.hold_ms} ms in {args.path}")


if __name__ == "__main__":
    main()
