import http.client
import importlib.util
import json
import secrets
import tempfile
import threading
import unittest
import time
import hashlib
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'engine'))

spec = importlib.util.spec_from_file_location('launcher_web_state', Path(__file__).parent / 'web/state.py')
web_state = importlib.util.module_from_spec(spec)
spec.loader.exec_module(web_state)
spec = importlib.util.spec_from_file_location('launcher_web', Path(__file__).parent / 'web/server.py')
web = importlib.util.module_from_spec(spec)
spec.loader.exec_module(web)


class WebTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='anyps5-web-test-')
        self.token = secrets.token_hex(32)
        self.server = web.create_server('127.0.0.1', 0, self.temp.name, self.token, [])
        self.port = self.server.server_port
        self.origin = 'http://127.0.0.1:' + str(self.port)
        self.server.origins = {self.origin}
        self.server.hosts = {'127.0.0.1:' + str(self.port)}
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.temp.cleanup()

    def request(self, method, path, value=None, headers=None):
        connection = http.client.HTTPConnection('127.0.0.1', self.port, timeout=5)
        headers = dict(headers or {})
        body = json.dumps(value) if value is not None else None
        if body is not None:
            headers['Content-Type'] = 'application/json'
        connection.request(method, path, body, headers)
        response = connection.getresponse()
        data = response.read()
        result = response.status, data, dict(response.getheaders())
        connection.close()
        return result

    def login(self):
        status, data, headers = self.request('POST', '/api/login', {'token': self.token}, {'Origin': self.origin})
        self.assertEqual(status, 200)
        self.assertIn('HttpOnly', headers['Set-Cookie'])
        self.assertIn('SameSite=Strict', headers['Set-Cookie'])
        return {'Origin': self.origin, 'Cookie': headers['Set-Cookie'].split(';')[0]}

    def snapshot(self, busy=False):
        snapshot = {'games': [{'id': '{01234567-89AB-4CDE-8012-3456789ABCDE}', 'title': 'Fixture', 'title_id': 'TEST', 'status': 'Unverified', 'running': False, 'working_directory': 'Fixture'}], 'folders': [{'id': 'root-fixture', 'path': 'Missing fixture root', 'exists': False}], 'notes': [], 'busy': busy, 'message': 'Fixture'}
        status, _, _ = self.request('POST', '/agent/report', snapshot, {'Authorization': 'Bearer ' + self.token})
        self.assertEqual(status, 200)

    def test_preparation_authorization_and_exclusion(self):
        headers = self.login()
        value = {'games': [], 'folders': [], 'preparation': {'supported': True, 'active': False},
                 'scan': {'active': False, 'results': [{'id': 'raw', 'prepared': False, 'registered': False},
                                                    {'id': 'ready', 'prepared': True, 'registered': False}]}}
        self.server.store.report(value)
        for identifier in ('C:/arbitrary/path', 'ready'):
            self.assertEqual(self.request('POST', '/api/action', {'action': 'prepare_scan', 'id': identifier}, headers)[0], 400)
        self.assertEqual(self.request('POST', '/api/action', {'action': 'prepare_scan', 'id': 'raw'}, headers)[0], 202)
        value['preparation']['active'] = True
        self.server.store.report(value)
        for action in ('prepare_scan', 'scan', 'rescan', 'remove_root', 'pick_folder'):
            self.assertEqual(self.request('POST', '/api/action', {'action': action, 'id': 'raw' if action == 'prepare_scan' else ''}, headers)[0], 400)
        self.assertEqual(self.request('POST', '/api/action', {'action': 'cancel_preparation'}, headers)[0], 202)
        self.assertEqual(self.request('POST', '/api/action', {'action': 'open_preparation_log', 'id': 'C:/arbitrary/path'}, headers)[0], 400)
        value['preparation'] = {'supported': False, 'active': False}
        self.server.store.report(value)
        self.assertEqual(self.request('POST', '/api/action', {'action': 'prepare_scan', 'id': 'raw'}, headers)[0], 400)

    def test_preparation_storage_capability_and_busy_guard(self):
        headers = self.login()
        value = {'games': [], 'folders': [], 'preparation': {'storage_supported': True, 'active': False}}
        self.server.store.report(value)
        for path in ('', 'D:/Games\nother'):
            self.assertEqual(self.request('POST', '/api/action', {'action': 'set_preparation_root', 'id': path}, headers)[0], 400)
        self.assertEqual(self.request('POST', '/api/action', {'action': 'set_preparation_root', 'id': 'D:/Prepared Games'}, headers)[0], 202)
        value['preparation']['active'] = True
        self.server.store.report(value)
        self.assertEqual(self.request('POST', '/api/action', {'action': 'set_preparation_root', 'id': 'E:/Prepared Games'}, headers)[0], 400)
        value['preparation'] = {'active': False}
        self.server.store.report(value)
        self.assertEqual(self.request('POST', '/api/action', {'action': 'set_preparation_root', 'id': 'E:/Prepared Games'}, headers)[0], 400)

    def test_authentication_host_origin_and_csrf(self):
        self.assertEqual(self.request('GET', '/healthz')[0], 200)
        self.assertEqual(self.request('GET', '/api/library')[0], 401)
        self.assertEqual(self.request('GET', '/agent/poll')[0], 401)
        self.assertEqual(self.request('POST', '/api/action', {'action': 'scan', 'id': 'C:/Games'}, {'Origin': self.origin})[0], 401)
        self.assertEqual(self.request('POST', '/api/login', {'token': self.token})[0], 403)
        self.assertEqual(self.request('POST', '/api/login', {'token': self.token}, {'Origin': 'http://evil.invalid'})[0], 403)
        self.assertEqual(self.request('GET', '/', headers={'Host': 'evil.invalid'})[0], 403)
        headers = self.login()
        self.assertEqual(self.request('GET', '/api/library', headers=headers)[0], 200)
        headers['Origin'] = 'null'
        self.assertEqual(self.request('POST', '/api/action', {'action': 'rescan'}, headers)[0], 403)

    def test_only_registered_ids_and_fixed_actions(self):
        self.snapshot()
        headers = self.login()
        for action in [{'action': 'shell', 'id': 'cmd.exe'}, {'action': 'launch', 'id': 'not registered'}, {'action': 'pick_folder', 'id': 'C:/arbitrary/path'}, {'action': 'rescan', 'id': 'C:/arbitrary/path'}, {'action': 'remove_root', 'id': 'not registered'}]:
            self.assertEqual(self.request('POST', '/api/action', action, headers)[0], 400)
        status, data, _ = self.request('POST', '/api/action', {'action': 'launch', 'id': '{01234567-89AB-4CDE-8012-3456789ABCDE}'}, headers)
        self.assertEqual(status, 202)
        ident = json.loads(data)['id']
        status, command, _ = self.request('GET', '/agent/poll', headers={'Authorization': 'Bearer ' + self.token})
        self.assertEqual(status, 200)
        self.assertEqual(command.decode(), ident + '\tlaunch\t{01234567-89AB-4CDE-8012-3456789ABCDE}')
        self.assertEqual(web.Store(self.temp.name).poll(), '')

    def test_exclusive_native_ownership_and_offline_helper(self):
        headers = self.login()
        self.assertEqual(self.request('POST', '/api/action', {'action': 'rescan'}, headers)[0], 400)
        self.snapshot(busy=True)
        self.assertEqual(self.request('POST', '/api/action', {'action': 'rescan'}, headers)[0], 400)

    def test_persistence_acknowledgement_and_no_replay(self):
        self.snapshot()
        headers = self.login()
        ident = json.loads(self.request('POST', '/api/action', {'action': 'rescan'}, headers)[1])['id']
        self.assertIn(ident, web.Store(self.temp.name).poll())
        self.assertEqual(web.Store(self.temp.name).poll(), '')
        self.server.store.report({'games': [], 'folders': [], 'busy': False, 'result': {'id': ident, 'ok': True, 'message': 'scanned'}})
        state = web.Store(self.temp.name).state()
        self.assertEqual(state['commands'][0]['status'], 'done')
        self.assertEqual(state['commands'][0]['result'], 'scanned')
        self.assertNotIn('token', json.dumps(state))

    def test_single_use_expiring_pairing_and_throttle(self):
        self.assertEqual(self.request('POST', '/agent/pair', {})[0], 401)
        status, code, _ = self.request('POST', '/agent/pair', {}, {'Authorization': 'Bearer ' + self.token})
        self.assertEqual(status, 200)
        self.assertEqual(len(code), 8)
        self.assertEqual(self.request('POST', '/api/login', {'token': code.decode()}, {'Origin': self.origin})[0], 200)
        self.assertEqual(self.request('POST', '/api/login', {'token': code.decode()}, {'Origin': self.origin})[0], 401)
        code = self.server.store.pairing()
        with self.server.store.connect() as db:
            db.execute('UPDATE pairings SET expires=?', (time.time() - 1,))
        self.assertEqual(self.request('POST', '/api/login', {'token': code}, {'Origin': self.origin})[0], 401)
        for _ in range(8):
            self.assertEqual(self.request('POST', '/api/login', {'token': 'wrong'}, {'Origin': self.origin})[0], 401)
        self.assertEqual(self.request('POST', '/api/login', {'token': self.token}, {'Origin': self.origin})[0], 429)

    def test_paste_path_transport_and_duplicate_launch(self):
        self.snapshot()
        headers = self.login()
        path = 'C:\\Games\\Snow \u2603 & punctuation!'
        status, _, _ = self.request('POST', '/api/action', {'action': 'scan', 'id': ' "' + path + '" '}, headers)
        self.assertEqual(status, 202)
        self.assertTrue(self.server.store.poll().endswith('\t' + path))
        self.assertEqual(self.request('POST', '/api/action', {'action': 'scan', 'id': path + '\tlaunch'}, headers)[0], 400)
        ident = '{01234567-89AB-4CDE-8012-3456789ABCDE}'
        self.assertEqual(self.request('POST', '/api/action', {'action': 'launch', 'id': ident}, headers)[0], 202)
        self.assertEqual(self.request('POST', '/api/action', {'action': 'launch', 'id': ident}, headers)[0], 400)
        self.server.store.poll()
        self.assertEqual(self.request('POST', '/api/action', {'action': 'launch', 'id': ident}, headers)[0], 400)
        self.assertEqual(self.request('POST', '/api/logout', {}, headers)[0], 200)
        self.assertEqual(self.request('GET', '/api/library', headers=headers)[0], 401)

    def test_no_arbitrary_static_files_and_content_policy(self):
        status, data, headers = self.request('GET', '/')
        self.assertEqual(status, 200)
        self.assertIn(b'Windows desktop', data)
        self.assertIn("frame-ancestors 'none'", headers['Content-Security-Policy'])
        self.assertEqual(self.request('GET', '/../../auth.token')[0], 404)

    def test_missing_engine_is_not_ready_even_when_helper_reports(self):
        self.assertEqual(self.request('GET', '/ready')[0], 200)
        self.server.engine = web.EngineCatalogue(Path(self.temp.name) / 'missing-engine')
        self.assertEqual(self.request('GET', '/ready')[0], 503)
        self.assertEqual(self.request('GET', '/healthz')[0], 200)
        self.snapshot()
        self.assertEqual(self.request('GET', '/ready')[0], 503)
        self.assertEqual(self.request('GET', '/engine/download')[0], 503)
        self.assertEqual(self.request('GET', '/engine/status')[0], 200)
        with self.assertRaisesRegex(ValueError, 'protocol'):
            self.server.store.report({'games': [], 'folders': [], 'helper_protocol': 2})


    def test_setup_status_reports_progress_without_library_details(self):
        status, data, _ = self.request('GET', '/setup/status')
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(data), {'engine': 'not_configured', 'helper_online': False})
        self.snapshot()
        self.server.engine = web.EngineCatalogue(Path(self.temp.name) / 'missing-engine')
        status, data, _ = self.request('GET', '/setup/status')
        self.assertEqual(json.loads(data), {'engine': 'missing', 'helper_online': True})
        self.assertNotIn(b'Fixture', data)
        self.assertEqual(self.request('GET', '/setup/status', headers={'Host': 'evil.invalid'})[0], 403)

    def test_state_setup_creates_and_keeps_a_private_credential(self):
        if not hasattr(os, 'getuid'):
            self.skipTest('Ownership is applied inside the Linux container')
        root = Path(self.temp.name) / 'state'
        owner = (os.getuid(), os.getgid())
        web_state.prepare(root, owner)
        credential = (root / 'auth.token').read_text()
        self.assertGreaterEqual(len(credential), 64)
        self.assertTrue((root / 'backend').is_dir())
        self.assertEqual((root / 'auth.token').stat().st_mode & 0o777, 0o600)
        web_state.prepare(root, owner)
        self.assertEqual((root / 'auth.token').read_text(), credential)
        (root / 'auth.token').write_text('short')
        with self.assertRaisesRegex(ValueError, 'invalid'):
            web_state.prepare(root, owner)
        self.assertEqual((root / 'auth.token').read_text(), 'short')


if __name__ == '__main__':
    unittest.main()
