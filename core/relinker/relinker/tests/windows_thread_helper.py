"""A minimal x86-64 helper DLL, built byte by byte, for the Windows TLS template tests.

It exports ``target`` (returns 42) and ``start`` (creates a thread that runs the function it
is given through kernel32!CreateThread), so a converted executable can call an imported
function pointer on the main thread and on a thread created after its entry stub ran.
"""

import struct

SECTION_RVA = 0x1000
IMAGE_BASE = 0x180000000
TEXT = 0x200
CODE_START = 0x000
CODE_TARGET = 0x040
EXPORT_DIRECTORY = 0x080
EXPORT_FUNCTIONS = 0x0A8
EXPORT_NAMES = 0x0B0
EXPORT_ORDINALS = 0x0B8
EXPORT_STRINGS = 0x0C0
IMPORT_DESCRIPTOR = 0x0D0
IMPORT_LOOKUP = 0x100
IMPORT_ADDRESS = 0x110
IMPORT_NAME = 0x120
MODULE_NAME = 0x130
IMPORT_MODULE = 0x140
RELOCATIONS = 0x150


def start_code():
    code = bytearray()
    code += b"\x48\x83\xec\x38"
    code += b"\x49\x89\xc8"
    code += b"\x31\xc9"
    code += b"\x31\xd2"
    code += b"\x45\x31\xc9"
    code += b"\x48\xc7\x44\x24\x20\x00\x00\x00\x00"
    code += b"\x48\xc7\x44\x24\x28\x00\x00\x00\x00"
    code += b"\x48\xff\x15\x00\x00\x00\x00"
    displacement = len(code) - 4
    struct.pack_into("<i", code, displacement, SECTION_RVA + IMPORT_ADDRESS - (SECTION_RVA + CODE_START + len(code)))
    code += b"\x31\xc0"
    code += b"\x48\x83\xc4\x38"
    code += b"\xc3"
    return bytes(code)


def build():
    text = bytearray(0x400)
    code = start_code()
    text[CODE_START:CODE_START + len(code)] = code
    text[CODE_TARGET:CODE_TARGET + 6] = b"\xb8\x2a\x00\x00\x00\xc3"
    strings = b"start\0target\0"
    text[EXPORT_STRINGS:EXPORT_STRINGS + len(strings)] = strings
    text[EXPORT_DIRECTORY:EXPORT_DIRECTORY + 40] = struct.pack(
        "<IIHHIIIIIII", 0, 0, 0, 0, SECTION_RVA + MODULE_NAME, 1, 2, 2,
        SECTION_RVA + EXPORT_FUNCTIONS, SECTION_RVA + EXPORT_NAMES, SECTION_RVA + EXPORT_ORDINALS)
    text[EXPORT_FUNCTIONS:EXPORT_FUNCTIONS + 8] = struct.pack("<II", SECTION_RVA + CODE_START, SECTION_RVA + CODE_TARGET)
    text[EXPORT_NAMES:EXPORT_NAMES + 8] = struct.pack(
        "<II", SECTION_RVA + EXPORT_STRINGS, SECTION_RVA + EXPORT_STRINGS + len(b"start\0"))
    text[EXPORT_ORDINALS:EXPORT_ORDINALS + 4] = struct.pack("<HH", 0, 1)
    text[IMPORT_DESCRIPTOR:IMPORT_DESCRIPTOR + 20] = struct.pack(
        "<IIIII", SECTION_RVA + IMPORT_LOOKUP, 0, 0, SECTION_RVA + IMPORT_MODULE, SECTION_RVA + IMPORT_ADDRESS)
    text[IMPORT_LOOKUP:IMPORT_LOOKUP + 16] = struct.pack("<QQ", SECTION_RVA + IMPORT_NAME, 0)
    text[IMPORT_ADDRESS:IMPORT_ADDRESS + 16] = struct.pack("<QQ", SECTION_RVA + IMPORT_NAME, 0)
    hint = struct.pack("<H", 0) + b"CreateThread\0"
    text[IMPORT_NAME:IMPORT_NAME + len(hint)] = hint
    text[MODULE_NAME:MODULE_NAME + len(b"helper.dll\0")] = b"helper.dll\0"
    text[IMPORT_MODULE:IMPORT_MODULE + len(b"kernel32.dll\0")] = b"kernel32.dll\0"
    text[RELOCATIONS:RELOCATIONS + 12] = struct.pack("<IIHH", SECTION_RVA, 12, 0, 0)

    headers = bytearray(0x200)
    headers[0:2] = b"MZ"
    struct.pack_into("<I", headers, 0x3C, 0x40)
    headers[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", headers, 0x44, 0x8664, 1, 0, 0, 0, 0xF0, 0x2022)
    optional = 0x58
    struct.pack_into("<HBBIIIII", headers, optional, 0x20B, 0, 0, 0x400, 0, 0, 0, 0)
    struct.pack_into("<QII", headers, optional + 0x18, IMAGE_BASE, 0x1000, 0x200)
    struct.pack_into("<HHHHHHIIIIHHQQQQII", headers, optional + 0x28,
                     6, 0, 0, 0, 6, 0, 0, 0x2000, 0x200, 0, 3, 0x40,
                     0x100000, 0x1000, 0x100000, 0x1000, 0, 16)
    directories = ((SECTION_RVA + EXPORT_DIRECTORY, 40), (SECTION_RVA + IMPORT_DESCRIPTOR, 40), (0, 0), (0, 0),
                   (0, 0), (SECTION_RVA + RELOCATIONS, 12), (0, 0), (0, 0), (0, 0), (0, 0), (0, 0), (0, 0),
                   (SECTION_RVA + IMPORT_ADDRESS, 16), (0, 0), (0, 0), (0, 0))
    for index, (rva, size) in enumerate(directories):
        struct.pack_into("<II", headers, optional + 0x70 + index * 8, rva, size)
    section = optional + 0xF0
    headers[section:section + 8] = b".text\0\0\0"
    struct.pack_into("<IIIIIIHHI", headers, section + 8, 0x400, SECTION_RVA, 0x400, TEXT, 0, 0, 0, 0, 0xE0000020)
    return bytes(headers) + bytes(text)


def write(path):
    path.write_bytes(build())
    return path