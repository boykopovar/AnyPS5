import os
import platform
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from test_guest_intel_trampolines import main_fixture
from test_guest_symbol_names import guest_module, string_table
from test_macos_output import MachO

STB_GLOBAL = 1
STB_WEAK = 2
OPTIONAL = 'AAAAAAAAAAA'
REQUIRED = 'BBBBBBBBBBB'
ENTRY = 'ProbeOptional'
HOST = 'libWeakHost.prx'


def macos_guest_module(binding):
    image = guest_module([(OPTIONAL, False), (REQUIRED, False), (ENTRY, True)])
    strings, offsets = string_table((OPTIONAL, REQUIRED, ENTRY, HOST))
    image[0x800:0x800 + len(strings)] = strings
    struct.pack_into('<Q', image, 0x618, len(strings))
    image[0x89c] = (binding << 4) | 2
    struct.pack_into('<Q', image, 0x668, 48)
    struct.pack_into('<QQqQQq', image, 0x900, 0x2320, (1 << 32) | 6, 0,
                     0x2328, (2 << 32) | 6, 0)
    struct.pack_into('<qQqQ', image, 0x680, 1, offsets[3], 0, 0)
    struct.pack_into('<QQ', image, 176 + 32, 10 * 16, 10 * 16)
    code = b'\x48\x8b\x05' + struct.pack('<i', 0x2320 - 0x1007)
    code += b'\x48\x85\xc0\x74\x02\xff\xe0\xff\x25' + struct.pack('<i', 0x2328 - 0x1014)
    image[0x400:0x400 + len(code)] = code
    return image


def macos_native_probe(work):
    if sys.platform != 'darwin':
        print('macOS guest weak imports: native execution skipped')
        return None
    if platform.machine() not in ('x86_64', 'arm64'):
        assert os.environ.get('GITHUB_ACTIONS') != 'true', 'macOS CI must execute the native x86_64 weak-import probe'
        print('macOS guest weak imports: native execution skipped')
        return None
    source, probe = work / 'probe.c', work / 'probe'
    source.write_text(
        '#include <dlfcn.h>\n'
        '#include <stdio.h>\n'
        '\n'
        'int main(int argc, char **argv) {\n'
        '    if (argc == 1) return 0;\n'
        '    void *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);\n'
        '    if (!module) { fprintf(stderr, "%s\\n", dlerror()); return 1; }\n'
        '    dlerror();\n'
        '    int (*entry)(void) = (int (*)(void))dlsym(module, argv[2]);\n'
        '    const char *error = dlerror();\n'
        '    if (error) { fprintf(stderr, "%s\\n", error); return 2; }\n'
        '    printf("%d\\n", entry());\n'
        '    return 0;\n'
        '}\n'
    )
    result = subprocess.run(['xcrun', 'clang', '-arch', 'x86_64', str(source), '-o', str(probe)],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, (result.stdout, result.stderr)
    command = (['/usr/bin/arch', '-x86_64'] if platform.machine() == 'arm64' else []) + [str(probe)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=10)
    if result.returncode != 0 and 'Bad CPU type in executable' in result.stderr:
        assert os.environ.get('GITHUB_ACTIONS') != 'true', 'macOS CI must execute the native x86_64 weak-import probe'
        print('macOS guest weak imports: x86_64 execution unavailable')
        return None
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
    kernel_source, kernel = work / 'kernel.c', work / 'libkernel.prx'
    kernel_source.write_text('int Anyps5TestKernel(void) { return 0; }\n')
    result = subprocess.run(['xcrun', 'clang', '-arch', 'x86_64', '-dynamiclib', str(kernel_source),
                             '-Wl,-install_name,@rpath/libkernel.prx', '-o', str(kernel)],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, (result.stdout, result.stderr)
    return command, kernel


def check_macos_guest_weak_imports(relinker, work):
    native = macos_native_probe(work)
    for name, binding, optional, required, expected, missing in (
            ('weak-missing', STB_WEAK, False, True, 42, None),
            ('weak-present', STB_WEAK, True, True, 77, None),
            ('required-missing', STB_WEAK, True, False, None, REQUIRED),
            ('global-missing', STB_GLOBAL, False, True, None, OPTIONAL)):
        case = work / ('macos-guest-' + name)
        modules, libraries = case / 'sce_module', case / 'libs'
        modules.mkdir(parents=True)
        libraries.mkdir()
        (modules / 'optional.prx').write_bytes(macos_guest_module(binding))
        source, output = case / 'input.elf', case / 'output'
        source.write_bytes(main_fixture())
        result = subprocess.run([str(relinker), '--macos', str(source), str(output)],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (name, result.stdout, result.stderr)
        converted = case / 'app0/sce_module/optional.prx.guest.prx'
        image = converted.read_bytes()
        macho = MachO(image)
        offset, size = macho.dyld_info[2:4]
        stream = image[offset:offset + size]
        for symbol, opcode in ((OPTIONAL, 0x41 if binding == STB_WEAK else 0x40), (REQUIRED, 0x40)):
            at = stream.find(('_' + symbol).encode() + b'\0')
            assert at > 0 and stream[at - 1] == opcode, (name, symbol, stream)
        assert '@rpath/' + HOST in macho.dylibs, (name, macho.dylibs)
        if native:
            command, kernel = native
            os.link(kernel, libraries / 'libkernel.prx')
            provider_source, host = case / 'provider.c', libraries / HOST
            provider_source.write_text((f'int {OPTIONAL}(void) {{ return 77; }}\n' if optional else '') +
                                       (f'int {REQUIRED}(void) {{ return 42; }}\n' if required else ''))
            result = subprocess.run(['xcrun', 'clang', '-arch', 'x86_64', '-dynamiclib', str(provider_source),
                                     '-Wl,-install_name,@rpath/' + HOST, '-o', str(host)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            run = subprocess.run([*command, str(converted), ENTRY + '#guest'],
                                 capture_output=True, text=True, timeout=10)
            if expected is None:
                assert run.returncode != 0 and missing in run.stderr, (name, run.returncode, run.stdout, run.stderr)
            else:
                assert run.returncode == 0 and run.stdout.strip() == str(expected), (name, run.returncode, run.stdout, run.stderr)
            if name == 'weak-missing':
                host.unlink()
                run = subprocess.run([*command, str(converted), ENTRY + '#guest'],
                                     capture_output=True, text=True, timeout=10)
                assert run.returncode != 0 and HOST in run.stderr, (run.returncode, run.stdout, run.stderr)
    if native:
        print('macOS guest weak imports: five native x86_64 loader controls passed')


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-macos-guest-weak-') as directory:
        check_macos_guest_weak_imports(relinker, Path(directory))
    print('macOS guest weak import tests passed')


if __name__ == '__main__':
    main()
