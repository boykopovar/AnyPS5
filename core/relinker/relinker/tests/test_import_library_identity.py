import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from test_windows_import_modules import consumer, dynamic, executable, provider


def import_library(image, name, guest=False, library_id=0, duplicate=False, bad_offset=False):
    offset, phoff, old_strings, new_strings, address = (
        (0x600, 176, 0x800, 0x940, 0x2340) if guest else
        (0x4600, 120, 0x4800, 0x4b00, 0x4b00))
    tags = []
    while True:
        tag, value = struct.unpack_from('<qQ', image, offset + len(tags) * 16)
        if tag == 0:
            break
        tags.append((tag, value))
    size = next(value for tag, value in tags if tag == 10)
    strings = image[old_strings:old_strings + size] + name.encode() + b'\0'
    assert new_strings + len(strings) <= (0xa00 if guest else 0x5000)
    image[new_strings:new_strings + len(strings)] = strings
    tags = [(tag, address if tag == 5 else len(strings) if tag == 10 else value)
            for tag, value in tags]
    entry = (0x61000049, (library_id << 48) | (len(strings) if bad_offset else size))
    tags.extend([entry] * (2 if duplicate else 1))
    dynamic(image, offset, phoff, tags)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='anyps5-import-library-') as directory:
        work = Path(directory)
        serial = 0

        def convert(guest=False, library='b2', module='Old', symbol='shared#A#B',
                    owner='b2.prx', extra=(), library_id=0, duplicate=False,
                    bad_offset=False, target_exports=True):
            nonlocal serial
            case = work / str(serial)
            serial += 1
            modules = case / 'prx'
            modules.mkdir(parents=True)
            (modules / 'a.prx').write_bytes(provider(11))
            target = provider(22)
            if not target_exports:
                target[0x800:0x80c] = b'\0other#A#B\0\0'
            (modules / owner).write_bytes(target)
            for dependency in extra:
                (modules / dependency).write_bytes(provider(44))
            if guest:
                image = consumer(owner, symbol, module_name=module)
                if extra:
                    raise AssertionError('Guest fixture uses one declared dependency')
            else:
                image = executable(owner, symbol, module, extra)
            if library is not None:
                image = import_library(image, library, guest, library_id, duplicate, bad_offset)
            source = case / 'input.elf'
            if guest:
                (modules / 'consumer.prx').write_bytes(image)
                source.write_bytes(executable(owner))
            else:
                source.write_bytes(image)
            output = case / 'output.exe'
            result = subprocess.run([str(relinker), '--windows', str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            return result, output

        def succeeds(expected=22, **kwargs):
            result, output = convert(**kwargs)
            assert result.returncode == 0, (kwargs, result.stdout, result.stderr)
            if os.name == 'nt':
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == expected, (kwargs, run.returncode, expected, run.stdout, run.stderr)

        def fails(message, **kwargs):
            result, output = convert(**kwargs)
            assert result.returncode == 2 and message in result.stderr, (kwargs, result.returncode, result.stderr)
            assert not output.exists()

        for guest in (False, True):
            succeeds(guest=guest)
            succeeds(guest=guest, module='b2', library='a')
            succeeds(guest=guest, module='b2', symbol='shared#?#B')
            succeeds(guest=guest, module='b2', symbol='shared#B#B')
            succeeds(guest=guest, library='b2.prx')
            succeeds(guest=guest, library='b2', owner='b2.suprx')
            succeeds(guest=guest, library='b2.suprx', owner='b2.suprx')
            succeeds(guest=guest, module='b2.suprx', library='a', owner='b2.suprx')
            succeeds(guest=guest, library='foo_native', owner='foo.native.prx')
            succeeds(guest=guest, library='b2', library_id=35, symbol='shared#j#B')
            succeeds(guest=guest, library='b2', library_id=65535, symbol='shared#P--#B')
            for symbol, message in [('shared#?#B', 'Invalid import library ID'),
                                    ('shared##B', 'Invalid import library ID'),
                                    ('shared#QAA#B', 'Invalid import library ID'),
                                    ('shared#B#B', 'Unknown import library ID')]:
                fails(message, guest=guest, symbol=symbol)
            fails('Duplicate import library ID', guest=guest, duplicate=True)
            fails('Unknown import module ID', guest=guest, symbol='shared#A#C')
            fails('String offset out of bounds' if guest else 'Dynamic string offset is outside DT_STRSZ',
                  guest=guest, bad_offset=True)
            for library in ('', '../b2', 'a\\b2', 'C:b2', '$ORIGIN', 'bad\nname'):
                fails('Invalid import library name', guest=guest, library=library)

        succeeds(module='b2', library='unmatched')
        succeeds(expected=11, library='unmatched')
        succeeds(expected=11, library=None)
        succeeds(expected=11, symbol='shared')
        succeeds(library='foo.native', owner='foo.native.prx', extra=('foo_native.prx',))
        fails('Ambiguous import library dependency', library='foo_native', owner='foo.native.prx',
              extra=('foo.native-module.prx',))
        fails('Ambiguous import module dependency', module='foo_native', library='foo.native',
              owner='foo.native.prx', extra=('foo.native-module.prx',))
        fails('Ambiguous guest import', guest=True, library='unmatched')
        fails('Ambiguous guest import', guest=True, library=None)
        result, output = convert(target_exports=False)
        assert result.returncode == 0, result.stderr
        if os.name == 'nt':
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode != 0 and 'unresolved ELF import shared' in run.stderr, (run.returncode, run.stderr)
            assert 'from b2.prx' in run.stderr, run.stderr
    print('Import library identity tests passed')


if __name__ == '__main__':
    main()
