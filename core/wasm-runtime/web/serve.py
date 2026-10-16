import argparse
import functools
import http.server
import os
import posixpath
import urllib.parse


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *arguments, roots, **keywords):
        self.roots = roots
        super().__init__(*arguments, **keywords)

    def translate_path(self, path):
        path = posixpath.normpath(urllib.parse.unquote(urllib.parse.urlsplit(path).path))
        parts = [part for part in path.split("/") if part and part not in (".", "..")]
        if parts and parts[0] in self.roots:
            return os.path.join(self.roots[parts[0]], *parts[1:])
        return os.path.join(self.roots[""], *parts)

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, *arguments):
        pass


def main():
    parser = argparse.ArgumentParser(description="Serves the AnyPS5 WebAssembly runtime with the headers SharedArrayBuffer needs")
    parser.add_argument("--runtime", required=True, help="directory holding anyps5-runtime-web.js and .wasm")
    parser.add_argument("--game", required=True, help="directory holding game.wasm, game.data and game.data.js")
    parser.add_argument("--port", type=int, default=8080)
    arguments = parser.parse_args()
    roots = {"": os.path.dirname(os.path.abspath(__file__)), "runtime": os.path.abspath(arguments.runtime), "game": os.path.abspath(arguments.game)}
    Handler.extensions_map[".wasm"] = "application/wasm"
    Handler.extensions_map[".mjs"] = "text/javascript"
    server = http.server.ThreadingHTTPServer(("127.0.0.1", arguments.port), functools.partial(Handler, roots=roots))
    print(f"http://127.0.0.1:{arguments.port}/")
    server.serve_forever()


if __name__ == "__main__":
    main()
