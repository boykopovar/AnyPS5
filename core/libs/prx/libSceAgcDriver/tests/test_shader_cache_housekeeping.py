import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def native(path):
    return Path('\\\\?\\' + str(path)) if os.name == 'nt' else path


def snapshot(directory):
    return {str(path.relative_to(directory)): None if path.is_dir() else path.read_bytes()
            for path in directory.rglob('*')}


def main():
    executable = str(Path(sys.argv[1]).resolve())
    mode = sys.argv[2]
    assert mode in ('plain', 'plain-long', 'extended', 'extended-long', 'device')
    assert os.name == 'nt' or mode in ('plain', 'plain-long')
    sandbox = Path(tempfile.mkdtemp(prefix='aps5-housekeeping-')).resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    try:
        outside = sandbox / 'outside'
        outside.mkdir()
        environment = dict(os.environ, ANYPS5_NO_SHADER_CACHE='0',
                           ANYPS5_SHADER_CACHE_DIR=str(sandbox / 'bootstrap'))

        def run():
            return subprocess.run([executable, '--housekeeping-safety'], cwd=outside,
                                  env=environment, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, errors='replace', timeout=5)

        bootstrap = run()
        assert bootstrap.returncode == 0, bootstrap.stdout
        match = re.search(r'housekeeping version ([0-9a-f]{16})', bootstrap.stdout)
        assert match, bootstrap.stdout
        version = match.group(1)
        siblings = [digit + version[1:] for digit in '0123' if digit != version[0]]
        root = sandbox / 'cache-\u00e4\u6f22'
        if mode.endswith('long'):
            while len(str(root)) < 230:
                root /= 'd' * 40
            root /= 'd' * max(1, 269 - len(str(root)))
            assert len(str(root)) >= 270
        root = native(root)
        current = root / version
        current.mkdir(parents=True)
        old = time.time() - 30 * 24 * 3600
        for parent in (root, outside):
            for index, name in enumerate(siblings):
                folder = parent / name
                folder.mkdir()
                (folder / 'sentinel').write_bytes(b'preserve outside the cache root')
                if index != 1:
                    (folder / 'last-used').write_bytes(b'')
                    os.utime(folder / 'last-used', (old if index == 0 else time.time(),) * 2)
                os.utime(folder, (old, old))
        (outside / 'outside.tmp.1').write_bytes(b'not a cache temporary')
        os.utime(outside / 'outside.tmp.1', (old, old))
        for name in ('shader.bin', 'old.tmp.1', 'fresh.tmp.1', 'last-used'):
            (current / name).write_bytes(b'cached bytes')
        for name in ('shader.bin', 'old.tmp.1', 'last-used'):
            os.utime(current / name, (old, old))
        before = snapshot(outside)
        if mode.startswith('extended'):
            environment['ANYPS5_SHADER_CACHE_DIR'] = str(root)
        elif mode == 'device':
            environment['ANYPS5_SHADER_CACHE_DIR'] = '\\\\.\\' + str(root)[4:]
        else:
            environment['ANYPS5_SHADER_CACHE_DIR'] = str(root)[4:] if os.name == 'nt' else str(root)
        started = time.time()
        try:
            result = run()
        except subprocess.TimeoutExpired as error:
            assert snapshot(outside) == before, 'housekeeping changed files outside its root'
            raise AssertionError('housekeeping did not return within five seconds; child terminated') from error
        assert snapshot(outside) == before, 'housekeeping changed files outside its root'
        assert result.returncode == 0, result.stdout
        assert 'housekeeping version ' + version in result.stdout, result.stdout
        assert (current / 'last-used').stat().st_mtime >= started - 2, 'active cache stamp was not refreshed'
        skipped = mode.startswith('extended') or mode == 'device'
        for name in siblings[:2]:
            assert (root / name).exists() == skipped, 'unexpected stale cache directory eviction'
        assert (root / siblings[2] / 'sentinel').read_bytes() == b'preserve outside the cache root'
        assert (current / 'old.tmp.1').exists() == skipped, 'unexpected old temporary removal'
        assert (current / 'fresh.tmp.1').read_bytes() == b'cached bytes'
        assert (current / 'shader.bin').read_bytes() == b'cached bytes'
        print(mode + ': returned, active stamp refreshed, outside tree intact, eviction policy verified')
    finally:
        assert sandbox.parent == temporary and sandbox.name.startswith('aps5-housekeeping-')
        shutil.rmtree(native(sandbox))


if __name__ == '__main__':
    main()
