from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, sys.argv[1])
from test_guest_symbol_names import importing_executable, string_table, DYNAMIC, STRINGS, SYMBOLS, RELOCATIONS, GOT

NAMES = ['Begin_nid_no_patch', 'mmap_nid_postfix', 'Fill_nid_no_patch', 'mprotect_nid_postfix',
         'Retired_nid_no_patch', 'Verify_nid_no_patch', 'sceKernelMapFlexibleMemory',
         'munmap_nid_postfix', 'Finish_nid_no_patch']


def fixture():
    image = importing_executable(NAMES)
    struct.pack_into('<I', image, 64 + 4, 7)
    libraries = ['libGuestMemoryFixture.prx', 'libkernel.prx', 'libc.prx']
    strings, offsets = string_table(NAMES + libraries)
    assert len(strings) < SYMBOLS - STRINGS
    image[STRINGS:STRINGS + len(strings)] = strings
    tags = [(5, STRINGS), (10, len(strings)), (6, SYMBOLS), (11, 24),
            (7, RELOCATIONS), (8, len(NAMES) * 24), (9, 24)]
    tags.extend((1, offset) for offset in offsets[len(NAMES):])
    tags.append((0, 0))
    for index, tag in enumerate(tags):
        struct.pack_into('<qQ', image, DYNAMIC + index * 16, *tag)
    struct.pack_into('<QQQQQ', image, 120 + 8, DYNAMIC, DYNAMIC, DYNAMIC, len(tags) * 16, len(tags) * 16)
    code = bytearray(bytes.fromhex('4883ec18'))
    checks = []

    def emit(value):
        code.extend(bytes.fromhex(value))

    def call(name):
        address = 0x4000 + len(code)
        code.extend(b'\xff\x15' + struct.pack('<i', GOT + NAMES.index(name) * 8 - address - 6))

    def check():
        emit('85c00f85')
        checks.append(len(code))
        code.extend(bytes(4))

    def pointer():
        emit('4c89e7')

    def fill(gpu):
        pointer()
        emit('be')
        code.extend(struct.pack('<I', gpu))
        call('Fill_nid_no_patch')
        check()

    def retired():
        pointer()
        call('Retired_nid_no_patch')
        check()

    def protect(value):
        pointer()
        emit('be00000100ba')
        code.extend(struct.pack('<I', value))
        call('mprotect_nid_postfix')
        check()

    def verify(zero):
        pointer()
        emit('be')
        code.extend(struct.pack('<I', zero))
        call('Verify_nid_no_patch')
        check()

    call('Begin_nid_no_patch')
    check()
    emit('31ffbe00000100ba03000000b90210000041b8ffffffff4531c9')
    call('mmap_nid_postfix')
    emit('4989c44883f8ff0f84')
    checks.append(len(code))
    code.extend(bytes(4))
    fill(0)
    protect(3)
    retired()
    verify(0)
    fill(0)
    protect(1)
    retired()
    verify(0)
    protect(0)
    protect(3)
    verify(0)
    fill(0)
    emit('4c8924244889e7be00000100ba33000000b910000000')
    call('sceKernelMapFlexibleMemory')
    check()
    retired()
    verify(1)
    fill(1)
    pointer()
    emit('be00000100')
    call('munmap_nid_postfix')
    check()
    retired()
    emit('31ff')
    call('Finish_nid_no_patch')
    emit('0f0b')
    failed = len(code)
    emit('bf01000000')
    call('Finish_nid_no_patch')
    emit('0f0b')
    for position in checks:
        struct.pack_into('<i', code, position, failed - position - 4)
    assert len(code) < 0x600
    image[0x4000:0x4000 + len(code)] = code
    return image


def main():
    relinker, fixture_library, kernel, libc, driver = (Path(value).resolve() for value in sys.argv[2:])
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-memory-lifetime-") as directory:
        work = Path(directory)
        libraries = work / "libs"
        libraries.mkdir()
        for path in (fixture_library, kernel, libc, driver):
            (libraries / path.name).symlink_to(path)
        source = work / "guest.elf"
        source.write_bytes(fixture())
        output = work / "native.elf"
        converted = subprocess.run(
            [str(relinker), "--skip-sce-module", "--registry", str(source), str(output)],
            cwd=work, capture_output=True, text=True, timeout=10)
        assert converted.returncode == 0, (converted.stdout, converted.stderr)
        output.chmod(0o755)
        result = subprocess.run([str(output)], cwd=work, capture_output=True, text=True, timeout=15)
        if result.returncode == 77:
            print(result.stdout, end="")
            sys.exit(77)
        assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
        assert "converted guest memory/Vulkan lifetime result=0" in result.stdout, result.stdout
    print("Converted guest memory/Vulkan lifetime tests passed")


if __name__ == "__main__":
    main()
