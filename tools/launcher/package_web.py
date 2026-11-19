import argparse
import hashlib
import json
import shutil
import time
from pathlib import Path


def package(repository, helper, output):
    repository, helper, output = Path(repository), Path(helper), Path(output)
    if not helper.is_file():
        raise ValueError('Build desktop_helper first')
    output.mkdir(parents=True, exist_ok=True)
    destination = output / ('AnyPS5-Phase1-' + time.strftime('%Y%m%d-%H%M%S'))
    destination.mkdir()
    files = ['Start-AnyPS5.cmd', 'Pair-Browser.cmd', 'compose.yaml', '.dockerignore', 'LICENSE',
             'tools/launcher/LICENSE', 'tools/launcher/Setup-WebHelper.ps1',
             'tools/launcher/Start-Web.ps1', 'tools/launcher/Pair-Browser.ps1',
             'tools/launcher/web/Dockerfile', 'tools/launcher/web/server.py',
             'tools/launcher/web/static/index.html', 'tools/launcher/web/static/app.css',
             'tools/launcher/web/static/app.js', 'docs/user/USAGE.md']
    for name in files:
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(repository / name, target)
    shutil.copy2(repository / 'tools/launcher/compose.prebuilt.yaml', destination / 'compose.yaml')
    for name in ('bundle.py', 'pe.py'):
        target = destination / 'tools/engine' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(repository / 'tools/engine' / name, target)
    (destination / 'helper').mkdir()
    shutil.copy2(helper, destination / 'helper/AnyPS5DesktopHelper.exe')
    (destination / 'Getting-Started.txt').write_text(
        'AnyPS5 Phase 1 - Windows local library\n\n'
        '1. Extract the entire bundle to a permanent writable folder.\n'
        '2. Install and start Docker Desktop with Linux containers.\n'
        '3. Double-click Start-AnyPS5.cmd. Allow Docker to build the app.\n'
        '4. Enter the desktop pairing code in the browser at http://127.0.0.1:8766.\n'
        '5. Add folder: paste a Windows Explorer path, scan, review, add, then Play.\n\n'
        'Already prepared Windows games need app0, matching libs, and an executable.\n'
        'Games are linked in place; the bundle contains no games or engine/runtime.\n'
        'Conversion, SELF extraction and graphical container execution are outside Phase 1.\n'
        'No CMake, compiler, Python or Node installation is needed to use this bundle.\n'
        'The first Docker build needs internet access to retrieve the pinned Python base image.\n\n'
        'Next time: run Start-AnyPS5.cmd, or docker compose up -d in this folder.\n'
        'Pair-Browser.cmd creates a new single-use two-minute code when needed.\n'
        'The helper starts at Windows sign-in. Keep the app folder in its original location.\n'
        'Only one primary helper/library is installed for the current Windows user.\n'
        'Uninstall helper: Start-AnyPS5.cmd -Uninstall. Stop app: docker compose down.\n'
        'Both preserve games, saves and the .local folder.\n\n'
        'Storage: .local/launcher (library, settings, session logs);\n'
        '.local/launcher-web (credentials, backend state, helper logs).\n'
        'Installed helper: %LOCALAPPDATA%/AnyPS5/DesktopHelper.\n'
        'See docs/user/USAGE.md for folder limits, diagnostics and Phase 2.\n', encoding='utf-8')
    identities = {str(path.relative_to(destination)).replace('\\', '/'): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in sorted(destination.rglob('*')) if path.is_file()}
    (destination / 'manifest.json').write_text(json.dumps({'format': 1, 'phase': 1, 'files': identities}, indent=2) + '\n')
    archive = shutil.make_archive(str(destination), 'zip', destination.parent, destination.name)
    print(archive)
    return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--helper', required=True)
    parser.add_argument('--output', default='.local/launcher-packages')
    args = parser.parse_args()
    package(Path(__file__).resolve().parents[2], args.helper, args.output)
