import hashlib
import importlib.util
import json
import struct
import sys
import os
from concurrent.futures import ThreadPoolExecutor
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


sys.path.insert(0, str(Path(__file__).parent))
specification = importlib.util.spec_from_file_location('engine_sources', Path(__file__).with_name('sources.py'))
sources = importlib.util.module_from_spec(specification)
specification.loader.exec_module(sources)
sys.path.insert(0, str(Path(__file__).parent))
from bundle import EngineCatalogue, identity, sha256, verify_bundle


class SourceTests(unittest.TestCase):
    def test_archive_and_git_provenance_do_not_claim_clean_sources(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = sources.source_metadata(root, 'a' * 64)
            self.assertTrue(archive['archive'])
            self.assertIsNone(archive['revision'])
            self.assertIsNone(archive['dirty'])
            (root / '.git').mkdir()
            (root / '.git/HEAD').write_text('b' * 40 + '\n')
            clone = sources.source_metadata(root, 'c' * 64)
            self.assertFalse(clone['archive'])
            self.assertEqual(clone['revision'], 'b' * 40)
            self.assertIsNone(clone['dirty'])

    def test_staged_patches_fail_on_conflicts_without_changing_supplied_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            supplied = root / 'supplied'
            patches = supplied / 'tools/engine/patches'
            patches.mkdir(parents=True)
            (supplied / 'value.txt').write_text('original\n')
            (patches / 'fixture.patch').write_text('--- a/value.txt\n+++ b/value.txt\n@@ -1 +1 @@\n-original\n+patched\n')
            with patch.object(sources, 'PATCHES', ((patches / 'fixture.patch').read_text(),)):
                sources.hydrate(supplied, root / 'staged', root / 'cache', {'submodules': []})
            self.assertEqual((root / 'staged/value.txt').read_text(), 'patched\n')
            self.assertEqual((supplied / 'value.txt').read_text(), 'original\n')
            (supplied / 'value.txt').write_text('user modification\n')
            with patch.object(sources, 'PATCHES', ((patches / 'fixture.patch').read_text(),)), self.assertRaisesRegex(RuntimeError, 'conflict'):
                sources.hydrate(supplied, root / 'staged', root / 'cache', {'submodules': []})
            self.assertEqual((supplied / 'value.txt').read_text(), 'user modification\n')

    def test_supplied_module_changes_and_deletions_survive(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            supplied, staged, cache = root / 'supplied', root / 'staged', root / 'cache'
            module = supplied / '3rdparty/fixture'
            module.mkdir(parents=True)
            (module / 'source.cpp').write_text('modified supplied source')
            (staged / '3rdparty/fixture').mkdir(parents=True)
            (staged / '3rdparty/fixture/deleted.cpp').write_text('old build source')
            lock = {'submodules': [{'path': '3rdparty/fixture', 'commit': 'a' * 40, 'url': 'https://example.invalid/source.git'}]}
            with patch.object(sources, 'PATCHES', ()), patch.object(sources, 'checkout', side_effect=AssertionError('Supplied source was replaced')):
                first = sources.hydrate(supplied, staged, cache, lock)
                self.assertEqual((staged / '3rdparty/fixture/source.cpp').read_text(), 'modified supplied source')
                self.assertFalse((staged / '3rdparty/fixture/deleted.cpp').exists())
                (module / 'source.cpp').write_text('another controlled fork change')
                self.assertNotEqual(first, sources.hydrate(supplied, staged, cache, lock))
            self.assertFalse((supplied / '3rdparty/fixture/.anyps5-locked-commit').exists())

    def test_failed_download_keeps_previous_completed_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            incoming, previous = root / 'incoming', root / 'download'
            incoming.write_bytes(b'wrong download')
            previous.write_bytes(b'previous completed download')
            with patch.object(sources.time, 'sleep'):
                with self.assertRaises(RuntimeError):
                    sources.download({'url': incoming.as_uri(), 'sha256': '0' * 64}, previous)
            self.assertEqual(previous.read_bytes(), b'previous completed download')
            self.assertFalse(previous.with_suffix('.partial').exists())
            correct = hashlib.sha256(incoming.read_bytes()).hexdigest()
            sources.download({'url': incoming.as_uri(), 'sha256': correct}, previous)
            self.assertEqual(previous.read_bytes(), incoming.read_bytes())

    def test_lock_has_unique_recursive_revisions_and_verified_downloads(self):
        lock = json.loads(Path(__file__).with_name('dependencies.lock').read_text())
        paths = [record['path'] for record in lock['submodules']]
        self.assertEqual(len(paths), len(set(paths)))
        for record in lock['submodules']:
            self.assertTrue(record['url'].startswith('https://'))
            self.assertRegex(record['commit'], r'^[0-9a-f]{40}$')
            self.assertNotIn('..', Path(record['path']).parts)
        for record in lock['downloads']:
            self.assertTrue(record['url'].startswith('https://'))
            self.assertRegex(record['sha256'], r'^[0-9a-f]{64}$')
        dockerfile = Path(__file__).with_name('Dockerfile').read_text()
        for value in lock['environment'].values():
            self.assertIn(value, dockerfile)


class BundleTests(unittest.TestCase):
    @unittest.skipIf(os.name == 'nt', 'Exporter runs inside Linux')
    def test_export_failure_preserves_completed_bundle_and_reports_unavailable(self):
        module = importlib.util.spec_from_file_location('engine_export', Path(__file__).with_name('export.py'))
        exporter = importlib.util.module_from_spec(module)
        module.loader.exec_module(exporter)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload, artifacts, state = root / 'payload', root / 'artifacts', root / 'state'
            manifest = self.make_bundle(payload)
            exporter.export(payload, artifacts, state)
            current = (artifacts / 'current.json').read_bytes()
            credential = (state / 'auth.token').read_bytes()
            (artifacts / '.pending-interrupted-process').mkdir()
            (artifacts / 'interrupted.zip.pending').write_bytes(b'partial download')
            with ThreadPoolExecutor(max_workers=2) as pool:
                list(pool.map(lambda _: exporter.export(payload, artifacts, state), range(2)))
            self.assertFalse((artifacts / 'interrupted.zip.pending').exists())
            self.assertFalse((artifacts / '.pending-interrupted-process').exists())
            self.assertEqual((state / 'auth.token').read_bytes(), credential)
            self.assertEqual(EngineCatalogue(artifacts).status()['status'], 'ready')
            helper = payload / 'helper/AnyPS5DesktopHelper.exe'
            data = bytearray(helper.read_bytes())
            data[-1] ^= 1
            helper.write_bytes(data)
            manifest['helper_build'] = sha256(helper)
            manifest['files']['helper/AnyPS5DesktopHelper.exe']['sha256'] = sha256(helper)
            manifest['id'] = identity(manifest)
            (payload / 'manifest.json').write_text(json.dumps(manifest))
            with patch.object(exporter.shutil, 'copytree', side_effect=OSError('Interrupted export')):
                with self.assertRaisesRegex(OSError, 'Interrupted export'):
                    exporter.export(payload, artifacts, state)
            self.assertEqual((artifacts / 'current.json').read_bytes(), current)
            self.assertFalse(list(artifacts.glob('.pending-*')))
            (payload / 'libs/libstdc++-6.dll').write_bytes(b'interrupted download')
            with self.assertRaises(ValueError):
                exporter.export(payload, artifacts, state)
            self.assertEqual((artifacts / 'current.json').read_bytes(), current)
            self.assertEqual(EngineCatalogue(artifacts).status()['status'], 'incompatible')
            self.assertTrue((artifacts / 'bundles' / json.loads(current)['id'] / 'relinker.exe').exists())

    def make_bundle(self, directory):
        image = bytearray(1024)
        image[:2] = b'MZ'
        struct.pack_into('<I', image, 60, 128)
        image[128:132] = b'PE\0\0'
        struct.pack_into('<HH', image, 132, 0x8664, 1)
        struct.pack_into('<H', image, 148, 240)
        struct.pack_into('<H', image, 152, 0x20b)
        image[392:400] = b'.text\0\0\0'
        struct.pack_into('<IIII', image, 400, 512, 4096, 512, 512)
        names = ['relinker.exe', 'AnyPS5Launcher.exe', 'helper/AnyPS5DesktopHelper.exe', 'libs/libc.prx',
                 'libs/libgcc_s_seh-1.dll', 'libs/libstdc++-6.dll', 'libs/libwinpthread-1.dll',
                 'Install-Helper.cmd', 'tools/launcher/Setup-WebHelper.ps1', 'portable.flag']
        files = {}
        for name in names:
            file = directory / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(image if name.endswith(('.exe', '.dll', '.prx')) else b'fixture')
            files[name] = {'sha256': sha256(file)}
        manifest = {'format': 1, 'target': {'os': 'windows', 'architecture': 'x86_64'}, 'helper_protocol': 1,
                    'source': {'fingerprint': 'a' * 64}, 'helper_build': files['helper/AnyPS5DesktopHelper.exe']['sha256'], 'libraries': ['libc.prx'], 'files': files,
                    'compiler_runtime': {Path(name).name: files[name]['sha256'] for name in names if name.endswith('.dll')}}
        manifest['id'] = identity(manifest)
        (directory / 'manifest.json').write_text(json.dumps(manifest))
        return manifest

    def test_runtime_corruption_missing_dll_and_wrong_target_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = self.make_bundle(root)
            self.assertEqual(verify_bundle(root)['id'], manifest['id'])
            file = root / 'libs/libstdc++-6.dll'
            original = file.read_bytes()
            file.write_bytes(b'wrong compiler runtime')
            with self.assertRaisesRegex(ValueError, 'checksum'):
                verify_bundle(root)
            file.unlink()
            with self.assertRaisesRegex(ValueError, 'missing'):
                verify_bundle(root)
            file.write_bytes(original)
            manifest['target']['os'] = 'linux'
            manifest['id'] = identity(manifest)
            (root / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'incompatible'):
                verify_bundle(root)

    def test_catalogue_rejects_stale_exports_and_corrupted_archives(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            staging = root / 'staging'
            manifest = self.make_bundle(staging)
            destination = root / 'bundles' / manifest['id']
            destination.parent.mkdir()
            staging.rename(destination)
            archive = root / (manifest['id'] + '.zip')
            archive.write_bytes(b'fixture archive')
            current = {'id': manifest['id'], 'archive_sha256': sha256(archive)}
            (root / 'current.json').write_text(json.dumps(current))
            (root / 'expected.json').write_text(json.dumps({'id': 'b' * 64, 'status': 'ready'}))
            catalogue = EngineCatalogue(root)
            self.assertEqual(catalogue.status()['status'], 'incompatible')
            (root / 'expected.json').write_text(json.dumps({'id': manifest['id'], 'status': 'ready'}))
            self.assertEqual(catalogue.status()['status'], 'ready')
            archive.write_bytes(b'corrupt archive')
            self.assertEqual(catalogue.status()['status'], 'incompatible')
            self.assertTrue((destination / 'libs/libc.prx').exists())


if __name__ == '__main__':
    unittest.main()
