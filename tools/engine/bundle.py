import hashlib
import json
import struct
import threading
import time
from pathlib import Path, PurePosixPath
from pe import inspect_pe


SYSTEM_DLLS = set(('advapi32 bcrypt cfgmgr32 comctl32 comdlg32 crypt32 dbghelp dnsapi dwmapi dxgi gdi32 hid imm32 '
                  'iphlpapi kernel32 msvcrt ncrypt netapi32 normaliz ntdll ole32 oleaut32 pdh powrprof propsys psapi rpcrt4 '
                  'secur32 setupapi shell32 shlwapi user32 userenv ucrtbase usp10 version winhttp wininet winmm '
                  'winspool ws2_32 wtsapi32').split())


def sha256(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            result.update(block)
    return result.hexdigest()


def member(root, name):
    path = PurePosixPath(name)
    if not name or path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name or path.as_posix() != name:
        raise ValueError('Invalid bundle member: ' + name)
    value = root.joinpath(*path.parts)
    if value.is_symlink():
        raise ValueError('Bundle member escapes its directory: ' + name)
    return value


def identity(manifest):
    value = dict(manifest)
    value.pop('id', None)
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def verify_bundle(root):
    root = Path(root)
    if root.is_symlink():
        raise ValueError('Engine root must not be a symbolic link')
    manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
    if not isinstance(manifest, dict):
        raise ValueError('Engine manifest must be an object')
    if manifest.get('format') != 1 or manifest.get('target') != {'os': 'windows', 'architecture': 'x86_64'} or manifest.get('helper_protocol') != 1:
        raise ValueError('Engine target, manifest or helper protocol is incompatible')
    if manifest.get('id') != identity(manifest):
        raise ValueError('Engine manifest identity does not match its contents')
    files = manifest.get('files', {})
    if not isinstance(files, dict) or not files or len(files) > 20000 or any(not isinstance(name, str) or not isinstance(record, dict) or not isinstance(record.get('sha256'), str) for name, record in files.items()):
        raise ValueError('Invalid engine file list')
    if not isinstance(manifest.get('source'), dict):
        raise ValueError('Engine source provenance is missing')
    fingerprint = manifest['source'].get('fingerprint', '')
    if not isinstance(fingerprint, str) or len(fingerprint) != 64 or any(ch not in '0123456789abcdef' for ch in fingerprint):
        raise ValueError('Engine source fingerprint is invalid')
    if manifest.get('helper_build') != files.get('helper/AnyPS5DesktopHelper.exe', {}).get('sha256'):
        raise ValueError('Desktop helper build identity does not match its executable')
    if any(path.is_symlink() for path in root.rglob('*')):
        raise ValueError('Engine bundle must not contain symbolic links')
    if {path.relative_to(root).as_posix() for path in root.rglob('*') if path.is_file()} != set(files) | {'manifest.json'}:
        raise ValueError('Engine bundle contains unlisted or missing files')
    libraries = set(manifest.get('libraries', []))
    if not libraries or libraries != {path.name for path in (root / 'libs').glob('*.prx')}:
        raise ValueError('Patched runtime library set does not match the build metadata')
    required = {'relinker.exe', 'AnyPS5Launcher.exe', 'helper/AnyPS5DesktopHelper.exe',
                'Install-Helper.cmd', 'tools/launcher/Setup-WebHelper.ps1', 'portable.flag'}
    if manifest.get('preparation_protocol') == 1:
        required.update({'preparer/prepare.py', 'preparer/unwrap_self.py', 'preparer/bundle.py', 'preparer/pe.py',
                         'preparer/import_audit.py', 'preparer/nid_names.py', 'preparer/progress.py',
                         'preparer/shader_scan.py', 'preparer/isa_profiles.py',
                         'preparer/python/python.exe', 'preparer/python/python313.dll', 'preparer/python/python313.zip', 'preparer/python/python313._pth'})
    required.update('libs/' + name for name in libraries)
    required.update('libs/' + name for name in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'))
    if not required <= files.keys():
        raise ValueError('Engine bundle omits required files')
    available = {name.lower() for name in files}
    if len(available) != len(files):
        raise ValueError('Engine contains names that collide on Windows')
    runtime_names = {'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'}
    if set(manifest.get('compiler_runtime', {})) != runtime_names:
        raise ValueError('Engine compiler runtime provenance is incomplete')
    for name, record in files.items():
        path = member(root, name)
        if not path.is_file() or sha256(path) != record['sha256']:
            raise ValueError('Engine file missing or checksum mismatch: ' + name)
        if name.lower().endswith(('.exe', '.dll', '.prx', '.pyd')):
            information = inspect_pe(path)
            for dependency in information['imports']:
                lower = dependency.lower()
                adjacent = (PurePosixPath(name).parent / lower).as_posix()
                if adjacent.lower() in available or lower in available or lower.startswith(('api-ms-win-', 'ext-ms-win-')):
                    continue
                if lower.endswith('.dll') and lower[:-4] in SYSTEM_DLLS:
                    continue
                raise ValueError(name + ': unresolved required Windows dependency ' + dependency)
    for name, expected in manifest.get('compiler_runtime', {}).items():
        if files.get('libs/' + name, {}).get('sha256') != expected:
            raise ValueError('Compiler runtime identity mismatch: ' + name)
    return manifest


class EngineCatalogue:
    def __init__(self, root):
        self.root = Path(root)
        self.cached = None
        self.lock = threading.Lock()
        self.validated_at = 0
        self.pointers = None

    def status(self, max_age=0):
        with self.lock:
            try:
                pointers = tuple((self.root / name).read_bytes() for name in ('expected.json', 'current.json'))
                if self.cached and pointers == self.pointers and time.monotonic() - self.validated_at < max_age:
                    return dict(self.cached)
            except OSError:
                pass
            return self.read_status()

    def read_status(self):
        try:
            expected = json.loads((self.root / 'expected.json').read_bytes())
            if expected.get('status') == 'exporting':
                self.cached = None
                return {'status': 'exporting', 'message': 'The engine container is exporting the Windows engine. This page updates when it is ready.'}
            if expected.get('status') == 'failed':
                self.cached = None
                return {'status': 'failed', 'message': 'The Windows engine export failed: ' + str(expected.get('message', 'no reason recorded')) + '. Run docker compose logs engine for details.'}
            pointers = tuple((self.root / name).read_bytes() for name in ('expected.json', 'current.json'))
            expected, current = (json.loads(value) for value in pointers)
            if expected.get('status') != 'ready' or current['id'] != expected['id']:
                raise ValueError('The expected engine export is incomplete or failed; the previous bundle is retained')
            if len(current['id']) != 64 or any(ch not in '0123456789abcdef' for ch in current['id']):
                raise ValueError('Invalid current engine identity')
            directory = self.root / 'bundles' / current['id']
            archive = self.root / (current['id'] + '.zip')
            if (self.root / 'bundles').is_symlink() or archive.is_symlink():
                raise ValueError('Engine storage must not contain symbolic links')
            verified = verify_bundle(directory)
            if verified['id'] != current['id'] or sha256(archive) != current['archive_sha256']:
                raise ValueError('Current engine archive or identity is inconsistent')
            if pointers != tuple((self.root / name).read_bytes() for name in ('expected.json', 'current.json')):
                raise ValueError('Engine export changed during verification; retry when export completes')
            self.cached = {'status': 'ready', 'id': verified['id'], 'source': verified['source'], 'target': verified['target'],
                           'helper_build': verified['helper_build'], 'helper_protocol': verified['helper_protocol'],
                           'library_count': len(verified['libraries']), 'download_url': '/engine/download'}
            self.validated_at = time.monotonic()
            self.pointers = pointers
            return dict(self.cached)
        except FileNotFoundError:
            self.cached = None
            return {'status': 'missing', 'message': 'Waiting for the engine container to export the Windows engine. If this does not change, run docker compose logs engine.'}
        except (ValueError, KeyError, TypeError, OSError, struct.error) as error:
            self.cached = None
            return {'status': 'incompatible', 'message': str(error)}

    def archive(self):
        state = self.status(30)
        if state['status'] != 'ready':
            raise ValueError(state.get('message', 'Engine is not ready'))
        current = json.loads((self.root / 'current.json').read_text())
        archive = self.root / (state['id'] + '.zip')
        if archive.is_symlink() or current['id'] != state['id'] or sha256(archive) != current['archive_sha256']:
            raise ValueError('Engine download checksum or identity mismatch')
        return archive
