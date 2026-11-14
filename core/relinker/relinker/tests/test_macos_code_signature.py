"""Check the ad-hoc code signature that `relinker --macos` writes into the executable and the guest module libraries.

The signature layout is checked on every platform: an LC_CODE_SIGNATURE at the end of __LINKEDIT whose SuperBlob holds
a SHA-256 CodeDirectory over every 4 KiB page, an empty requirements blob and an empty signature wrapper. On macOS,
`codesign` verifies both files, the executable runs under Rosetta without being signed again and the module loads with
dlopen."""

import hashlib
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import sys
import tempfile
import time

from test_guest_intel_trampolines import main_fixture
from test_macos_executable_exports import executable
from test_macos_guest_weak_exports import module
from test_macos_output import MachO

LC_CODE_SIGNATURE = 0x1D
CSMAGIC_EMBEDDED_SIGNATURE = 0xFADE0CC0
CSMAGIC_CODEDIRECTORY = 0xFADE0C02
CSMAGIC_REQUIREMENTS = 0xFADE0C01
CSMAGIC_BLOBWRAPPER = 0xFADE0B01
CS_SUPPORTSEXECSEG = 0x20400
CS_ADHOC = 0x2
CS_HASHTYPE_SHA256 = 2
CS_EXECSEG_MAIN_BINARY = 0x1
PAGE = 0x1000
RUN_TIMEOUT = 10


def commands(data):
    ncmds = struct.unpack_from('<I', data, 16)[0]
    offset = 32
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', data, offset)
        yield cmd, offset
        offset += size


def check_signature(name, data, identifier, main_binary):
    macho = MachO(data)
    found = [offset for cmd, offset in commands(data) if cmd == LC_CODE_SIGNATURE]
    assert len(found) == 1, f'{name}: {len(found)} LC_CODE_SIGNATURE commands'
    dataoff, datasize = struct.unpack_from('<II', data, found[0] + 8)
    linkedit = macho.segment('__LINKEDIT')
    assert dataoff % 16 == 0 and dataoff + datasize == len(data), (name, hex(dataoff), hex(datasize), hex(len(data)))
    assert linkedit['fileoff'] <= dataoff and linkedit['fileoff'] + linkedit['filesize'] == len(data), (name, linkedit)
    assert linkedit['vmsize'] >= linkedit['filesize'] and linkedit['vmsize'] % PAGE == 0, (name, linkedit)
    blob = data[dataoff:dataoff + datasize]
    magic, length, count = struct.unpack_from('>III', blob, 0)
    assert magic == CSMAGIC_EMBEDDED_SIGNATURE and length <= datasize and count == 3, (name, hex(magic), length, count)
    assert not any(blob[length:]), f'{name}: bytes after the SuperBlob are not zero'
    index = dict(struct.unpack_from('>II', blob, 12 + 8 * slot) for slot in range(count))
    assert set(index) == {0, 2, 0x10000}, (name, index)
    requirements = blob[index[2]:index[2] + 12]
    assert requirements == struct.pack('>III', CSMAGIC_REQUIREMENTS, 12, 0), (name, requirements.hex())
    assert blob[index[0x10000]:index[0x10000] + 8] == struct.pack('>II', CSMAGIC_BLOBWRAPPER, 8), name
    cd = index[0]
    (cd_magic, cd_length, version, flags, hash_offset, ident_offset, special, slots, code_limit, hash_size, hash_type,
     platform_id, page_shift, _, scatter, team, _, code_limit64, exec_base, exec_limit, exec_flags) = \
        struct.unpack_from('>IIIIIIIIIBBBBIIIIQQQQ', blob, cd)
    assert cd_magic == CSMAGIC_CODEDIRECTORY and version == CS_SUPPORTSEXECSEG and flags == CS_ADHOC, (name, hex(version), hex(flags))
    assert hash_size == 32 and hash_type == CS_HASHTYPE_SHA256 and page_shift == 12 and platform_id == 0, name
    assert scatter == team == code_limit64 == 0, name
    assert code_limit == dataoff and slots == (dataoff + PAGE - 1) // PAGE and special == 2, (name, code_limit, slots, special)
    assert cd_length == hash_offset + slots * 32 and index[2] == cd + cd_length, name
    text = macho.segment('__TEXT')
    assert (exec_base, exec_limit) == (text['fileoff'], text['filesize']), (name, exec_base, exec_limit)
    assert exec_flags == (CS_EXECSEG_MAIN_BINARY if main_binary else 0), (name, exec_flags)
    actual = blob[cd + ident_offset:blob.index(b'\0', cd + ident_offset)].decode()
    assert actual == identifier, (name, actual)
    hashes = blob[cd + hash_offset - 2 * 32:cd + hash_offset + slots * 32]
    assert hashes[:32] == hashlib.sha256(requirements).digest(), f'{name}: requirements slot hash'
    assert hashes[32:64] == bytes(32), f'{name}: Info.plist slot is not empty'
    for page in range(slots):
        expected = hashlib.sha256(data[page * PAGE:min((page + 1) * PAGE, code_limit)]).digest()
        assert hashes[64 + page * 32:96 + page * 32] == expected, f'{name}: page {page} hash'


def check_codesign(path):
    verify = subprocess.run(['codesign', '--verify', '--strict', '-v', str(path)], capture_output=True, text=True, timeout=60)
    assert verify.returncode == 0, (path.name, verify.stdout, verify.stderr)
    display = subprocess.run(['codesign', '-dvvv', str(path)], capture_output=True, text=True, timeout=60)
    assert display.returncode == 0, (path.name, display.stderr)
    for line in ('Signature=adhoc', 'CodeDirectory v=20400 ', 'Hash type=sha256 size=32', f'Identifier={path.name}\n'):
        assert line in display.stderr, (path.name, line, display.stderr)


def x86_command():
    return ['/usr/bin/arch', '-x86_64'] if platform.machine() == 'arm64' else []


def build_stub(work, name, text, library=True):
    source, output = work / (name + '.c'), work / name
    source.write_text(text)
    command = ['xcrun', 'clang', '-arch', 'x86_64', str(source), '-o', str(output)]
    if library:
        command += ['-dynamiclib', '-undefined', 'dynamic_lookup', '-Wl,-install_name,@rpath/' + name]
    subprocess.run(command, check=True, capture_output=True, timeout=30)
    return output


def run_native(work, output, converted):
    kernel = build_stub(work, 'libkernel.prx',
        '#include <unistd.h>\n'
        'extern int FixtureExport(void);\n'
        'extern int FixtureData;\n'
        'void Aps5StartGuest_nid_no_patch(void) { _exit(FixtureExport() + FixtureData - 5); }\n')
    libc = build_stub(work, 'libc.prx', 'int FixtureHostLibc(void) { return 0; }\n')
    (work / 'module-stubs').mkdir()
    module_kernel = build_stub(work / 'module-stubs', 'libkernel.prx', 'int FixtureKernel(void) { return 0; }\n')
    for libraries, stubs in ((output.parent / 'libs', (kernel, libc)), (converted.parent.parent.parent / 'libs', (module_kernel, libc))):
        libraries.mkdir()
        for stub in stubs:
            os.link(stub, libraries / stub.name)
    probe = build_stub(work, 'probe',
        '#include <dlfcn.h>\n#include <stdio.h>\n'
        'int main(int argc, char **argv) {\n'
        '    if (!dlopen(argv[1], RTLD_NOW)) { puts(dlerror()); return 1; }\n'
        '    return 0;\n}\n', library=False)
    output.chmod(0o755)
    start = time.monotonic()
    try:
        run = subprocess.run([*x86_command(), str(output)], capture_output=True, timeout=RUN_TIMEOUT)
    except subprocess.TimeoutExpired:
        raise AssertionError(f'the signed executable did not finish within {RUN_TIMEOUT} s of its first launch')
    elapsed = time.monotonic() - start
    if run.returncode != 42 and b'Bad CPU type in executable' in run.stderr:
        assert os.environ.get('GITHUB_ACTIONS') != 'true', 'macOS CI must execute the signed x86_64 executable'
        return
    assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
    print(f'macOS code signature: first launch of the unmodified output exited 42 after {elapsed:.2f} s')
    loaded = subprocess.run([*x86_command(), str(probe), str(converted)], capture_output=True, text=True, timeout=RUN_TIMEOUT)
    assert loaded.returncode == 0, (loaded.returncode, loaded.stdout, loaded.stderr)
    print('macOS code signature: dlopen loaded the signed guest module')


def check_resign(work, output):
    copy = work / 'resigned'
    shutil.copyfile(output, copy)
    run = subprocess.run(['codesign', '-s', '-', '-f', str(copy)], capture_output=True, text=True, timeout=60)
    assert run.returncode == 0, (run.stdout, run.stderr)
    verify = subprocess.run(['codesign', '--verify', '--strict', str(copy)], capture_output=True, text=True, timeout=60)
    assert verify.returncode == 0, (verify.stdout, verify.stderr)
    entryoff = MachO(output.read_bytes()).entryoff
    assert copy.read_bytes()[entryoff:entryoff + 64] == output.read_bytes()[entryoff:entryoff + 64], 'codesign -f moved the entry stub'


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-macos-code-signature-') as directory:
        work = Path(directory)
        source = work / 'exports.elf'
        source.write_bytes(executable())
        output = work / 'exports'
        run = subprocess.run([str(relinker), '--macos', '--skip-sce-module', str(source), str(output)], capture_output=True, text=True, timeout=60)
        assert run.returncode == 0, (run.stdout, run.stderr)
        check_signature('executable', output.read_bytes(), 'exports', True)

        case = work / 'modules'
        (case / 'sce_module').mkdir(parents=True)
        (case / 'sce_module' / 'sample.prx').write_bytes(module(1))
        (case / 'input.elf').write_bytes(main_fixture())
        run = subprocess.run([str(relinker), '--macos', str(case / 'input.elf'), str(case / 'title')], capture_output=True, text=True, timeout=60)
        assert run.returncode == 0, (run.stdout, run.stderr)
        converted = case / 'app0/sce_module/sample.prx.guest.prx'
        check_signature('module', converted.read_bytes(), 'sample.prx.guest.prx', False)
        check_signature('title', (case / 'title').read_bytes(), 'title', True)

        if sys.platform == 'darwin' and platform.machine() in ('arm64', 'x86_64'):
            for path in (output, converted, case / 'title'):
                check_codesign(path)
            run_native(work, output, converted)
            check_resign(work, output)
    print('macOS code signature tests passed')


if __name__ == '__main__':
    main()
