import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_release


def digest(path):
    with path.open('rb') as stream:
        result = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
        return result.hexdigest()


def package(args):
    build = args.build.resolve()
    libs = build / 'core/libs/libs'
    runtimes = package_release.windows_runtime(build)
    files = sorted(libs.glob('*.prx'))
    if not files:
        raise RuntimeError('No patched runtime libraries; build the libs target first')
    for name in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
        source = runtimes / name
        if not source.is_file():
            raise RuntimeError(f'Matching compiler runtime missing: {source}')
        existing = libs / name
        if existing.exists() and digest(existing) != digest(source):
            raise RuntimeError(f'Runtime DLL differs from the configured compiler: {existing}')
        files.append(source)
    files.extend(path for path in package_release.native_auxiliary_runtimes('windows', build) if path.exists())
    files.extend(libs.glob('*.dll'))
    unique = {}
    for path in files:
        if path.name in unique and digest(path) != digest(unique[path.name]):
            raise RuntimeError(f'Conflicting runtime: {path.name}')
        unique[path.name] = path
    required = sum(path.stat().st_size for path in unique.values()) + args.launcher.stat().st_size + args.converter.stat().st_size
    args.output.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(args.output).free < required:
        raise RuntimeError(f'Insufficient space: runtime package needs at least {required} bytes before game copies')
    destination = args.output / ('AnyPS5-' + datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8])
    if args.include_games:
        result = subprocess.run([str(args.launcher.resolve()), '--data-root', str(args.collection.resolve()), '--export', str(destination)], check=False)
        if result.returncode:
            raise RuntimeError(f'Collection export failed ({result.returncode}); close the launcher/games and inspect {destination}')
    else:
        destination.mkdir()
        (destination / 'Games').mkdir()
        (destination / 'portable.flag').write_text('AnyPS5 Launcher portable collection\n', encoding='utf-8')
    shutil.copy2(args.launcher, destination / 'AnyPS5Launcher.exe')
    (destination / 'data').mkdir(exist_ok=True)
    (destination / 'logs').mkdir(exist_ok=True)
    tools = destination / 'Tools'
    runtime = tools / 'Runtime'
    runtime.mkdir(parents=True)
    shutil.copy2(args.converter, tools / 'relinker.exe')
    for name, path in unique.items():
        shutil.copy2(path, runtime / name)
    version = subprocess.check_output([str(args.converter.resolve()), '--version'], text=True).strip()
    identity = {'converter': version, 'converterSHA256': digest(args.converter), 'launcherSHA256': digest(args.launcher), 'runtimeSHA256': {name: digest(path) for name, path in sorted(unique.items())}, 'collectionIncluded': args.include_games, 'gamesReconverted': False}
    (destination / 'runtime-identity.json').write_text(json.dumps(identity, indent=2) + '\n', encoding='utf-8')
    source = Path(__file__).resolve().parents[2]
    shutil.copy2(source / 'LICENSE', destination / 'LICENSE')
    shutil.copy2(Path(__file__).with_name('LICENSE'), destination / 'Launcher-LICENSE.txt')
    (destination / 'Getting-Started.txt').write_text('Start AnyPS5Launcher.exe. Add existing converted games or scan Games. Keep this whole folder when relocating. Game saves remain in their complete game folders. Tools/relinker.exe and Tools/Runtime belong to the recorded build; existing games keep their own runtime versions. No Python installation is required. Portable export does not rewrite existing scripts or arbitrary absolute environment paths: review them before moving. Close games before copying saves. Runtime identity and hashes are in runtime-identity.json. The package directory is fresh and no earlier collection was overwritten.\n', encoding='utf-8')
    print(destination, flush=True)
    return destination


def main():
    parser = argparse.ArgumentParser(description='Create a fresh native Windows launcher package without overwriting collections')
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--launcher', type=Path, required=True)
    parser.add_argument('--converter', type=Path, required=True)
    parser.add_argument('--collection', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--include-games', action='store_true')
    try:
        package(parser.parse_args())
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f'Launcher packaging failed: {error}. Incomplete new output is retained; existing collections are unchanged.', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
