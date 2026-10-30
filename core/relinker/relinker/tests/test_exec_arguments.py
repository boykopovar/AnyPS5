from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time

from test_windows_import_modules import executable


def fixture(symbol):
    image = executable('guest_exec_host.prx', symbol)
    struct.pack_into('<H', image, 16, 0xFE10)
    struct.pack_into('<H', image, 56, 10)
    for index in range(6, 10):
        struct.pack_into('<IIQQQQQQ', image, 64 + index * 56, 0x6FFFFF01, 0, 0, 0, 0, 0, 0, 1)
    struct.pack_into('<QQ', image, 64 + 32, 0x100, 0x100)
    struct.pack_into('<IIQQQQQQ', image, 176, 1, 6, 0x4400, 0x5400, 0x5400, 0xC00, 0xC00, 0x1000)
    struct.pack_into('<QQ', image, 120 + 16, 0x5600, 0x5600)
    for position in range(0x4600, 0x4800, 16):
        tag, value = struct.unpack_from('<qQ', image, position)
        if tag == 0:
            break
        if tag in (4, 5, 6, 7):
            struct.pack_into('<Q', image, position + 8, value + 0x1000)
    struct.pack_into('<Q', image, 0x4900, 0x5A20)
    image[0x4000:0x4100] = b'\x90' * 0x100
    image[0x4000:0x400B] = b'\x48\x83\xec\x08\xff\x15' + struct.pack('<i', 0x5A20 - 0x400A) + b'\xcc'
    return image


def remove_tree(path):
    for attempt in range(50):
        try:
            shutil.rmtree(path)
            return
        except PermissionError:
            if attempt == 49:
                raise
            time.sleep(0.2)


def main():
    relinker = Path(sys.argv[1]).resolve()
    host = Path(sys.argv[2]).resolve()
    dependencies = [Path(value).resolve() for value in sys.argv[3:-1]]
    toolchain = Path(sys.argv[-1]).resolve().parent
    windows = os.name == 'nt'
    if not windows and sys.platform != 'linux':
        print('Exec arguments: unsupported host skipped')
        return
    directory = tempfile.mkdtemp(prefix='anyps5-exec-')
    try:
        work = Path(directory) / 'title tree'
        title = work / 'title'
        output = work / 'native output'
        (title / 'picker').mkdir(parents=True)
        (title / 'sce_module').mkdir()
        output.mkdir()
        (title / 'eboot.bin').write_bytes(fixture('ExecFirst_nid_no_patch'))
        (title / 'picker' / 'second.bin').write_bytes(fixture('ExecSecond_nid_no_patch'))
        (title / 'picker' / 'noexec.bin').write_bytes(b'no')
        if not windows:
            (title / 'eboot.bin').chmod(0o755)
            (title / 'picker' / 'second.bin').chmod(0o755)
            (title / 'picker' / 'noexec.bin').chmod(0o644)
        app = output / ('app.exe' if windows else 'app.elf')
        command = [str(relinker), 'unused-filter=0', '--executable', 'picker/second.bin']
        if windows:
            command += ['--windows']
        run = subprocess.run(command + [str(title / 'eboot.bin'), str(app)], capture_output=True, timeout=120)
        assert run.returncode == 0, (run.stdout, run.stderr)
        second = output / ('picker.second.bin.exe' if windows else 'picker.second.bin.elf')
        assert second.is_file(), list(output.iterdir())
        shutil.copytree(title, output / 'app0')
        (output / 'app0' / 'working').mkdir()
        libraries = output / 'libs'
        libraries.mkdir()
        for library in [host, *dependencies]:
            shutil.copy2(library, libraries / library.name)
        if windows:
            for name in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
                shutil.copy2(toolchain / name, libraries / name)
        else:
            app.chmod(0o755)
            second.chmod(0o755)
        environment = {**os.environ, 'ANYPS5_LOADEXEC_TEST_ROOT': str(output),
                       'LD_LIBRARY_PATH': str(libraries), 'PATH': str(libraries) + os.pathsep + os.environ['PATH']}
        events_path = output / 'events.txt'
        (output / 'pids.txt').unlink(missing_ok=True)
        events_path.unlink(missing_ok=True)
        run = subprocess.run([str(app)], cwd=output, env=environment, capture_output=True, timeout=120)
        events = events_path.read_text() if events_path.exists() else ''
        assert events == 'first\nlaunch\nsecond\n', (run.returncode, events, run.stdout, run.stderr)
        assert run.returncode == (0 if windows else 37), (run.returncode, events, run.stdout, run.stderr)
        pids = (output / 'pids.txt').read_text().splitlines()
        assert len(pids) == 2 and len(set(pids)) == (2 if windows else 1), pids
        print('Exec arguments: first -> second; entry/getargv arguments, Unicode, exact environment, cwd, '
              'validation rejections and no exit handlers passed')
    finally:
        remove_tree(directory)


if __name__ == '__main__':
    main()
