import argparse
import getpass
import json
import os
import platform
from pathlib import Path
import re
import socket
import sys
import zipfile


IPV4 = re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b")


def redact(text, *, home, user, host):
    if home and len(home) >= 3:
        text = text.replace(home, "~")
    if user and len(user) >= 2:
        text = re.sub(r"(?<![\w-])" + re.escape(user) + r"(?![\w-])", "<user>", text)
    if host and len(host) >= 2:
        text = re.sub(r"(?<![\w-])" + re.escape(host) + r"(?![\w-])", "<host>", text)
    return IPV4.sub("<ip>", text)


README = """AnyPS5 bug report

anyps5-report.json  the run report of the last run, redacted
anyps5.log          the log of the run (ANYPS5_LOG_FILE), redacted
anyps5-input.ini    the input mapping in use, redacted
host.txt            operating system, machine and Python version
README.txt          this file

Redacted: the home directory (as ~), the user and host names (as <user> and <host>),
and IPv4 addresses (as <ip>). Nothing else is changed. Check the files before you
attach the bundle to a public report.
"""


def make_bundle(report, out, *, home, user, host, log=None, input_config=None):
    report = Path(report)
    out = Path(out)
    if out.exists():
        raise FileExistsError(f"{out} already exists")
    text = report.read_text(encoding="utf-8")
    json.loads(text)
    identity = {"home": home, "user": user, "host": host}
    members = {"anyps5-report.json": redact(text, **identity)}
    if log is not None and Path(log).is_file():
        members["anyps5.log"] = redact(Path(log).read_text(encoding="utf-8", errors="replace"), **identity)
    if input_config is not None and Path(input_config).is_file():
        members["anyps5-input.ini"] = redact(Path(input_config).read_text(encoding="utf-8", errors="replace"), **identity)
    members["host.txt"] = f"os: {platform.system()}\nmachine: {platform.machine()}\npython: {platform.python_version()}\n"
    members["README.txt"] = README
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
        for name, content in members.items():
            bundle.writestr(name, content.encode("utf-8"))
    return sorted(members)


def identities():
    try:
        user = getpass.getuser()
    except Exception:
        user = ""
    return {"home": str(Path.home()), "user": user, "host": socket.gethostname().split(".")[0]}


def main(argv=None):
    parser = argparse.ArgumentParser(description="Bundle the run report, log and input mapping of an AnyPS5 run, with personal details redacted.")
    parser.add_argument("--report", default="anyps5-report.json", help="run report to include (default: anyps5-report.json)")
    parser.add_argument("--log", default=os.environ.get("ANYPS5_LOG_FILE") or None, help="log file to include (default: ANYPS5_LOG_FILE)")
    default_config = os.environ.get("ANYPS5_INPUT_CONFIG") or ("anyps5-input.ini" if Path("anyps5-input.ini").is_file() else None)
    parser.add_argument("--input-config", default=default_config, help="input mapping to include (default: ANYPS5_INPUT_CONFIG or ./anyps5-input.ini)")
    parser.add_argument("--out", default="bug-report.zip", help="bundle to write; an existing file is never overwritten (default: bug-report.zip)")
    args = parser.parse_args(argv)
    if not Path(args.report).is_file():
        print(f"bug_report: no run report at {args.report}", file=sys.stderr)
        return 1
    try:
        names = make_bundle(args.report, args.out, log=args.log, input_config=args.input_config, **identities())
    except (OSError, ValueError) as error:
        print(f"bug_report: {error}", file=sys.stderr)
        return 1
    print(f"wrote {args.out}: {', '.join(names)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
