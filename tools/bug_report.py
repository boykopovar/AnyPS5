import argparse
from contextlib import suppress
import getpass
import json
import os
import platform
from pathlib import Path
import re
import socket
import sys
import zipfile

from performance_compare import unique_json_object


IPV4_OCTET = r"(?!25[6-9]|2[6-9][0-9]|[3-9][0-9]{2})\d{1,3}"
IPV4 = re.compile(rf"\b(?:{IPV4_OCTET}\.){{3}}{IPV4_OCTET}\b")


def redact(text, *, home, user, host):
    windows_home = re.match(r"^(?:[a-zA-Z]:[\\/]|\\\\)", home or "") is not None
    if home and len(home) >= 3:
        if windows_home:
            parts = re.split(r"[\\/]", home.rstrip("\\/"))
            pattern = r"[\\/]".join(re.escape(part) for part in parts)
            text = re.sub(pattern + r"(?![\w-]|\.+[\w-])", "~", text, flags=re.IGNORECASE)
        elif home.rstrip("/"):
            text = re.sub(re.escape(home.rstrip("/")) + r"(?![\w-]|\.+[\w-])", "~", text)
    if user and len(user) >= 2:
        text = re.sub(r"(?<![\w-])" + re.escape(user) + r"(?![\w-])", "<user>", text,
                      flags=re.IGNORECASE if windows_home else 0)
    host = (host or "").rstrip(".")
    for name in dict.fromkeys((host, host.split(".")[0])):
        if len(name) >= 2:
            text = re.sub(r"(?<![\w-])" + re.escape(name) + r"(?![\w-])", "<host>", text, flags=re.IGNORECASE)
    return IPV4.sub("<ip>", text)


def redact_report(value, identity):
    if isinstance(value, str):
        return redact(value, **identity)
    if isinstance(value, list):
        return [redact_report(item, identity) for item in value]
    if isinstance(value, dict):
        result = {}
        for key, item in value.items():
            redacted_key = redact_report(key, identity)
            if redacted_key in result:
                raise ValueError("duplicate JSON object keys after redaction")
            result[redacted_key] = redact_report(item, identity)
        return result
    return value


README = """AnyPS5 bug report

anyps5-report.json        the run report of the last run, redacted
anyps5.log                the log of the run (ANYPS5_LOG_FILE), redacted
anyps5-input.ini          the shared input mapping, redacted
anyps5-input-profile.ini  the title's input mapping overrides, redacted
anyps5-settings.txt       the ANYPS5_ and APS5_ environment variables of the run, redacted
host.txt                  operating system, machine and Python version
README.txt                this file

Redacted: the home directory (as ~), the user and host names (as <user> and <host>),
and IPv4 addresses (as <ip>). Nothing else is changed. Check the files before you
attach the bundle to a public report.
"""


def settings_text(environ, identity):
    lines = [f"{name}={redact(value, **identity)}" for name, value in sorted(environ.items()) if name.startswith(("ANYPS5_", "APS5_"))]
    return "\n".join(lines) + ("\n" if lines else "No ANYPS5_ or APS5_ variables were set.\n")


def input_profile_path(report, input_config):
    game = report.get("game") if isinstance(report, dict) else None
    title_id = game.get("titleId") if isinstance(game, dict) else None
    if isinstance(title_id, str) and re.fullmatch(r"[A-Za-z0-9_-]{1,32}", title_id):
        config = Path(input_config)
        if config.name:
            return config.with_name(f"anyps5-input.{title_id}.ini")
    return None


def host_text():
    return (
        f"os: {platform.system()} {platform.release()}\n"
        f"machine: {platform.machine()}\n"
        f"processor: {platform.processor() or 'unknown'}\n"
        f"logical cpus: {os.cpu_count() or 'unknown'}\n"
        f"python: {platform.python_version()}\n"
    )


def bundle_members(report, *, home, user, host, log=None, input_config=None, environ=None):
    data = json.loads(Path(report).read_text(encoding="utf-8-sig"), object_pairs_hook=unique_json_object)
    identity = {"home": home, "user": user, "host": host}
    members = {"anyps5-report.json": json.dumps(redact_report(data, identity), indent=2, allow_nan=False) + "\n"}
    profile = input_profile_path(data, input_config) if input_config else None
    if profile is not None and profile.is_file():
        members["anyps5-input-profile.ini"] = redact(profile.read_text(encoding="utf-8", errors="replace"), **identity)
    if log is not None and Path(log).is_file():
        members["anyps5.log"] = redact(Path(log).read_text(encoding="utf-8", errors="replace"), **identity)
    if input_config is not None and Path(input_config).is_file():
        members["anyps5-input.ini"] = redact(Path(input_config).read_text(encoding="utf-8", errors="replace"), **identity)
    if environ is not None:
        members["anyps5-settings.txt"] = settings_text(environ, identity)
    members["host.txt"] = redact(host_text(), **identity)
    members["README.txt"] = README
    return ((name, content.encode("utf-8", errors="backslashreplace")) for name, content in members.items())


def make_bundle(report, out, *, home, user, host, log=None, input_config=None, environ=None):
    out = Path(out)
    if out.exists():
        raise FileExistsError(f"{out} already exists")
    members = bundle_members(report, home=home, user=user, host=host, log=log, input_config=input_config, environ=environ)
    names = []
    destination = out.open("xb")
    try:
        with destination, zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
            for name, content in members:
                bundle.writestr(name, content)
                names.append(name)
    except BaseException:
        with suppress(OSError):
            out.unlink()
        raise
    return sorted(names)


def identities():
    try:
        user = getpass.getuser()
    except Exception:
        user = ""
    return {"home": str(Path.home()), "user": user, "host": socket.gethostname()}


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Bundle the run report, log and input mapping of an AnyPS5 run, with personal details redacted.")
    parser.add_argument("--report", default="anyps5-report.json", help="run report to include (default: anyps5-report.json)")
    parser.add_argument("--log", default=os.environ.get("ANYPS5_LOG_FILE") or None, help="log file to include (default: ANYPS5_LOG_FILE)")
    default_config = os.environ.get("ANYPS5_INPUT_CONFIG") or "anyps5-input.ini"
    parser.add_argument("--input-config", default=default_config, help="input mapping to include (default: ANYPS5_INPUT_CONFIG or ./anyps5-input.ini)")
    parser.add_argument("--out", default="bug-report.zip", help="bundle to write; an existing file is never overwritten (default: bug-report.zip)")
    parser.add_argument("--preview", action="store_true", help="list redacted ZIP entries and uncompressed sizes without writing an archive")
    args = parser.parse_args(argv)
    if not args.preview and not Path(args.report).is_file():
        print(f"bug_report: no run report at {args.report}", file=sys.stderr)
        return 1
    try:
        inputs = dict(log=args.log, input_config=args.input_config, environ=os.environ, **identities())
        if args.preview:
            sizes = sorted((name, len(content)) for name, content in bundle_members(args.report, **inputs))
            for name, size in sizes:
                print(f"{name} ({size} bytes)")
            return 0
        names = make_bundle(args.report, args.out, **inputs)
    except (OSError, ValueError, RecursionError) as error:
        message = "could not prepare the preview. Check the run report and input files." if args.preview else str(error)
        print(f"bug_report: {message}", file=sys.stderr)
        return 1
    print(f"wrote {args.out}: {', '.join(names)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
