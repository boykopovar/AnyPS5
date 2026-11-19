import json
import os
from pathlib import Path
import struct
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'relinker/relinker/tests'))
from test_windows_import_modules import executable


def main():
    relinker = Path(sys.argv[1]).resolve()
    runtime = Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-main-stack-') as directory:
        work = Path(directory)
        for name in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
            candidates = [Path(item) / name for item in os.environ['PATH'].split(os.pathsep)]
            dll = next((item for item in candidates if item.is_file()), None)
            assert dll is not None, f'Missing compiler runtime {name}'
            shutil.copyfile(dll, work / name)
        source = work / 'input.elf'
        image = executable('libc.prx', 'GuestArenaAvailable_nid_postfix')
        code = bytearray.fromhex('53 48 89 fb 48 83 ec 20')
        code += b'\xff\x15' + struct.pack('<i', 0x4a20 - (0x4000 + len(code) + 6))
        branches = []
        for check, opcode in (
                ('84 c0', 0x74),
                ('48 89 e0 48 c1 e8 1e 48 85 c0', 0x74),
                ('48 83 3b 02', 0x75),
                ('48 8b 43 10 81 38 73 74 61 63', 0x75),
                ('66 81 78 04 6b 00', 0x75)):
            code += bytes.fromhex(check) + bytes([opcode, 0])
            branches.append(len(code) - 1)
        code += bytes.fromhex('b8 2a 00 00 00 eb 05')
        failure = len(code)
        code += bytes.fromhex('b8 59 00 00 00 48 83 c4 20 5b c3')
        for branch in branches:
            code[branch] = failure - branch - 1
        image[0x4000:0x4000 + len(code)] = code
        source.write_bytes(image)
        output = work / 'output.exe'
        converted = subprocess.run([str(relinker), '--skip-sce-module', '--windows', '--rpath', str(runtime), str(source), str(output)],
                                   capture_output=True, text=True, timeout=20)
        assert converted.returncode == 0, (converted.stdout, converted.stderr)
        run = subprocess.run([str(output), 'stack'], env={**os.environ, 'PATH': str(runtime) + os.pathsep + os.environ['PATH']},
                             capture_output=True, text=True, timeout=20)
        assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)

        image = executable('libc.prx', 'exit_nid_postfix')
        code = bytes.fromhex('48 83 ec 08 bf 2a 00 00 00 ff 15')
        code += struct.pack('<i', 0x4a20 - (0x4000 + len(code) + 4)) + bytes.fromhex('0f 0b')
        image[0x4000:0x4000 + len(code)] = code
        source.write_bytes(image)
        output = work / 'early-exit.exe'
        converted = subprocess.run([str(relinker), '--skip-sce-module', '--windows', '--rpath', str(runtime), str(source), str(output)],
                                   capture_output=True, text=True, timeout=20)
        assert converted.returncode == 0, (converted.stdout, converted.stderr)
        env = {**os.environ, 'PATH': str(runtime) + os.pathsep + os.environ['PATH'], 'APS5_REPORT': '1'}
        run = subprocess.run([str(output)], env=env, capture_output=True, text=True, timeout=20)
        assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
        report_path = work / 'anyps5-report.json'
        report = json.loads(report_path.read_text(encoding='utf-8'))['run']
        assert report['stage'] == 'starts' and report['framesPresented'] == 0, report
        milestones = report['milestones']
        assert set(milestones) == {'loaded', 'started'}, milestones
        assert 0 <= milestones['loaded'] <= milestones['started'] <= report['seconds'], report

        report_path.unlink()
        run = subprocess.run([str(output), '--aps5-report=off'], env=env, capture_output=True, text=True, timeout=20)
        assert run.returncode == 42 and not report_path.exists(), (run.returncode, run.stdout, run.stderr)
    print('Windows guest entry uses a nonzero stack region and retains its argument block')
    print('Windows guest exit before rendering reports only the two startup milestones')


if __name__ == '__main__':
    main()
