import json
import shutil
import subprocess
from pathlib import Path
from bundle import identity, sha256, verify_bundle


def package(source, payload, metadata, compiler_runtime):
    expected = {p.name + '.prx' for p in (source / 'core/libs/prx').iterdir() if p.is_dir() and not p.name.startswith('.')}
    actual = {p.name for p in (payload / 'libs').glob('*.prx')}
    if expected != actual:
        raise ValueError('Incomplete runtime set: ' + repr(expected ^ actual))
    (payload / 'helper').mkdir(exist_ok=True)
    (payload / 'AnyPS5DesktopHelper.exe').replace(payload / 'helper/AnyPS5DesktopHelper.exe')
    for name in ('Setup-WebHelper.ps1', 'Pair-Browser.ps1'):
        target = payload / 'tools/launcher' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / 'tools/launcher' / name, target)
    shutil.copy2(source / 'tools/engine/Install-Helper.ps1', payload / 'Install-Helper.ps1')
    shutil.copy2(source / 'Install-Helper.cmd', payload / 'Install-Helper.cmd')
    shutil.copy2(source / 'Pair-Browser.cmd', payload / 'Pair-Browser.cmd')
    shutil.copy2(source / 'LICENSE', payload / 'LICENSE')
    (payload / 'portable.flag').touch()
    preparer = payload / 'preparer'
    preparer.mkdir()
    shutil.copytree('/opt/python', preparer / 'python')
    (preparer / 'python/python313._pth').write_text('python313.zip\n.\n')
    for name in ('bundle.py', 'pe.py'):
        shutil.copy2(source / 'tools/engine' / name, preparer / name)
    shutil.copy2(source / 'tools/launcher/prepare.py', preparer / 'prepare.py')
    shutil.copy2(source / 'tools/unwrap_self.py', preparer / 'unwrap_self.py')
    for name in ('import_audit.py', 'nid_names.py', 'progress.py', 'shader_scan.py', 'isa_profiles.py'):
        shutil.copy2(source / 'tools' / name, preparer / name)
    subprocess.run(['xvfb-run', '-a', 'wine', 'Z:' + str(preparer / 'python/python.exe'), '-I', '-S', '-B',
                    'Z:' + str(preparer / 'prepare.py'), '--audit', '--help'], check=True)
    for path in (source / '3rdparty').rglob('*'):
        if path.is_file() and (path.name.upper().startswith(('LICENSE', 'COPYING', 'NOTICE', 'PATENTS'))):
            target = payload / 'notices' / path.relative_to(source / '3rdparty')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    shutil.copy2(source / 'tools/launcher/LICENSE', payload / 'notices/launcher.txt')
    manifest = {'format': 1, 'target': {'os': 'windows', 'architecture': 'x86_64'},
                'helper_protocol': 1, 'preparation_protocol': 1, 'helper_build': sha256(payload / 'helper/AnyPS5DesktopHelper.exe'),
                'source': metadata, 'libraries': sorted(expected),
                'compiler_runtime': compiler_runtime,
                'files': {p.relative_to(payload).as_posix(): {'sha256': sha256(p), 'bytes': p.stat().st_size}
                          for p in sorted(payload.rglob('*')) if p.is_file() and p.name != 'manifest.json'}}
    manifest['id'] = identity(manifest)
    (payload / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    verify_bundle(payload)
    print('VERIFIED ENGINE: ' + manifest['id'], flush=True)
