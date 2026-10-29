import argparse
import json
import sys
from pathlib import Path

from pad_timeline import compile_timeline


def preview(document, repeat=1, gap_ms=200):
    compiled = compile_timeline(document, 1, repeat, gap_ms)
    events = []
    for line in compiled.splitlines()[1:]:
        at_ms, hold_ms, buttons = map(int, line.split())
        events.append({"at_ms": at_ms, "hold_ms": hold_ms, "buttons": buttons})
    return {"preview": True, "event_count": len(events), "duration_ms": events[-1]["at_ms"] + events[-1]["hold_ms"],
            "events": events}


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Validate and preview a controller timeline without accessing a controller command file.")
    parser.add_argument("timeline", type=Path)
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--gap-ms", type=int, default=200)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        document = json.loads(args.timeline.read_text(encoding="utf-8"))
        result = preview(document, args.repeat, args.gap_ms)
    except (OSError, ValueError, RecursionError) as error:
        parser.exit(2, f"{parser.prog}: {error}\n")
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"Preview only: {result['event_count']} events, ending at {result['duration_ms']} ms. No controller command file was written.")
        print("at_ms hold_ms button_mask")
        for event in result["events"]:
            print(f"{event['at_ms']} {event['hold_ms']} 0x{event['buttons']:x}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
