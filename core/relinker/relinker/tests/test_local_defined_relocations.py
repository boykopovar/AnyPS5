import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_call_registry import make_image
from test_linux_dynamic_strings import dynamic_strings, file_offset as linux_file_offset
from test_linux_load_alignment import loads
from test_windows_fixed_base import file_offset as pe_file_offset, pe_headers


DEFAULT_BASE = 0x400000
WINDOWS_BASE = 0x3F0000
WINDOWS_LOAD_RVA = 0x10000
SLOT = 0x300
SECOND_SLOT = 0x308
SHN_ABS = 0xFFF1
SHN_COMMON = 0xFFF2
TARGETS = (
    ("linux", [], b"\x7fELF"),
    ("pie", ["--lazy-binding"], b"\x7fELF"),
    ("windows", ["--windows"], b"MZ"),
    ("macos", ["--macos"], b"\xcf\xfa\xed\xfe"),
)


def build_image(relocation, binding, visibility, sym_type=2, section=1, value=0x200, addend=0, slot=SLOT, name_offset=8, extra_relocations=()):
    image = make_image(b"\0lib.so\0symbol\0ext\0")
    struct.pack_into("<IBBHQQ", image, 0x638, name_offset, (binding << 4) | sym_type, visibility, section, value, 0)
    struct.pack_into("<IBBHQQ", image, 0x650, 15, (1 << 4) | 2, 0, 0, 0, 0)
    entries = [(slot, (1 << 32) | relocation, addend), *extra_relocations]
    for index, entry in enumerate(entries):
        struct.pack_into("<QQq", image, 0x700 + index * 24, *entry)
    if relocation == 7:
        tags = [
            (1, 1), (5, 0x600), (10, 19), (6, 0x620), (11, 24), (0x6100003f, 72),
            (3, SLOT), (23, 0x700), (2, len(entries) * 24), (20, 7), (0, 0),
        ]
    else:
        tags = [
            (1, 1), (5, 0x600), (10, 19), (6, 0x620), (11, 24), (0x6100003f, 72),
            (7, 0x700), (8, len(entries) * 24), (9, 24), (0, 0),
        ]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    struct.pack_into("<QQ", image, 120 + 32, len(tags) * 16, len(tags) * 16)
    return image


def linux_relocations(data):
    tags, _ = dynamic_strings(data)
    segments = loads(data)
    rela = []
    if 7 in tags and 8 in tags:
        start = linux_file_offset(data, segments, tags[7])
        for offset in range(start, start + tags[8], 24):
            target, info, addend = struct.unpack_from("<QQq", data, offset)
            rela.append((target, info & 0xffffffff, info >> 32, addend))
    jmprel = []
    if 23 in tags and 2 in tags:
        start = linux_file_offset(data, segments, tags[23])
        for offset in range(start, start + tags[2], 24):
            target, info, addend = struct.unpack_from("<QQq", data, offset)
            jmprel.append((target, info & 0xffffffff, info >> 32, addend))
    return rela, jmprel


def linux_read_u64(data, vaddr):
    segments = loads(data)
    offset = linux_file_offset(data, segments, vaddr)
    return struct.unpack_from("<Q", data, offset)[0]


def pe_base_relocations(data):
    _, optional = pe_headers(data)
    dir_rva, dir_size = struct.unpack_from("<II", data, optional + 112 + 5 * 8)
    if dir_rva == 0 or dir_size == 0:
        return set()
    offset = pe_file_offset(data, dir_rva)
    end = offset + dir_size
    rvas = set()
    while offset + 8 <= end:
        page_rva, block_size = struct.unpack_from("<II", data, offset)
        if block_size < 8:
            break
        count = (block_size - 8) // 2
        for index in range(count):
            entry = struct.unpack_from("<H", data, offset + 8 + index * 2)[0]
            if (entry >> 12) == 10:
                rvas.add(page_rva + (entry & 0xfff))
        offset += block_size
    return rvas


def pe_read_u64(data, elf_vaddr):
    rva = WINDOWS_LOAD_RVA + elf_vaddr
    return struct.unpack_from("<Q", data, pe_file_offset(data, rva))[0]


def main():
    relinker = Path(sys.argv[1]).resolve()
    case_id = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-local-defined-") as directory:
        work = Path(directory)

        def run_case(name, image, options, expected_error=None, extra_args=()):
            nonlocal case_id
            case = work / f"case-{case_id}-{name}"
            case_id += 1
            case.mkdir()
            source = case / "input.elf"
            output = case / "output.out"
            source.write_bytes(image)
            cmd = [str(relinker), "--skip-sce-module", "--registry", *options, *extra_args, str(source), str(output)]
            result = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", timeout=20)
            if expected_error is not None:
                assert result.returncode == 2 and expected_error in result.stderr, (
                    name, options, result.returncode, result.stdout, result.stderr)
                assert not output.exists(), (name, options)
                return None, None
            assert result.returncode == 0 and output.is_file(), (
                name, options, result.returncode, result.stdout, result.stderr)
            registry = json.loads(output.with_suffix(".registry.json").read_text(encoding="utf-8"))
            return output.read_bytes(), registry

        cases = [
            ("local-default-64-pos", 1, 0, 0, 2, 0x200, 0x18),
            ("local-default-64-neg", 1, 0, 0, 1, 0x240, -0x10),
            ("local-section-64", 1, 0, 0, 3, 0x200, 0x20),
            ("global-hidden-64", 1, 1, 2, 1, 0x210, 0x08),
            ("weak-internal-64", 1, 2, 1, 2, 0x220, 0),
            ("local-globdat", 6, 0, 0, 2, 0x200, 0),
            ("global-hidden-globdat", 6, 1, 2, 1, 0x230, 0),
            ("global-internal-jumpslot", 7, 1, 1, 2, 0x200, 0),
            ("weak-hidden-jumpslot", 7, 2, 2, 2, 0x210, 0),
            ("local-jumpslot", 7, 0, 0, 2, 0x200, 0),
        ]
        for label, reloc, binding, visibility, sym_type, value, addend in cases:
            name_off = 0 if sym_type == 3 else 8
            image = build_image(reloc, binding, visibility, sym_type=sym_type, section=1, value=value, addend=addend, name_offset=name_off)
            resolved = value + addend
            for target, options, magic in TARGETS:
                data, registry = run_case(f"{label}-{target}", image, options)
                assert data.startswith(magic), (label, target)
                assert registry == [], (label, target, registry)
                if target == "linux":
                    rela, jmprel = linux_relocations(data)
                    assert (DEFAULT_BASE + SLOT, 8, 0, DEFAULT_BASE + resolved) in rela, (label, rela)
                    assert jmprel == [], (label, jmprel)
                elif target == "pie":
                    rela, jmprel = linux_relocations(data)
                    assert (SLOT, 8, 0, resolved) in rela, (label, rela)
                    assert jmprel == [], (label, jmprel)
                elif target == "windows":
                    assert pe_read_u64(data, SLOT) == WINDOWS_BASE + WINDOWS_LOAD_RVA + resolved, (label,)
                    assert (WINDOWS_LOAD_RVA + SLOT) in pe_base_relocations(data), (label,)

        for label, reloc, binding, visibility, value, addend in (
            ("abs-local-64", 1, 0, 0, 0x123456789ABCDEF0, 0x10),
            ("abs-hidden-globdat", 6, 1, 2, 0x0000CAFEBABE0000, 0),
            ("abs-internal-jumpslot", 7, 1, 1, 0x0000112233445566, 0),
        ):
            image = build_image(reloc, binding, visibility, sym_type=1, section=SHN_ABS, value=value, addend=addend)
            expected = value + addend
            for target, options, magic in TARGETS:
                data, registry = run_case(f"{label}-{target}", image, options)
                assert data.startswith(magic), (label, target)
                assert registry == [], (label, target, registry)
                if target == "linux":
                    rela, jmprel = linux_relocations(data)
                    assert all(entry[0] != DEFAULT_BASE + SLOT for entry in rela), (label, rela)
                    assert jmprel == [], (label, jmprel)
                    assert linux_read_u64(data, DEFAULT_BASE + SLOT) == expected, (label,)
                elif target == "pie":
                    rela, jmprel = linux_relocations(data)
                    assert all(entry[0] != SLOT for entry in rela), (label, rela)
                    assert jmprel == [], (label, jmprel)
                    assert linux_read_u64(data, SLOT) == expected, (label,)
                elif target == "windows":
                    assert pe_read_u64(data, SLOT) == expected, (label,)
                    assert (WINDOWS_LOAD_RVA + SLOT) not in pe_base_relocations(data), (label,)

        mixed_plt = build_image(7, 1, 2, sym_type=2, section=1, value=0x200, addend=0,
                                extra_relocations=((SECOND_SLOT, (2 << 32) | 7, 0),))
        for target, options, magic in TARGETS:
            if target == "pie":
                run_case("mixed-plt-pie", mixed_plt, options,
                         expected_error="Cannot mix local, hidden or internal JUMP_SLOT relocations")
                continue
            data, registry = run_case(f"mixed-plt-{target}", mixed_plt, options)
            assert data.startswith(magic), target
            assert len(registry) == 1 and registry[0]["nid"] == "ext", (target, registry)
            if target == "linux":
                rela, jmprel = linux_relocations(data)
                assert (DEFAULT_BASE + SLOT, 8, 0, DEFAULT_BASE + 0x200) in rela, rela
                assert len(jmprel) == 1 and jmprel[0][0] == DEFAULT_BASE + SECOND_SLOT and jmprel[0][1] == 7, jmprel
            elif target == "windows":
                assert pe_read_u64(data, SLOT) == WINDOWS_BASE + WINDOWS_LOAD_RVA + 0x200
                assert (WINDOWS_LOAD_RVA + SLOT) in pe_base_relocations(data)

        run_case("nonzero-globdat-addend", build_image(6, 1, 2, addend=8), [],
                 expected_error="GLOB_DAT/JUMP_SLOT relocation has a nonzero addend")
        run_case("nonzero-jumpslot-addend", build_image(7, 0, 0, addend=4), [],
                 expected_error="GLOB_DAT/JUMP_SLOT relocation has a nonzero addend")
        run_case("unsupported-pc32", build_image(2, 0, 0), [],
                 expected_error="Unsupported relocation type for defined local, hidden or internal symbol")
        run_case("unsupported-32", build_image(10, 1, 2), [],
                 expected_error="Unsupported relocation type for defined local, hidden or internal symbol")
        run_case("unsupported-section", build_image(1, 0, 0, section=SHN_COMMON), [],
                 expected_error="Unsupported defined symbol section")
        run_case("unsupported-ifunc", build_image(1, 1, 2, sym_type=10), [],
                 expected_error="Unsupported defined local, hidden or internal symbol type")
        run_case("unmapped-value", build_image(1, 0, 0, value=0x50000), [],
                 expected_error="Defined local, hidden or internal symbol is not inside a matching PT_LOAD segment")
        run_case("addend-overflow", build_image(1, 0, 0, section=SHN_ABS, value=0xFFFFFFFFFFFFFFF8, addend=0x20), [],
                 expected_error="Defined symbol relocation address overflow")
        run_case("addend-underflow", build_image(1, 0, 0, value=0x200, addend=-0x300), [],
                 expected_error="Defined symbol relocation address underflow")

        bss_abs = build_image(1, 0, 0, sym_type=1, section=SHN_ABS, value=0x1234, slot=0x1010)
        struct.pack_into("<QQ", bss_abs, 64 + 32, len(bss_abs) - 0x200, len(bss_abs) - 0x200 + 0x200)
        run_case("abs-bss-target", bss_abs, [],
                 expected_error="Absolute defined symbol relocation target is not inside a writable file-backed PT_LOAD segment")

        overlap_abs = build_image(1, 0, 0, sym_type=1, section=SHN_ABS, value=0x1234,
                                  extra_relocations=((SLOT + 4, (1 << 32) | 1, 0),))
        run_case("abs-overlap", overlap_abs, [],
                 expected_error="Overlapping relocation target for absolute defined symbol")

    print(f"Local defined relocation tests passed: {case_id} cases")


if __name__ == "__main__":
    main()
