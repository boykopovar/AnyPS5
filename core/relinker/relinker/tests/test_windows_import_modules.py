import ctypes
import os
import struct
import subprocess
import sys
import tempfile
import time
from ctypes import wintypes
from pathlib import Path

from guest_import_fixtures import dynamic, executable
from test_guest_module_directories import module_with_symbol, needed_libraries
from test_guest_tls import data_directory, guest_with_tls


def check_native_load_paths(output, libraries):
    pe = output.read_bytes()
    header = struct.unpack_from('<I', pe, 0x3c)[0]
    optional = header + 24
    section_table = optional + struct.unpack_from('<H', pe, header + 20)[0]
    sections = {}
    for index in range(struct.unpack_from('<H', pe, header + 6)[0]):
        offset = section_table + index * 40
        name = pe[offset:offset + 8].rstrip(b'\0')
        size, rva, raw_size, raw = struct.unpack_from('<IIII', pe, offset + 8)
        sections[name] = (rva, pe[raw:raw + min(size, raw_size)])

    def file_offset(rva):
        for index in range(struct.unpack_from('<H', pe, header + 6)[0]):
            offset = section_table + index * 40
            size, start, raw_size, raw = struct.unpack_from('<IIII', pe, offset + 8)
            if start <= rva < start + min(size, raw_size):
                return raw + rva - start
        raise AssertionError(f'RVA {rva:#x} has no file data')

    descriptor = file_offset(struct.unpack_from('<I', pe, optional + 112 + 8)[0])
    slots = {}
    while any(pe[descriptor:descriptor + 20]):
        lookup, _, _, _, iat = struct.unpack_from('<IIIII', pe, descriptor)
        index = 0
        while (name_rva := struct.unpack_from('<Q', pe, file_offset((lookup or iat) + index * 8))[0]):
            if not name_rva & (1 << 63):
                name_offset = file_offset(name_rva) + 2
                slots[pe[name_offset:pe.index(b'\0', name_offset)]] = iat + index * 8
            index += 1
        descriptor += 20
    load_library_iat = slots.get(b'LoadLibraryExW')
    assert load_library_iat is not None, 'LoadLibraryExW is not imported'
    attributes_iat = slots.get(b'GetFileAttributesW')

    data_rva, data = sections[b'.startup']
    code_rva, code = sections[b'.entry']
    path_loads = []
    for library in libraries:
        path = ('libs\\' + library + '\0').encode('utf-16le')
        path_rva = data_rva + data.index(path)
        references = [offset for offset in range(len(code) - 6)
                      if code[offset:offset + 3] == b'\x48\x8d\x35'
                      and code_rva + offset + 7 + struct.unpack_from('<i', code, offset + 3)[0] == path_rva]
        assert len(references) == 1, f'{library}: expected a loader path reference, found {len(references)}'
        path_loads.append((references[0], library))
    path_loads.sort()
    for index, (start, library) in enumerate(path_loads):
        end = path_loads[index + 1][0] if index + 1 < len(path_loads) else len(code)
        def calls_through(slot):
            return [offset for offset in range(start, end - 5)
                    if code[offset:offset + 2] == b'\xff\x15'
                    and code_rva + offset + 6 + struct.unpack_from('<i', code, offset + 2)[0] == slot]

        calls = calls_through(load_library_iat)
        assert calls, f'{library}: loader path does not reach a LoadLibraryExW call'
        assert code[calls[0] - 11:calls[0]] == b'\x48\x89\xf1\x31\xd2\x41\xb8\x00\x11\x00\x00', library
        probes = [offset for offset in calls_through(attributes_iat) if offset < calls[0]]
        assert len(probes) == 1, f'{library}: optional provider is not probed with GetFileAttributesW before LoadLibraryExW'
        probe = probes[0]
        assert code[probe - 3:probe] == b'\x48\x89\xf1' and code[probe + 6:probe + 11] == b'\x83\xf8\xff\x0f\x85', library
        assert probe + 15 + struct.unpack_from('<i', code, probe + 11)[0] == calls[0] - 11, \
            f'{library}: a present optional provider does not continue to LoadLibraryExW'


def imported_symbols_by_dll(path):
    pe = path.read_bytes()
    header = struct.unpack_from('<I', pe, 0x3c)[0]
    assert struct.unpack_from('<H', pe, header + 24)[0] == 0x20b, path
    optional = header + 24
    section_table = optional + struct.unpack_from('<H', pe, header + 20)[0]

    def file_offset(rva):
        for index in range(struct.unpack_from('<H', pe, header + 6)[0]):
            offset = section_table + index * 40
            size, start, raw_size, raw = struct.unpack_from('<IIII', pe, offset + 8)
            if start <= rva < start + min(size, raw_size):
                return raw + rva - start
        raise AssertionError(f'RVA {rva:#x} has no file data in {path}')

    import_rva = struct.unpack_from('<I', pe, optional + 112 + 8)[0]
    assert import_rva, f'{path} has no import directory'
    descriptor = file_offset(import_rva)
    imports = {}
    while any(pe[descriptor:descriptor + 20]):
        lookup, _, _, name_rva, iat = struct.unpack_from('<IIIII', pe, descriptor)
        name_offset = file_offset(name_rva)
        dll = pe[name_offset:pe.index(b'\0', name_offset)].decode('ascii').lower()
        symbols = set()
        index = 0
        while (symbol_rva := struct.unpack_from('<Q', pe, file_offset((lookup or iat) + index * 8))[0]):
            if not symbol_rva & (1 << 63):
                symbol_offset = file_offset(symbol_rva) + 2
                symbols.add(pe[symbol_offset:pe.index(b'\0', symbol_offset)].decode('ascii'))
            index += 1
        imports[dll] = symbols
        descriptor += 20
    return imports


def provider(value, soname=None, code=None):
    image = module_with_symbol(True)
    code = code or b'\xb8' + struct.pack('<I', value) + b'\xc3'
    image[0x400:0x400 + len(code)] = code
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
    zero = b'\x31\xc0\xc3'
    zero_xmm = b'\x66\x0f\xef\xc0\x31\xc0\xc3'
    for name, native_owner, symbol, code, expected in (
            ('guest-only', None, 'shared#A#B', None, 22),
            ('internal-first', 'libSceLibcInternal.prx', 'shared#A#B', None, 11),
            ('native-libc-first', 'libc.prx', 'shared#A#B', None, 11),
            ('unresolved', None, 'absent#A#B', None, None),
            ('guest-zero-return', None, 'shared#A#B', zero, None),
            ('guest-zero-return-xmm', None, 'shared#A#B', zero_xmm, None),
            ('internal-first-zero-return', 'libSceLibcInternal.prx', 'shared#A#B', zero, 11)):
        case = work / ('internal-guest-libc-' + name)
        (case / 'sce_module').mkdir(parents=True)
        (case / 'sce_module' / 'libc.prx').write_bytes(provider(22, code=code))
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
                assert run.returncode != 0 and 'unresolved ELF import ' + symbol.split('#')[0] in run.stderr, (run.returncode, run.stdout, run.stderr)
            else:
                assert run.returncode == expected, (run.returncode, run.stdout, run.stderr)


def consumer(owner, symbol='shared#A#B', expected=22, module_name=None, needed_module_tag=0x61000045):
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
            (1, owner_offset), (needed_module_tag, (1 << 48) | module_offset)]
    dynamic(image, 0x600, 176, tags)
    code = b'\xff\x15' + struct.pack('<i', 0x2320 - 0x1016) + bytes([0x83, 0xf8, expected, 0x74, 2, 0x0f, 0x0b, 0xc3])
    image[0x410:0x410 + len(code)] = code
    return image


def exit_code_without_standard_handles(executable_path):
    sem_failcriticalerrors = 0x0001
    sem_nogpfaulterrorbox = 0x0002
    startf_usestdhandles = 0x00000100
    wait_object_0 = 0
    wait_timeout = 0x00000102

    class StartupInfo(ctypes.Structure):
        _fields_ = [
            ('cb', wintypes.DWORD),
            ('lpReserved', wintypes.LPWSTR),
            ('lpDesktop', wintypes.LPWSTR),
            ('lpTitle', wintypes.LPWSTR),
            ('dwX', wintypes.DWORD),
            ('dwY', wintypes.DWORD),
            ('dwXSize', wintypes.DWORD),
            ('dwYSize', wintypes.DWORD),
            ('dwXCountChars', wintypes.DWORD),
            ('dwYCountChars', wintypes.DWORD),
            ('dwFillAttribute', wintypes.DWORD),
            ('dwFlags', wintypes.DWORD),
            ('wShowWindow', wintypes.WORD),
            ('cbReserved2', wintypes.WORD),
            ('lpReserved2', wintypes.LPVOID),
            ('hStdInput', wintypes.HANDLE),
            ('hStdOutput', wintypes.HANDLE),
            ('hStdError', wintypes.HANDLE),
        ]

    class ProcessInformation(ctypes.Structure):
        _fields_ = [
            ('hProcess', wintypes.HANDLE),
            ('hThread', wintypes.HANDLE),
            ('dwProcessId', wintypes.DWORD),
            ('dwThreadId', wintypes.DWORD),
        ]

    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.GetErrorMode.restype = wintypes.UINT
    kernel32.SetErrorMode.argtypes = [wintypes.UINT]
    kernel32.SetErrorMode.restype = wintypes.UINT
    kernel32.CreateProcessW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.LPVOID, wintypes.LPVOID, wintypes.BOOL,
                                        wintypes.DWORD, wintypes.LPVOID, wintypes.LPCWSTR, ctypes.POINTER(StartupInfo),
                                        ctypes.POINTER(ProcessInformation)]
    kernel32.CreateProcessW.restype = wintypes.BOOL
    kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.WaitForSingleObject.restype = wintypes.DWORD
    kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel32.GetExitCodeProcess.restype = wintypes.BOOL
    kernel32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel32.TerminateProcess.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    kernel32.SetErrorMode(kernel32.GetErrorMode() | sem_failcriticalerrors | sem_nogpfaulterrorbox)
    info = StartupInfo()
    info.cb = ctypes.sizeof(info)
    info.dwFlags = startf_usestdhandles
    process = ProcessInformation()
    command = ctypes.create_unicode_buffer(f'"{executable_path}"')
    if not kernel32.CreateProcessW(str(executable_path), command, None, None, False, 0, None, None,
                                   ctypes.byref(info), ctypes.byref(process)):
        raise OSError(ctypes.get_last_error(), 'CreateProcessW failed')
    try:
        result = kernel32.WaitForSingleObject(process.hProcess, 30_000)
        if result == wait_timeout:
            kernel32.TerminateProcess(process.hProcess, 0xffffffff)
            kernel32.WaitForSingleObject(process.hProcess, 0xffffffff)
            raise TimeoutError('GUI child did not exit')
        if result != wait_object_0:
            raise OSError(f'WaitForSingleObject returned {result}')
        exit_code = wintypes.DWORD()
        if not kernel32.GetExitCodeProcess(process.hProcess, ctypes.byref(exit_code)):
            raise OSError(ctypes.get_last_error(), 'GetExitCodeProcess failed')
        return exit_code.value
    finally:
        kernel32.CloseHandle(process.hThread)
        kernel32.CloseHandle(process.hProcess)


def exit_code_with_preloaded_runtime(executable_path, runtime_library):
    sem_failcriticalerrors = 0x0001
    sem_nogpfaulterrorbox = 0x0002
    startf_usestdhandles = 0x00000100
    create_suspended = 0x00000004
    mem_commit_and_reserve = 0x3000
    mem_release = 0x8000
    page_readwrite = 0x04
    wait_object_0 = 0
    wait_timeout = 0x00000102

    class StartupInfo(ctypes.Structure):
        _fields_ = [
            ('cb', wintypes.DWORD),
            ('lpReserved', wintypes.LPWSTR),
            ('lpDesktop', wintypes.LPWSTR),
            ('lpTitle', wintypes.LPWSTR),
            ('dwX', wintypes.DWORD),
            ('dwY', wintypes.DWORD),
            ('dwXSize', wintypes.DWORD),
            ('dwYSize', wintypes.DWORD),
            ('dwXCountChars', wintypes.DWORD),
            ('dwYCountChars', wintypes.DWORD),
            ('dwFillAttribute', wintypes.DWORD),
            ('dwFlags', wintypes.DWORD),
            ('wShowWindow', wintypes.WORD),
            ('cbReserved2', wintypes.WORD),
            ('lpReserved2', wintypes.LPVOID),
            ('hStdInput', wintypes.HANDLE),
            ('hStdOutput', wintypes.HANDLE),
            ('hStdError', wintypes.HANDLE),
        ]

    class ProcessInformation(ctypes.Structure):
        _fields_ = [
            ('hProcess', wintypes.HANDLE),
            ('hThread', wintypes.HANDLE),
            ('dwProcessId', wintypes.DWORD),
            ('dwThreadId', wintypes.DWORD),
        ]

    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.GetErrorMode.restype = wintypes.UINT
    kernel32.SetErrorMode.argtypes = [wintypes.UINT]
    kernel32.SetErrorMode.restype = wintypes.UINT
    kernel32.CreateProcessW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.LPVOID, wintypes.LPVOID, wintypes.BOOL,
                                        wintypes.DWORD, wintypes.LPVOID, wintypes.LPCWSTR, ctypes.POINTER(StartupInfo),
                                        ctypes.POINTER(ProcessInformation)]
    kernel32.CreateProcessW.restype = wintypes.BOOL
    kernel32.GetModuleHandleW.argtypes = [wintypes.LPCWSTR]
    kernel32.GetModuleHandleW.restype = wintypes.HMODULE
    kernel32.GetProcAddress.argtypes = [wintypes.HMODULE, ctypes.c_char_p]
    kernel32.GetProcAddress.restype = ctypes.c_void_p
    kernel32.VirtualAllocEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_size_t, wintypes.DWORD, wintypes.DWORD]
    kernel32.VirtualAllocEx.restype = ctypes.c_void_p
    kernel32.VirtualFreeEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_size_t, wintypes.DWORD]
    kernel32.VirtualFreeEx.restype = wintypes.BOOL
    kernel32.WriteProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t,
                                            ctypes.POINTER(ctypes.c_size_t)]
    kernel32.WriteProcessMemory.restype = wintypes.BOOL
    kernel32.CreateRemoteThread.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                                            ctypes.c_void_p, wintypes.DWORD, ctypes.c_void_p]
    kernel32.CreateRemoteThread.restype = wintypes.HANDLE
    kernel32.GetExitCodeThread.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel32.GetExitCodeThread.restype = wintypes.BOOL
    kernel32.ResumeThread.argtypes = [wintypes.HANDLE]
    kernel32.ResumeThread.restype = wintypes.DWORD
    kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.WaitForSingleObject.restype = wintypes.DWORD
    kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel32.GetExitCodeProcess.restype = wintypes.BOOL
    kernel32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel32.TerminateProcess.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    kernel32.SetErrorMode(kernel32.GetErrorMode() | sem_failcriticalerrors | sem_nogpfaulterrorbox)
    info = StartupInfo()
    info.cb = ctypes.sizeof(info)
    info.dwFlags = startf_usestdhandles
    process = ProcessInformation()
    command = ctypes.create_unicode_buffer(f'"{executable_path}"')
    if not kernel32.CreateProcessW(str(executable_path), command, None, None, False, create_suspended, None, None,
                                   ctypes.byref(info), ctypes.byref(process)):
        raise OSError(ctypes.get_last_error(), 'CreateProcessW failed')
    child_exited = False
    try:
        load_library = kernel32.GetProcAddress(kernel32.GetModuleHandleW('kernel32.dll'), b'LoadLibraryA')
        assert load_library, ctypes.get_last_error()
        name = runtime_library.encode('ascii') + b'\0'
        remote_name = kernel32.VirtualAllocEx(process.hProcess, None, len(name), mem_commit_and_reserve, page_readwrite)
        assert remote_name, ctypes.get_last_error()
        written = ctypes.c_size_t()
        assert kernel32.WriteProcessMemory(process.hProcess, remote_name, name, len(name), ctypes.byref(written))
        assert written.value == len(name), written.value
        loader = kernel32.CreateRemoteThread(process.hProcess, None, 0, load_library, remote_name, 0, None)
        assert loader, ctypes.get_last_error()
        try:
            if kernel32.WaitForSingleObject(loader, 30_000) != wait_object_0:
                raise TimeoutError(f'{runtime_library} did not load in the suspended child')
            loaded = wintypes.DWORD()
            assert kernel32.GetExitCodeThread(loader, ctypes.byref(loaded))
            assert loaded.value, (runtime_library, 'LoadLibraryA returned NULL in the suspended child')
        finally:
            kernel32.CloseHandle(loader)
        assert kernel32.VirtualFreeEx(process.hProcess, remote_name, 0, mem_release), ctypes.get_last_error()
        assert kernel32.ResumeThread(process.hThread) == 1
        result = kernel32.WaitForSingleObject(process.hProcess, 30_000)
        if result == wait_timeout:
            raise TimeoutError('GUI child did not exit')
        if result != wait_object_0:
            raise OSError(f'WaitForSingleObject returned {result}')
        child_exited = True
        exit_code = wintypes.DWORD()
        if not kernel32.GetExitCodeProcess(process.hProcess, ctypes.byref(exit_code)):
            raise OSError(ctypes.get_last_error(), 'GetExitCodeProcess failed')
        return exit_code.value
    finally:
        try:
            if not child_exited:
                terminated = kernel32.TerminateProcess(process.hProcess, 0xffffffff)
                if kernel32.WaitForSingleObject(process.hProcess, 10_000) != wait_object_0:
                    raise OSError(ctypes.get_last_error(),
                                  f'child {process.dwProcessId} survived cleanup'
                                  f' (TerminateProcess returned {terminated})')
        finally:
            kernel32.CloseHandle(process.hThread)
            kernel32.CloseHandle(process.hProcess)


def snapshot_children_named(snapshot, executable_name):
    error_no_more_files = 18

    class ProcessEntry(ctypes.Structure):
        _fields_ = [
            ('dwSize', wintypes.DWORD),
            ('cntUsage', wintypes.DWORD),
            ('th32ProcessID', wintypes.DWORD),
            ('th32DefaultHeapID', ctypes.c_size_t),
            ('th32ModuleID', wintypes.DWORD),
            ('cntThreads', wintypes.DWORD),
            ('th32ParentProcessID', wintypes.DWORD),
            ('pcPriClassBase', wintypes.LONG),
            ('dwFlags', wintypes.DWORD),
            ('szExeFile', wintypes.WCHAR * 260),
        ]

    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.Process32FirstW.argtypes = [ctypes.c_void_p, ctypes.POINTER(ProcessEntry)]
    kernel32.Process32FirstW.restype = wintypes.BOOL
    kernel32.Process32NextW.argtypes = [ctypes.c_void_p, ctypes.POINTER(ProcessEntry)]
    kernel32.Process32NextW.restype = wintypes.BOOL
    children = []
    entry = ProcessEntry()
    entry.dwSize = ctypes.sizeof(entry)
    found = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
    while found:
        if entry.th32ParentProcessID == os.getpid() and entry.szExeFile.lower() == executable_name.lower():
            children.append(entry.th32ProcessID)
        found = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    error = ctypes.get_last_error()
    if error != error_no_more_files:
        raise OSError(error, 'process snapshot enumeration failed')
    return children


def live_children_named(executable_name):
    th32cs_snapprocess = 0x00000002
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel32.CreateToolhelp32Snapshot.restype = ctypes.c_void_p
    kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
    kernel32.CloseHandle.restype = wintypes.BOOL
    snapshot = kernel32.CreateToolhelp32Snapshot(th32cs_snapprocess, 0)
    assert snapshot not in (None, ctypes.c_void_p(-1).value), ctypes.get_last_error()
    try:
        return snapshot_children_named(snapshot, executable_name)
    finally:
        kernel32.CloseHandle(snapshot)


def lingering_children_named(executable_name, timeout_seconds=10.0):
    deadline = time.monotonic() + timeout_seconds
    children = live_children_named(executable_name)
    while children and time.monotonic() < deadline:
        time.sleep(0.1)
        children = live_children_named(executable_name)
    return children


def run_with_standard_pipes(executable_path, capture_stdout, capture_stderr):
    startf_usestdhandles = 0x00000100
    handle_flag_inherit = 0x00000001
    wait_object_0 = 0
    wait_timeout = 0x00000102
    error_broken_pipe = 109

    class StartupInfo(ctypes.Structure):
        _fields_ = [
            ('cb', wintypes.DWORD),
            ('lpReserved', wintypes.LPWSTR),
            ('lpDesktop', wintypes.LPWSTR),
            ('lpTitle', wintypes.LPWSTR),
            ('dwX', wintypes.DWORD),
            ('dwY', wintypes.DWORD),
            ('dwXSize', wintypes.DWORD),
            ('dwYSize', wintypes.DWORD),
            ('dwXCountChars', wintypes.DWORD),
            ('dwYCountChars', wintypes.DWORD),
            ('dwFillAttribute', wintypes.DWORD),
            ('dwFlags', wintypes.DWORD),
            ('wShowWindow', wintypes.WORD),
            ('cbReserved2', wintypes.WORD),
            ('lpReserved2', wintypes.LPVOID),
            ('hStdInput', wintypes.HANDLE),
            ('hStdOutput', wintypes.HANDLE),
            ('hStdError', wintypes.HANDLE),
        ]

    class ProcessInformation(ctypes.Structure):
        _fields_ = [
            ('hProcess', wintypes.HANDLE),
            ('hThread', wintypes.HANDLE),
            ('dwProcessId', wintypes.DWORD),
            ('dwThreadId', wintypes.DWORD),
        ]

    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.CreatePipe.argtypes = [ctypes.POINTER(wintypes.HANDLE), ctypes.POINTER(wintypes.HANDLE),
                                   wintypes.LPVOID, wintypes.DWORD]
    kernel32.CreatePipe.restype = wintypes.BOOL
    kernel32.SetHandleInformation.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD]
    kernel32.SetHandleInformation.restype = wintypes.BOOL
    kernel32.CreateProcessW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.LPVOID, wintypes.LPVOID,
                                        wintypes.BOOL, wintypes.DWORD, wintypes.LPVOID, wintypes.LPCWSTR,
                                        ctypes.POINTER(StartupInfo), ctypes.POINTER(ProcessInformation)]
    kernel32.CreateProcessW.restype = wintypes.BOOL
    kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.WaitForSingleObject.restype = wintypes.DWORD
    kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel32.GetExitCodeProcess.restype = wintypes.BOOL
    kernel32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel32.TerminateProcess.restype = wintypes.BOOL
    kernel32.ReadFile.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
                                  ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID]
    kernel32.ReadFile.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    read_handles = {}
    write_handles = {}
    for stream, capture in (('stdout', capture_stdout), ('stderr', capture_stderr)):
        if not capture:
            continue
        read_handle = wintypes.HANDLE()
        write_handle = wintypes.HANDLE()
        if not kernel32.CreatePipe(ctypes.byref(read_handle), ctypes.byref(write_handle), None, 0):
            raise OSError(ctypes.get_last_error(), 'CreatePipe failed')
        if not kernel32.SetHandleInformation(read_handle, handle_flag_inherit, 0):
            kernel32.CloseHandle(read_handle)
            kernel32.CloseHandle(write_handle)
            raise OSError(ctypes.get_last_error(), 'SetHandleInformation failed')
        if not kernel32.SetHandleInformation(write_handle, handle_flag_inherit, handle_flag_inherit):
            kernel32.CloseHandle(read_handle)
            kernel32.CloseHandle(write_handle)
            raise OSError(ctypes.get_last_error(), 'SetHandleInformation failed')
        read_handles[stream] = read_handle
        write_handles[stream] = write_handle

    info = StartupInfo()
    info.cb = ctypes.sizeof(info)
    info.dwFlags = startf_usestdhandles
    info.hStdOutput = write_handles.get('stdout')
    info.hStdError = write_handles.get('stderr')
    process = ProcessInformation()
    command = ctypes.create_unicode_buffer(f'"{executable_path}"')
    if not kernel32.CreateProcessW(str(executable_path), command, None, None, bool(write_handles), 0, None, None,
                                   ctypes.byref(info), ctypes.byref(process)):
        for handle in [*read_handles.values(), *write_handles.values()]:
            kernel32.CloseHandle(handle)
        raise OSError(ctypes.get_last_error(), 'CreateProcessW failed')

    try:
        for handle in write_handles.values():
            kernel32.CloseHandle(handle)
        write_handles.clear()
        result = kernel32.WaitForSingleObject(process.hProcess, 30_000)
        if result == wait_timeout:
            kernel32.TerminateProcess(process.hProcess, 0xffffffff)
            kernel32.WaitForSingleObject(process.hProcess, 0xffffffff)
            raise TimeoutError('GUI child did not exit')
        if result != wait_object_0:
            raise OSError(f'WaitForSingleObject returned {result}')
        exit_code = wintypes.DWORD()
        if not kernel32.GetExitCodeProcess(process.hProcess, ctypes.byref(exit_code)):
            raise OSError(ctypes.get_last_error(), 'GetExitCodeProcess failed')

        output = {}
        for stream, handle in read_handles.items():
            chunks = []
            while True:
                buffer = ctypes.create_string_buffer(4096)
                count = wintypes.DWORD()
                if not kernel32.ReadFile(handle, buffer, len(buffer), ctypes.byref(count), None):
                    if ctypes.get_last_error() != error_broken_pipe:
                        raise OSError(ctypes.get_last_error(), f'ReadFile failed for {stream}')
                    break
                if count.value == 0:
                    break
                chunks.append(buffer.raw[:count.value])
            output[stream] = b''.join(chunks).decode(errors='replace')
        return exit_code.value, output.get('stdout', ''), output.get('stderr', '')
    finally:
        for handle in [*read_handles.values(), process.hThread, process.hProcess]:
            kernel32.CloseHandle(handle)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-import-modules-') as directory:
        work = Path(directory)

        def convert(name, owner, symbol='shared#A#B', guest_owner=None, module_name=None, provider_name='b.prx', extra_dependencies=(), windows_gui=False, extra_symbols=()):
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
            source.write_bytes(executable(owner, symbol, module_name, extra_dependencies, extra_symbols))
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

        result, output = convert('unicode-ü日', 'a.prx', guest_owner='b.prx')
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert output.is_file() and (output.parent / 'app0' / 'prx' / 'a.prx.guest.prx').is_file()

        result, native_output = convert('unscoped-native-provider', 'b.prx')
        assert result.returncode == 0, result.stderr
        case = work / 'unscoped-native-import'
        (case / 'prx').mkdir(parents=True)
        (case / 'libs').mkdir()
        (case / 'prx' / 'tls.prx').write_bytes(guest_with_tls(True))
        native_provider = native_output.parent / 'app0' / 'prx' / 'b.prx.guest.prx'
        (case / 'libs' / 'available.prx').write_bytes(native_provider.read_bytes())
        source = case / 'input.elf'
        source.write_bytes(executable('available.prx', 'shared', extra_dependencies=('unused.prx',)))
        output = case / 'output.exe'
        result = subprocess.run([str(relinker), '--windows', str(source), str(output)], capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stderr
        check_native_load_paths(output, ('available.prx', 'unused.prx'))
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode == 22 and 'unused.prx' not in run.stderr, (run.returncode, run.stdout, run.stderr)

        broken = (case / 'app0' / 'prx' / 'tls.prx.guest.prx').read_bytes()
        assert data_directory(broken, 1)[0] and broken.count(b'KERNEL32.dll\0') == 1
        broken = broken.replace(b'KERNEL32.dll\0', b'APS5MISS.dll\0')
        (case / 'libs' / 'unused.prx').write_bytes(broken)
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode != 0 and 'Failed to load module:' in run.stderr and 'unused.prx' in run.stderr and 'GetLastError: 126' in run.stderr, (run.returncode, run.stdout, run.stderr)
            assert 'nothing is imported from it' not in run.stderr, run.stderr

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
            try:
                snapshot_children_named(ctypes.c_void_p(-1).value, 'output.exe')
            except OSError as error:
                assert error.errno, error
            else:
                raise AssertionError('planted enumeration failure did not raise')

            for host_library in sys.argv[2:]:
                library = Path(host_library).resolve()
                runtime = library.stem.removeprefix('windows_stdio_host_')
                if runtime == library.stem:
                    runtime = 'default'
                if runtime == 'msvcrt':
                    imports = imported_symbols_by_dll(library)
                    assert {'__iob_func', 'fputs'} <= imports.get('msvcrt.dll', set()), (library, imports)
                    assert not any(name.startswith('api-ms-win-crt-') for name in imports), (library, imports)

                def convert_stdio_case(case_name, image=None, lazy_binding=False):
                    case = work / case_name
                    (case / 'prx').mkdir(parents=True)
                    libs = case / 'libs'
                    libs.mkdir()
                    source = case / 'input.elf'
                    source.write_bytes(image if image is not None else executable('a.prx', 'shared'))
                    executable_path = case / 'output.exe'
                    arguments = [str(relinker), '--windows', '--windows-gui']
                    if lazy_binding:
                        arguments.append('--lazy-binding')
                    arguments.extend([str(source), str(executable_path)])
                    result = subprocess.run(arguments, capture_output=True, text=True, timeout=30)
                    assert result.returncode == 0, result.stderr
                    (libs / 'a.prx').write_bytes(library.read_bytes())
                    return executable_path, executable_path.parent / 'anyps5.log'

                executable_path, log = convert_stdio_case(f'gui-dll-{runtime}-both-missing')
                run_status = exit_code_without_standard_handles(executable_path)
                assert run_status == 11, (runtime, hex(run_status))
                assert log.is_file(), (runtime, log)
                content = log.read_text(errors='replace')
                assert 'Windows DLL stdout probe' in content, (runtime, content)
                assert 'Windows DLL stderr probe' in content, (runtime, content)

                executable_path, log = convert_stdio_case(f'gui-dll-{runtime}-stdout-missing')
                run_status, stdout, stderr = run_with_standard_pipes(executable_path, False, True)
                assert run_status == 11, (runtime, hex(run_status))
                assert log.is_file(), (runtime, log)
                content = log.read_text(errors='replace')
                assert 'Windows DLL stdout probe' in content, (runtime, content)
                assert 'Windows DLL stderr probe' in stderr, (runtime, stderr)
                assert not stdout

                executable_path, log = convert_stdio_case(f'gui-dll-{runtime}-stderr-missing')
                run_status, stdout, stderr = run_with_standard_pipes(executable_path, True, False)
                assert run_status == 11, (runtime, hex(run_status))
                assert log.is_file(), (runtime, log)
                content = log.read_text(errors='replace')
                assert 'Windows DLL stdout probe' in stdout, (runtime, stdout)
                assert 'Windows DLL stderr probe' in content, (runtime, content)
                assert not stderr

                executable_path, log = convert_stdio_case(f'gui-dll-{runtime}-both-redirected')
                run_status, stdout, stderr = run_with_standard_pipes(executable_path, True, True)
                assert run_status == 11, (runtime, hex(run_status))
                assert 'Windows DLL stdout probe' in stdout, (runtime, stdout)
                assert 'Windows DLL stderr probe' in stderr, (runtime, stderr)
                assert not log.exists(), (runtime, log)

                preloaded_runtime = {'msvcrt': 'msvcrt.dll', 'ucrt': 'ucrtbase.dll'}.get(runtime)
                if preloaded_runtime:
                    executable_path, log = convert_stdio_case(f'gui-dll-{runtime}-preloaded-runtime')
                    run_status = exit_code_with_preloaded_runtime(executable_path, preloaded_runtime)
                    assert run_status == 11, (runtime, hex(run_status))
                    assert log.is_file(), (runtime, log)
                    content = log.read_text(errors='replace')
                    assert 'Windows DLL stdout probe' in content, (runtime, content)
                    assert 'Windows DLL stderr probe' in content, (runtime, content)

                    executable_path, log = convert_stdio_case(f'gui-dll-{runtime}-preloaded-runtime-loader-failure')
                    try:
                        exit_code_with_preloaded_runtime(executable_path, 'anyps5_planted_missing_runtime.dll')
                    except AssertionError as error:
                        assert 'LoadLibraryA returned NULL' in str(error), error
                    else:
                        raise AssertionError('planted loader failure did not raise')
                    assert not lingering_children_named(executable_path.name), (runtime, executable_path)
                    assert not log.exists(), (runtime, log)

                    image = executable('a.prx', 'shared', extra_symbols=('absent#A#B',))
                    image[0x4000:0x400d] = (b'\xff\x15' + struct.pack('<i', 0x4a20 - 0x4006)
                                            + b'\xff\x15' + struct.pack('<i', 0x4a28 - 0x400c) + b'\xc3')
                    executable_path, log = convert_stdio_case(f'gui-dll-{runtime}-preloaded-runtime-stub-after-crt',
                                                              image=image, lazy_binding=True)
                    log.write_text('stale previous run', encoding='utf-8')
                    run_status = exit_code_with_preloaded_runtime(executable_path, preloaded_runtime)
                    assert run_status == 0xc0000139, (runtime, hex(run_status))
                    assert log.is_file(), (runtime, log)
                    content = log.read_text(errors='replace')
                    assert 'stale previous run' not in content, (runtime, content)
                    assert 'Transferring control to ELF entry point' in content, (runtime, content)
                    assert 'Windows DLL stdout probe' in content, (runtime, content)
                    assert 'Windows DLL stderr probe' in content, (runtime, content)
                    assert 'unresolved ELF import absent' in content, (runtime, content)

            for name, owner, symbol, expected, message in [
                    ('missing-symbol-gui', 'a.prx', 'absent#A#B', 0xc0000139, 'unresolved ELF import absent'),
                    ('missing-dependency-gui', 'missing.prx', 'shared#A#B', 0xc0000135, 'Failed to load module')]:
                result, output = convert(name, owner, symbol, windows_gui=True)
                assert result.returncode == 0, result.stderr
                run_status = exit_code_without_standard_handles(output)
                assert run_status == expected, (name, hex(run_status), hex(expected))
                log = output.parent / 'anyps5.log'
                assert log.is_file() and message in log.read_text(errors='replace'), (name, log.exists())

            result, output = convert('gui-success-log', 'a.prx', guest_owner='b.prx', windows_gui=True)
            assert result.returncode == 0, result.stderr
            log = output.parent / 'anyps5.log'
            log.write_text('stale previous run', encoding='utf-8')
            run_status = exit_code_without_standard_handles(output)
            assert run_status == 11, hex(run_status)
            content = log.read_text(errors='replace')
            assert 'Transferring control to ELF entry point' in content, content
            assert 'stale previous run' not in content, content

            result, output = convert('gui-log-create-denied', 'a.prx', guest_owner='b.prx', windows_gui=True)
            assert result.returncode == 0, result.stderr
            log = output.parent / 'anyps5.log'
            log.mkdir()
            run_status = exit_code_without_standard_handles(output)
            assert run_status == 11, hex(run_status)
            assert log.is_dir()

        result, output = convert('missing-symbol', 'a.prx', 'absent#A#B')
        assert result.returncode == 0, result.stderr
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode != 0 and 'unresolved ELF import absent' in run.stderr, (run.returncode, run.stderr)
            assert not (output.parent / 'anyps5.log').exists()
        result, output = convert('missing-symbols', 'a.prx', 'absent#A#B', extra_symbols=('shared#A#B', 'missing#A#B'))
        assert result.returncode == 0, result.stderr
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode & 0xffffffff == 0xc0000139, (hex(run.returncode & 0xffffffff), run.stderr)
            lines = [line for line in run.stderr.splitlines() if line.startswith('FAIL: unresolved ELF import ')]
            assert lines == ['FAIL: unresolved ELF import absent from a.prx', 'FAIL: unresolved ELF import missing from a.prx'], run.stderr
            assert 'Transferring control to ELF entry point' not in run.stderr, run.stderr
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
            assert run.stderr.startswith('Failed to load module:'), run.stderr
            assert 'GetLastError: 126' in run.stderr and 'hint: the module or one of its dependent DLLs was not found' in run.stderr, run.stderr

        for index, (symbol, module_name) in enumerate([('shared#A#B', None), ('shared', None), ('shared#A#B', 'libSceJson')]):
            case = work / ('missing-unused-dependency-' + str(index))
            modules = case / 'prx'
            modules.mkdir(parents=True)
            (modules / 'a.prx').write_bytes(provider(11))
            source = case / 'input.elf'
            source.write_bytes(executable('a.prx', symbol, module_name, extra_dependencies=('unused.prx',)))
            output = case / 'output.exe'
            result = subprocess.run([str(relinker), '--windows', str(source), str(output)], capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (symbol, module_name, result.stdout, result.stderr)
            if os.name == 'nt':
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 11 and 'unused.prx' not in run.stderr, (symbol, module_name, run.returncode, run.stdout, run.stderr)

    print('Windows module-scoped import tests passed')


if __name__ == '__main__':
    main()
