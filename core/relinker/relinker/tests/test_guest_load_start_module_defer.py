from pathlib import Path
import subprocess
import sys
import tempfile

from guest_import_fixtures import executable
from test_guest_intel_trampolines import main_fixture
from test_guest_module_dir import pe_export_names
from test_guest_module_directories import module_with_symbol, needed_libraries
from test_guest_needed_modules import module_with_needed

LOAD_START_MODULE = 'wzvqT4UqKX8#A#B'
BUNDLED = 'bundled.prx'
DEPENDENCY = 'dependency.prx'
LINUX_ENTRY = f'$ORIGIN/app0/sce_module/{BUNDLED}.guest.prx'
LINUX_DEPENDENCY_ENTRY = f'$ORIGIN/app0/sce_module/{DEPENDENCY}.guest.prx'


def convert(relinker, case, system, kind):
    modules = case / 'sce_module'
    modules.mkdir(parents=True)
    dependency = None
    if kind == 'loader-needs-dependency':
        (modules / DEPENDENCY).write_bytes(module_with_symbol(True))
        (modules / BUNDLED).write_bytes(module_with_needed((DEPENDENCY.encode(),), (0,)))
        dependency = case / 'app0' / 'sce_module' / (DEPENDENCY + '.guest.prx')
    else:
        (modules / BUNDLED).write_bytes(module_with_symbol(True))
    if kind == 'plain':
        image = main_fixture()
    elif kind in ('loader', 'loader-needs-dependency'):
        image = executable('libkernel.prx', symbol=LOAD_START_MODULE)
    else:
        image = executable('libkernel.prx', symbol=LOAD_START_MODULE, extra_dependencies=(BUNDLED,))
    source = case / 'input.elf'
    source.write_bytes(image)
    output = case / ('output.exe' if system == 'windows' else 'output.elf')
    result = subprocess.run([str(relinker), '--skip-syscall-check', *(['--windows'] if system == 'windows' else []), str(source), str(output)],
                            capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, (system, kind, result.stdout, result.stderr)
    return output, case / 'app0' / 'sce_module' / (BUNDLED + '.guest.prx'), dependency


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-load-start-module-defer-') as directory:
        for system in ('linux', 'windows'):
            for kind, deferred in (('plain', False), ('loader', True), ('loader-needs-bundled', False), ('loader-needs-dependency', True)):
                output, module, dependency = convert(relinker, Path(directory) / system / kind, system, kind)
                if system == 'windows':
                    assert ('__aps5_guest_initialize' in pe_export_names(module.read_bytes())) == deferred, (system, kind)
                    if dependency is not None:
                        assert '__aps5_guest_initialize' not in pe_export_names(dependency.read_bytes()), (system, kind)
                else:
                    needed = needed_libraries(output.read_bytes())
                    assert (LINUX_ENTRY in needed) != deferred, (system, kind, needed)
                    if dependency is not None:
                        assert LINUX_DEPENDENCY_ENTRY in needed, (system, kind, needed)
    print('Bundled modules start at launch unless the executable loads modules itself and does not need them')


if __name__ == '__main__':
    main()
