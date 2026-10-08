import ctypes
import os
import struct
import subprocess
import sys
import tempfile
from ctypes import wintypes
from pathlib import Path

from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol, needed_libraries


def dynamic(image, offset, phoff, tags):
    for index, tag in enumerate(tags + [(0, 0)]):
        struct.pack_into('<qQ', image, offset + index * 16, *tag)
    struct.pack_into('<QQ', image, phoff + 32, (len(tags) + 1) * 16, (len(tags) + 1) * 16)


def provider(value, soname=None):
    image = module_with_symbol(True)
    code = b'\xb8' + struct.pack('<I', value) + b'\xc3'
    image[0x400:0x406] = code
    struct.pack_into('<Q', image, 0x898 + 16, len(code))
    if soname:
        strings = b'\0shared#A#B\0' + soname.encode() + b'\0'
        image[0x800:0x800 + len(strings)] = strings
        tags = [(5, 0x2200), (10, len(strings)), (6, 0x2280), (11, 24),
                (4, 0x2240), (7, 0x2300), (8, 0), (9, 24), (14, 12)]
        dynamic(image, 0x600, 176, tags)
    return image


def check_internal_guest_libc(convert, work, relinker):
    result, dummy = convert('empty-native-provider', 'c.prx')
    assert result.returncode == 0, result.stderr
    result, shared = convert('shared-native-provider', 'a.prx')
    assert result.returncode == 0, result.stderr
    empty_provider = dummy.parent / 'app0' / 'prx' / 'c.prx.guest.prx'
    shared_provider = shared.parent / 'app0' / 'prx' / 'a.prx.guest.prx'
    for name, native_owner, symbol, expected in (
            ('guest-only', None, 'shared#A#B', 22),
            ('internal-first', 'libSceLibcInternal.prx', 'shared#A#B', 11),
            ('native-libc-first', 'libc.prx', 'shared#A#B', 11),
            ('unresolved', None, 'absent#A#B', None)):
        case = work / ('internal-guest-libc-' + name)
        (case / 'sce_module').mkdir(parents=True)
        (case / 'sce_module' / 'libc.prx').write_bytes(provider(22))
        source = case / 'input.elf'
        source.write_bytes(executable('libSceLibcInternal.prx', symbol, extra_dependencies=('libc.prx',)))
        output = case / 'output.exe'
        result = subprocess.run([str(relinker), '--windows', str(source), str(output)], capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stderr
        (case / 'libs').mkdir()
        for owner in ('libSceLibcInternal.prx', 'libc.prx'):
            selected = shared_provider if owner == native_owner else empty_provider
            (case / 'libs' / owner).write_bytes(selected.read_bytes())
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            if expected is None:
                assert run.returncode != 0 and 'unresolved ELF import absent' in run.stderr, (run.returncode, run.stdout, run.stderr)
            else:
                assert run.returncode == expected, (run.returncode, run.stdout, run.stderr)


def consumer(owner, symbol='shared#A#B', expected=22, module_name=None):
    image = module_with_symbol(False)
    strings = b'\0' + symbol.encode() + b'\0' + owner.encode() + b'\0'
    owner_offset = len(symbol) + 2
    module_offset = owner_offset
    if module_name is not None:
        module_offset = len(strings)
        strings += module_name.encode() + b'\0'
    image[0x800:0x800 + len(strings)] = strings
    tags = [(5, 0x2200), (10, len(strings)), (6, 0x2280), (11, 24),
            (4, 0x2240), (7, 0x2300), (8, 24), (9, 24), (12, 0x1010),
            (1, owner_offset), (0x61000045, (1 << 48) | module_offset)]
    dynamic(image, 0x600, 176, tags)
    code = b'\xff\x15' + struct.pack('<i', 0x2320 - 0x1016) + bytes([0x83, 0xf8, expected, 0x74, 2, 0x0f, 0x0b, 0xc3])
    image[0x410:0x410 + len(code)] = code
    return image


def executable(owner, symbol='shared#A#B', module_name=None, extra_dependencies=()):
    image = main_fixture()
    struct.pack_into('<I', image, 68, 7)
    strings = b'\0' + symbol.encode() + b'\0' + owner.encode() + b'\0'
    owner_offset = len(symbol) + 2
    module_offset = owner_offset
    if module_name is not None:
        module_offset = len(strings)
        strings += module_name.encode() + b'\0'
    dependency_offsets = []
    for dependency in extra_dependencies:
        dependency_offsets.append(len(strings))
        strings += dependency.encode() + b'\0'
    image[0x4800:0x4800 + len(strings)] = strings
    tags = [(5, 0x4800), (10, len(strings)), (6, 0x4880), (11, 24),
            (4, 0x4840), (7, 0x4900), (8, 24), (9, 24),
            (1, owner_offset), (0x61000045, (1 << 48) | module_offset)]
    tags.extend((1, offset) for offset in dependency_offsets)
    dynamic(image, 0x4600, 120, tags)
    struct.pack_into('<IIIII', image, 0x4840, 1, 2, 1, 0, 0)
    struct.pack_into('<IBBHQQ', image, 0x4898, 1, 0x12, 0, 0, 0, 0)
    struct.pack_into('<QQq', image, 0x4900, 0x4a20, (1 << 32) | 6, 0)
    image[0x4000:0x4007] = b'\xff\x15' + struct.pack('<i', 0x4a20 - 0x4006) + b'\xc3'
    return image


def shell_execute_exit_code(executable_path):
    see_mask_nocloseprocess = 0x00000040
    wait_object_0 = 0
    wait_timeout = 0x00000102

    class ShellExecuteInfo(ctypes.Structure):
        _fields_ = [
            ('cbSize', wintypes.DWORD),
            ('fMask', wintypes.DWORD),
            ('hwnd', wintypes.HWND),
            ('lpVerb', wintypes.LPCWSTR),
            ('lpFile', wintypes.LPCWSTR),
            ('lpParameters', wintypes.LPCWSTR),
            ('lpDirectory', wintypes.LPCWSTR),
            ('nShow', ctypes.c_int),
            ('hInstApp', wintypes.HANDLE),
            ('lpIDList', wintypes.LPVOID),
            ('lpClass', wintypes.LPCWSTR),
            ('hkeyClass', wintypes.HANDLE),
            ('dwHotKey', wintypes.DWORD),
            ('hIconOrMonitor', wintypes.HANDLE),
            ('hProcess', wintypes.HANDLE),
        ]

    shell32 = ctypes.WinDLL('shell32', use_last_error=True)
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    shell32.ShellExecuteExW.argtypes = [ctypes.POINTER(ShellExecuteInfo)]
    shell32.ShellExecuteExW.restype = wintypes.BOOL
    kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.WaitForSingleObject.restype = wintypes.DWORD
    kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel32.GetExitCodeProcess.restype = wintypes.BOOL
    kernel32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel32.TerminateProcess.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    info = ShellExecuteInfo()
    info.cbSize = ctypes.sizeof(info)
    info.fMask = see_mask_nocloseprocess
    info.lpVerb = 'open'
    info.lpFile = str(executable_path)
    info.nShow = 1
    if not shell32.ShellExecuteExW(ctypes.byref(info)):
        raise OSError(ctypes.get_last_error(), 'ShellExecuteExW failed')
    try:
        result = kernel32.WaitForSingleObject(info.hProcess, 30_000)
        if result == wait_timeout:
            kernel32.TerminateProcess(info.hProcess, 0xffffffff)
            kernel32.WaitForSingleObject(info.hProcess, 0xffffffff)
            raise TimeoutError('ShellExecuteExW child did not exit')
        if result != wait_object_0:
            raise OSError(f'WaitForSingleObject returned {result}')
        exit_code = wintypes.DWORD()
        if not kernel32.GetExitCodeProcess(info.hProcess, ctypes.byref(exit_code)):
            raise OSError(ctypes.get_last_error(), 'GetExitCodeProcess failed')
        return exit_code.value
    finally:
        kernel32.CloseHandle(info.hProcess)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-import-modules-') as directory:
        work = Path(directory)

        def convert(name, owner, symbol='shared#A#B', guest_owner=None, module_name=None, provider_name='b.prx', extra_dependencies=(), windows_gui=False):
            case = work / name
            modules = case / 'prx'
            modules.mkdir(parents=True)
            (modules / 'a.prx').write_bytes(provider(11))
            (modules / provider_name).write_bytes(provider(22, 'alias.prx'))
            other = provider(33)
            other[0x800:0x80c] = b'\0other#A#B\0\0'
            (modules / 'c.prx').write_bytes(other)
            if guest_owner:
                (modules / 'consumer.prx').write_bytes(consumer(guest_owner, symbol, module_name=module_name))
            for dependency in extra_dependencies:
                (modules / dependency).write_bytes(provider(44))
            source = case / 'input.elf'
            source.write_bytes(executable(owner, symbol, module_name, extra_dependencies))
            output = case / 'output.exe'
            arguments = [str(relinker), '--windows']
            if windows_gui:
                arguments.append('--windows-gui')
            arguments.extend([str(source), str(output)])
            result = subprocess.run(arguments, capture_output=True, text=True, timeout=30)
            return result, output

        for owner in ('a.prx', 'b.prx', 'alias.prx'):
            result, output = convert(owner, owner, guest_owner='b.prx')
            assert result.returncode == 0, (result.stdout, result.stderr)
            if os.name == 'nt':
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == (11 if owner == 'a.prx' else 22), (run.returncode, run.stdout, run.stderr)

        check_internal_guest_libc(convert, work, relinker)

        for filename, module_name in [('foo.native.prx', 'foo_native'),
                                      ('libSceFont-module.prx', 'libSceFont')]:
            result, output = convert(module_name, filename, guest_owner=filename,
                                     module_name=module_name, provider_name=filename)
            assert result.returncode == 0, (result.stdout, result.stderr)
            if os.name == 'nt':
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 22, (run.returncode, run.stdout, run.stderr)

        for index, filename in enumerate(('party.prx', 'party.PRX')):
            result, output = convert('case-' + str(index), 'Party.prx', guest_owner='Party.prx', provider_name=filename)
            assert result.returncode == 0, (result.stdout, result.stderr)
            if os.name == 'nt':
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 22, (run.returncode, run.stdout, run.stderr)

        case = work / 'exact-soname-before-filename-case'
        modules = case / 'prx'
        modules.mkdir(parents=True)
        (modules / 'party.prx').write_bytes(provider(11))
        (modules / 'other.prx').write_bytes(provider(22, 'Party.prx'))
        (modules / 'consumer.prx').write_bytes(consumer('Party.prx'))
        source = case / 'input.elf'
        source.write_bytes(executable('Party.prx'))
        output = case / 'output.exe'
        result = subprocess.run([str(relinker), '--windows', str(source), str(output)], capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode == 22, (run.returncode, run.stdout, run.stderr)

        result, output = convert('wrong-case-soname', 'ALIAS.prx')
        assert result.returncode == 0, result.stderr
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode != 0 and 'Failed to load module:' in run.stderr and 'ALIAS.prx' in run.stderr, (run.returncode, run.stderr)

        case = work / 'linux-filename-case'
        modules = case / 'prx'
        modules.mkdir(parents=True)
        (modules / 'party.prx').write_bytes(provider(22))
        source = case / 'input.elf'
        source.write_bytes(executable('Party.prx'))
        output = case / 'output.elf'
        result = subprocess.run([str(relinker), str(source), str(output)], capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert 'Party.prx' in needed_libraries(output.read_bytes())

        result, output = convert('ambiguous-alias', 'foo.native.prx', module_name='foo_native',
                                 provider_name='foo.native.prx', extra_dependencies=('foo.native-module.prx',))
        assert result.returncode == 2 and 'Ambiguous import module dependency' in result.stderr, result.stderr
        assert not output.exists()

        result, output = convert('exact-before-alias', 'foo.native.prx', module_name='foo.native',
                                 provider_name='foo.native.prx', extra_dependencies=('foo_native.prx',))
        assert result.returncode == 0, result.stderr
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode == 22, (run.returncode, run.stdout, run.stderr)

        result, output = convert('unmatched-module', 'libFoo2.prx', module_name='libFoo', provider_name='libFoo2.prx')
        assert result.returncode == 0, result.stderr
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode == 11, (run.returncode, run.stdout, run.stderr)

        for symbol, message in [('shared#A#C', 'Unknown import module ID'),
                                ('shared#A#', 'Invalid qualified import'),
                                ('shared#A#?', 'Invalid import module ID'),
                                ('shared#A#BBBBBBBB', 'Invalid import module ID')]:
            result, output = convert(symbol.replace('?', 'invalid'), 'b.prx', symbol)
            assert result.returncode == 2 and message in result.stderr, result.stderr
            assert not output.exists()

        result, output = convert('legacy', 'b.prx', 'shared', guest_owner='b.prx')
        assert result.returncode == 2 and 'Ambiguous guest import' in result.stderr, result.stderr
        assert not output.exists()

        if os.name == 'nt':
            for name, owner, symbol, expected in [
                    ('missing-symbol-gui', 'a.prx', 'absent#A#B', 0xc0000139),
                    ('missing-dependency-gui', 'missing.prx', 'shared#A#B', 0xc0000135)]:
                result, output = convert(name, owner, symbol, windows_gui=True)
                assert result.returncode == 0, result.stderr
                run_status = shell_execute_exit_code(output)
                assert run_status == expected, (name, hex(run_status), hex(expected))

        result, output = convert('missing-symbol', 'a.prx', 'absent#A#B')
        assert result.returncode == 0, result.stderr
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode != 0 and 'unresolved ELF import absent' in run.stderr, (run.returncode, run.stderr)
        result, output = convert('wrong-provider', 'c.prx')
        assert result.returncode == 0, result.stderr
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode != 0 and 'unresolved ELF import shared' in run.stderr, (run.returncode, run.stderr)
        result, output = convert('missing-dependency', 'missing.prx')
        assert result.returncode == 0, result.stderr
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode != 0 and 'Failed to load module:' in run.stderr and 'missing.prx' in run.stderr, (run.returncode, run.stderr)

    print('Windows module-scoped import tests passed')


if __name__ == '__main__':
    main()
