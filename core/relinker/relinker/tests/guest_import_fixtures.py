import contextlib
import shutil
import struct
import tempfile
import time

from test_guest_intel_trampolines import main_fixture


@contextlib.contextmanager
def fixture_directory(prefix):
    directory = tempfile.mkdtemp(prefix=prefix)
    try:
        yield directory
    finally:
        deadline = time.monotonic() + 10
        while True:
            try:
                shutil.rmtree(directory)
                break
            except PermissionError:
                if time.monotonic() >= deadline:
                    raise
                time.sleep(0.05)


def dynamic(image, offset, phoff, tags):
    for index, tag in enumerate(tags + [(0, 0)]):
        struct.pack_into('<qQ', image, offset + index * 16, *tag)
    struct.pack_into('<QQ', image, phoff + 32, (len(tags) + 1) * 16, (len(tags) + 1) * 16)


def executable(owner, symbol='shared#A#B', module_name=None, extra_dependencies=(), extra_symbols=()):
    image = main_fixture()
    struct.pack_into('<I', image, 68, 7)
    symbols = (symbol,) + tuple(extra_symbols)
    strings = b'\0'
    symbol_offsets = []
    for name in symbols:
        symbol_offsets.append(len(strings))
        strings += name.encode() + b'\0'
    owner_offset = len(strings)
    strings += owner.encode() + b'\0'
    module_offset = owner_offset
    if module_name is not None:
        module_offset = len(strings)
        strings += module_name.encode() + b'\0'
    dependency_offsets = []
    for dependency in extra_dependencies:
        dependency_offsets.append(len(strings))
        strings += dependency.encode() + b'\0'
    image[0x4800:0x4800 + len(strings)] = strings
    tags = [(5, 0x4800), (10, len(strings)), (6, 0x4880), (11, 24),
            (4, 0x4840), (7, 0x4900), (8, 24 * len(symbols)), (9, 24),
            (1, owner_offset), (0x61000045, (1 << 48) | module_offset)]
    tags.extend((1, offset) for offset in dependency_offsets)
    dynamic(image, 0x4600, 120, tags)
    chains = [0] + [index + 1 for index in range(1, len(symbols))] + [0]
    struct.pack_into('<' + 'I' * (len(chains) + 3), image, 0x4840, 1, len(symbols) + 1, 1, *chains)
    for index, offset in enumerate(symbol_offsets):
        struct.pack_into('<IBBHQQ', image, 0x4898 + 24 * index, offset, 0x12, 0, 0, 0, 0)
        struct.pack_into('<QQq', image, 0x4900 + 24 * index, 0x4a20 + 8 * index, ((index + 1) << 32) | 6, 0)
    image[0x4000:0x4007] = b'\xff\x15' + struct.pack('<i', 0x4a20 - 0x4006) + b'\xc3'
    return image
