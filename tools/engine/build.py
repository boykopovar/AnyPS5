import json
import os
import shutil
import subprocess
import time
from pathlib import Path
from sources import hydrate, source_metadata, digest
from package import package
from upstream import prepare_upstream


def windows(path):
    return 'Z:' + str(path).replace('/', '\\')


def build_engine():
    started = time.monotonic()
    lock = json.loads(Path('/opt/buildscripts/dependencies.lock').read_text())
    configuration = json.loads(Path('/opt/buildscripts/upstream.lock').read_text())
    source, build = Path('/work/source'), Path('/work/windows')
    supplied, lock, updates = prepare_upstream(Path('/supplied'), Path('/upstream'), Path('/work'), lock, configuration)
    fingerprint = hydrate(supplied, source, Path('/work/dependencies'), lock)
    print('ENGINE SOURCE SHA256: ' + fingerprint, flush=True)
    cmake = '/opt/cmake/cmake-3.31.6-windows-x86_64/bin/cmake.exe'
    environment = dict(os.environ, WINEPATH='Z:\\opt\\toolchain\\mingw64\\bin;Z:\\opt\\ninja;Z:\\opt\\python')
    arguments = [windows(Path(cmake)), '-S', windows(source), '-B', windows(build), '-G', 'Ninja',
                 '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF', '-DANYPS5_RELINKER_ONLY=OFF',
                 '-DCMAKE_C_COMPILER=Z:/opt/toolchain/mingw64/bin/gcc.exe',
                 '-DCMAKE_CXX_COMPILER=Z:/opt/toolchain/mingw64/bin/g++.exe',
                 '-DCMAKE_MAKE_PROGRAM=Z:/opt/ninja/ninja.exe', '-DPython3_EXECUTABLE=Z:/opt/python/python.exe',
                 '-DFFMPEG_PREBUILT_DIR=Z:/opt/ffmpeg', '-DANYPS5_LAUNCHER_DATA_ROOT=Z:/persistent/launcher',
                 '-DANYPS5_ENGINE_REVISION=source-' + fingerprint[:20]]
    subprocess.run(['xvfb-run', '-a', 'wine', *arguments], env=environment, check=True)
    jobs = int(os.environ.get('ANYPS5_BUILD_JOBS', '4'))
    if not 1 <= jobs <= 32:
        raise ValueError('ANYPS5_BUILD_JOBS must be between 1 and 32')
    subprocess.run(['xvfb-run', '-a', 'wine', windows(Path(cmake)), '--build', windows(build), '--target',
                    'relinker', 'libs', 'launcher', 'desktop_helper', '--parallel', str(jobs)], env=environment, check=True)
    payload = Path('/payload')
    payload.mkdir(exist_ok=True)
    shutil.copytree(build / 'core/libs/libs', payload / 'libs', ignore=shutil.ignore_patterns('unpatched', 'implib'), dirs_exist_ok=True)
    shutil.copy2(build / 'core/relinker/relinker.exe', payload / 'relinker.exe')
    for name in ('AnyPS5Launcher.exe', 'AnyPS5DesktopHelper.exe'):
        shutil.copy2(build / 'tools/launcher' / name, payload / name)
    compiler_runtime = {}
    for name in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
        compiler_runtime[name] = digest(Path('/opt/toolchain/mingw64/bin') / name)
        shutil.copy2(Path('/opt/toolchain/mingw64/bin') / name, payload / 'libs' / name)
    metrics = {'elapsed_seconds': time.monotonic() - started,
               'work_bytes': int(subprocess.check_output(['du', '-sb', '/work'], text=True).split()[0]),
               'peak_memory_bytes': int(Path('/sys/fs/cgroup/memory.peak').read_text()) if Path('/sys/fs/cgroup/memory.peak').is_file() else None}
    print('BUILD MEASUREMENTS: ' + json.dumps(metrics), flush=True)
    (payload / 'build-identity.json').write_text(json.dumps({'source_fingerprint': fingerprint, 'seconds': time.monotonic() - started, 'dependencies': lock}, indent=2))
    package(source, payload, dict(source_metadata(Path('/supplied'), fingerprint), **updates, dependencies=lock,
                                  measurements=metrics, configuration='Release/full/WinLibs15.2/Wine9'), compiler_runtime)
    print('WINDOWS ENGINE BUILT: ' + str(len(list((payload / 'libs').glob('*.prx')))) + ' patched libraries', flush=True)


if __name__ == '__main__':
    build_engine()
