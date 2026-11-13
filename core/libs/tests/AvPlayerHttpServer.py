import http.server
import pathlib
import re
import subprocess
import sys
import threading
import time
import urllib.parse


identity_requests = {}
unknown_range_requests = {}
identity_targets = []
legacy_dates = {
    "date-rfc850": ("Wednesday, 01-Jan-20 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT", "Wed, 01 Jan 2020 00:00:00 GMT"),
    "date-asctime": ("Wed Jan  1 00:00:00 2020", "Wed, 01 Jan 2020 00:02:00 GMT", "Wed, 01 Jan 2020 00:00:00 GMT"),
    "date-legacy-rfc850-date": ("Wed, 01 Jan 2020 00:00:00 GMT", "Wednesday, 01-Jan-20 00:02:00 GMT", "Wed, 01 Jan 2020 00:00:00 GMT"),
    "date-legacy-asctime-date": ("Wed, 01 Jan 2020 00:00:00 GMT", "Wed Jan  1 00:02:00 2020", "Wed, 01 Jan 2020 00:00:00 GMT"),
    "date-legacy-rfc850-both": ("Wednesday, 01-Jan-20 00:00:00 GMT", "Wednesday, 01-Jan-20 00:02:00 GMT", "Wed, 01 Jan 2020 00:00:00 GMT"),
    "date-legacy-asctime-both": ("Wed Jan  1 00:00:00 2020", "Wed Jan  1 00:02:00 2020", "Wed, 01 Jan 2020 00:00:00 GMT"),
    "date-legacy-asctime-two-digit": ("Wed Jan 01 00:00:00 2020", "Wed Jan 01 00:02:00 2020", "Wed, 01 Jan 2020 00:00:00 GMT"),
    "date-legacy-century": ("Friday, 31-Dec-99 00:00:00 GMT", "Fri, 31 Dec 1999 00:02:00 GMT", "Fri, 31 Dec 1999 00:00:00 GMT"),
    "date-legacy-200-match": ("Wednesday, 01-Jan-20 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT", "Wed, 01 Jan 2020 00:00:00 GMT"),
    "date-legacy-206-omit": ("Wednesday, 01-Jan-20 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT", "Wed, 01 Jan 2020 00:00:00 GMT"),
}
date_snapshots = {
    "date-short-year": ("Wed, 01 Jan 20 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-short-day": ("Wed, 1 Jan 2020 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-short-time": ("Wed, 01 Jan 2020 0:0:0 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-full-names": ("Wednesday, 01 January 2020 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-wrong-case": ("wed, 01 jan 2020 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-wrong-weekday": ("Thu, 01 Jan 2020 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-early-year": ("Sun, 01 Jan 1899 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-invalid-day": ("Mon, 31 Feb 2020 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-short-response-time": ("Wed, 01 Jan 2020 00:00:00 GMT", "Wed, 01 Jan 2020 0:02:00 GMT"),
    "date-wrong-response-weekday": ("Wed, 01 Jan 2020 00:00:00 GMT", "Thu, 01 Jan 2020 00:02:00 GMT"),
    "date-extra-response-space": ("Wed, 01 Jan 2020 00:00:00 GMT", "Wed,  01 Jan 2020 00:02:00 GMT"),
    "date-legacy-rfc850-weekday": ("Thursday, 01-Jan-20 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-legacy-asctime-weekday": ("Thu Jan  1 00:00:00 2020", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-legacy-rfc850-month": ("Wednesday, 01-Jax-20 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-legacy-asctime-month": ("Wed Jax  1 00:00:00 2020", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-legacy-rfc850-recent": ("Wednesday, 01-Jan-20 00:00:00 GMT", "Wed, 01 Jan 2020 00:00:59 GMT"),
    "date-legacy-asctime-recent": ("Wed Jan  1 00:00:00 2020", "Wed, 01 Jan 2020 00:00:59 GMT"),
    "date-legacy-rfc850-short-year": ("Wednesday, 01-Jan-2 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-legacy-asctime-short-year": ("Wed Jan  1 00:00:00 20", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-legacy-rfc850-time": ("Wednesday, 01-Jan-20 24:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-legacy-asctime-day": ("Mon Feb 31 00:00:00 2020", "Wed, 01 Jan 2020 00:02:00 GMT"),
    "date-trailer-only": ("Wed, 01 Jan 2020 00:00:00 GMT", "Wed, 01 Jan 2020 00:00:59 GMT"),
    "weak-trailer-date": ("Wed, 01 Jan 2020 00:00:00 GMT", "Wed, 01 Jan 2020 00:02:00 GMT"),
}
date_validators = {
    "date-leap-second": "Sat, 31 Dec 2016 23:59:60 GMT",
    "date-minimum-year": "Mon, 01 Jan 1900 00:00:00 GMT",
    **{name: fields[2] for name, fields in legacy_dates.items()},
}
trailer_full_responses = {"etag-200-trailer", "etag-200-mixed-trailer"}
trailer_responses = {
    "etag-initial-trailer", "etag-206-trailer", "etag-206-trailer-framing",
    *trailer_full_responses, "etag-200-trailer-changed", "etag-200-trailer-weak", "etag-206-trailer-changed",
    "etag-200-trailer-conflict", "etag-206-trailer-conflict", "etag-200-trailer-duplicate",
    "etag-200-trailer-malformed", "etag-200-trailer-name", "date-200-trailer-only",
}
fallbacks = {
    "absent", "absent-chunk", "absent-close", "weak", "weak-date", "invalid-tag", "tag-space",
    "tag-quote", "tag-unclosed", "recent-date", "invalid-date", "date-without-date",
    "fallback-206", "fallback-short", "fallback-chunk", "fallback-refused",
} | set(date_snapshots)
single_responses = {"complete-range", "initial-full", "initial-chunk", "initial-close"}
validated_responses = {
    "etag-206-omit", "date-206-omit", "date-boundary", "etag-200-match", "date-200-match", "both-200-match",
    *date_validators, "etag-initial-trailer", "etag-206-trailer", "etag-206-trailer-framing", *trailer_full_responses,
}
invalid_responses = {
    "etag-200-absent", "etag-200-changed", "etag-200-weak", "etag-200-invalid", "date-200-absent",
    "date-200-changed", "both-200-absent", "etag-206-changed", "date-206-changed", "late-size", "late-range",
    "late-short", "late-chunk", "late-redirect-path", "late-redirect-query", "late-redirect-host",
    "etag-200-trailer-changed", "etag-200-trailer-weak", "etag-200-trailer-conflict",
    "etag-206-trailer-conflict", "etag-206-trailer-changed", "etag-200-trailer-duplicate", "etag-200-trailer-malformed",
    "etag-200-trailer-name", "date-200-trailer-only",
    "date-legacy-206-changed",
}
requests = []
targets = []
failures = []
stalled = {"read": threading.Event(), "open": threading.Event()}
references = {
    "absolute": ("http://{authority}/resolved/a/.././payload.bin?next=/./../%2e#fragment", "/resolved/payload.bin?next=/./../%2e"),
    "network": ("//{authority}/resolved/a/.././payload.bin?next=/./../%2e#fragment", "/resolved/payload.bin?next=/./../%2e"),
    "relative": ("../resolved/a/.././payload.bin?relative=/../.#fragment", "/resolved/payload.bin?relative=/../."),
    "root": ("/../../resolved/a/../payload.bin?root=/./..#fragment", "/resolved/payload.bin?root=/./.."),
    "query": ("?next=/./../%2e#fragment", "/client-reference/query?next=/./../%2e"),
    "empty-query": ("?#fragment", "/client-reference/empty-query?"),
    "encoded": ("http://{authority}/resolved/%2e/%2e%2e/payload.bin?encoded=1#fragment", "/resolved/%2e/%2e%2e/payload.bin?encoded=1"),
    "absolute-slashes": ("http://{authority}/resolved//a/..//./payload.bin?slashes=1#fragment", "/resolved///payload.bin?slashes=1"),
    "network-slashes": ("//{authority}/resolved//a/..//./payload.bin?slashes=1#fragment", "/resolved///payload.bin?slashes=1"),
    "absolute-empty": ("http://{authority}?empty=absolute#fragment", "/?empty=absolute"),
    "network-empty": ("//{authority}?empty=network#fragment", "/?empty=network"),
}
redirected = set()
empty_ranges = {
    "lower": "bytes */0",
    "title": "Bytes */0",
    "upper": "BYTES */0",
    "mixed": "bYtEs */0",
    "padded": "bytes */000",
    "other-unit": "items */0",
    "bad-unit": "BYTES= */0",
    "missing-length": "bytes */",
    "nonzero": "bytes */1",
    "negative": "bytes */-0",
    "signed": "bytes */+0",
    "suffix": "bytes */0junk",
    "spacing": "bytes * /0",
    "unknown": "bytes */*",
    "overflow": "bytes */18446744073709551616",
    "satisfied": "bytes 0-0/0",
    "missing-header": None,
}



class Server(http.server.ThreadingHTTPServer):
    def handle_error(self, request, client_address):
        failures.append(repr(sys.exc_info()[1]))
        super().handle_error(request, client_address)


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def unknown_range(self, path):
        name = path.split("/")[2]
        history = unknown_range_requests.setdefault(name, [])
        ranged = self.headers.get("Range")
        history.append((ranged, self.headers.get("If-Range")))
        content = pathlib.Path("app0", "payload.bin").read_bytes()
        status = 200
        first, last = 0, len(content) - 1
        total = "*"
        if ranged is not None:
            match = re.fullmatch(r"bytes=(\d+)-(\d+)", ranged)
            assert match is not None
            first, end = map(int, match.groups())
            last = min(first + 511, end, len(content) - 1)
            if first:
                expected = "Wed, 01 Jan 2020 00:00:00 GMT" if name.endswith("date") else '"unknown-v1"'
                assert self.headers.get("If-Range") == expected
            status = 206
            if name == "known" or (name.startswith("late") and first == 0):
                total = str(len(content))
        else:
            assert self.headers.get("If-Range") is None
            if name == "full-partial":
                status, last, total = 206, 511, str(len(content))
            elif name == "full-refused":
                status = 404
            elif name == "late-redirect":
                status = 302
        body = content[first:last + 1]
        if name == "late-size" and ranged is None:
            body = body[:-1]
        if name == "empty-body":
            body = b""
        header_first = first + int(name == "wrong-start")
        header_last = last
        if name in {"wrong-span", "empty-body"}:
            header_last = first
        elif name == "overflow-span":
            header_last = 18446744073709551615
        total = "*invalid" if name == "bad-total" else "" if name == "missing-total" else total
        self.send_response_only(status)
        if status == 206:
            unit = "items" if name == "unknown-unit" else "ByTeS"
            self.send_header("Content-Range", f"{unit} {header_first}-{header_last}/{total}")
        if name.endswith("date"):
            self.send_header("Last-Modified", "Wed, 01 Jan 2020 00:00:00 GMT")
            self.send_header("Date", "Wed, 01 Jan 2020 00:02:00 GMT")
        elif name != "unvalidated" and not (name == "late-absent" and ranged is None):
            self.send_header("ETag", '"unknown-v2"' if name == "late-changed" and ranged is None else '"unknown-v1"')
        if status == 302:
            self.send_header("Location", "/range/payload.bin")
        if name == "chunked":
            self.send_header("Transfer-Encoding", "chunked")
        elif name != "closed":
            self.send_header("Content-Length", str(len(body) + int(name == "full-short" and ranged is None)))
        self.send_header("Connection", "close")
        self.end_headers()
        if name == "chunked":
            for at in range(0, len(body), 4096):
                chunk = body[at:at + 4096]
                self.wfile.write(f"{len(chunk):X}\r\n".encode() + chunk + b"\r\n")
            self.wfile.write(b"0\r\n\r\n")
        else:
            self.wfile.write(body)

    def do_GET(self):
        try:
            self.respond()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def respond(self):
        path = urllib.parse.urlsplit(self.path).path
        transformed = path.startswith("/transformed/")
        if transformed:
            path = path.removeprefix("/transformed")
        if path.startswith("/identity/"):
            self.identity(path, transformed)
            return
        requests.append((path, self.headers.get("Range")))
        targets.append(self.path)
        if path.startswith("/length-list/"):
            self.length_list(path)
            return
        if path.startswith("/client-unsupported/"):
            location = {"https": "https://{authority}/payload.bin", "scheme": "ftp://{authority}/payload.bin", "userinfo": "http://user@{authority}/payload.bin"}[path.removeprefix("/client-unsupported/")]
            self.send_response(307)
            self.send_header("Location", location.format(authority=self.server.redirect_authority))
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        reference = path.removeprefix("/client-reference/")
        if path.startswith("/client-reference/") and reference not in redirected:
            redirected.add(reference)
            self.send_response(307)
            self.send_header("Location", references[reference][0].format(authority=self.server.redirect_authority))
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        resolved = self.path in {target for _, target in references.values()}
        authority_targets = {target for name, (_, target) in references.items() if name.startswith(("absolute", "network")) or name == "encoded"}
        if ((path.startswith(("/client-reference/", "/resolved/")) or path == "/") and not resolved
                or self.path in authority_targets and not self.server.redirect_destination):
            print(f"Refused unexpected redirect request target: {self.path!r}", file=sys.stderr)
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if path.startswith("/sync/"):
            ready = stalled[path.removeprefix("/sync/")].wait(2)
            self.send_response(200 if ready else 500)
            self.send_header("Content-Length", "5")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(b"ready" if ready else b"error")
            return
        if path.startswith("/unknown-range/"):
            self.unknown_range(path)
            return
        if path.startswith("/redirect/") or path.startswith("/client-redirect/"):
            self.send_response(307)
            name = "payload.bin" if path.startswith("/client-redirect/") else "avplayer.mp4"
            self.send_header("Location", f"../range/{name}?redirected=1")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        content = pathlib.Path("app0", "payload.bin" if resolved else pathlib.PurePosixPath(path).name).read_bytes()
        match = re.fullmatch(r"bytes=(\d+)-(\d+)", self.headers.get("Range", ""))
        assert match is not None
        start, end = map(int, match.groups())
        if path.startswith("/stall/") and start:
            stalled["read"].set()
            time.sleep(2)
        if path.startswith("/stall-open/"):
            stalled["open"].set()
            time.sleep(2)
        full = path.startswith(("/ignore/", "/full/", "/full-chunk/", "/full-close/"))
        if not full and start >= len(content):
            self.send_response(416)
            content_range = empty_ranges[path.split("/")[2]] if path.startswith("/empty-range/") else f"bytes */{len(content)}"
            if content_range is not None:
                self.send_header("Content-Range", content_range)
            self.send_header("Content-Length", "13")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(b"unsatisfiable")
            return
        if full:
            start, end = 0, len(content) - 1
            self.send_response(200)
        else:
            self.send_response(206)
            end = min(end, start + 511, len(content) - 1)
            first = start + 1 if path.startswith("/bad/") or path.startswith("/bad-late/") and start else start
            unit = path.split("/")[2] if path.startswith("/unit/") else "BYTES" if path.startswith(("/bad/", "/range-number/")) else "bytes"
            first = "invalid" if path.startswith("/range-number/") else first
            self.send_header("Content-Range", f"{unit} {first}-{end}/{len(content)}")
        if start:
            expected = "Wed, 01 Jan 2020 00:00:00 GMT" if path.startswith("/modified/") else '"fixture-v1"'
            assert self.headers.get("If-Range") == expected
        if path.startswith("/modified/"):
            self.send_header("Last-Modified", "Wed, 01 Jan 2020 00:00:00 GMT")
        else:
            self.send_header("ETag", '"fixture-v2"' if path.startswith("/changed/") and start else '"fixture-v1"')
        body = content[start:end + 1]
        chunked = path.startswith(("/chunk/", "/full-chunk/", "/chunk-bad/"))
        if chunked:
            self.send_header("Transfer-Encoding", "chunked")
        elif not path.startswith("/full-close/"):
            self.send_header("Content-Length", str(len(body) + int(path.startswith("/short/"))))
        self.send_header("Connection", "close")
        self.end_headers()
        if path.startswith("/chunk-bad/"):
            self.wfile.write(b"ZZ\r\n")
        elif chunked:
            for at in range(0, len(body), 19):
                chunk = body[at:at + 19]
                self.wfile.write(f"{len(chunk):X} ;test=1\r\n".encode() + chunk + b"\r\n")
            self.wfile.write(b"0\r\nX-Trailer: yes\r\n\r\n")
        else:
            self.wfile.write(body)

    def identity(self, path, transformed=False):
        name = path.split("/")[2]
        if name == "late-target" or urllib.parse.urlsplit(self.path).query or self.headers.get("Host", "").startswith("localhost:"):
            identity_targets.append(self.path)
            content = bytes((offset * 13 + 71) % 251 for offset in range(100000))
            self.send_response(200)
            self.send_header("ETag", '"identity-v1"')
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(content)
            return
        assert name in fallbacks | single_responses | validated_responses | invalid_responses, name
        history = identity_requests.setdefault(name, [])
        history.append((self.headers.get("Range"), self.headers.get("If-Range")))
        initial = len(history) == 1
        full = not initial and name in fallbacks or name.startswith("initial-")
        match = re.fullmatch(r"bytes=(\d+)-(\d+)", self.headers.get("Range", ""))
        if full and name in fallbacks:
            assert self.headers.get("Range") is None, history
            assert self.headers.get("If-Range") is None, history
            start, end = 0, 776
        else:
            assert match is not None, history
            start, end = map(int, match.groups())
            expected = None if initial else date_validators.get(name, "Wed, 01 Jan 2020 00:00:00 GMT") if name.startswith("date-") else '"identity-v1"'
            assert self.headers.get("If-Range") == expected, history
        content = pathlib.Path("app0", "alternate.bin" if full or name == "complete-range" else "payload.bin").read_bytes()
        total = len(content)
        status = 200 if full else 206
        failed = not initial and name in invalid_responses and len(history) == 2
        if not initial and (name.endswith("200-match") or name in trailer_full_responses or failed and "200-" in name):
            status = 200
        if failed and name.startswith("late-redirect-"):
            location = "/identity/late-target/payload.bin"
            if name.endswith("query"):
                location = self.path + "?other=1"
            elif name.endswith("host"):
                location = f"http://localhost:{self.server.server_port}{self.path}"
            self.send_response(307)
            self.send_header("Location", location)
            self.send_header("Content-Length", "0")
            self.send_header("Connection", "close")
            self.end_headers()
            return
        if status == 200 or name == "complete-range" or name == "fallback-206" and full:
            start, end = 0, len(content) - 1
        else:
            end = min(end, start + 511, len(content) - 1)
        if failed:
            content = bytes((offset * 13 + 71) % 251 for offset in range(len(content)))
        if name == "fallback-206" and full:
            status = 206
        elif name == "fallback-refused" and full:
            status = 404
        body = content[start:end + 1]
        date = "Wed, 01 Jan 2020 00:02:00 GMT"
        modified = None
        etag = '"identity-v1"'
        if name.startswith("date-") or name in {"weak-date", "recent-date", "invalid-date"}:
            etag = None
            modified = date_validators.get(name, "Wed, 01 Jan 2020 00:00:00 GMT")
        elif name.startswith("both-"):
            modified = "Wed, 01 Jan 2020 00:00:00 GMT" if initial else None
        if name == "date-boundary":
            date = "Wed, 01 Jan 2020 00:01:00 GMT"
        elif name == "recent-date":
            date = "Wed, 01 Jan 2020 00:00:59 GMT"
        elif name == "invalid-date":
            modified = "not a date"
        elif name == "date-without-date":
            date = None
        elif name == "date-leap-second":
            date = "Sun, 01 Jan 2017 00:02:00 GMT"
        if name in date_snapshots:
            modified, date = date_snapshots[name]
        elif name in legacy_dates:
            modified, date = legacy_dates[name][:2]
        if name in fallbacks:
            etag = {
                "weak": 'W/"identity-v1"', "weak-date": 'W/"identity-v1"', "invalid-tag": "identity-v1",
                "tag-space": '"identity v1"', "tag-quote": '"identity"v1"', "tag-unclosed": '"identity-v1',
            }.get(name)
        if name in single_responses or full:
            etag = modified = None
        elif not initial and name.endswith("206-omit"):
            etag = modified = None
        elif failed:
            if name.endswith("absent"):
                etag = modified = None
                if name.startswith("both-"):
                    modified = "Wed, 01 Jan 2020 00:00:00 GMT"
            elif name.endswith("changed"):
                if name.startswith("date-"):
                    modified = "Wed, 01 Jan 2020 00:00:01 GMT"
                else:
                    etag = '"identity-v2"'
            elif name.endswith("weak"):
                etag = 'W/"identity-v1"'
            elif name.endswith("invalid"):
                etag = '"identity v1"'
        if name == "date-legacy-200-match" and not initial:
            modified = "Wed Jan  1 00:00:00 2020"
        elif name == "date-legacy-206-changed":
            modified = "Wed Jan  1 00:00:01 2020" if failed else "Wednesday, 01-Jan-20 00:00:00 GMT"
        trailers = []
        trailer_response = name == "etag-initial-trailer" or not initial and name in trailer_responses and (name in validated_responses or failed)
        if name == "etag-initial-trailer" or not initial and name in {"etag-206-trailer", "etag-200-trailer"}:
            trailers, etag = [b'ETag: "identity-v1"'], None
        elif not initial and name in {"etag-200-mixed-trailer", "etag-206-trailer-framing"}:
            trailers = [b'ETag: "identity-v1"']
            if name.endswith("framing"):
                trailers += [b"Content-Length: 0", b"Content-Range: bytes 0-0/1", b"Transfer-Encoding: invalid"]
        elif failed and name in trailer_responses:
            if name == "date-200-trailer-only":
                trailers, modified = [b"Last-Modified: Wed, 01 Jan 2020 00:00:00 GMT"], None
            elif name.endswith("changed"):
                trailers, etag = [b'ETag: "identity-v2"'], None
            elif name.endswith("weak"):
                trailers, etag = [b'ETag: W/"identity-v1"'], None
            elif name.endswith("conflict"):
                trailers, etag = [b'ETag: "identity-v2"'], '"identity-v1"'
            elif name.endswith("duplicate"):
                trailers, etag = [b'ETag: "identity-v1"', b'ETag: "identity-v2"'], None
            elif name.endswith("malformed"):
                trailers = [b"not a field"]
            elif name.endswith("name"):
                trailers = [b"ETag : \"identity-v1\""]
        if initial and name == "date-trailer-only":
            trailers = [b"Date: Wed, 01 Jan 2020 00:02:00 GMT"]
        elif initial and name == "weak-trailer-date":
            trailers = [b'ETag: W/"identity-v1"']
        chunked = trailer_response or bool(trailers) or name == "initial-chunk" or full and name in {"absent-chunk", "fallback-chunk"} or failed and name == "late-chunk"
        closed = name == "initial-close" or full and name == "absent-close"
        short = full and name == "fallback-short" or failed and name == "late-short"
        self.send_response_only(203 if transformed and status == 200 else status)
        if date is not None:
            self.send_header("Date", date)
        if etag is not None:
            self.send_header("ETag", etag)
        if modified is not None:
            self.send_header("Last-Modified", modified)
        if status == 206:
            first = start + int(failed and name == "late-range")
            self.send_header("Content-Range", f"bytes {first}-{end}/{total + int(failed and name == 'late-size')}")
        if chunked:
            self.send_header("Transfer-Encoding", "chunked")
        elif not closed:
            self.send_header("Content-Length", str(len(body) + int(short)))
        self.send_header("Connection", "close")
        self.end_headers()
        if chunked and (name == "fallback-chunk" or failed and name == "late-chunk"):
            self.wfile.write(b"ZZ\r\n")
        elif chunked:
            self.wfile.write(f"{len(body):X}\r\n".encode() + body + b"\r\n0\r\n" + b"".join(field + b"\r\n" for field in trailers) + b"\r\n")
        else:
            self.wfile.write(body)


    def length_list(self, path):
        _, _, mode, name, _ = path.split("/")
        content = pathlib.Path("app0", "payload.bin").read_bytes() if mode != "empty" else b""
        start = 0
        end = len(content)
        if mode == "range":
            match = re.fullmatch(r"bytes=(\d+)-(\d+)", self.headers.get("Range", ""))
            assert match is not None
            start, last = map(int, match.groups())
            end = min(start + 512, last + 1, len(content))
            if start:
                assert self.headers.get("If-Range") == '"length-v1"'
        body = content[start:end]
        fields = {
            "single": "{n}",
            "pair": "{n}, {n}",
            "compact": "{n},{n}",
            "whitespace": " \t{n}\t, \t{n} ",
            "many": "{n}, {n}, {n}",
            "zeros": "000{n}, {n}",
            "different": "{n}, {other}",
            "suffix": "{n}, {n}x",
            "empty": "",
            "leading-empty": ", {n}",
            "trailing-empty": "{n},",
            "middle-empty": "{n}, , {n}",
            "plus": "+{n}, {n}",
            "negative": "-{n}, {n}",
            "overflow": "18446744073709551616, {n}",
            "hex": "0x{n}, {n}",
            "fraction": "{n}.0, {n}",
            "embedded-space": "{n} 0, {n}",
            "embedded-tab": "{n}\t0, {n}",
        }
        repeated = {
            "repeat-exact": ["{n}", "{n}"],
            "repeat-ows": ["{n}", " \t{n}\t "],
            "repeat-zeros": ["{n}", "000{n}"],
            "repeat-zeros-first": ["000{n}", "{n}"],
            "repeat-list-second": ["{n}", "{n}, {n}"],
            "repeat-list-first": ["{n}, {n}", "{n}"],
            "repeat-two-lists": ["{n},{n}", "000{n}, {n}"],
            "repeat-many": ["{n}", "000{n}", "{n}, {n}"],
            "repeat-whitespace-lists": [" \t{n}\t,\t{n} ", "{n}"],
            "repeat-different": ["{n}", "{other}"],
            "repeat-mismatch-list": ["{n}", "{n}, {other}"],
            "repeat-mismatch-first": ["{n}, {other}", "{n}"],
            "repeat-mismatch-identical": ["{n}, {other}", "{n}, {other}"],
            "repeat-empty-first": ["", "{n}"],
            "repeat-empty-second": ["{n}", ""],
            "repeat-signed": ["{n}", "+{n}"],
            "repeat-overflow": ["{n}", "18446744073709551616"],
            "repeat-suffix": ["{n}", "{n}x"],
            "repeat-last": ["{n}", "000{n}", "{other}"],
            "etag-duplicate": ["{n}", "{n}"],
            "repeat-late-mismatch": ["{n}", "000{n}", "{other}" if start else "{n}"],
        }
        self.send_response(206 if mode == "range" else 200)
        if mode == "range":
            self.send_header("Content-Range", f"bytes {start}-{end - 1}/{len(content)}")
        self.send_header("ETag", '"length-v1"')
        if name == "etag-duplicate":
            self.send_header("ETag", '"length-v2"')
        for value in repeated[name] if name in repeated else [fields[name]]:
            self.send_header("Content-Length", value.format(n=len(body), other=len(body) + 1))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)


with Server(("127.0.0.1", 0), Handler) as server, Server(("127.0.0.1", 0), Handler) as destination:
    server.redirect_authority = destination.redirect_authority = f"127.0.0.1:{destination.server_port}"
    server.redirect_destination = False
    destination.redirect_destination = True
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    destination_thread = threading.Thread(target=destination.serve_forever, daemon=True)
    thread.start()
    destination_thread.start()
    mode = sys.argv[2] if len(sys.argv) > 2 else "avplayer"
    arguments = [sys.argv[1], f"http://127.0.0.1:{server.server_port}", *sys.argv[3:]] if mode != "avplayer" else [sys.argv[1], "--http", f"http://127.0.0.1:{server.server_port}"]
    result = subprocess.run(arguments)
    server.shutdown()
    destination.shutdown()
    thread.join()
    destination_thread.join()
    if result.returncode:
        sys.exit(result.returncode)
    assert not failures, failures
    assert any(header is not None and not header.startswith("bytes=0-") for _, header in requests)
    if unknown_range_requests:
        valid = {"known", "initial", "late", "chunked", "closed", "initial-date", "late-date", "unvalidated"}
        invalid = {"wrong-start", "wrong-span", "empty-body", "bad-total", "missing-total", "overflow-span", "unknown-unit"}
        bad_full = {"full-partial", "full-refused", "full-short"}
        bad_late = {"late-changed", "late-size", "late-absent", "late-redirect"}
        assert set(unknown_range_requests) == valid | invalid | bad_full | bad_late
        for name, history in unknown_range_requests.items():
            initial = [("bytes=0-65535", None)]
            validator = "Wed, 01 Jan 2020 00:00:00 GMT" if name.endswith("date") else '"unknown-v1"'
            if name == "known":
                expected = initial + [("bytes=90000-99999", validator), ("bytes=0-65535", validator)]
            elif name.startswith("late"):
                expected = initial + [("bytes=90000-99999", validator), (None, None)]
            elif name in invalid:
                expected = initial
            else:
                expected = initial + [(None, None)]
            assert history == expected, (name, history, expected)
    if mode == "avplayer":
        assert {"/range/avplayer.mp4", "/chunk/avplayer.webm", "/ignore/avplayer.mp4", "/redirect/avplayer.mp4"}.issubset({path for path, _ in requests})
    if mode == "client":
        for name in empty_ranges:
            assert (f"/empty-range/{name}/empty.bin", "bytes=0-65535") in requests
        assert set(references) == redirected
        assert {target for _, target in references.values()}.issubset(targets)
        expected = {"/range/payload.bin", "/range/alternate.bin", "/full/payload.bin", "/full-chunk/payload.bin", "/full-close/payload.bin", "/client-redirect/payload.bin", "/changed/payload.bin", "/modified/payload.bin", "/bad/payload.bin", "/short/payload.bin", "/chunk-bad/payload.bin", "/stall/payload.bin", "/stall-open/payload.bin", "/sync/read", "/sync/open"}
        assert expected.issubset({path for path, _ in requests})
    if mode in {"client", "client203"}:
        assert set(identity_requests) == fallbacks | single_responses | validated_responses | invalid_responses
        assert not identity_targets, identity_targets
        for name, history in identity_requests.items():
            assert history[0] == ("bytes=0-65535", None), (name, history)
            if name in fallbacks:
                assert history == [("bytes=0-65535", None), (None, None)], (name, history)
            elif name in single_responses:
                assert len(history) == 1, (name, history)
            else:
                validator = date_validators.get(name, "Wed, 01 Jan 2020 00:00:00 GMT") if name.startswith("date-") else '"identity-v1"'
                assert history[1] == ("bytes=90000-99999", validator), (name, history)
                if name in invalid_responses:
                    assert history[2:] == [("bytes=90000-99999", validator)], (name, history)
                elif name.endswith("200-match") or name in trailer_full_responses:
                    assert len(history) == 2, (name, history)
                else:
                    assert history[2:] == [("bytes=0-65535", validator)], (name, history)
    if mode == "client":
        assert sum(path == "/cache/payload.bin" for path, _ in requests) == 1
        assert any(target == "/range/payload.bin?redirected=1" for target in targets)
        assert any(target == "/range/payload.bin?token=direct" for target in targets)
        assert all("#" not in target for target in targets)
        for unit in ("bytes", "Bytes", "BYTES", "bYtEs"):
            path = f"/unit/{unit}/payload.bin"
            assert (path, "bytes=0-65535") in requests
            assert (path, "bytes=90000-99999") in requests
