from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_module_directories import needed_libraries
from test_guest_module_identity import declared_provider
from test_windows_import_modules import executable


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-needed-nested-') as directory:
        work = Path(directory)

        def convert(label, windows, request, files, excluded=()):
            case = work / label
            standard = next((p.split('/')[0] for p in files if p.split('/')[0].lower() in ('sce_module', 'sce_modules')), 'sce_module')
            (case / standard).mkdir(parents=True)
            for relative, image in files.items():
                target = case / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(image)
            source = case / 'input.elf'
            source.write_bytes(executable(request, module_name='libGuest'))
            output = case / ('output.exe' if windows else 'output.elf')
            options = [arg for name in excluded for arg in ('--exclude-sce-module', name)]
            result = subprocess.run([str(relinker), *(['--windows'] if windows else []), *options,
                                     '--rpath', '$ORIGIN/custom-hosts', str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            return result, output

        def success(result, output, relative, windows):
            assert result.returncode == 0, (result.stdout, result.stderr)
            expected = output.parent / 'app0' / (relative + '.guest.prx')
            artifacts = list((output.parent / 'app0').rglob('*.guest.prx'))
            assert artifacts == [expected], artifacts
            if windows and os.name == 'nt':
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 22, (run.returncode, run.stdout, run.stderr)
            elif not windows:
                actual = needed_libraries(output.read_bytes())
                expected = ['$ORIGIN/app0/' + relative + '.guest.prx']
                assert actual == expected, (actual, expected)

        unsupported = declared_provider(77, ('libUnused',))
        struct.pack_into('<qQ', unsupported, 0x600 + 5 * 16, 0x70000000, 0)
        for windows in (True, False):
            for directory in ('prx/shipping', 'sce_module/nested', 'sce_modules/nested', 'Media/Modules', 'old/games/app0/sce_module'):
                label = f'{windows}-{directory.replace("/", "-")}'
                for identity in (False, True):
                    filename = 'renamed.prx' if identity else 'libGuest.suprx'
                    files = {directory + '/' + filename: declared_provider(22, ('libGuest',)),
                             directory + '/deeper/' + filename: declared_provider(33, ('libGuest',)),
                             directory + '/unused.prx': unsupported,
                             directory + '/self.prx': bytes.fromhex('4f153d1d') + bytes(128),
                             directory + '/old.prx.guest.prx': b'\x7fELF'}
                    result, output = convert(f'{label}-{identity}', windows, 'libGuest.suprx', files)
                    assert result.returncode == 0, (result.stdout, result.stderr)
                    assert output.is_file()
                    assert not list((output.parent / 'app0').rglob('*.guest.prx'))
                    if not windows:
                        assert needed_libraries(output.read_bytes()) == ['libGuest.suprx']
                    result, output = convert(f'{label}-{identity}-excluded', windows, 'libGuest.suprx', files, (filename,))
                    assert result.returncode == 2 and 'Excluded guest module file not found' in result.stderr, result.stderr
                    assert not output.exists()

                standard = directory.split('/')[0] if directory.split('/')[0] in ('prx', 'sce_module', 'sce_modules') else 'sce_module'
                relative = standard + '/renamed.prx'
                files = {relative: declared_provider(22, ('libGuest',)),
                         directory + '/duplicate.prx': declared_provider(33, ('libGuest',)),
                         directory + '/libGuest.suprx': bytes.fromhex('4f153d1d') + bytes(128)}
                result, output = convert(f'{label}-direct', windows, 'libGuest.suprx', files)
                success(result, output, relative, windows)
                result, output = convert(f'{label}-direct-excluded', windows, 'libGuest.suprx', files, ('renamed.prx',))
                assert result.returncode == 0, result.stderr
                assert not list((output.parent / 'app0').rglob('*.guest.prx'))

    print('Nested required guest module tests passed')


if __name__ == '__main__':
    main()
