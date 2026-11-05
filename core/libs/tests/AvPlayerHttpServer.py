import http.server
import pathlib
import re
import subprocess
import sys
import threading
import time
import urllib.parse


identity_requests = {}
identity_targets = []
fallbacks = {
    "absent", "absent-chunk", "absent-close", "weak", "weak-date", "invalid-tag", "tag-space",
    "tag-quote", "tag-unclosed", "recent-date", "invalid-date", "date-without-date",
    "fallback-206", "fallback-short", "fallback-chunk", "fallback-refused",
}
single_responses = {"complete-range", "initial-full", "initial-chunk", "initial-close"}
validated_responses = {"etag-206-omit", "date-206-omit", "date-boundary", "etag-200-match", "date-200-match", "both-200-match"}
invalid_responses = {
    "etag-200-absent", "etag-200-changed", "etag-200-weak", "etag-200-invalid", "date-200-absent",
    "date-200-changed", "both-200-absent", "etag-206-changed", "date-206-changed", "late-size", "late-range",
    "late-short", "late-chunk", "late-redirect-path", "late-redirect-query", "late-redirect-host",
}
requests = []
targets = []
failures = []
stalled = {"read": threading.Event(), "open": threading.Event()}



class Server(http.server.ThreadingHTTPServer):
    def handle_error(self, request, client_address):
        failures.append(repr(sys.exc_info()[1]))
        super().handle_error(request, client_address)


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        try:
            self.respond()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def respond(self):
        path = urllib.parse.urlsplit(self.path).path
        if path.startswith("/identity/"):
            self.identity(path)
            return
        requests.append((path, self.headers.get("Range")))
        targets.append(self.path)
        if path.startswith("/sync/"):
            ready = stalled[path.removeprefix("/sync/")].wait(2)
            self.send_response(200 if ready else 500)
            self.send_header("Content-Length", "5")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(b"ready" if ready else b"error")
            return
        if path.startswith("/redirect/") or path.startswith("/client-redirect/"):
            self.send_response(307)
            name = "payload.bin" if path.startswith("/client-redirect/") else "avplayer.mp4"
            self.send_header("Location", f"../range/{name}?redirected=1")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        content = pathlib.Path("app0", pathlib.PurePosixPath(path).name).read_bytes()
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
        if full:
            start, end = 0, len(content) - 1
            self.send_response(200)
        else:
            self.send_response(206)
            end = min(end, start + 511, len(content) - 1)
            first = start + 1 if path.startswith("/bad/") or path.startswith("/bad-late/") and start else start
            self.send_header("Content-Range", f"bytes {first}-{end}/{len(content)}")
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

    def identity(self, path):
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
            expected = None if initial else "Wed, 01 Jan 2020 00:00:00 GMT" if name.startswith("date-") else '"identity-v1"'
            assert self.headers.get("If-Range") == expected, history
        content = pathlib.Path("app0", "alternate.bin" if full or name == "complete-range" else "payload.bin").read_bytes()
        total = len(content)
        status = 200 if full else 206
        failed = not initial and name in invalid_responses and len(history) == 2
        if not initial and (name.endswith("200-match") or failed and "200-" in name):
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
            modified = "Wed, 01 Jan 2020 00:00:00 GMT"
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
        chunked = name == "initial-chunk" or full and name in {"absent-chunk", "fallback-chunk"} or failed and name == "late-chunk"
        closed = name == "initial-close" or full and name == "absent-close"
        short = full and name == "fallback-short" or failed and name == "late-short"
        self.send_response_only(status)
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
            self.wfile.write(f"{len(body):X}\r\n".encode() + body + b"\r\n0\r\n\r\n")
        else:
            self.wfile.write(body)


with Server(("127.0.0.1", 0), Handler) as server:
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    mode = sys.argv[2] if len(sys.argv) > 2 else "avplayer"
    arguments = [sys.argv[1], f"http://127.0.0.1:{server.server_port}", *sys.argv[3:]] if mode != "avplayer" else [sys.argv[1], "--http", f"http://127.0.0.1:{server.server_port}"]
    result = subprocess.run(arguments)
    server.shutdown()
    thread.join()
    if result.returncode:
        sys.exit(result.returncode)
    assert not failures, failures
    assert any(header is not None and not header.startswith("bytes=0-") for _, header in requests)
    if mode == "avplayer":
        assert {"/range/avplayer.mp4", "/chunk/avplayer.webm", "/ignore/avplayer.mp4", "/redirect/avplayer.mp4"}.issubset({path for path, _ in requests})
    if mode == "client":
        expected = {"/range/payload.bin", "/range/alternate.bin", "/full/payload.bin", "/full-chunk/payload.bin", "/full-close/payload.bin", "/client-redirect/payload.bin", "/changed/payload.bin", "/modified/payload.bin", "/bad/payload.bin", "/short/payload.bin", "/chunk-bad/payload.bin", "/stall/payload.bin", "/stall-open/payload.bin", "/sync/read", "/sync/open"}
        assert expected.issubset({path for path, _ in requests})
        assert set(identity_requests) == fallbacks | single_responses | validated_responses | invalid_responses
        assert not identity_targets, identity_targets
        for name, history in identity_requests.items():
            assert history[0] == ("bytes=0-65535", None), (name, history)
            if name in fallbacks:
                assert history == [("bytes=0-65535", None), (None, None)], (name, history)
            elif name in single_responses:
                assert len(history) == 1, (name, history)
            else:
                validator = "Wed, 01 Jan 2020 00:00:00 GMT" if name.startswith("date-") else '"identity-v1"'
                assert history[1] == ("bytes=90000-99999", validator), (name, history)
                if name in invalid_responses:
                    assert history[2:] == [("bytes=90000-99999", validator)], (name, history)
                elif name.endswith("200-match"):
                    assert len(history) == 2, (name, history)
                else:
                    assert history[2:] == [("bytes=0-65535", validator)], (name, history)
        assert sum(path == "/cache/payload.bin" for path, _ in requests) == 1
        assert any(target == "/range/payload.bin?redirected=1" for target in targets)
        assert any(target == "/range/payload.bin?token=direct" for target in targets)
        assert all("#" not in target for target in targets)
