from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_windows_import_modules import consumer, dynamic, executable, provider
from test_guest_module_directories import needed_libraries


def declared_provider(value, identities, soname=None, tag=0x61000043, symbol='shared'):
    image = provider(value)
    strings = b'\0' + symbol.encode() + b'#A#B\0'
    tags = [(5, 0x2200), (10, 0), (6, 0x2280), (11, 24),
            (4, 0x2240), (7, 0x2300), (8, 0), (9, 24)]
    if soname:
        tags.append((14, len(strings)))
        strings += soname.encode() + b'\0'
    for index, identity in enumerate(identities, 1):
        tags.append((tag, (index << 48) | (0x101 << 32) | len(strings)))
        strings += identity.encode() + b'\0'
    assert len(strings) <= 0x40
    tags[1] = (10, len(strings))
    image[0x800:0x800 + len(strings)] = strings
    dynamic(image, 0x600, 176, tags)
    return image


def sce_strings(image):
    struct.pack_into('<H', image, 56, 4)
    struct.pack_into('<IIQQQQQQ', image, 232, 0x61000000, 4, 0x800, 0, 0, 0x40, 0x40, 8)
    struct.pack_into('<qQ', image, 0x600, 0x61000035, 0)
    struct.pack_into('<q', image, 0x610, 0x61000037)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-module-identity-') as directory:
        work = Path(directory)

        def convert(label, windows, request, module_name='libGuest', providers=None, guest_request=None, symbol='shared#A#B', module_directory='sce_module', extra=None):
            case = work / label
            modules = case / module_directory
            modules.mkdir(parents=True)
            (case / 'sce_module').mkdir(exist_ok=True)
            for filename, image in (providers or {'renamed.prx': declared_provider(22, ('libGuest',))}).items():
                (modules / filename).write_bytes(image)
            for filename, image in (extra or {}).items():
                target = case / filename
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(image)
            if guest_request:
                (case / 'sce_module/consumer.prx').write_bytes(consumer(guest_request, symbol=symbol, module_name=module_name))
            source = case / 'input.elf'
            source.write_bytes(executable(request, symbol=symbol, module_name=module_name))
            output = case / ('output.exe' if windows else 'output.elf')
            result = subprocess.run([str(relinker), *(['--windows'] if windows else []),
                                     '--rpath', '$ORIGIN/custom-hosts', str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            return result, output

        def run(output, expected=22, error=None):
            if os.name != 'nt':
                return
            result = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            if error:
                assert result.returncode != 0 and error in result.stderr, (result.returncode, result.stdout, result.stderr)
            else:
                assert result.returncode == expected, (result.returncode, result.stdout, result.stderr)

        for windows in (True, False):
            for directory in ('sce_module', 'prx'):
                result, output = convert(f'sprx-request-{windows}-{directory.replace("/", "-")}', windows,
                    'libGuest.sprx', module_directory=directory, guest_request='libGuest.sprx',
                    providers={'renamed.sprx': declared_provider(22, ('libGuest',))})
                assert result.returncode == 0, result.stderr
                artifact = output.parent / 'app0' / directory / 'renamed.sprx.guest.prx'
                assert artifact.is_file(), list((output.parent / 'app0').rglob('*'))
                assert len(list((output.parent / 'app0').rglob('*.guest.prx'))) == 2
                if windows:
                    run(output)
                else:
                    assert needed_libraries(output.read_bytes()) == [
                        '$ORIGIN/app0/' + directory + '/renamed.sprx.guest.prx',
                        '$ORIGIN/app0/sce_module/consumer.prx.guest.prx']

            for directory in ('.', 'Media/Modules'):
                for suffix in ('.prx', '.sprx', '.suprx'):
                    relative = Path(directory) / ('renamed' + suffix)
                    result, output = convert(f'outside-{windows}-{directory.replace("/", "-")}-{suffix}', windows,
                        'libGuest.suprx', module_directory=directory, guest_request='libGuest.suprx',
                        providers={relative.name: declared_provider(22, ('libGuest',))},
                        extra={'unused.prx': declared_provider(77, ('libUnused',)), 'opaque.sprx': b'not an ELF'})
                    assert result.returncode == 0, result.stderr
                    artifact = output.parent / 'app0' / (relative.as_posix() + '.guest.prx')
                    assert not artifact.exists(), artifact
                    assert len(list((output.parent / 'app0').rglob('*.guest.prx'))) == 1
                    if not windows:
                        assert 'libGuest.suprx' in needed_libraries(output.read_bytes())

            result, output = convert(f'outside-ambiguous-{windows}', windows, 'libGuest.suprx', module_directory='.',
                providers={'one.prx': declared_provider(22, ('libGuest',))},
                extra={'other/two.prx': declared_provider(33, ('libGuest',))})
            assert result.returncode == 0, result.stderr
            assert not list((output.parent / 'app0').rglob('*.guest.prx'))

            result, output = convert(f'sce-strings-{windows}', windows, 'libGuest.suprx', module_directory='sce_module',
                providers={'renamed.prx': sce_strings(declared_provider(22, ('libGuest',)))})
            assert result.returncode == 0, result.stderr
            if windows:
                run(output)

            for soname in (False, True):
                filename = 'physical.prx' if soname else 'libGuest.suprx'
                result, output = convert(f'known-before-outside-{windows}-{soname}', windows, 'libGuest.suprx',
                    providers={filename: declared_provider(22, ('unrelated',), 'libGuest.suprx' if soname else None)},
                    extra={'renamed.prx': declared_provider(11, ('libGuest',))})
                assert result.returncode == 0, result.stderr
                assert len(list((output.parent / 'app0').rglob('*.guest.prx'))) == 1
                if windows:
                    run(output)

            malformed = declared_provider(22, ('libGuest',))
            struct.pack_into('<Q', malformed, 0x600 + 8 * 16 + 8, (1 << 48) | 0xffffffff)
            result, output = convert(f'direct-bad-name-{windows}', windows, 'libGuest.suprx',
                module_directory='sce_module', providers={'renamed.prx': malformed})
            assert result.returncode == 2 and 'String offset out of bounds' in result.stderr, result.stderr
            assert not output.exists()

            malformed = sce_strings(declared_provider(22, ('libGuest',)))
            struct.pack_into('<Q', malformed, 0x618, 0x100)
            result, output = convert(f'direct-bad-sce-range-{windows}', windows, 'libGuest.suprx',
                module_directory='sce_module', providers={'renamed.prx': malformed})
            assert result.returncode == 2 and 'SCE table exceeds dynamic data segment' in result.stderr, result.stderr
            assert not output.exists()

            unrelated = declared_provider(77, ('libUnused',))
            struct.pack_into('<qQ', unrelated, 0x600 + 5 * 16, 0x70000000, 0)
            result, output = convert(f'ignored-unrelated-unsupported-body-{windows}', windows, 'libGuest.suprx',
                module_directory='sce_module', extra={'unused.prx': unrelated})
            assert result.returncode == 0, result.stderr
            assert len(list((output.parent / 'app0').rglob('*.guest.prx'))) == 1
            if windows:
                run(output)

        for windows in (True, False):
            for tag in (0x6100000d, 0x61000043):
                for suffix in ('.suprx', '.prx', '.sprx'):
                    request = 'libGuest' + suffix
                    result, output = convert(f'{windows}-{tag}-{suffix}', windows, request,
                        providers={'renamed.prx': declared_provider(22, ('libGuest',), tag=tag)}, guest_request=request)
                    assert result.returncode == 0, result.stderr
                    if windows:
                        run(output)
                    else:
                        needed = needed_libraries(output.read_bytes())
                        assert needed == ['$ORIGIN/app0/sce_module/renamed.prx.guest.prx',
                                          '$ORIGIN/app0/sce_module/consumer.prx.guest.prx'], needed
                        child = output.parent / 'app0/sce_module/consumer.prx.guest.prx'
                        assert needed_libraries(child.read_bytes()) == ['$ORIGIN/renamed.prx.guest.prx']

            providers = {'one.prx': declared_provider(22, ('libGuest',)),
                         'two.prx': declared_provider(33, ('libGuest',), symbol='other')}
            result, output = convert(f'{windows}-ambiguous', windows, 'libGuest.suprx', providers=providers)
            assert result.returncode == 2 and 'Ambiguous guest module identity' in result.stderr, result.stderr
            assert not output.exists()

            for request in ('LIBGUEST.suprx', 'libGuest.xyz', 'unavailable.suprx'):
                result, output = convert(f'{windows}-{request}', windows, request, module_name=request.rsplit('.', 1)[0])
                assert result.returncode == 0, result.stderr
                if windows:
                    run(output, error='Failed to load module:')
                else:
                    assert request in needed_libraries(output.read_bytes())

        providers = {'wrong.prx': declared_provider(11, ('other',)),
                     'right.prx': declared_provider(22, ('libGuest',))}
        result, output = convert('windows-owner', True, 'libGuest.suprx', providers=providers, guest_request='libGuest.suprx')
        assert result.returncode == 0, result.stderr
        run(output)

        providers = {'LIBGUEST.suprx': declared_provider(22, ('unrelated',)),
                     'renamed.prx': declared_provider(11, ('libGuest',))}
        result, output = convert('windows-physical-before-identity', True, 'libGuest.suprx',
            providers=providers, guest_request='libGuest.suprx')
        assert result.returncode == 0, result.stderr
        run(output)

        providers = {'LIBGUEST.suprx': declared_provider(11, ('unrelated',)),
                     'renamed.prx': declared_provider(22, ('libOther',), soname='libGuest.suprx')}
        result, output = convert('windows-soname-before-folded-file', True, 'libGuest.suprx',
            providers=providers, guest_request='libGuest.suprx')
        assert result.returncode == 0, result.stderr
        run(output)

        for exact_soname in (False, True):
            providers = {'other.prx' if exact_soname else 'libGuest.suprx':
                         declared_provider(22, ('unrelated',), 'libGuest.suprx' if exact_soname else None),
                         'alias.prx': declared_provider(11, ('libGuest',))}
            result, output = convert(f'exact-{exact_soname}', True, 'libGuest.suprx', providers=providers, guest_request='libGuest.suprx')
            assert result.returncode == 0, result.stderr
            run(output)

        result, output = convert('missing-symbol', True, 'libGuest.suprx', symbol='absent#A#B')
        assert result.returncode == 0, result.stderr
        run(output, error='unresolved ELF import absent')
        providers = {'wrong.prx': declared_provider(11, ('other',)),
                     'right.prx': declared_provider(22, ('libGuest',), symbol='other')}
        result, output = convert('wrong-provider', True, 'libGuest.suprx', providers=providers)
        assert result.returncode == 0, result.stderr
        run(output, error='unresolved ELF import shared')
        result, output = convert('multiple-identities', True, 'libGuest.suprx',
            providers={'renamed.prx': declared_provider(22, ('custom', 'libGuest'))}, guest_request='libGuest.suprx')
        assert result.returncode == 0, result.stderr
        run(output)
    print('Guest module identity dependency tests passed')


if __name__ == '__main__':
    main()
