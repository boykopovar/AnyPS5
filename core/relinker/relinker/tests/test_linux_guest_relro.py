from pathlib import Path
import platform
import signal
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import executable
from signal_run import run_without_core_dump
from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import needed_libraries

PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3
PT_GNU_STACK = 0x6474E551
PT_GNU_RELRO = 0x6474E552
PT_SCE_RELRO = 0x61000010
PT_SCE_VERSION = 0x6FFFFF01

PF_X = 1
PF_W = 2
PF_R = 4

R_X86_64_RELATIVE = 8
DF_BIND_NOW = 8
LOAD_START_MODULE = "wzvqT4UqKX8#A#B"


def build_guest_module(
    data_loads=None,
    relro_headers=(),
    extra_headers=(),
    rela_offset=0x2000,
    include_init_array=False,
):
    if data_loads is None:
        data_loads = [(PT_LOAD, PF_R | PF_W, 0x2000, 0x2000, 0x2000, 0x2000, 0x1000)]

    image = bytearray(0x5000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)

    def emit_read(fn_va, slot_va):
        off = fn_va
        image[off:off + 3] = b"\x48\x8b\x05"
        struct.pack_into("<i", image, off + 3, slot_va - (fn_va + 7))
        image[off + 7] = 0xC3

    def emit_lea(fn_va, target_va):
        off = fn_va
        image[off:off + 3] = b"\x48\x8d\x05"
        struct.pack_into("<i", image, off + 3, target_va - (fn_va + 7))
        image[off + 7] = 0xC3

    def emit_write_test(fn_va, target_va):
        off = fn_va
        image[off:off + 3] = b"\x48\x8d\x05"
        struct.pack_into("<i", image, off + 3, target_va - (fn_va + 7))
        image[off + 7:off + 16] = b"\xc7\x00\x2a\x00\x00\x00\x8b\x00\xc3"

    emit_read(0x1000, rela_offset)
    emit_lea(0x1010, 0x1200)
    emit_write_test(0x1020, 0x2008)
    emit_write_test(0x1040, 0x3000)
    emit_write_test(0x1060, 0x4508)
    image[0x1080] = 0xC3
    image[0x1200:0x120D] = b"RELRO_TARGET\0"

    exports = [
        ("read_slot", 0x1000, 8),
        ("target_addr", 0x1010, 8),
        ("write_relro", 0x1020, 16),
        ("write_trailing", 0x1040, 16),
        ("write_after_trailing", 0x1060, 16),
    ]
    strings = bytearray(b"\0")
    symtab = bytearray(24)
    for name, value, size in exports:
        name_off = len(strings)
        strings.extend(name.encode() + b"\0")
        symtab.extend(struct.pack("<IBBHQQ", name_off, 0x12, 0, 1, value, size))

    str_off = 0x4000
    sym_off = 0x4100
    hash_off = 0x4200
    rela_off = 0x4300
    dyn_off = 0x4400

    image[str_off:str_off + len(strings)] = strings
    image[sym_off:sym_off + len(symtab)] = symtab
    nchain = len(exports) + 1
    struct.pack_into("<II", image, hash_off, 1, nchain)
    struct.pack_into("<I", image, hash_off + 8, 1)
    for idx in range(1, nchain):
        struct.pack_into("<I", image, hash_off + 12 + idx * 4, idx + 1 if idx + 1 < nchain else 0)

    relocations = [(rela_offset, R_X86_64_RELATIVE, 0x1200)]
    if include_init_array:
        relocations.append((0x4600, R_X86_64_RELATIVE, 0x1080))
    for idx, (r_offset, r_info, r_addend) in enumerate(relocations):
        struct.pack_into("<QQq", image, rela_off + idx * 24, r_offset, r_info, r_addend)

    tags = [
        (5, str_off),
        (10, len(strings)),
        (6, sym_off),
        (11, 24),
        (4, hash_off),
        (7, rela_off),
        (8, len(relocations) * 24),
        (9, 24),
    ]
    if include_init_array:
        tags.extend([(25, 0x4600), (27, 8)])
    tags.append((0, 0))
    for idx, (tag, value) in enumerate(tags):
        struct.pack_into("<qQ", image, dyn_off + idx * 16, tag, value)

    headers = [
        (PT_LOAD, PF_R | PF_X, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000),
        *data_loads,
        (PT_LOAD, PF_R | PF_W, 0x4000, 0x4000, 0x1000, 0x1000, 0x1000),
        (PT_DYNAMIC, PF_R | PF_W, dyn_off, dyn_off, len(tags) * 16, len(tags) * 16, 8),
        *relro_headers,
        *extra_headers,
    ]
    struct.pack_into(
        "<HHIQQQIHHHHHH",
        image,
        16,
        3,
        62,
        1,
        0,
        64,
        0,
        0,
        64,
        56,
        len(headers),
        64,
        0,
        0,
    )
    for idx, (kind, flags, offset, address, file_size, mem_size, align) in enumerate(headers):
        struct.pack_into(
            "<IIQQQQQQ",
            image,
            64 + idx * 56,
            kind,
            flags,
            offset,
            address,
            address,
            file_size,
            mem_size,
            align,
        )
    return image


def read_program_headers(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    return [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]


def file_offset_for_vaddr(headers, vaddr):
    for kind, _, offset, address, _, file_size, _, _ in headers:
        if kind == PT_LOAD and address <= vaddr < address + file_size:
            return offset + (vaddr - address)
    raise AssertionError(f"Unmapped virtual address {vaddr:#x}")


def read_dynamic_tags(elf):
    headers = read_program_headers(elf)
    dyn = next(header for header in headers if header[0] == PT_DYNAMIC)
    start = file_offset_for_vaddr(headers, dyn[3])
    tags = {}
    for pos in range(start, start + dyn[5], 16):
        tag, val = struct.unpack_from("<qQ", elf, pos)
        if tag == 0:
            break
        tags.setdefault(tag, val)
    return headers, tags


def read_relocations(elf, headers, tags):
    if 7 not in tags or tags.get(8, 0) == 0:
        return []
    start = file_offset_for_vaddr(headers, tags[7])
    return [
        struct.unpack_from("<QQq", elf, pos)
        for pos in range(start, start + tags[8], 24)
    ]


def convert(relinker, case, module_bytes, host_bytes=None, extra_args=()):
    (case / "sce_module").mkdir(parents=True)
    (case / "sce_module" / "mod.prx").write_bytes(module_bytes)
    source = case / "input.elf"
    source.write_bytes(main_fixture() if host_bytes is None else host_bytes)
    output = case / "output.elf"
    result = subprocess.run(
        [str(relinker), "--skip-syscall-check", *extra_args, str(source), str(output)],
        capture_output=True,
        text=True,
        timeout=20,
    )
    guest_prx = case / "app0" / "sce_module" / "mod.prx.guest.prx"
    return result, output, guest_prx


def probe_linux_runtime(guest_prx, action):
    script = (
        "import ctypes, sys\n"
        "lib = ctypes.CDLL(sys.argv[1])\n"
        "action = sys.argv[2]\n"
        "if action == 'read':\n"
        "    r = lib['read_slot#guest']\n"
        "    t = lib['target_addr#guest']\n"
        "    r.restype = ctypes.c_uint64\n"
        "    t.restype = ctypes.c_uint64\n"
        "    assert r() == t(), (hex(r()), hex(t()))\n"
        "elif action == 'write_relro':\n"
        "    f = lib['write_relro#guest']\n"
        "    f.restype = ctypes.c_uint32\n"
        "    sys.exit(0 if f() == 42 else 2)\n"
        "elif action == 'write_trailing':\n"
        "    f = lib['write_trailing#guest']\n"
        "    f.restype = ctypes.c_uint32\n"
        "    assert f() == 42\n"
        "elif action == 'write_after_trailing':\n"
        "    f = lib['write_after_trailing#guest']\n"
        "    f.restype = ctypes.c_uint32\n"
        "    assert f() == 42\n"
    )
    return run_without_core_dump(
        [sys.executable, "-c", script, str(guest_prx), action],
        capture_output=True,
        text=True,
        timeout=20,
    )


def verify_valid_output(
    output_elf,
    guest_prx,
    expected_relro,
    expected_deferred=None,
    expected_rela_offset=0x2000,
):
    assert output_elf.exists() and guest_prx.exists(), (output_elf, guest_prx)
    data = guest_prx.read_bytes()
    assert struct.unpack_from("<H", data, 16)[0] == 3
    headers, tags = read_dynamic_tags(data)
    assert all(header[0] != PT_INTERP for header in headers), headers
    assert any(header[0] == PT_GNU_STACK for header in headers), headers
    assert (tags.get(30, 0) & DF_BIND_NOW) == DF_BIND_NOW, tags
    if expected_deferred is not None:
        assert (25 in tags) == (not expected_deferred), tags
        host_needed = needed_libraries(output_elf.read_bytes())
        assert ("$ORIGIN/app0/sce_module/mod.prx.guest.prx" in host_needed) == (not expected_deferred), host_needed

    loads = [header for header in headers if header[0] == PT_LOAD]
    assert [load[3] for load in loads] == sorted(load[3] for load in loads), loads
    for kind, flags, offset, vaddr, _, filesz, memsz, align in loads:
        if align > 1:
            assert offset % align == vaddr % align, (offset, vaddr, align)
        assert filesz <= memsz

    relros = [header for header in headers if header[0] == PT_GNU_RELRO]
    if expected_relro is None:
        assert relros == [], relros
    else:
        assert len(relros) == 1, f"Missing PT_GNU_RELRO header in guest module: {relros}"
        relro = relros[0]
        exp_vaddr, exp_filesz, exp_memsz = expected_relro
        assert relro[3] == exp_vaddr and relro[4] == exp_vaddr, relro
        assert relro[5] == exp_filesz and relro[6] == exp_memsz, relro
        assert relro[1] == PF_R, relro

    relocs = read_relocations(data, headers, tags)
    assert any(
        (r_info & 0xFFFFFFFF) == R_X86_64_RELATIVE and r_offset == expected_rela_offset
        for r_offset, r_info, _ in relocs
    ), relocs


def main():
    relinker = Path(sys.argv[1]).resolve()
    runnable = sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64")

    with tempfile.TemporaryDirectory(prefix="anyps5-linux-guest-relro-") as directory:
        work = Path(directory)

        standalone_sce = build_guest_module(
            data_loads=[(PT_LOAD, PF_R | PF_W, 0x3000, 0x3000, 0x1000, 0x1000, 0x1000)],
            relro_headers=[(PT_SCE_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)],
        )
        covered_sce = build_guest_module(
            relro_headers=[(PT_SCE_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)],
        )
        covered_gnu = build_guest_module(
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 1)],
        )
        partial_trailing_gnu = build_guest_module(
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1800, 0x1800, 1)],
        )
        subpage_gnu = build_guest_module(
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x800, 0x800, 1)],
        )
        unaligned_both_gnu = build_guest_module(
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2800, 0x2800, 0x1000, 0x1000, 1)],
        )
        bss_only_gnu = build_guest_module(
            relro_headers=[(PT_GNU_RELRO, PF_R, 0, 0x2000, 0, 0x1000, 1)],
        )
        adjacent_loads_gnu = build_guest_module(
            data_loads=[
                (PT_LOAD, PF_R | PF_W, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000),
                (PT_LOAD, PF_R | PF_W, 0x3000, 0x3000, 0x1000, 0x1000, 0x1000),
            ],
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1800, 0x1800, 1)],
        )
        write_only_load_gnu = build_guest_module(
            data_loads=[(PT_LOAD, PF_W, 0x2000, 0x2000, 0x2000, 0x2000, 0x1000)],
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 1)],
        )

        readonly_covered = build_guest_module(
            data_loads=[(PT_LOAD, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000),
                        (PT_LOAD, PF_R | PF_W, 0x3000, 0x3000, 0x1000, 0x1000, 0x1000)],
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 1)],
            rela_offset=0x4500,
        )

        rx_covered = build_guest_module(
            data_loads=[(PT_LOAD, PF_R | PF_X, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000),
                        (PT_LOAD, PF_R | PF_W, 0x3000, 0x3000, 0x1000, 0x1000, 0x1000)],
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 1)],
            rela_offset=0x4500,
        )

        ro_shared_page_relro = build_guest_module(
            data_loads=[
                (PT_LOAD, PF_R, 0x2000, 0x2000, 0x800, 0x800, 0x1000),
                (PT_LOAD, PF_R | PF_W, 0x2800, 0x2800, 0x1800, 0x1800, 0x1000),
            ],
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x800, 0x800, 1)],
            rela_offset=0x4500,
        )

        high_subpage_gnu = build_guest_module(
            relro_headers=[(PT_GNU_RELRO, PF_R, 0, 0xFFFFFFFFFFFFF000, 0, 0xFFF, 1)],
        )
        aligned_tail_gnu = build_guest_module(
            data_loads=[(PT_LOAD, PF_R | PF_W, 0x2000, 0x2000, 0x1000, 0x1000, 0x2000)],
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x2000, 1)],
        )
        unmapped_partial_tail = build_guest_module(
            data_loads=[(PT_LOAD, PF_R | PF_W, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)],
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1800, 1)],
        )

        positive_cases = (
            ("standalone-sce", standalone_sce, (0x2000, 0x1000, 0x1000), True, False, 0x2000),
            ("covered-sce", covered_sce, (0x2000, 0x1000, 0x1000), True, False, 0x2000),
            ("covered-gnu", covered_gnu, (0x2000, 0x1000, 0x1000), True, False, 0x2000),
            ("partial-trailing-gnu", partial_trailing_gnu, (0x2000, 0x1800, 0x1800), True, True, 0x2000),
            ("subpage-gnu", subpage_gnu, (0x2000, 0x800, 0x800), False, False, 0x2000),
            ("unaligned-both-gnu", unaligned_both_gnu, (0x2800, 0x1000, 0x1000), True, True, 0x2000),
            ("bss-only-gnu", bss_only_gnu, (0x2000, 0, 0x1000), True, False, 0x2000),
            ("adjacent-loads-gnu", adjacent_loads_gnu, (0x2000, 0x1800, 0x1800), True, True, 0x2000),
            ("write-only-load-gnu", write_only_load_gnu, (0x2000, 0x1000, 0x1000), True, False, 0x2000),
            ("absent", build_guest_module(relro_headers=()), None, False, False, 0x2000),
            ("empty-sce", build_guest_module(relro_headers=[(PT_SCE_RELRO, PF_R, 0, 0x2000, 0, 0, 1)]), None, False, False, 0x2000),
            ("empty-gnu", build_guest_module(relro_headers=[(PT_GNU_RELRO, PF_R, 0, 0x2000, 0, 0, 1)]), None, False, False, 0x2000),
            (
                "effective-plus-empty-gnu",
                build_guest_module(relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 1), (PT_GNU_RELRO, PF_R, 0, 0, 0, 0, 1)]),
                (0x2000, 0x1000, 0x1000),
                True,
                False,
                0x2000,
            ),
            ("aligned-tail-gnu", aligned_tail_gnu, (0x2000, 0x1000, 0x1000), True, False, 0x2000),
            ("unmapped-partial-tail", unmapped_partial_tail, (0x2000, 0x1000, 0x1800), True, False, 0x2000),
            ("readonly-load", readonly_covered, (0x2000, 0x1000, 0x1000), True, False, 0x4500),
            ("rx-load", rx_covered, (0x2000, 0x1000, 0x1000), True, False, 0x4500),
            ("ro-shared-page", ro_shared_page_relro, (0x2000, 0x800, 0x800), False, False, 0x4500),
            ("high-subpage", high_subpage_gnu, (0xFFFFFFFFFFFFF000, 0, 0xFFF), False, False, 0x2000),
        )

        unmapped_trailing_cases = {"aligned-tail-gnu"}
        for name, mod, expected_relro, expects_ro_page, expects_writable_trailing, relocation_slot in positive_cases:
            res, out, guest_prx = convert(relinker, work / f"valid-{name}", mod)
            assert res.returncode == 0, (name, res.stdout, res.stderr)
            verify_valid_output(out, guest_prx, expected_relro, expected_rela_offset=relocation_slot)
            if runnable:
                read_run = probe_linux_runtime(guest_prx, "read")
                assert read_run.returncode == 0, (name, read_run.returncode, read_run.stdout, read_run.stderr)
                after_run = probe_linux_runtime(guest_prx, "write_after_trailing")
                assert after_run.returncode == 0, (name, after_run.returncode, after_run.stdout, after_run.stderr)
                wr_run = probe_linux_runtime(guest_prx, "write_relro")
                if expects_ro_page:
                    assert wr_run.returncode == -signal.SIGSEGV, (name, wr_run.returncode, wr_run.stdout, wr_run.stderr)
                else:
                    assert wr_run.returncode == 0, (name, wr_run.returncode, wr_run.stdout, wr_run.stderr)
                if name in unmapped_trailing_cases:
                    tr_run = probe_linux_runtime(guest_prx, "write_trailing")
                    assert tr_run.returncode == -signal.SIGSEGV, (name, tr_run.returncode, tr_run.stdout, tr_run.stderr)
                elif expects_writable_trailing or (expects_ro_page and expected_relro[2] <= 0x1000):
                    tr_run = probe_linux_runtime(guest_prx, "write_trailing")
                    assert tr_run.returncode == 0, (name, tr_run.returncode, tr_run.stdout, tr_run.stderr)

        for deferred in (False, True):
            host = executable("libkernel.prx", symbol=LOAD_START_MODULE) if deferred else main_fixture()
            mod = build_guest_module(
                relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 1)],
                include_init_array=True,
            )
            res, out, guest_prx = convert(relinker, work / f"defer-{deferred}", mod, host_bytes=host)
            assert res.returncode == 0, (deferred, res.stdout, res.stderr)
            verify_valid_output(out, guest_prx, (0x2000, 0x1000, 0x1000), expected_deferred=deferred)
            if runnable:
                read_run = probe_linux_runtime(guest_prx, "read")
                assert read_run.returncode == 0, (deferred, read_run.stderr)
                wr_run = probe_linux_runtime(guest_prx, "write_relro")
                assert wr_run.returncode == -signal.SIGSEGV, (deferred, wr_run.returncode)

        lazy_res, lazy_out, lazy_guest = convert(relinker, work / "lazy-binding", covered_gnu, extra_args=["--lazy-binding"])
        assert lazy_res.returncode != 0 and not lazy_out.exists() and not lazy_guest.exists(), (
            lazy_res.returncode,
            lazy_res.stdout,
            lazy_res.stderr,
        )

        unmapped_relro = build_guest_module(
            relro_headers=[(PT_GNU_RELRO, PF_R, 0, 0x8000, 0, 0x1000, 1)],
        )


        flagless_covered = build_guest_module(
            data_loads=[(PT_LOAD, 0, 0x2000, 0x2000, 0x2000, 0x2000, 0x1000)],
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 1)],
            rela_offset=0x4500,
        )
        past_end_relro = build_guest_module(
            data_loads=[(PT_LOAD, PF_R | PF_W, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)],
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x2000, 1)],
        )

        gap_across_loads = build_guest_module(
            data_loads=[(PT_LOAD, PF_R | PF_W, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)],
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x2500, 0x2500, 1)],
        )
        add_overflow = build_guest_module(
            relro_headers=[(PT_GNU_RELRO, PF_R, 0, 0xFFFFFFFFFFFFF000, 0, 0x2000, 1)],
        )

        filesz_overflow = build_guest_module(
            relro_headers=[(PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x100, 0, 1)],
        )
        disjoint_two = build_guest_module(
            relro_headers=[
                (PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x800, 0x800, 1),
                (PT_GNU_RELRO, PF_R, 0x3000, 0x3000, 0x800, 0x800, 1),
            ],
        )
        overlapping_two = build_guest_module(
            relro_headers=[
                (PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 1),
                (PT_GNU_RELRO, PF_R, 0x2800, 0x2800, 0x1000, 0x1000, 1),
            ],
        )
        identical_two = build_guest_module(
            relro_headers=[
                (PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 1),
                (PT_GNU_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 1),
            ],
        )
        mixed_sce_gnu = build_guest_module(
            data_loads=[(PT_LOAD, PF_R | PF_W, 0x3000, 0x3000, 0x1000, 0x1000, 0x1000)],
            relro_headers=[
                (PT_SCE_RELRO, PF_R, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000),
                (PT_GNU_RELRO, PF_R, 0x3000, 0x3000, 0x1000, 0x1000, 1),
            ],
        )

        negative_cases = (
            ("unmapped", unmapped_relro),
            ("flagless-load", flagless_covered),
            ("past-writable-end", past_end_relro),
            ("gap-across-loads", gap_across_loads),
            ("add-overflow", add_overflow),
            ("filesz-exceeds-memsz", filesz_overflow),
            ("two-disjoint", disjoint_two),
            ("two-overlapping", overlapping_two),
            ("two-identical", identical_two),
            ("mixed-sce-gnu", mixed_sce_gnu),
        )
        for name, mod in negative_cases:
            res, out, guest_prx = convert(relinker, work / f"invalid-{name}", mod)
            assert res.returncode != 0, (name, res.returncode, res.stdout, res.stderr)
            assert not out.exists() and not guest_prx.exists(), (name, out.exists(), guest_prx.exists())

    if runnable:
        print("Linux guest RELRO structural and x86_64 runtime tests passed")
    else:
        print("Linux guest RELRO structural tests passed (Linux runtime checks skipped on non-Linux host)")


if __name__ == "__main__":
    main()
