from pathlib import Path
import os
import shutil
import subprocess
import sys

from guest_import_fixtures import fixture_directory
from test_guest_module_directories import needed_libraries
from test_guest_module_identity import declared_provider
from test_macos_output import MachO
from test_windows_executable_exports import exports, fixture
from test_windows_import_modules import consumer


MODULE = 'SyntheticMain'
EXPORTED = 'exported#A#B'
ABSENT = 'absent#A#B'
SYMBOLS = [('exported', 0x12, 0, 1, 0x4020, 6)]
EXECUTABLE_VALUE = 43
ENTRY_VALUE = 42
LIBRARY_VALUE = 44
LOAD_FAILURE = 'Failed to load module:'
NAME = ['--executable-module', MODULE]
RESOLVED = f'Needed module {MODULE} is resolved on the executable'
PROBED = (MODULE + '.dll').encode('utf-16-le')


def importer(symbol=EXPORTED, owner=MODULE, expected=EXECUTABLE_VALUE):
    return consumer(owner, symbol=symbol, expected=expected, module_name=MODULE)


def convert(relinker, case, options, modules, symbols=SYMBOLS):
    directory = case / 'sce_module'
    directory.mkdir(parents=True)
    for filename, image in modules.items():
        (directory / filename).write_bytes(image)
    source = case / 'input.elf'
    source.write_bytes(fixture(symbols))
    output = case / 'output.exe'
    result = subprocess.run([str(relinker), *options, str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (case.name, result.stdout, result.stderr)
    convert.last = result
    return output


def convert_error(relinker, case, options, modules, symbols=SYMBOLS):
    directory = case / 'sce_module'
    directory.mkdir(parents=True)
    for filename, image in modules.items():
        (directory / filename).write_bytes(image)
    source = case / 'input.elf'
    source.write_bytes(fixture(symbols))
    output = case / 'output.exe'
    result = subprocess.run([str(relinker), *options, str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode != 0 and not output.exists(), (case.name, result.stdout, result.stderr)
    assert f'--executable-module {MODULE}: the executable does not export' in result.stderr, (case.name, result.stderr)


def assert_resolved_message(case):
    assert RESOLVED in convert.last.stdout, (case, convert.last.stdout, convert.last.stderr)


def assert_no_resolved_message(case):
    assert RESOLVED not in convert.last.stdout, (case, convert.last.stdout)


def run(output):
    return subprocess.run([str(output)], capture_output=True, text=True, timeout=30)


def expect_entry(output):
    result = run(output)
    assert result.returncode == ENTRY_VALUE, (output, result.returncode, result.stdout, result.stderr)
    return result


def expect_load_failure(output, name):
    result = run(output)
    assert result.returncode != 0 and LOAD_FAILURE in result.stderr and name in result.stderr, \
        (output, result.returncode, result.stdout, result.stderr)
    assert 'unresolved ELF import' not in result.stderr, result.stderr


def check_windows(relinker, work):
    native = os.name == 'nt'
    output = convert(relinker, work / 'windows-resolved', ['--windows', *NAME], {'consumer.prx': importer()})
    assert_resolved_message('windows-resolved')
    assert 'exported' in exports(output.read_bytes())
    assert PROBED in output.read_bytes()
    if native:
        expect_entry(output)
        libraries = output.parent / 'libs'
        libraries.mkdir()
        expect_entry(output)
        (libraries / (MODULE + '.dll')).write_bytes(b'not a module')
        expect_load_failure(output, MODULE)

    output = convert(relinker, work / 'windows-diagnostics', ['--windows', '--windows-diagnostics', *NAME], {'consumer.prx': importer()})
    if native:
        result = expect_entry(output)
        assert 'the executable exports every symbol imported from it' in result.stderr, result.stderr

    output = convert(relinker, work / 'windows-unnamed', ['--windows'], {'consumer.prx': importer()})
    assert_no_resolved_message('windows-unnamed')
    assert PROBED not in output.read_bytes()
    if native:
        expect_load_failure(output, MODULE)

    output = convert(relinker, work / 'windows-absent', ['--windows'], {'consumer.prx': importer(ABSENT)})
    assert_no_resolved_message('windows-absent')
    if native:
        expect_load_failure(output, MODULE)
    convert_error(relinker, work / 'windows-absent-named', ['--windows', *NAME], {'consumer.prx': importer(ABSENT)})

    output = convert(relinker, work / 'windows-partial', ['--windows'],
                     {'first.prx': importer(), 'second.prx': importer(ABSENT)})
    if native:
        expect_load_failure(output, MODULE)
    convert_error(relinker, work / 'windows-partial-named', ['--windows', *NAME],
                  {'first.prx': importer(), 'second.prx': importer(ABSENT)})

    output = convert(relinker, work / 'windows-suffix', ['--windows'], {'consumer.prx': importer(owner=MODULE + '.prx')})
    if native:
        expect_load_failure(output, MODULE + '.prx')

    provider = convert(relinker, work / 'windows-provider', ['--windows'],
                       {'provider.prx': declared_provider(LIBRARY_VALUE, ('SyntheticProvider',), symbol='exported')}, [])
    output = convert(relinker, work / 'windows-library', ['--windows'], {'consumer.prx': importer(expected=LIBRARY_VALUE)})
    if native:
        result = run(output)
        assert result.returncode not in (0, ENTRY_VALUE), (result.returncode, result.stdout, result.stderr)
        libraries = output.parent / 'libs'
        libraries.mkdir()
        shutil.copyfile(provider.parent / 'app0' / 'sce_module' / 'provider.prx.guest.prx', libraries / (MODULE + '.dll'))
        expect_entry(output)


def check_linux(relinker, work):
    def needed(label, modules, child, options=()):
        output = convert(relinker, work / label, list(options), modules)
        guest = output.parent / 'app0' / 'sce_module' / child
        return needed_libraries(output.read_bytes()), needed_libraries(guest.read_bytes())

    executable, module = needed('linux-resolved', {'consumer.prx': importer()}, 'consumer.prx.guest.prx', NAME)
    assert_resolved_message('linux-resolved')
    assert MODULE not in executable and '$ORIGIN/app0/sce_module/consumer.prx.guest.prx' in executable, executable
    assert MODULE not in module, module

    executable, module = needed('linux-unnamed', {'consumer.prx': importer()}, 'consumer.prx.guest.prx')
    assert_no_resolved_message('linux-unnamed')
    assert MODULE in executable and MODULE in module, (executable, module)

    executable, module = needed('linux-partial', {'first.prx': importer(), 'second.prx': importer(ABSENT)}, 'first.prx.guest.prx')
    assert MODULE in executable and MODULE in module, (executable, module)
    convert_error(relinker, work / 'linux-partial-named', NAME, {'first.prx': importer(), 'second.prx': importer(ABSENT)})

    executable, module = needed('linux-suffix', {'consumer.prx': importer(owner=MODULE + '.prx')}, 'consumer.prx.guest.prx')
    assert MODULE + '.prx' in executable and MODULE + '.prx' in module, (executable, module)


def check_macos(relinker, work):
    dependency = '@rpath/' + MODULE
    output = convert(relinker, work / 'macos-resolved', ['--macos', *NAME], {'consumer.prx': importer()})
    assert_resolved_message('macos-resolved')
    assert dependency not in MachO(output.read_bytes()).dylibs
    output = convert(relinker, work / 'macos-unnamed', ['--macos'], {'consumer.prx': importer()})
    assert dependency in MachO(output.read_bytes()).dylibs
    output = convert(relinker, work / 'macos-absent', ['--macos'], {'consumer.prx': importer(ABSENT)})
    assert dependency in MachO(output.read_bytes()).dylibs


def main():
    relinker = Path(sys.argv[1]).resolve()
    with fixture_directory('anyps5-executable-module-') as directory:
        work = Path(directory)
        check_windows(relinker, work)
        check_linux(relinker, work)
        check_macos(relinker, work)
    print('Executable module import tests passed')


if __name__ == '__main__':
    main()
