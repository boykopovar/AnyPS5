import configparser
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path, PurePosixPath
from sources import sync, source_metadata


def git(repository, *arguments, check=True):
    return subprocess.run(['git', '-c', 'safe.directory=*', '-C', str(repository), *arguments],
                          check=check, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def safe_path(root, name):
    value = PurePosixPath(name)
    if value.is_absolute() or '..' in value.parts or not value.parts or '\\' in name or ':' in name or value.parts[0] in ('.git', '.local'):
        raise ValueError('Invalid upstream overlay path: ' + name)
    return root.joinpath(*value.parts)


def tree(repository, revision):
    result = {}
    for entry in git(repository, 'ls-tree', '-rz', revision).stdout.split(b'\0'):
        if entry:
            metadata, name = entry.split(b'\t', 1)
            mode, kind, identity = metadata.decode().split()
            result[name.decode('utf-8')] = (mode, kind, identity)
    return result


def blob_identity(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def equivalent(left, right):
    return left == right or b'\0' not in left + right and left.replace(b'\r\n', b'\n') == right.replace(b'\r\n', b'\n')


def supplied_files(root):
    for directory, children, files in os.walk(root):
        children[:] = [name for name in children if name not in ('.git', '.local', '__pycache__') and not (Path(directory) == root and name == '3rdparty')]
        for name in files:
            path = Path(directory) / name
            if name == '.git':
                continue
            if path.is_symlink():
                raise ValueError('Fork overlay files must not be symbolic links: ' + str(path))
            yield path


def merge_supplied(supplied, destination, repository, base, revision, deleted=()):
    baseline, latest = tree(repository, base), tree(repository, revision)
    changed, added = [], []
    with tempfile.TemporaryDirectory(prefix='anyps5-merge-') as temporary:
        temporary = Path(temporary)
        for source in supplied_files(supplied):
            name = source.relative_to(supplied).as_posix()
            target = safe_path(destination, name)
            local = source.read_bytes()
            entry = baseline.get(name)
            if entry is None:
                if target.exists():
                    if not target.is_file() or not equivalent(local, target.read_bytes()):
                        raise ValueError('Upstream added a conflicting fork path: ' + name)
                    continue
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
                added.append(name)
                continue
            if entry[1] != 'blob':
                continue
            if blob_identity(local) == entry[2] or blob_identity(local.replace(b'\r\n', b'\n')) == entry[2]:
                continue
            original = git(repository, 'cat-file', 'blob', entry[2]).stdout
            if equivalent(local, original):
                continue
            if name not in latest or not target.is_file():
                raise ValueError('Upstream removed a file changed by this fork: ' + name)
            remote = target.read_bytes()
            if equivalent(local, remote):
                continue
            if equivalent(remote, original):
                merged = local
            else:
                if b'\0' in local + original + remote:
                    raise ValueError('Binary fork/upstream conflict: ' + name)
                for filename, data in (('fork', local), ('base', original), ('upstream', remote)):
                    (temporary / filename).write_bytes(data.replace(b'\r\n', b'\n'))
                result = subprocess.run(['git', 'merge-file', '-p', str(temporary / 'fork'), str(temporary / 'base'), str(temporary / 'upstream')], capture_output=True)
                if result.returncode:
                    raise ValueError('Fork/upstream merge conflict: ' + name + '. Resolve the fork change before rebuilding; no previous release was replaced.')
                merged = result.stdout
            target.write_bytes(merged)
            changed.append(name)
        for name in deleted:
            target = safe_path(destination, name)
            if name in latest and (name not in baseline or latest[name] != baseline[name]):
                raise ValueError('Upstream changed a file deleted by this fork: ' + name)
            if target.is_file():
                target.unlink()
    return {'changed_files': changed, 'added_files': added, 'deleted_files': list(deleted)}


def reference_repository(cache, url, revisions):
    directory = cache / hashlib.sha256(url.encode()).hexdigest()
    if not (directory / 'HEAD').is_file():
        directory.mkdir(parents=True, exist_ok=True)
        subprocess.run(['git', 'init', '--bare', '-q', str(directory)], check=True)
    for revision in revisions:
        if len(revision) != 40 or any(character not in '0123456789abcdef' for character in revision):
            raise ValueError('Invalid upstream revision')
        if git(directory, 'cat-file', '-e', revision + '^{commit}', check=False).returncode:
            subprocess.run(['git', '-c', 'http.version=HTTP/1.1', '-C', str(directory), 'fetch', '--depth=1', url, revision], check=True, timeout=600)
    return directory


def resolve_modules(repository, revision, known, cache, prefix='', overrides=()):
    data = git(repository, 'show', revision + ':.gitmodules', check=False)
    if data.returncode:
        return []
    configuration = configparser.ConfigParser()
    configuration.read_string(data.stdout.decode('utf-8'))
    result = []
    for section in configuration.sections():
        name = configuration[section]['path']
        safe_path(Path('/source'), name)
        path = prefix + name
        entry = git(repository, 'ls-tree', revision, name).stdout.decode().split()
        if len(entry) < 3 or entry[0] != '160000':
            raise ValueError('Upstream submodule has no gitlink: ' + path)
        record = {'path': path, 'url': configuration[section]['url'], 'commit': entry[2]}
        if path in overrides:
            if path not in known:
                raise ValueError('Fork submodule override is not locked: ' + path)
            record = dict(known[path])
        result.append(record)
        if path in known and known[path] == record:
            result.extend(dict(item) for nested, item in known.items() if nested.startswith(path + '/'))
        else:
            child = reference_repository(cache, record['url'], [record['commit']])
            result.extend(resolve_modules(child, record['commit'], known, cache, path + '/', overrides))
    return result


def prepare_upstream(supplied, upstream, work, lock, configuration):
    revision = source_metadata(upstream, '0' * 64)['revision']
    if not revision:
        raise ValueError('Upstream Git metadata is missing. The Git source must use ADD --keep-git-dir=true.')
    print('UPSTREAM RESOLVED: ' + configuration['repository'] + ' ' + os.environ.get('ANYPS5_UPSTREAM_REF', 'main') + ' -> ' + revision, flush=True)
    base = configuration['base_revision']
    references = work / 'upstream-references'
    repository = reference_repository(references, configuration['repository'], [base, revision])
    combined = work / 'combined'
    sync(upstream, combined)
    overlay = merge_supplied(supplied, combined, repository, base, revision, configuration.get('deleted_files', ()))
    effective = dict(lock)
    effective['submodules'] = sorted(resolve_modules(repository, revision, {record['path']: record for record in lock['submodules']}, references,
                                                   overrides=configuration.get('submodule_overrides', ())), key=lambda record: record['path'])
    upstream_identity = {'repository': configuration['repository'], 'ref': os.environ.get('ANYPS5_UPSTREAM_REF', 'main'), 'revision': revision}
    print('FORK OVERLAY: ' + str(len(overlay['changed_files'])) + ' changed files, ' + str(len(overlay['added_files'])) + ' additions; base ' + base, flush=True)
    return combined, effective, {'upstream': upstream_identity, 'fork_overlay': dict(overlay, base_revision=base)}
