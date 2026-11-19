import argparse
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path


def smoke(bundle, output):
    if os.name != 'nt':
        raise RuntimeError('This check must execute on real Windows, not Wine')
    root = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(root / 'core/relinker/relinker/tests'))
    sys.path.insert(0, str(root / 'tools'))
    from guest_import_fixtures import executable
    from nid_names import compute_nid
    output.mkdir(parents=True, exist_ok=False)
    image = executable('libkernel.prx', compute_nid('sceKernelGetProcessTimeCounterFrequency') + '#A#B')
    code = bytes.fromhex('48 83 ec 08 ff 15') + struct.pack('<i', 0x4a20 - 0x400a)
    code += bytes.fromhex('48 83 c4 08 48 85 c0 74 06 b8 2a 00 00 00 c3 b8 2b 00 00 00 c3')
    image[0x4000:0x4000 + len(code)] = code
    source = output / 'synthetic.elf'
    source.write_bytes(image)
    relinker = bundle / 'relinker.exe'
    environment = {key: value for key, value in os.environ.items() if not key.startswith(('ANYPS5_', 'APS5_', 'VK_', 'SDL_'))}
    environment['PATH'] = os.path.join(os.environ['SystemRoot'], 'System32')
    environment['ANYPS5_LOG_FILE'] = str(output / 'smoke-runtime.log')
    for argument in ('--version', '--help'):
        result = subprocess.run([str(relinker), argument], env=environment, capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise RuntimeError(result.stderr)
        print(result.stdout[:400].strip())
    app = output / 'app.exe'
    subprocess.run([str(relinker), '--windows', '--skip-sce-module', str(source), str(app)], env=environment, check=True, timeout=60)
    shutil.copytree(bundle / 'libs', output / 'libs')
    (output / 'app0').mkdir(exist_ok=True)
    result = subprocess.run([str(app)], cwd=output, env=environment, capture_output=True, timeout=30)
    (output / 'smoke-output.log').write_bytes(result.stdout + result.stderr)
    if result.returncode != 42:
        raise RuntimeError('New-runtime synthetic executable returned ' + str(result.returncode) + ': ' + result.stderr.decode(errors='replace'))
    print('PASS: new Windows relinker converted a synthetic ELF; new libkernel/libc runtime executed the imported frequency call and returned 42 with a System32-only PATH')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    arguments = parser.parse_args()
    smoke(arguments.bundle.resolve(), arguments.output.resolve())
