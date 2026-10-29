import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_symbol_names import guest_module
from test_macos_executable_exports import exports
from test_macos_output import MachO


def module(binding, tls=False):
    image = guest_module([('Sample', True), ('Data', True)] + ([('Tls', True)] if tls else []))
    image[0x400:0x406] = bytes.fromhex('b807000000c3')
    image[0x89C] = binding << 4 | 2
    struct.pack_into('<Q', image, 0x8A8, 6)
    image[0x8B4] = binding << 4 | 1
    struct.pack_into('<QQ', image, 0x8B8, 0x2380, 4)
    struct.pack_into('<I', image, 0x980, 11)
    if tls:
        image[0x8CC] = binding << 4 | 6
        struct.pack_into('<QQ', image, 0x8D0, 0, 8)
        struct.pack_into('<H', image, 56, 4)
        struct.pack_into('<IIQQQQQQ', image, 232, 7, 4, 0x990, 0x2390, 0x2390, 8, 8, 8)
    return image


def compile_c(work, name, text, library=True):
    source, output = work / (name + '.c'), work / name
    source.write_text(text)
    command = ['xcrun', 'clang', '-arch', 'x86_64', str(source), '-o', str(output)]
    if library:
        command += ['-dynamiclib', '-Wl,-install_name,@rpath/' + name]
    result = subprocess.run(command, capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    return output


def native_probe(work):
    if sys.platform != 'darwin' or platform.machine() not in ('arm64', 'x86_64'):
        return None
    probe = compile_c(work, 'probe',
        '#include <dlfcn.h>\n#include <stdio.h>\n'
        'int main(int argc, char **argv) {\n'
        '    if (argc == 1) return 0;\n'
        '    void *provider = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);\n'
        '    if (!provider) { puts(dlerror()); return 1; }\n'
        '    void *consumer = dlopen(argv[2], RTLD_NOW | RTLD_GLOBAL);\n'
        '    if (!consumer) { puts(dlerror()); return 2; }\n'
        '    int (*read)(void) = dlsym(consumer, "Read");\n'
        '    if (!read) return 3;\n'
        '    printf("%d\\n", read());\n'
        '    return 0;\n}\n', library=False)
    command = (['/usr/bin/arch', '-x86_64'] if platform.machine() == 'arm64' else []) + [str(probe)]
    run = subprocess.run(command, capture_output=True, text=True, timeout=10)
    if run.returncode and 'Bad CPU type in executable' in run.stderr:
        assert os.environ.get('GITHUB_ACTIONS') != 'true', 'macOS CI must execute the x86_64 weak-export probe'
        return None
    assert run.returncode == 0, (run.returncode, run.stdout, run.stderr)
    kernel = compile_c(work, 'libkernel.prx', 'int FixtureKernel(void) { return 0; }\n')
    consumer = compile_c(work, 'consumer.dylib',
        'int Sample(void) __asm__("_Sample#guest");\n'
        'int Data __asm__("_Data#guest");\n'
        '__attribute__((weak)) int Sample(void) { return 77; }\n'
        '__attribute__((weak)) int Data = 88;\n'
        'int Read(void) { return Sample() + Data; }\n')
    controls = {}
    for binding in (1, 2):
        attribute = '__attribute__((weak)) ' if binding == 2 else ''
        controls[binding] = compile_c(work, f'native-{binding}.dylib',
            'int Sample(void) __asm__("_Sample#guest");\n'
            'int Data __asm__("_Data#guest");\n' +
            attribute + 'int Sample(void) { return 7; }\n' +
            attribute + 'int Data = 11;\n')
    return command, kernel, consumer, controls


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-macos-weak-exports-') as directory:
        work = Path(directory)
        native = native_probe(work)
        for binding in (1, 2):
            for tls in (False, True):
                case = work / f'{binding}-{tls}'
                modules = case / 'sce_module'
                modules.mkdir(parents=True)
                (modules / 'sample.prx').write_bytes(module(binding, tls))
                source = case / 'input.elf'
                source.write_bytes(main_fixture())
                run = subprocess.run([str(relinker), '--macos', str(source), str(case / 'output')],
                                     capture_output=True, text=True, timeout=10)
                assert run.returncode == 0, (binding, tls, run.stdout, run.stderr)
                converted = case / 'app0/sce_module/sample.prx.guest.prx'
                data = converted.read_bytes()
                entries = exports(data)
                flags = 4 if binding == 2 else 0
                if native and not tls:
                    command, kernel, consumer, controls = native
                    libraries = case / 'libs'
                    libraries.mkdir()
                    shutil.copyfile(kernel, libraries / 'libkernel.prx')
                    results = []
                    for provider in (controls[binding], converted):
                        probe = subprocess.run([*command, str(provider), str(consumer)],
                                               capture_output=True, text=True, timeout=10)
                        assert probe.returncode == 0, (binding, probe.returncode, probe.stdout, probe.stderr)
                        results.append(probe.stdout.strip())
                    assert results[0] == results[1], (binding, results)
                    if binding == 2:
                        assert results[0] == '18', results
                assert set(entries) == {'_Sample#guest', '_Data#guest'} | ({'_Tls#guest'} if tls else set()), entries
                assert all(value[0] == flags for value in entries.values()), (binding, tls, entries)
                assert bool(MachO(data).flags & 0x8000) == (binding == 2)
        if native:
            print('macOS guest weak exports: x86_64 function/data coalescing matches native weak and strong controls')
    print('macOS guest weak export tests passed')


if __name__ == '__main__':
    main()
