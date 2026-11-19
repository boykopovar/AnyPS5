import argparse
import os
import json
import subprocess
import tempfile
from pathlib import Path


def snapshot(output):
    repository = Path(__file__).resolve().parents[2]
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    lock = json.loads((repository / 'tools/engine/dependencies.lock').read_text())
    for record in lock['submodules']:
        module = repository / record['path']
        if not (module / '.git').exists():
            continue
        revision = subprocess.check_output(['git', '-C', str(module), 'rev-parse', 'HEAD'], text=True).strip()
        changes = subprocess.check_output(['git', '-C', str(module), 'status', '--porcelain', '--untracked-files=normal', '--ignore-submodules=none'], text=True).strip()
        if revision != record['commit'] or changes:
            raise ValueError('Source ZIP would omit dependency changes: ' + record['path'] + '. Commit and lock them before snapshotting; the checkout was not changed.')
    with tempfile.TemporaryDirectory(prefix='anyps5-review-index-') as temporary:
        environment = dict(os.environ, GIT_INDEX_FILE=str(Path(temporary) / 'index'))

        def git(*arguments, text=None):
            return subprocess.check_output(['git', '-C', str(repository), *arguments], input=text, text=True, env=environment).strip()

        git('read-tree', 'HEAD')
        git('add', '-A', '--', '.')
        tree = git('write-tree')
        commit = git('commit-tree', tree, '-p', 'HEAD', text='feat(engine): build Windows distribution in Linux containers\n')
        archive = output / ('AnyPS5-Source-' + commit[:12] + '.zip')
        if archive.exists():
            raise ValueError('Source archive already exists: ' + str(archive))
        subprocess.run(['git', '-C', str(repository), 'archive', '--format=zip', '--output=' + str(archive), commit], check=True)
        clone = output / ('AnyPS5-Clone-' + commit[:12])
        subprocess.run(['git', 'clone', '--shared', '--no-checkout', str(repository), str(clone)], check=True)
        subprocess.run(['git', '-C', str(clone), 'checkout', '--detach', commit], check=True)
        subprocess.run(['git', '-C', str(clone), 'repack', '-a', '-d'], check=True)
        (clone / '.git/objects/info/alternates').unlink()
        subprocess.run(['git', '-C', str(clone), 'cat-file', '-e', commit + '^{tree}'], check=True)
        for path in ('b', 'build_tools', '.local'):
            if (clone / path).exists():
                raise RuntimeError('Source snapshot contains local build/state: ' + path)
        for path in ('compose.yaml', 'tools/engine/Dockerfile', 'tools/engine/dependencies.lock', 'tools/launcher/DesktopHelper.cpp'):
            if not (clone / path).is_file():
                raise RuntimeError('Source snapshot omitted required file: ' + path)
        print('Prepared snapshot commit: ' + commit)
        print('Fresh clone with uninitialized submodules: ' + str(clone))
        print('Source ZIP without Git metadata or submodule contents: ' + str(archive))
        print('Original branch, index, build output and user data were not changed')
        return commit, clone, archive


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    snapshot(parser.parse_args().output)
