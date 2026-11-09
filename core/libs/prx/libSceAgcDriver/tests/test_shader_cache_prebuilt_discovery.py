import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def native(path):
    return Path('\\\\?\\' + str(path)) if os.name == 'nt' else path


def snapshot(directory):
    return {str(path.relative_to(directory)): None if path.is_dir() else path.read_bytes()
            for path in directory.rglob('*')}


def main():
    executable = str(Path(sys.argv[1]).resolve())
    mode = sys.argv[2]
    assert mode in ('plain', 'relative', 'extended', 'extended-forward', 'device', 'plain-long', 'extended-long')
    assert os.name == 'nt' or mode in ('plain', 'relative', 'plain-long')
    temporary = Path(tempfile.gettempdir()).resolve()
    sandbox = Path(tempfile.mkdtemp(prefix='aps5-prebuilt-discovery-')).resolve()
    try:
        outside = sandbox / 'outside'
        outside.mkdir()
        (outside / 'z.pack').write_bytes(b'outside directory canary')
        (outside / 'sentinel').write_bytes(b'unchanged outside the cache root')
        original = snapshot(outside)
        for layout in ('pack', 'loose', 'versioned', 'file', 'empty', 'missing'):
            root = sandbox / (layout + '-\u00e4\u6f22')
            if mode.endswith('long'):
                while len(str(root)) < 270:
                    root /= 'd' * 40
                assert len(str(root / 'prebuilt')) >= 278
            root = native(root)
            root.mkdir(parents=True)
            plain = str(root)[4:] if os.name == 'nt' else str(root)
            alias = plain
            if mode.startswith('extended'):
                alias = str(root)
                if mode == 'extended-forward':
                    alias = '\\\\?\\' + plain.replace('\\', '/')
            elif mode == 'device':
                alias = '\\\\.\\' + plain
            elif mode == 'relative':
                alias = os.path.join('.', os.path.relpath(plain, outside))
            environment = dict(os.environ, ANYPS5_NO_SHADER_CACHE='0', ANYPS5_SHADER_CACHE_DIR=alias,
                               APS5_NO_PIPELINE_JOURNAL='1')

            def run(action):
                try:
                    result = subprocess.run([executable, '--' + action + '-prebuilt-discovery', layout], cwd=outside,
                                            env=environment, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                            text=True, errors='replace', timeout=5)
                except subprocess.TimeoutExpired as error:
                    raise AssertionError('prebuilt discovery did not return within five seconds; child terminated') from error
                assert snapshot(outside) == original, 'prebuilt discovery changed files outside its root'
                assert result.returncode == 0, mode + ' ' + layout + ':\n' + result.stdout
                assert 'prebuilt discovery ' + ('seeded' if action == 'seed' else 'verified') + ' ' + layout in result.stdout

            run('seed')
            before = snapshot(root)
            run('verify')
            assert snapshot(root) == before, 'prebuilt discovery modified its read-only input'
        print(mode + ': six prebuilt layouts verified; cache and outside trees unchanged')
    finally:
        assert sandbox.parent == temporary and sandbox.name.startswith('aps5-prebuilt-discovery-')
        shutil.rmtree(native(sandbox))


if __name__ == '__main__':
    main()
