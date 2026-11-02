import argparse
import json
import os
import stat
from pathlib import Path
import tempfile

from pad_input import BUTTONS, next_sequence


def integer(value, field, minimum, maximum):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{field} must be an integer between {minimum} and {maximum}")
    return value


def compile_timeline(document, sequence, repeat=1, gap_ms=200):
    integer(sequence, "sequence", 1, 2**64 - 1)
    integer(repeat, "repeat", 1, 4096)
    integer(gap_ms, "gap_ms", 0, 3600000)
    if not isinstance(document, dict) or set(document) != {"version", "events"}:
        raise ValueError("timeline must contain version and events")
    if type(document["version"]) is not int or document["version"] != 1:
        raise ValueError("unsupported timeline version")
    source = document["events"]
    if not isinstance(source, list) or not 1 <= len(source) * repeat <= 4096:
        raise ValueError("timeline must contain 1 to 4096 events after repetition")
    events = []
    previous_end = 0
    for index, event in enumerate(source):
        prefix = f"events[{index}]"
        if not isinstance(event, dict) or set(event) != {"at_ms", "hold_ms", "buttons"}:
            raise ValueError(f"{prefix} must contain at_ms, hold_ms and buttons")
        start = integer(event["at_ms"], f"{prefix}.at_ms", 0, 3600000)
        duration = integer(event["hold_ms"], f"{prefix}.hold_ms", 1, 5000)
        if start < previous_end or start + duration > 3600000:
            raise ValueError(f"{prefix} overlaps an earlier event or exceeds one hour")
        names = event["buttons"]
        if not isinstance(names, list) or not all(isinstance(name, str) and name in BUTTONS for name in names):
            raise ValueError(f"{prefix}.buttons must be a list of known button names")
        if len(set(names)) != len(names):
            raise ValueError(f"{prefix}.buttons must not contain duplicate button names")
        if "release" in names and len(names) != 1:
            raise ValueError(f"{prefix}: release cannot be combined with another button")
        buttons = 0
        for name in names:
            buttons |= BUTTONS[name]
        events.append((start, duration, buttons))
        previous_end = start + duration
    period = previous_end + gap_ms
    if (repeat - 1) * period + previous_end > 3600000:
        raise ValueError("repeated timeline exceeds one hour")
    rows = [f"APS5_PAD_TIMELINE 1 {sequence} {len(events) * repeat}"]
    for iteration in range(repeat):
        rows.extend(f"{start + iteration * period} {duration} {buttons}" for start, duration, buttons in events)
    return "\n".join(rows) + "\n"


def queue_timeline(path, text):
    metadata = None
    if os.name == "posix":
        try:
            metadata = path.stat()
        except FileNotFoundError:
            pass
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n", dir=path.parent, delete=False) as output:
            temporary = Path(output.name)
            if metadata is not None:
                if os.geteuid() == 0:
                    os.fchown(output.fileno(), metadata.st_uid, metadata.st_gid)
                os.fchmod(output.fileno(), stat.S_IMODE(metadata.st_mode))
            output.write(text)
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description="Queue a timed controller button sequence for APS5_PAD_INPUT_FILE.")
    parser.add_argument("timeline", type=Path)
    parser.add_argument("path", type=Path)
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--gap-ms", type=int, default=200)
    args = parser.parse_args()
    try:
        if os.path.realpath(args.timeline) == os.path.realpath(args.path) or (args.path.exists() and os.path.samefile(args.timeline, args.path)):
            raise ValueError("the command file must be separate from the timeline JSON")
        if args.path.is_dir():
            raise ValueError(f"{args.path} is a directory")
        if not args.path.parent.is_dir():
            raise ValueError(f"directory does not exist: {args.path.parent}")
        document = json.loads(args.timeline.read_text(encoding="utf-8"))
        text = compile_timeline(document, next_sequence(args.path), args.repeat, args.gap_ms)
        queue_timeline(args.path, text)
    except (OSError, ValueError, RecursionError) as error:
        parser.exit(2, f"pad_timeline: {error}\n")
    print(f"Queued {len(text.splitlines()) - 1} timed events in {args.path}")


if __name__ == "__main__":
    main()
