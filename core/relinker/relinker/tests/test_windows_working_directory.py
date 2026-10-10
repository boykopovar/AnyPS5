"""A converted Windows executable makes its own directory the current directory before it loads libraries."""

import ctypes
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time

from test_windows_import_modules import executable, provider


class ProcessBasicInformation(ctypes.Structure):
    _fields_ = [
        ('ExitStatus', ctypes.c_void_p),
        ('PebBaseAddress', ctypes.c_void_p),
        ('AffinityMask', ctypes.c_void_p),
        ('BasePriority', ctypes.c_void_p),
        ('UniqueProcessId', ctypes.c_void_p),
        ('InheritedFromUniqueProcessId', ctypes.c_void_p),
    ]


def current_directory(pid):
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    ntdll = ctypes.WinDLL('ntdll')
    kernel32.OpenProcess.restype = ctypes.c_void_p
    kernel32.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
    kernel32.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                           ctypes.POINTER(ctypes.c_size_t)]
    kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
    ntdll.NtQueryInformationProcess.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32,
                                                ctypes.c_void_p]
    handle = kernel32.OpenProcess(0x1000 | 0x0010, 0, pid)
    if not handle:
        raise OSError(ctypes.get_last_error(), 'OpenProcess failed')
    try:
        def read(address, size):
            buffer = ctypes.create_string_buffer(size)
            done = ctypes.c_size_t()
            if not kernel32.ReadProcessMemory(handle, address, buffer, size, ctypes.byref(done)) or done.value != size:
                raise OSError(ctypes.get_last_error(), 'ReadProcessMemory failed')
            return buffer.raw

        information = ProcessBasicInformation()
        status = ntdll.NtQueryInformationProcess(handle, 0, ctypes.byref(information), ctypes.sizeof(information), None)
        assert status == 0, f'NtQueryInformationProcess returned {status:#x}'
        parameters = struct.unpack('<Q', read(information.PebBaseAddress + 0x20, 8))[0]
        length = struct.unpack('<H', read(parameters + 0x38, 2))[0]
        text = struct.unpack('<Q', read(parameters + 0x40, 8))[0]
        return Path(read(text, length).decode('utf-16-le'))
    finally:
        kernel32.CloseHandle(handle)


def spinning_provider():
    image = provider(0)
    image[0x400:0x402] = b'\xeb\xfe'
    struct.pack_into('<Q', image, 0x898 + 16, 2)
    return image


def check(relinker, work, name, extra, renamed=None):
    case = work / name / 'app'
    (case / 'prx').mkdir(parents=True)
    (case / 'prx' / 'a.prx').write_bytes(spinning_provider())
    source = case / 'input.elf'
    source.write_bytes(executable('a.prx'))
    output = case / 'output.exe'
    result = subprocess.run([str(relinker), '--windows', *extra, str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    if os.name != 'nt':
        return
    if renamed is not None:
        (work / name).rename(work / renamed)
        name = renamed
        case = work / name / 'app'
        output = case / 'output.exe'
    elsewhere = work / name / 'elsewhere'
    elsewhere.mkdir()
    process = subprocess.Popen([str(output)], cwd=elsewhere, stdin=subprocess.DEVNULL,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        seen = None
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            assert process.poll() is None, f'{name}: exited with {process.returncode:#x} before reaching the guest'
            try:
                seen = current_directory(process.pid)
            except OSError:
                time.sleep(0.05)
                continue
            if seen.resolve() == case.resolve():
                return
            time.sleep(0.05)
        raise AssertionError(f'{name}: current directory is {seen}, expected {case}')
    finally:
        process.kill()
        process.wait(timeout=20)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-working-directory-') as directory:
        work = Path(directory)
        check(relinker, work, 'console', [])
        check(relinker, work, 'gui', ['--windows-gui'])
        check(relinker, work, 'unicode', [], 'unicode ü日')
    print('Windows working directory integration tests passed')


if __name__ == '__main__':
    main()
