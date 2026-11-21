import importlib.util
import ctypes
import os
import threading
import time
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('preparer', Path(__file__).with_name('prepare.py'))
preparer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preparer)


class PreparationTests(unittest.TestCase):
    @unittest.skipUnless(os.name == 'nt', 'Windows sharing semantics')
    def test_status_publish_survives_windows_reader_lock(self):
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'status.json'
            preparer.atomic_json(path, {'generation': 0})
            handle = kernel.CreateFileW(str(path), 0x80000000, 3, None, 3, 0x80, None)
            self.assertNotEqual(handle, ctypes.c_void_p(-1).value)
            pending = path.with_suffix('.probe')
            pending.write_text('{}')
            try:
                with self.assertRaises(PermissionError):
                    os.replace(pending, path)
                released = threading.Event()
                def release():
                    time.sleep(0.15)
                    kernel.CloseHandle(handle)
                    released.set()
                thread = threading.Thread(target=release)
                thread.start()
                try:
                    preparer.atomic_json(path, {'generation': 1})
                finally:
                    thread.join()
                    handle = None
                self.assertTrue(released.is_set())
                self.assertEqual(json.loads(path.read_text()), {'generation': 1})
                self.assertFalse(list(Path(directory).glob('*.pending')))
            finally:
                if handle is not None:
                    kernel.CloseHandle(handle)

    def test_failed_status_publish_preserves_last_valid_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'status.json'
            preparer.atomic_json(path, {'generation': 0})
            denied = PermissionError('File temporarily locked')
            denied.winerror = 5
            with patch.object(preparer.os, 'replace', side_effect=denied):
                with self.assertRaises(PermissionError):
                    preparer.atomic_json(path, {'generation': 1}, timeout=0)
            self.assertEqual(json.loads(path.read_text()), {'generation': 0})
            self.assertFalse(list(Path(directory).glob('*.pending')))

    def test_discovery_selects_required_external_modules_not_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            paths = [root / name for name in ('eboot.bin', 'sce_module/nested/runtime.prx', 'Plugins/Network/transport.prx', 'PackageInfo/legal/license.sprx')]
            for path in paths:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'\x7fELF' + bytes(60))
            files = preparer.inventory(root)
            modules, folders = preparer.discover_modules(root, files, paths[0], {'TRANSPORT.sprx'})
            self.assertEqual(set(modules), {paths[1], paths[2]})
            self.assertIn(paths[1].parent, folders)
            self.assertIn(paths[2].parent, folders)
            self.assertNotIn(paths[3].parent, folders)

    def test_conversion_failure_does_not_copy_bulk_resources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            image = bytearray(64)
            image[:6] = b'\x7fELF\x02\x01'
            image[18] = 62
            (source / 'eboot.bin').write_bytes(image)
            (source / 'resource.dat').write_bytes(b'original assets')
            engine = root / 'engine'
            (engine / 'libs').mkdir(parents=True)
            worker = preparer.Preparation(engine, source, root / 'output', root / 'job')
            copied = []
            original = worker.copy
            def copy(path, target, expected=None):
                copied.append(path)
                return original(path, target, expected)
            with patch.object(preparer, 'verify_bundle', return_value={'id': 'fixture'}), patch.object(worker, 'copy', side_effect=copy), patch.object(worker, 'command', side_effect=ValueError('Unsupported conversion')):
                self.assertEqual(worker.run(), 2)
            self.assertNotIn(source / 'resource.dat', copied)
            self.assertFalse((root / 'output').exists())
            self.assertFalse(list(root.glob('.anyps5-preparing-*')))
            self.assertEqual((source / 'resource.dat').read_bytes(), b'original assets')

    def test_package_helper_does_not_replace_nested_game_main(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'helper.elf').write_bytes(b'helper')
            game = root / 'TITLE'
            game.mkdir()
            (game / 'eboot.bin').write_bytes(b'game')
            (game / 'helper.elf').write_bytes(b'helper')
            self.assertEqual(preparer.resources(root), (game, game / 'eboot.bin'))
            second = root / 'SECOND'
            second.mkdir()
            (second / 'eboot.bin').touch()
            with self.assertRaisesRegex(ValueError, 'Multiple game folders'):
                preparer.resources(root)

    def test_companions_are_cpu_executables_outside_module_folders(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = bytearray(64)
            image[:6] = b'\x7fELF\x02\x01'
            image[18] = 62
            for name in ('eboot.elf', 'helpers/worker.elf', 'sce_module/module.elf'):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(image)
            image[18] = 224
            (root / 'shader.elf').write_bytes(image)
            result = preparer.discover_executables(root, preparer.inventory(root), root / 'eboot.elf', [root / 'sce_module'])
            self.assertEqual(result, [root / 'helpers/worker.elf'])
            self.assertEqual(preparer.resources(root), (root, root / 'eboot.elf'))

    def test_ambiguous_inputs_and_package_only_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'game.pkg').write_bytes(b'package')
            with self.assertRaisesRegex(ValueError, 'extraction and decryption'):
                preparer.resources(root)
            for name in ('first.elf', 'second.self'):
                (root / name).touch()
            with self.assertRaisesRegex(ValueError, 'Multiple executable'):
                preparer.resources(root)

    def test_existing_output_and_source_overlap_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            original = source / 'eboot.bin'
            original.write_bytes(b'original')
            output = root / 'existing'
            output.mkdir()
            (output / 'save.dat').write_bytes(b'keep')
            for index, destination in enumerate((output, source / 'output')):
                job = root / ('job' + str(index))
                worker = preparer.Preparation(root / 'engine', source, destination, job)
                self.assertEqual(worker.run(), 2)
                self.assertEqual((job / 'done').read_text(), 'failed')
            self.assertEqual(original.read_bytes(), b'original')
            self.assertEqual((output / 'save.dat').read_bytes(), b'keep')
            self.assertFalse((source / 'output').exists())

    def test_insufficient_space_reports_capacity_without_publishing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            image = bytearray(64)
            image[:6] = b'\x7fELF\x02\x01'
            image[18] = 62
            (source / 'eboot.bin').write_bytes(image)
            engine = root / 'engine'
            (engine / 'libs').mkdir(parents=True)
            job = root / 'job'
            worker = preparer.Preparation(engine, source, root / 'output', job)
            usage = preparer.shutil._ntuple_diskusage(100, 100, 0)
            with patch.object(preparer, 'verify_bundle', return_value={'id': 'fixture'}), patch.object(preparer.shutil, 'disk_usage', return_value=usage):
                self.assertEqual(worker.run(), 2)
            status = json.loads((job / 'status.json').read_text())
            self.assertGreater(status['required_bytes'], 0)
            self.assertEqual(status['available_bytes'], 0)
            self.assertIn('short by', status['message'])
            self.assertIn('Settings', status['message'])
            self.assertFalse((root / 'output').exists())
            self.assertFalse(list(root.glob('.anyps5-preparing-*')))
            self.assertEqual((source / 'eboot.bin').read_bytes(), image)

    def test_cancel_and_failed_validation_never_publish(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            original = source / 'eboot.bin'
            original.write_bytes(b'invalid input')
            for index, cancel in enumerate((False, True)):
                job = root / ('job' + str(index))
                output = root / ('output' + str(index))
                worker = preparer.Preparation(root / 'engine', source, output, job)
                if cancel:
                    (job / 'cancel').touch()
                with patch.object(preparer, 'verify_bundle', return_value={'id': 'fixture'}):
                    self.assertEqual(worker.run(), 2)
                status = json.loads((job / 'status.json').read_text())
                self.assertEqual(status['status'], 'cancelled' if cancel else 'failed')
                self.assertFalse(output.exists())
                self.assertFalse(list(root.glob('.anyps5-preparing-*')))
                self.assertEqual(original.read_bytes(), b'invalid input')


if __name__ == '__main__':
    unittest.main()
