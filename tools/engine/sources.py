import argparse
import configparser
import hashlib
import json
import os
import shutil
import subprocess
import time
from compatibility import PATCHES
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            result.update(block)
    return result.hexdigest()


def source_metadata(supplied, fingerprint):
    head = supplied / '.git/HEAD'
    revision = None
    if head.is_file():
        value = head.read_text().strip()
        if value.startswith('ref: '):
            reference = value[5:]
            if not reference.startswith('refs/heads/') or '..' in Path(reference).parts:
                raise ValueError('Unsupported Git HEAD reference')
            loose = supplied / '.git' / reference
            value = loose.read_text().strip() if loose.is_file() else ''
            packed = supplied / '.git/packed-refs'
            if not value and packed.is_file():
                value = next((line.split()[0] for line in packed.read_text().splitlines() if line.endswith(' ' + reference)), '')
        if len(value) == 40 and all(ch in '0123456789abcdef' for ch in value):
            revision = value
    return {'fingerprint': fingerprint, 'revision': revision, 'archive': not head.is_file(), 'dirty': None,
            'working_tree_status': 'Fingerprint identifies supplied content; cleanliness is not inferred from metadata-only Git context'}


def download(record, destination):
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_file() and digest(destination) == record['sha256']:
        return destination
    temporary = destination.with_suffix('.partial')
    for attempt in range(3):
        try:
            with urllib.request.urlopen(record['url'], timeout=120) as response, temporary.open('wb') as output:
                shutil.copyfileobj(response, output)
            if digest(temporary) != record['sha256']:
                raise ValueError('SHA-256 mismatch for ' + record['url'])
            temporary.replace(destination)
            return destination
        except Exception as error:
            temporary.unlink(missing_ok=True)
            if attempt == 2:
                raise RuntimeError('Download failed after three attempts: ' + record['url']) from error
            time.sleep(2)


def git_modules(root, prefix=''):
    result = []
    configuration = configparser.ConfigParser()
    configuration.read(root / '.gitmodules')
    for section in configuration.sections():
        relative, url = configuration[section]['path'], configuration[section]['url']
        value = subprocess.check_output(['git', '-C', str(root), 'ls-tree', 'HEAD', relative], text=True).split()
        if not value or value[0] != '160000':
            raise ValueError('Missing gitlink: ' + prefix + relative)
        result.append({'path': prefix + relative, 'url': url, 'commit': value[2]})
        result.extend(git_modules(root / relative, prefix + relative + '/'))
    return result


def checkout(record, destination):
    destination = Path(destination)
    marker = destination / '.anyps5-locked-commit'
    if marker.is_file() and marker.read_text() == record['commit']:
        return
    pending = destination.with_name(destination.name + '.fetching')
    if pending.exists():
        shutil.rmtree(pending)
    pending.mkdir(parents=True)
    subprocess.run(['git', 'init', '-q', str(pending)], check=True)
    for attempt in range(3):
        fetched = subprocess.run(['git', '-c', 'http.version=HTTP/1.1', '-C', str(pending), 'fetch', '--depth=1', record['url'], record['commit']], timeout=600)
        if fetched.returncode == 0:
            break
        if attempt == 2:
            raise RuntimeError('Cannot fetch locked dependency ' + record['path'] + ' at ' + record['commit'])
        time.sleep(2)
    actual = subprocess.check_output(['git', '-C', str(pending), 'rev-parse', 'FETCH_HEAD'], text=True).strip()
    if actual != record['commit']:
        raise ValueError('Dependency commit mismatch: ' + record['path'])
    subprocess.run(['git', '-C', str(pending), 'checkout', '--detach', '-q', 'FETCH_HEAD'], check=True)
    shutil.rmtree(pending / '.git')
    (pending / '.anyps5-locked-commit').write_text(record['commit'])
    if destination.exists():
        shutil.rmtree(destination)
    pending.rename(destination)


def sync(source, destination):
    destination.mkdir(parents=True, exist_ok=True)
    names = set()
    for path in source.iterdir():
        if path.name == '.git':
            continue
        names.add(path.name)
        target = destination / path.name
        if path.is_symlink():
            if not target.is_symlink() or os.readlink(target) != os.readlink(path):
                if target.exists() or target.is_symlink():
                    target.unlink()
                target.symlink_to(os.readlink(path))
        elif path.is_dir():
            sync(path, target)
        elif not target.is_file() or digest(path) != digest(target):
            shutil.copy2(path, target)
    for path in destination.iterdir():
        if path.name not in names:
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
            else:
                path.unlink()


def hydrate(supplied, destination, cache, lock):
    sync(supplied, destination)
    for record in lock['submodules']:
        target = destination / record['path']
        if target.is_dir() and any(target.iterdir()):
            print('Preserving supplied dependency: ' + record['path'], flush=True)
            continue
        downloaded = cache / record['path']
        checkout(record, downloaded)
        sync(downloaded, target)
    for patch in PATCHES:
        patch = patch.replace('\r\n', '\n')
        targets = [line[6:] for line in patch.splitlines() if line.startswith('+++ b/')]
        if len(targets) != 1 or '..' in Path(targets[0]).parts or Path(targets[0]).is_absolute():
            raise ValueError('Build patches must modify one staged relative file')
        if b'\r\n' in (destination / targets[0]).read_bytes():
            patch = patch.replace('\n', '\r\n')
        patch = patch.encode()
        command = ['git', '-C', str(destination), 'apply']
        applicable = subprocess.run([*command, '--check', '-'], input=patch, capture_output=True)
        if applicable.returncode:
            previous = subprocess.run([*command, '--reverse', '--check', '-'], input=patch, capture_output=True)
            if previous.returncode:
                raise RuntimeError('Supplied dependency modifications conflict with required build patch: ' + targets[0] + '\n' + applicable.stderr.decode(errors='replace'))
        else:
            subprocess.run([*command, '-'], input=patch, check=True)
    identity = hashlib.sha256()
    for path in sorted(destination.rglob('*')):
        if path.is_file() and '.git' not in path.parts:
            identity.update(path.relative_to(destination).as_posix().encode() + b'\0')
            identity.update(bytes.fromhex(digest(path)))
    return identity.hexdigest()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    path = root / 'tools/engine/dependencies.lock'
    lock = json.loads(path.read_text())
    modules = sorted(git_modules(root), key=lambda value: value['path'])
    if args.write:
        lock['submodules'] = modules
        path.write_text(json.dumps(lock, indent=2) + '\n')
    elif modules != lock['submodules']:
        raise SystemExit('Dependency lock differs from recursive gitlinks. Run tools/engine/sources.py --write with initialized submodules.')
    print('Dependency lock matches ' + str(len(modules)) + ' recursive gitlinks')
