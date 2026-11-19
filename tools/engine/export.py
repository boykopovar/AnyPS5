import fcntl
import json
import os
import secrets
import shutil
import tempfile
import zipfile
from pathlib import Path
from bundle import sha256, verify_bundle


def atomic(path, value):
    pending = path.with_suffix('.pending')
    with pending.open('w') as stream:
        json.dump(value, stream, indent=2)
        stream.flush()
        os.fsync(stream.fileno())
    pending.replace(path)


def export(payload, root, state):
    root.mkdir(parents=True, exist_ok=True)
    state.mkdir(parents=True, exist_ok=True)
    with (root / '.export.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        for pending in root.iterdir():
            if pending.name.startswith('.pending-'):
                if pending.is_dir() and not pending.is_symlink():
                    shutil.rmtree(pending)
                else:
                    pending.unlink()
            elif pending.name.endswith('.zip.pending'):
                pending.unlink()
        atomic(root / 'expected.json', {'status': 'exporting'})
        staging = None
        try:
            print('Verifying the compiled Windows payload', flush=True)
            manifest = verify_bundle(payload)
            identifier = manifest['id']
            print('Exporting engine ' + identifier, flush=True)
            (root / 'bundles').mkdir(exist_ok=True)
            destination = root / 'bundles' / identifier
            archive = root / (identifier + '.zip')
            if destination.exists():
                verify_bundle(destination)
            else:
                staging = Path(tempfile.mkdtemp(prefix='.pending-', dir=root))
                shutil.copytree(payload, staging, dirs_exist_ok=True)
                print('Verifying staged Windows files', flush=True)
                verify_bundle(staging)
                staging.replace(destination)
                staging = None
            pending = root / (identifier + '.zip.pending')
            if not archive.exists():
                print('Creating the Windows download archive', flush=True)
                with zipfile.ZipFile(pending, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=1) as zipped:
                    for path in sorted(destination.rglob('*')):
                        if path.is_file():
                            with path.open('rb') as incoming, zipped.open(path.relative_to(destination).as_posix(), 'w', force_zip64=True) as outgoing:
                                shutil.copyfileobj(incoming, outgoing, length=1048576)
                with zipfile.ZipFile(pending) as zipped:
                    if zipped.testzip() is not None:
                        raise ValueError('Engine archive CRC verification failed')
                pending.replace(archive)
            print('Checking download members and checksums', flush=True)
            with zipfile.ZipFile(archive) as zipped:
                if set(zipped.namelist()) != set(manifest['files']) | {'manifest.json'}:
                    raise ValueError('Engine archive member list mismatch')
                import hashlib
                for name, record in manifest['files'].items():
                    if hashlib.sha256(zipped.read(name)).hexdigest() != record['sha256']:
                        raise ValueError('Engine archive checksum mismatch: ' + name)
            backend = state / 'backend'
            backend.mkdir(exist_ok=True)
            token = state / 'auth.token'
            try:
                with token.open('x') as stream:
                    stream.write(secrets.token_hex(32))
            except FileExistsError:
                if len(token.read_text().strip()) < 32:
                    raise ValueError('Existing pairing credential is invalid; retained without modification')
            for path in (state, backend, token):
                os.chown(path, 10001, 10001)
            os.chmod(token, 0o600)
            atomic(root / 'current.json', {'id': identifier, 'archive_sha256': sha256(archive)})
            atomic(root / 'expected.json', {'status': 'ready', 'id': identifier})
            print('Windows engine ready: ' + identifier, flush=True)
        except BaseException as error:
            atomic(root / 'expected.json', {'status': 'failed', 'message': str(error)})
            raise
        finally:
            if staging is not None:
                shutil.rmtree(staging)


if __name__ == '__main__':
    export(Path('/payload'), Path('/artifacts'), Path('/web-state'))
