import os
import subprocess
import sys
import tempfile
from pathlib import Path

from guest_import_fixtures import executable
from test_windows_import_modules import provider


def main():
    assert os.name == 'nt', 'This test executes Windows startup stubs'
    relinker = Path(sys.argv[1]).resolve()
    variant = sys.argv[2]
    assert variant in ('libkernel_sys.prx', 'libkernel_web.prx')
    with tempfile.TemporaryDirectory(prefix='anyps5-kernel-variant-') as directory:
        work = Path(directory)
        modules = work / 'helpers' / 'prx'
        modules.mkdir(parents=True)
        for name, value in (('variant', 11), ('canonical', 22), ('runtime', 33), ('guest', 44)):
            (modules / (name + '.prx')).write_bytes(provider(value))
        empty = provider(0)
        empty[0x800:0x80c] = b'\0other#A#B\0\0'
        (modules / 'empty.prx').write_bytes(empty)
        source = modules.parent / 'input.elf'
        source.write_bytes(executable('variant.prx', 'shared#A#B'))
        result = subprocess.run([str(relinker), '--windows', str(source), str(modules.parent / 'output.exe')],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stderr
        converted = modules.parent / 'app0' / 'prx'
        images = {name: (converted / (name + '.prx.guest.prx')).read_bytes()
                  for name in ('variant', 'canonical', 'runtime', 'guest', 'empty')}
        other = 'libkernel_web.prx' if variant == 'libkernel_sys.prx' else 'libkernel_sys.prx'
        cases = (
            ('runtime-fallback', 'empty', 'empty', 'runtime', 33),
            ('variant-first', 'variant', 'canonical', 'runtime', 11),
            ('canonical-before-runtime', 'empty', 'canonical', 'runtime', 22),
            ('missing', 'empty', 'empty', 'empty', None),
            ('guest-isolation', 'empty', 'empty', 'runtime', 33),
            ('guest-cannot-fill-missing', 'empty', 'empty', 'empty', None),
            ('unrelated-host', 'empty', 'empty', 'empty', None),
            ('guest-requested-variant', 'variant', 'canonical', 'runtime', 44),
            ('unqualified', 'variant', 'canonical', 'runtime', 11),
        )
        for label, requested, canonical, runtime, expected in cases:
            case = work / label
            libraries = case / 'libs'
            libraries.mkdir(parents=True)
            (case / 'prx').mkdir()
            for name, image in ((variant, requested), ('libkernel.prx', canonical), (other, 'empty'), ('libc.prx', runtime)):
                (libraries / name).write_bytes(images[image])
            if label.startswith('guest-'):
                guests = case / 'sce_module'
                guests.mkdir()
                names = (variant,) if label == 'guest-requested-variant' else ('libkernel.prx', 'libc.prx')
                for name in names:
                    (guests / name).write_bytes(provider(44))
            extra = ('unrelated.prx',) if label == 'unrelated-host' else ()
            if extra:
                (libraries / extra[0]).write_bytes(images['guest'])
            source = case / 'input.elf'
            symbol = 'shared' if label == 'unqualified' else 'shared#A#B'
            source.write_bytes(executable(variant, symbol, extra_dependencies=extra))
            output = case / 'output.exe'
            result = subprocess.run([str(relinker), '--windows', str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (variant, label, result.stderr)
            result = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            if expected is None:
                assert result.returncode & 0xffffffff == 0xc0000139, (label, result.returncode, result.stderr)
                assert 'unresolved ELF import shared' in result.stderr, (label, result.stderr)
            else:
                assert result.returncode == expected, (variant, label, result.returncode, result.stderr)
        sibling_expected = 11 if variant == 'libkernel_sys.prx' else 22
        for label, owner, variant_image, canonical_image, other_image, expected in (
            ('kernel-requests-variant', 'libkernel.prx', 'variant', 'empty', 'empty', 11),
            ('variant-requests-other', variant, 'empty', 'empty', 'guest', 44),
            ('kernel-prefers-declared', 'libkernel.prx', 'variant', 'canonical', 'guest', 22),
            ('kernel-declared-siblings', 'libkernel.prx', 'variant', 'empty', 'canonical', sibling_expected),
            ('variant-missing-sibling', variant, 'variant', 'canonical', None, 11),
            ('kernel-missing-sibling', 'libkernel.prx', 'variant', 'canonical', None, 22),
        ):
            case = work / label
            libraries = case / 'libs'
            libraries.mkdir(parents=True)
            (case / 'prx').mkdir()
            for name, image in ((variant, variant_image), ('libkernel.prx', canonical_image), (other, other_image), ('libc.prx', 'empty')):
                if image is None:
                    continue
                (libraries / name).write_bytes(images[image])
            source = case / 'input.elf'
            source.write_bytes(executable(owner, 'shared#A#B'))
            output = case / 'output.exe'
            result = subprocess.run([str(relinker), '--windows', str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (variant, label, result.stderr)
            result = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert result.returncode == expected, (variant, label, result.returncode, result.stderr)
    print(variant + ': runtime, precedence and isolation checks passed')


if __name__ == '__main__':
    main()
