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
            for directory in ('prx/shipping', 'sce_module/nested', 'sce_modules/nested'):
                label = f'{windows}-{directory.replace("/", "-")}'
                for identity in (False, True):
                    filename = 'renamed.prx' if identity else 'libGuest.suprx'
                    relative = directory + '/' + filename
                    files = {relative: declared_provider(22, ('libGuest' if identity else 'other',)),
                             directory + '/unused.prx': unsupported,
                             directory + '/opaque.sprx': b'not an ELF',
                             directory + '/old.prx.guest.prx': b'\x7fELF'}
                    result, output = convert(f'{label}-{identity}', windows, 'libGuest.suprx', files)
                    success(result, output, relative, windows)

                    other = directory + '/deeper/' + ('duplicate.prx' if identity else filename)
                    files[other] = declared_provider(33, ('libGuest',))
                    result, output = convert(f'{label}-{identity}-ambiguous', windows, 'libGuest.suprx', files)
                    diagnostic = 'Ambiguous needed module identity' if identity else 'Ambiguous needed module:'
                    assert result.returncode == 2 and diagnostic in result.stderr, result.stderr
                    assert not output.exists()

                for soname in (False, True):
                    standard = directory.split('/')[0]
                    relative = standard + ('/physical.prx' if soname else '/libGuest.suprx')
                    files = {relative: declared_provider(22, ('other',), 'libGuest.suprx' if soname else None),
                             directory + '/renamed.prx': declared_provider(11, ('libGuest',))}
                    result, output = convert(f'{label}-precedence-{soname}', windows, 'libGuest.suprx', files)
                    success(result, output, relative, windows)

                files = {directory + '/kept.prx': declared_provider(22, ('libGuest',)),
                         directory + '/omitted.prx': declared_provider(11, ('libGuest',))}
                result, output = convert(f'{label}-excluded-identity', windows, 'libGuest.suprx', files, ('omitted.prx',))
                success(result, output, directory + '/kept.prx', windows)

                files = {directory + '/libGuest.prx': declared_provider(22, ('libGuest',)),
                         directory + '/libGuest.sprx': declared_provider(33, ('libGuest',))}
                result, output = convert(f'{label}-excluded-stem', windows, 'libGuest.debug_prx', files, ('libGuest.sprx',))
                success(result, output, directory + '/libGuest.prx', windows)

                result, output = convert(f'{label}-ambiguous-stem', windows, 'libGuest.debug_prx', files)
                assert result.returncode == 2 and 'Ambiguous needed module:' in result.stderr, result.stderr
                assert not output.exists()

                files = {directory + '/libGuest.prx': declared_provider(22, ('libGuest',))}
                result, output = convert(f'{label}-excluded-only-stem', windows, 'libGuest.debug_prx', files, ('libGuest.prx',))
                assert result.returncode == 0, result.stderr
                assert not list((output.parent / 'app0').rglob('*.guest.prx'))
                if not windows:
                    assert 'libGuest.debug_prx' in needed_libraries(output.read_bytes())

                files = {directory + '/libGuest.suprx': declared_provider(22, ('libGuest',))}
                result, output = convert(f'{label}-excluded-required', windows, 'libGuest.suprx', files, ('libGuest.suprx',))
                assert result.returncode == 0, result.stderr
                assert not list((output.parent / 'app0').rglob('*.guest.prx'))

                files = {directory + '/kept.prx': declared_provider(22, ('libGuest',)),
                         'unrelated/omitted.prx': declared_provider(11, ('other',))}
                for excluded in ('absent.prx', 'omitted.prx'):
                    result, output = convert(f'{label}-unknown-{excluded}', windows, 'libGuest.suprx', files, (excluded,))
                    assert result.returncode == 2 and 'Excluded guest module file not found' in result.stderr, result.stderr
                    assert not output.exists()

                generated = 'libGuest.prx.guest.prx'
                files = {directory + '/' + generated: declared_provider(22, ('libGuest',))}
                result, output = convert(f'{label}-generated-required', windows, generated, files)
                assert result.returncode == 0, result.stderr
                assert not list((output.parent / 'app0').rglob('*.guest.prx'))
                if not windows:
                    assert generated in needed_libraries(output.read_bytes())
                result, output = convert(f'{label}-generated-excluded', windows, generated, files, (generated,))
                assert result.returncode == 2 and 'Excluded guest module file not found' in result.stderr, result.stderr
                assert not output.exists()

        if os.name == 'nt':
            for windows in (True, False):
                for directory in ('PrX/shipping', 'Sce_Module/nested', 'Sce_Modules/nested'):
                    label = f'host-case-{windows}-{directory.replace("/", "-")}'
                    files = {directory + '/kept.prx': declared_provider(22, ('libGuest',)),
                             directory + '/omitted.prx': declared_provider(11, ('libGuest',))}
                    result, output = convert(label, windows, 'libGuest.suprx', files, ('omitted.prx',))
                    success(result, output, directory + '/kept.prx', windows)

                    files = {directory + '/libGuest.suprx': declared_provider(22, ('libGuest',))}
                    result, output = convert(label + '-required', windows, 'libGuest.suprx', files, ('libGuest.suprx',))
                    assert result.returncode == 0, result.stderr
                    assert not list((output.parent / 'app0').rglob('*.guest.prx'))

        for filename, identity in (('party.prx', 'Party'), ('libplayfabmultiplayer.prx', 'libPlayFabMultiplayer')):
            relative = 'prx/shipping/' + filename
            for windows in (True, False):
                result, output = convert(f'case-identity-{windows}-{filename}', windows, identity + '.prx',
                    {relative: declared_provider(22, (identity, 'libGuest'))})
                success(result, output, relative, windows)

        for suffix in ('.prx', '.sprx', '.suprx'):
            relative = 'sce_module/libGuest' + suffix
            result, output = convert('normal-exact' + suffix, False, 'libGuest' + suffix,
                                     {relative: declared_provider(22, ('libGuest',))})
            success(result, output, relative, False)

        result, output = convert('missing-dependency', False, 'missing.prx',
                                 {'sce_module/present.prx': declared_provider(22, ('present',))})
        assert result.returncode == 0, result.stderr
        artifacts = list((output.parent / 'app0').rglob('*.guest.prx'))
        assert artifacts == [output.parent / 'app0/sce_module/present.prx.guest.prx'], artifacts
        assert 'missing.prx' in needed_libraries(output.read_bytes())

        result, output = convert('duplicate-candidate-paths', False, 'libGuest.prx',
                                 {'sce_module/libGuest.prx': declared_provider(22, ('libGuest',)),
                                  'sce_module/duplicate/libGuest.prx': declared_provider(22, ('libGuest',))})
        success(result, output, 'sce_module/libGuest.prx', False)

        relative = 'prx/shipping/custom.debug_prx'
        result, output = convert('identity-debug-prx', False, 'libGuest.prx',
                                 {relative: declared_provider(22, ('libGuest',))})
        success(result, output, relative, False)

        for windows in (True, False):
            relative = 'prx/shipping/RENAMED.PRX'
            result, output = convert(f'identity-uppercase-{windows}', windows, 'libGuest.prx',
                                     {relative: declared_provider(22, ('libGuest',))})
            success(result, output, relative, windows)

        for windows in (True, False):
            relative = 'prx/shipping/module_blob.bin'
            result, output = convert(f'identity-nonstandard-ext-{windows}', windows, 'libGuest.prx',
                                     {relative: declared_provider(22, ('libGuest',))})
            success(result, output, relative, windows)

        bench_case = work / 'benchmark'
        bench_case.mkdir(parents=True)
        (bench_case / 'sce_module').mkdir()
        bench_assets = bench_case / 'assets'
        bench_assets.mkdir()
        for i in range(40):
            bench_sub = bench_assets / f'dir_{i // 10}'
            bench_sub.mkdir(exist_ok=True)
            (bench_sub / f'asset_{i}.dat').write_bytes(b'DATA')
            (bench_sub / f'extra_{i}.prx').write_bytes(declared_provider(100 + i, (f'libExtra_{i}',)))
        bench_target = bench_assets / 'dir_0' / 'renamed_target.prx'
        bench_target.write_bytes(declared_provider(200, ('libTarget',)))
        bench_source = bench_case / 'input.elf'
        bench_source.write_bytes(executable('libTarget.prx', module_name='libTarget'))
        bench_output = bench_case / 'output.elf'
        result = subprocess.run([str(relinker), str(bench_source), str(bench_output)], capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
        expected_guest = bench_case / 'app0' / 'assets' / 'dir_0' / 'renamed_target.prx.guest.prx'
        assert list((bench_case / 'app0').rglob('*.guest.prx')) == [expected_guest]
        assert needed_libraries(bench_output.read_bytes()) == ['$ORIGIN/app0/assets/dir_0/renamed_target.prx.guest.prx']
    print('Nested required guest module tests passed')


if __name__ == '__main__':
    main()
