import ctypes
import mmap
from pathlib import Path
import struct
import sys

SYS_ARCH_PRCTL = 158
ARCH_SET_GS = 0x1001
THREAD_LOCAL_STORAGE_POINTER = 0x58
DLL_PROCESS_ATTACH = 1


def run(path, entry):
    pe = Path(path).read_bytes()
    header = struct.unpack_from("<I", pe, 0x3c)[0]
    count = struct.unpack_from("<H", pe, header + 6)[0]
    optional = header + 24
    image_base = struct.unpack_from("<Q", pe, optional + 24)[0]
    image_size = struct.unpack_from("<I", pe, optional + 56)[0]
    tls_directory = struct.unpack_from("<I", pe, optional + 112 + 9 * 8)[0]
    if tls_directory == 0:
        raise SystemExit(f"{path}: no TLS directory")
    image = mmap.mmap(-1, image_size, prot=mmap.PROT_READ | mmap.PROT_WRITE | mmap.PROT_EXEC)
    base = ctypes.addressof(ctypes.c_char.from_buffer(image))
    sections = optional + struct.unpack_from("<H", pe, header + 20)[0]
    for index in range(count):
        virtual_size, rva, raw_size, raw = struct.unpack_from("<IIII", pe, sections + index * 40 + 8)
        size = min(raw_size, virtual_size)
        image[rva:rva + size] = pe[raw:raw + size]
    start, end, index_address, callbacks, zero_fill = struct.unpack_from("<QQQQI", image, tls_directory)
    struct.pack_into("<I", image, index_address - image_base, 0)
    block = ctypes.create_string_buffer(image[start - image_base:end - image_base] + bytes(zero_fill), end - start + zero_fill)
    slots = (ctypes.c_uint64 * 1)(ctypes.addressof(block))
    teb = ctypes.create_string_buffer(0x100)
    struct.pack_into("<Q", teb, THREAD_LOCAL_STORAGE_POINTER, ctypes.addressof(slots))
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.syscall(SYS_ARCH_PRCTL, ARCH_SET_GS, ctypes.c_uint64(ctypes.addressof(teb))) != 0:
        raise OSError(ctypes.get_errno(), "arch_prctl(ARCH_SET_GS) failed")
    callback = ctypes.CFUNCTYPE(None, *[ctypes.c_uint64] * 5)
    offset = callbacks - image_base
    while (address := struct.unpack_from("<Q", image, offset)[0]) != 0:
        callback(base + address - image_base)(0, 0, DLL_PROCESS_ATTACH, base, 0)
        offset += 8
    return ctypes.CFUNCTYPE(ctypes.c_int)(base + entry)()


if __name__ == "__main__":
    sys.exit(run(sys.argv[1], int(sys.argv[2], 0)))
