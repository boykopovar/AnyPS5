from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import dynamic
from test_guest_deferred_dependencies import page_aligned
from test_guest_intel_trampolines import main_fixture
from test_guest_module_args import args_module, load_and_record
from test_guest_module_directories import module_with_symbol, needed_libraries
from test_guest_module_dir import pe_export_names
from test_guest_needed_modules import module_with_needed
from test_guest_symbol_names import dynamic_tags
from test_macos_output import MachO


def consumer(kind):
    image = module_with_symbol(False)
    tags = dynamic_tags(image)
    if kind in ('unreferenced', 'weak-unreferenced', 'needed'):
        tags[8] = 0
    elif kind == 'noop':
        struct.pack_into('<Q', image, 0x908, 1 << 32)
    if kind.startswith('weak-'):
        image[0x89c] = 0x22
    if kind == 'plt':
        tags[23], tags[2], tags[20] = tags[7], tags[8], 7
        for tag in (7, 8, 9):
            del tags[tag]
        struct.pack_into('<Q', image, 0x908, (1 << 32) | 7)
    if kind == 'needed':
        offset = tags[10]
        name = b'provider.prx\0'
        image[0x800 + offset:0x800 + offset + len(name)] = name
        tags[10] += len(name)
        tags[1] = offset
    dynamic(image, 0x600, 176, list(tags.items()))
    return page_aligned(image)


def main():
    relinker = Path(sys.argv[1]).resolve()
    host = Path(sys.argv[2]).resolve()
    cases = 0
    with tempfile.TemporaryDirectory(prefix='anyps5-unused-import-dependencies-') as directory:
        for system in ('linux', 'macos', 'windows'):
            flags = {'linux': [], 'macos': ['--macos'], 'windows': ['--windows']}[system]
            for kind in ('unreferenced', 'weak-unreferenced', 'noop', 'referenced', 'weak-referenced', 'plt', 'needed', 'missing'):
                case = Path(directory) / f'{system}-{kind}'
                bundled = case / 'sce_module'
                plugins = case / 'Media' / 'Plugins'
                bundled.mkdir(parents=True)
                plugins.mkdir(parents=True)
                (bundled / 'consumer.prx').write_bytes(consumer(kind))
                if kind != 'missing':
                    (plugins / 'provider.prx').write_bytes(args_module())
                source = case / 'input.elf'
                image = main_fixture()
                code = bytes.fromhex('b83c000000bf2a0000000f05')
                image[0x4000:0x4000 + len(code)] = code
                source.write_bytes(image)
                output = case / {'linux': 'output.elf', 'macos': 'output.macho', 'windows': 'output.exe'}[system]
                result = subprocess.run([str(relinker), *flags,
                                         '--skip-syscall-check', '--module-dir', 'Media/Plugins',
                                         str(source), str(output)], capture_output=True, text=True, timeout=30)
                assert result.returncode == 0, (system, kind, result.stdout, result.stderr)
                provider = 'app0/Media/Plugins/provider.prx.guest.prx'
                artifact = case / 'app0/sce_module/consumer.prx.guest.prx'
                if system == 'linux':
                    root_needed = needed_libraries(output.read_bytes())
                    module_needed = needed_libraries(artifact.read_bytes())
                    assert (f'$ORIGIN/{provider}' in root_needed) == (kind in ('referenced', 'weak-referenced', 'plt', 'needed')), (kind, root_needed)
                    assert ('$ORIGIN/../Media/Plugins/provider.prx.guest.prx' in module_needed) == (kind in ('referenced', 'weak-referenced', 'plt', 'needed')), (kind, module_needed)
                elif system == 'macos':
                    root_needed = MachO(output.read_bytes()).dylibs
                    module_needed = MachO(artifact.read_bytes()).dylibs
                    assert (f'@executable_path/{provider}' in root_needed) == (kind in ('referenced', 'weak-referenced', 'plt', 'needed')), (kind, root_needed)
                    assert ('@loader_path/../Media/Plugins/provider.prx.guest.prx' in module_needed) == (kind in ('referenced', 'weak-referenced', 'plt', 'needed')), (kind, module_needed)
                else:
                    if kind != 'missing':
                        provider_exports = pe_export_names((case / provider).read_bytes())
                        assert ('__aps5_guest_initialize' in provider_exports) == (kind not in ('referenced', 'weak-referenced', 'plt', 'needed')), (kind, provider_exports)
                    assert (b'shared\0' in output.read_bytes()) == (kind in ('referenced', 'weak-referenced', 'plt', 'missing')), kind
                if system == 'linux' and sys.platform == 'linux' and os.uname().machine == 'x86_64':
                    libraries = case / 'libs'
                    libraries.mkdir()
                    shutil.copyfile(host, libraries / 'libc.prx')
                    shutil.copyfile(host, libraries / 'libkernel.prx')
                    events = case / 'events.txt'
                    output.chmod(0o755)
                    run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30,
                                         env={**os.environ, 'ANYPS5_GUEST_MODULE_ARGS': str(events)})
                    if kind == 'missing':
                        assert run.returncode != 0 and 'undefined symbol: shared' in run.stderr, (run.returncode, run.stdout, run.stderr)
                    else:
                        assert run.returncode == 42, (kind, run.returncode, run.stdout, run.stderr)
                        recorded = events.read_text() if events.exists() else ''
                        assert recorded.count('args=') == 0, (kind, recorded)
                        load_and_record(case, libraries / 'libc.prx', case / provider)
                cases += 1
            cycle = Path(directory) / f'{system}-cycle'
            (cycle / 'sce_module').mkdir(parents=True)
            for name, needed in (('first.prx', b'second.prx'), ('second.prx', b'first.prx')):
                (cycle / 'sce_module' / name).write_bytes(page_aligned(module_with_needed((needed,), (0,))))
            (cycle / 'input.elf').write_bytes(main_fixture())
            output = cycle / 'output'
            result = subprocess.run([str(relinker), *flags,
                                     str(cycle / 'input.elf'), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 2 and 'Cyclic guest initialization dependency' in result.stderr, result.stderr
            assert not output.exists()
            cases += 1
    print(f'{cases} unused guest import dependency cases passed')


if __name__ == '__main__':
    main()
