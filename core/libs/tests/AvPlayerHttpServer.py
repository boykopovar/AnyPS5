import http.server
import pathlib
import re
import subprocess
import sys
import threading
import time
import urllib.parse


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
        assert sum(path == "/cache/payload.bin" for path, _ in requests) == 1
        assert any(target == "/range/payload.bin?redirected=1" for target in targets)
        assert any(target == "/range/payload.bin?token=direct" for target in targets)
        assert all("#" not in target for target in targets)
