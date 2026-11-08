#!/usr/bin/env python3
import argparse
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from nid_names import compute_nid

NID = re.compile(r"[A-Za-z0-9+-]{11}")
COMMANDS = ("list", "args", "continue")


def to_nid(text, as_name):
    if not as_name and NID.fullmatch(text):
        return text
    if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", text):
        return compute_nid(text)
    raise ValueError(f"'{text}' is neither an 11-character NID nor a symbol name")


def command_lines(args):
    if args.action in COMMANDS:
        return [args.action]
    lines = []
    for target in args.targets:
        nid = to_nid(target, args.name)
        lines.append(f"{args.action} {nid}")
    return lines


def build_parser():
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--cmd", help="command file; defaults to ANYPS5_DEBUG_CMD")
    common.add_argument("--name", action="store_true", help="treat every target as a symbol name, even when it looks like a NID")
    common.add_argument("--print", dest="show", action="store_true", help="print the lines instead of writing the file")
    parser = argparse.ArgumentParser(description="Write breakpoint commands for the NID debugger.")
    sub = parser.add_subparsers(dest="action", required=True)
    for action in ("add", "remove"):
        p = sub.add_parser(action, parents=[common], help=f"{action} breakpoints by NID or symbol name")
        p.add_argument("targets", nargs="+")
    for action in COMMANDS:
        sub.add_parser(action, parents=[common], help=f"write the '{action}' command")
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    try:
        lines = command_lines(args)
    except ValueError as error:
        print(f"debug_nids: {error}", file=sys.stderr)
        return 2
    if args.show:
        print("\n".join(lines))
        return 0
    path = args.cmd or os.environ.get("ANYPS5_DEBUG_CMD")
    if not path:
        print("debug_nids: set ANYPS5_DEBUG_CMD or pass --cmd", file=sys.stderr)
        return 2
    with open(path, "w", encoding="utf-8") as out:
        out.write("".join(line + "\n" for line in lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
