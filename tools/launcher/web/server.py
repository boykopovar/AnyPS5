import base64
import hashlib
import hmac
import http.cookies
import json
import os
import secrets
import sqlite3
import threading
import time
import uuid
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit, unquote
from bundle import EngineCatalogue


class Store:
    def __init__(self, directory):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.lock = threading.RLock()
        with self.connect() as db:
            db.executescript('CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL); CREATE TABLE IF NOT EXISTS commands (id TEXT PRIMARY KEY, verb TEXT NOT NULL, arg TEXT NOT NULL, created REAL NOT NULL, status TEXT NOT NULL, result TEXT); CREATE TABLE IF NOT EXISTS sessions (digest TEXT PRIMARY KEY, expires REAL NOT NULL); CREATE TABLE IF NOT EXISTS pairings (digest TEXT PRIMARY KEY, expires REAL NOT NULL);')

    @contextmanager
    def connect(self):
        db = sqlite3.connect(self.directory / 'backend.sqlite3', timeout=5)
        try:
            with db:
                yield db
        finally:
            db.close()

    def state(self):
        with self.lock, self.connect() as db:
            row = db.execute("SELECT value FROM meta WHERE key='snapshot'").fetchone()
            state = json.loads(row[0]) if row else {'games': [], 'folders': [], 'notes': [], 'message': 'Waiting for the Windows desktop helper', 'last_seen': 0, 'busy': True}
            state['online'] = time.time() - state.get('last_seen', 0) < 15
            state['commands'] = [dict(zip(('id', 'verb', 'status', 'result', 'arg'), row)) for row in db.execute('SELECT id,verb,status,result,arg FROM commands ORDER BY created DESC LIMIT 20')]
            return state

    def report(self, value):
        if not isinstance(value, dict) or not isinstance(value.get('games'), list) or not isinstance(value.get('folders'), list):
            raise ValueError('Invalid helper snapshot')
        if value.get('helper_protocol', 1) != 1:
            raise ValueError('Desktop helper protocol is incompatible. Install the generated helper for this app.')
        if len(value['games']) > 4096 or len(value['folders']) > 4096:
            raise ValueError('Snapshot exceeds collection limit')
        value['last_seen'] = time.time()
        result = value.pop('result', None)
        with self.lock, self.connect() as db:
            if value.get('busy'):
                previous = db.execute("SELECT value FROM meta WHERE key='snapshot'").fetchone()
                if previous:
                    previous = json.loads(previous[0])
                    value['games'], value['folders'] = previous['games'], previous['folders']
            db.execute("INSERT OR REPLACE INTO meta VALUES ('snapshot', ?)", (json.dumps(value),))
            if result:
                db.execute("UPDATE commands SET status=?,result=? WHERE id=? AND status='delivered'", ('done' if result.get('ok') else 'failed', str(result.get('message', ''))[:8000], result.get('id')))
            db.execute("UPDATE commands SET status='unknown',result='The helper did not acknowledge this request. Inspect the Windows desktop before trying again.' WHERE status='delivered' AND created<?", (time.time() - 120,))

    def queue(self, verb, arg):
        if verb not in ('launch', 'pick_folder', 'rescan', 'remove_root', 'scan', 'cancel_scan', 'add_scan', 'acknowledge_exit', 'prepare_scan', 'cancel_preparation', 'open_preparation_log', 'set_preparation_root') or not isinstance(arg, str):
            raise ValueError('Unsupported action')
        with self.lock:
            state = self.state()
            if not state['online'] or state.get('busy'):
                raise ValueError('Windows helper offline or native launcher owns the collection. Close the native window and retry.')
            if state.get('preparation', {}).get('active') and verb in ('prepare_scan', 'scan', 'rescan', 'remove_root', 'pick_folder', 'set_preparation_root'):
                raise ValueError('Wait for preparation to finish or cancel it first')
            if verb == 'prepare_scan':
                if any(command['verb'] in ('scan', 'rescan', 'prepare_scan') and command['status'] in ('queued', 'delivered') for command in state['commands']):
                    raise ValueError('Wait for the current scan or preparation request to start')
                if not state.get('preparation', {}).get('supported'):
                    raise ValueError('Install the current engine bundle desktop helper to enable preparation')
                if state.get('scan', {}).get('active') or not any(item.get('id') == arg and not item.get('prepared') and not item.get('registered') for item in state.get('scan', {}).get('results', [])):
                    raise ValueError('Select an unprepared result from the completed scan')
            elif verb == 'launch':
                if not any(game['id'] == arg for game in state['games']):
                    raise ValueError('Game ID is not registered')
                if any(game.get('running') or game.get('uncertain') for game in state['games']):
                    raise ValueError('A launch process is already running; close it before launching another title')
            elif verb == 'acknowledge_exit':
                if not any(game['id'] == arg and game.get('uncertain') for game in state['games']):
                    raise ValueError('Game has no uncertain session')
            elif verb == 'remove_root':
                if not any(folder['id'] == arg for folder in state['folders']):
                    raise ValueError('Library folder ID is not registered')
            elif verb in ('scan', 'set_preparation_root'):
                if verb == 'set_preparation_root' and not state.get('preparation', {}).get('storage_supported'):
                    raise ValueError('Update the desktop helper to choose a preparation folder')
                arg = arg.strip()
                if len(arg) >= 2 and arg[0] == arg[-1] == '"':
                    arg = arg[1:-1]
                if not arg or len(arg) > 4096 or any(ord(ch) < 32 for ch in arg):
                    raise ValueError('Paste an absolute Windows folder path')
            elif verb == 'add_scan':
                if not arg or len(arg) > 160000 or any(ch not in '0123456789abcdefABCDEF{},-' for ch in arg):
                    raise ValueError('Select scan results to add')
            elif arg:
                raise ValueError('This action accepts no arguments')
            with self.connect() as db:
                if verb == 'launch' and db.execute("SELECT COUNT(*) FROM commands WHERE verb='launch' AND status IN ('queued','delivered')").fetchone()[0]:
                    raise ValueError('A launch request is already pending')
                if db.execute("SELECT COUNT(*) FROM commands WHERE status='queued'").fetchone()[0] >= 8:
                    raise ValueError('Too many pending commands')
                ident = str(uuid.uuid4())
                db.execute('INSERT INTO commands VALUES (?,?,?,?,?,NULL)', (ident, verb, arg, time.time(), 'queued'))
                return ident

    def poll(self):
        with self.lock, self.connect() as db:
            db.execute("UPDATE commands SET status='expired',result='Command was not delivered within 60 seconds' WHERE status='queued' AND created<?", (time.time() - 60,))
            row = db.execute("SELECT id,verb,arg FROM commands WHERE status='queued' ORDER BY created LIMIT 1").fetchone()
            if not row:
                return ''
            db.execute("UPDATE commands SET status='delivered' WHERE id=?", (row[0],))
            return '\t'.join(row)

    def pairing(self, code=None):
        with self.lock, self.connect() as db:
            db.execute('DELETE FROM pairings WHERE expires<?', (time.time(),))
            if code is None:
                code = str(secrets.randbelow(100000000)).zfill(8)
                db.execute('DELETE FROM pairings')
                db.execute('INSERT INTO pairings VALUES (?,?)', (hashlib.sha256(code.encode()).hexdigest(), time.time() + 120))
                return code
            return db.execute('DELETE FROM pairings WHERE digest=?', (hashlib.sha256(code.encode()).hexdigest(),)).rowcount == 1

    def session(self, token=None):
        with self.lock, self.connect() as db:
            db.execute('DELETE FROM sessions WHERE expires<?', (time.time(),))
            if token is None:
                token = secrets.token_urlsafe(32)
                db.execute('INSERT INTO sessions VALUES (?,?)', (hashlib.sha256(token.encode()).hexdigest(), time.time() + 8 * 3600))
                return token
            return db.execute('SELECT 1 FROM sessions WHERE digest=?', (hashlib.sha256(token.encode()).hexdigest(),)).fetchone() is not None


class LauncherWebHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        return

    def respond(self, code, value, content_type='application/json', cookie=None):
        data = json.dumps(value).encode() if content_type == 'application/json' else value.encode() if isinstance(value, str) else value
        self.send_response(code)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Referrer-Policy', 'no-referrer')
        self.send_header('Content-Security-Policy', "default-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'")
        if cookie:
            self.send_header('Set-Cookie', cookie)
        self.end_headers()
        self.wfile.write(data)

    def host_allowed(self):
        return self.headers.get('Host') in self.server.hosts

    def origin_allowed(self):
        return self.headers.get('Origin') in self.server.origins

    def agent(self):
        supplied = self.headers.get('Authorization', '')
        return not self.headers.get('Origin') and hmac.compare_digest(supplied, 'Bearer ' + self.server.token)

    def browser(self):
        try:
            cookie = http.cookies.SimpleCookie(self.headers.get('Cookie', ''))
            return 'anyps5_session' in cookie and self.server.store.session(cookie['anyps5_session'].value)
        except http.cookies.CookieError:
            return False

    def body(self, limit=262144):
        if self.headers.get('Content-Type', '').split(';')[0] != 'application/json':
            raise ValueError('Expected application/json')
        size = int(self.headers.get('Content-Length', '0'))
        if size < 1 or size > limit:
            raise ValueError('Invalid request size')
        self.connection.settimeout(5)
        value = json.loads(self.rfile.read(size))
        if not isinstance(value, dict):
            raise ValueError('Expected a JSON object')
        return value

    def do_GET(self):
        if not self.host_allowed():
            return self.respond(403, {'error': 'Host not allowed'})
        if self.headers.get('Origin') and not self.origin_allowed():
            return self.respond(403, {'error': 'Origin not allowed'})
        path = self.path
        if path == '/healthz':
            engine = self.server.engine.status(30) if self.server.engine else {'status': 'not_configured'}
            ready = engine['status'] in ('ready', 'not_configured')
            return self.respond(200 if ready else 503, {'status': 'ok' if ready else 'engine_unavailable', 'engine': engine})
        if path == '/engine/status':
            return self.respond(200, self.server.engine.status(30) if self.server.engine else {'status': 'not_configured'})
        if path == '/engine/download' and self.server.engine:
            try:
                archive = self.server.engine.archive()
            except ValueError as error:
                return self.respond(503, {'error': str(error)})
            self.send_response(200)
            self.send_header('Content-Type', 'application/zip')
            self.send_header('Content-Disposition', 'attachment; filename="AnyPS5-Windows-Engine.zip"')
            self.send_header('Content-Length', str(archive.stat().st_size))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            with archive.open('rb') as stream:
                while data := stream.read(1024 * 1024):
                    self.wfile.write(data)
            return
        if path == '/agent/poll':
            return self.respond(200, self.server.store.poll(), 'text/plain; charset=utf-8') if self.agent() else self.respond(401, {'error': 'Helper authentication required'})
        if path == '/api/library':
            if not self.browser():
                return self.respond(401, {'error': 'Pair this browser to continue'})
            state = self.server.store.state()
            for game in state['games']:
                if game.pop('artwork', ''):
                    game['artwork_url'] = '/api/art/' + game['id']
            return self.respond(200, state)
        if path.startswith('/api/art/') and self.browser():
            for game in self.server.store.state()['games']:
                if game['id'] == unquote(path[len('/api/art/'):]):
                    data = base64.b64decode(game.get('artwork', ''))
                    if data.startswith(b'\x89PNG\r\n\x1a\n'):
                        return self.respond(200, data, 'image/png')
            return self.respond(404, {'error': 'Artwork unavailable'})
        files = {'/': ('index.html', 'text/html; charset=utf-8'), '/app.js': ('app.js', 'text/javascript; charset=utf-8'), '/app.css': ('app.css', 'text/css; charset=utf-8')}
        if path in files:
            name, kind = files[path]
            return self.respond(200, (Path(__file__).parent / 'static' / name).read_bytes(), kind)
        return self.respond(404, {'error': 'Not found'})

    def do_POST(self):
        if not self.host_allowed():
            return self.respond(403, {'error': 'Host not allowed'})
        try:
            if self.path == '/agent/pair':
                if not self.agent():
                    return self.respond(401, {'error': 'Helper authentication required'})
                self.body()
                return self.respond(200, self.server.store.pairing(), 'text/plain; charset=utf-8')
            if self.path == '/agent/report':
                if not self.agent():
                    return self.respond(401, {'error': 'Helper authentication required'})
                self.server.store.report(self.body(8 * 1024 * 1024))
                return self.respond(200, {'ok': True})
            if not self.origin_allowed():
                return self.respond(403, {'error': 'Origin not allowed'})
            value = self.body()
            if self.path == '/api/login':
                with self.server.login_lock:
                    now = time.time()
                    self.server.attempts = [stamp for stamp in self.server.attempts if now - stamp < 60]
                    if len(self.server.attempts) >= 10:
                        return self.respond(429, {'error': 'Too many attempts; wait one minute'})
                    if not isinstance(value.get('token'), str) or not (self.server.store.pairing(value['token']) or hmac.compare_digest(value['token'], self.server.token)):
                        self.server.attempts.append(now)
                        return self.respond(401, {'error': 'Invalid or expired pairing code'})
                token = self.server.store.session()
                return self.respond(200, {'ok': True}, cookie='anyps5_session=' + token + '; HttpOnly; SameSite=Strict; Path=/api; Max-Age=28800')
            if not self.browser():
                return self.respond(401, {'error': 'Sign in required'})
            if self.path == '/api/logout':
                cookie = http.cookies.SimpleCookie(self.headers.get('Cookie', ''))
                with self.server.store.connect() as db:
                    db.execute('DELETE FROM sessions WHERE digest=?', (hashlib.sha256(cookie['anyps5_session'].value.encode()).hexdigest(),))
                return self.respond(200, {'ok': True}, cookie='anyps5_session=; HttpOnly; SameSite=Strict; Path=/api; Max-Age=0')
            if self.path == '/api/action':
                ident = self.server.store.queue(value.get('action'), value.get('id', ''))
                return self.respond(202, {'id': ident})
            return self.respond(404, {'error': 'Not found'})
        except (ValueError, TypeError, KeyError, OSError) as error:
            return self.respond(400, {'error': str(error)})


def create_server(host, port, directory, token, origins):
    if len(token) < 32 or not token.isascii():
        raise ValueError('Pairing token must have at least 32 ASCII characters')
    server = ThreadingHTTPServer((host, port), LauncherWebHandler)
    server.store = Store(directory)
    server.token = token
    server.origins = set(origins)
    server.hosts = {urlsplit(origin).netloc for origin in origins} | {'127.0.0.1:8765'}
    server.attempts = []
    server.login_lock = threading.Lock()
    server.engine = EngineCatalogue(os.environ['ANYPS5_ENGINE_ROOT']) if os.environ.get('ANYPS5_ENGINE_ROOT') else None
    return server


if __name__ == '__main__':
    token = Path(os.environ.get('ANYPS5_WEB_TOKEN_FILE', '/run/secrets/auth.token')).read_text().strip()
    origins = os.environ.get('ANYPS5_WEB_ORIGINS', 'http://localhost:8765,http://127.0.0.1:8765').split(',')
    create_server('0.0.0.0', 8765, os.environ.get('ANYPS5_WEB_STATE', '/state'), token, origins).serve_forever()
