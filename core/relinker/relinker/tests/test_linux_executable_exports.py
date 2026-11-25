import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile
from test_linux_load_alignment import fixture, loads

DT_PLTRELSZ = 2
DT_PLTGOT = 3
DT_HASH = 4
DT_STRTAB = 5
DT_SYMTAB = 6
DT_RELA = 7
DT_RELASZ = 8
DT_RELAENT = 9
DT_STRSZ = 10
DT_SYMENT = 11
DT_PLTREL = 20
DT_JMPREL = 23
DT_GNU_HASH = 0x6FFFFEF5
DT_OS_SYMTABSZ = 0x6100003F
R_X86_64_GLOB_DAT = 6
R_X86_64_JUMP_SLOT = 7


def gnu_hash(name):
    h = 5381
    for b in name.encode():
        h = ((h * 33) + b) & 0xFFFFFFFF
    return h


def elf_hash(name):
    h = 0
    for b in name.encode():
        h = (h << 4) + b
        h = (h ^ ((h & 0xF0000000) >> 24)) & 0x0FFFFFFF
    return h


def file_offset(elf, vaddr):
    for _, _, offset, vaddr_start, _, filesz, memsz, _ in loads(elf):
        if vaddr_start <= vaddr < vaddr_start + memsz:
            assert vaddr - vaddr_start < filesz, hex(vaddr)
            return offset + (vaddr - vaddr_start)
    raise AssertionError(hex(vaddr))


def dynamic_tags(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    for index in range(phnum):
        p_type, _, p_offset, _, _, p_filesz, _, _ = struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize)
        if p_type == 2:
            tags = {}
            for off in range(p_offset, p_offset + p_filesz, 16):
                tag, val = struct.unpack_from("<qQ", elf, off)
                if tag == 0:
                    break
                tags.setdefault(tag, val)
            return tags
    raise AssertionError("missing PT_DYNAMIC")


def read_cstr(elf, strtab_off, strsz, offset):
    assert offset < strsz, (offset, strsz)
    end = elf.index(0, strtab_off + offset, strtab_off + strsz)
    return elf[strtab_off + offset:end].decode()


def build_structural_fixture(exports, imports, needed):
    image = fixture()
    struct.pack_into("<I", image, 64 + 4, 7)
    code = bytearray()
    if imports:
        code += b"\xff\x15" + struct.pack("<i", 0x500 - (len(code) + 6))
        code += b"\xe8" + struct.pack("<i", 0x30 - (len(code) + 5))
    code += bytes.fromhex("b82a000000c3")
    image[0x4000:0x4000 + len(code)] = code
    image[0x4020] = 0xC3
    thunk = (
        b"\xff\x25" + struct.pack("<i", 0x508 - 0x36)
        + b"\x68" + struct.pack("<I", 0)
        + b"\xe9" + struct.pack("<i", 0x20 - 0x40)
    )
    image[0x4030:0x4030 + len(thunk)] = thunk
    image[0x4210:0x4216] = bytes.fromhex("b82a000000c3")

    strings = bytearray(b"\0")
    sym_entries = [(0, 0, 0, 0, 0, 0)]
    for name in imports:
        off = len(strings)
        strings.extend(f"{name}#A#B\0".encode())
        sym_entries.append((off, 0x12, 0, 0, 0, 0))
    for name in exports:
        off = len(strings)
        strings.extend(f"{name}#A#B\0".encode())
        sym_entries.append((off, 0x12, 0, 1, 0x210, 6))
    needed_offsets = []
    for lib in needed:
        needed_offsets.append(len(strings))
        strings.extend(lib.encode() + b"\0")

    image[0x4600:0x4600 + len(strings)] = strings
    for index, entry in enumerate(sym_entries):
        struct.pack_into("<IBBHQQ", image, 0x4900 + index * 24, *entry)

    rela_size = 0
    jmprel_size = 0
    if imports:
        struct.pack_into("<QQq", image, 0x4780, 0x508, (1 << 32) | R_X86_64_JUMP_SLOT, 0)
        jmprel_size = 24
        struct.pack_into("<QQq", image, 0x4700, 0x500, (2 << 32) | R_X86_64_GLOB_DAT, 0)
        rela_size = 24

    tags = [
        (DT_STRTAB, 0x600),
        (DT_STRSZ, len(strings)),
        (DT_SYMTAB, 0x900),
        (DT_SYMENT, 24),
        (DT_RELA, 0x700),
        (DT_RELASZ, rela_size),
        (DT_RELAENT, 24),
        (DT_OS_SYMTABSZ, len(sym_entries) * 24),
    ]
    if jmprel_size:
        tags.extend([
            (DT_PLTGOT, 0x520),
            (DT_PLTREL, DT_RELA),
            (DT_JMPREL, 0x780),
            (DT_PLTRELSZ, jmprel_size),
        ])
    for off in needed_offsets:
        tags.append((1, off))
    tags.append((0, 0))
    struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x4800, 0x800, 0x800, len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4800 + index * 16, *tag)
    return image


def verify_structural_output(elf, exports, imports, helpers, base):
    tags = dynamic_tags(elf)
    strtab_off = file_offset(elf, tags[DT_STRTAB])
    strsz = tags[DT_STRSZ]
    symtab_off = file_offset(elf, tags[DT_SYMTAB])
    assert tags[DT_SYMENT] == 24

    hash_off = file_offset(elf, tags[DT_HASH])
    sysv_nbuckets, nchain = struct.unpack_from("<II", elf, hash_off)
    sysv_buckets = struct.unpack_from(f"<{sysv_nbuckets}I", elf, hash_off + 8)
    sysv_chains = struct.unpack_from(f"<{nchain}I", elf, hash_off + 8 + sysv_nbuckets * 4)

    expected_undef = list(imports) + list(helpers)
    expected_symoffset = 1 + len(expected_undef)
    expected_total = expected_symoffset + len(exports)
    assert nchain == expected_total, (nchain, expected_total)
    assert sysv_nbuckets == (len(exports) if exports else 1), (sysv_nbuckets, exports)

    symbols = []
    for index in range(nchain):
        st_name, st_info, st_other, st_shndx, st_value, st_size = struct.unpack_from(
            "<IBBHQQ", elf, symtab_off + index * 24
        )
        name = read_cstr(elf, strtab_off, strsz, st_name) if st_name else ""
        symbols.append((name, st_info, st_other, st_shndx, st_value, st_size))

    assert symbols[0] == ("", 0, 0, 0, 0, 0), symbols[0]

    gnu_off = file_offset(elf, tags[DT_GNU_HASH])
    gnu_nbuckets, symoffset, bloom_size, bloom_shift = struct.unpack_from("<IIII", elf, gnu_off)
    assert all(symbols[i][3] == 0 for i in range(symoffset)), (symoffset, symbols)
    assert all(symbols[i][3] != 0 for i in range(symoffset, nchain)), (symoffset, symbols)
    assert symoffset == expected_symoffset, (symoffset, expected_symoffset, symbols)
    assert gnu_nbuckets == (len(exports) if exports else 1), (gnu_nbuckets, exports)
    for index, expected_name in enumerate(expected_undef, 1):
        name, st_info, st_other, st_shndx, st_value, st_size = symbols[index]
        assert name == expected_name, (index, name, expected_name)
        assert (st_info, st_other, st_shndx, st_value, st_size) == (0x12, 0, 0, 0, 0), (index, symbols[index])

    bloom_off = gnu_off + 16
    bloom = struct.unpack_from(f"<{bloom_size}Q", elf, bloom_off)
    buckets_off = bloom_off + bloom_size * 8
    gnu_buckets = struct.unpack_from(f"<{gnu_nbuckets}I", elf, buckets_off)
    chains_off = buckets_off + gnu_nbuckets * 4

    if not exports:
        assert symoffset == nchain, (symoffset, nchain)
        assert bloom == (0,) and gnu_buckets == (0,), (bloom, gnu_buckets)
        assert all(b == 0 for b in sysv_buckets) and all(c == 0 for c in sysv_chains)
    else:
        gnu_chains = struct.unpack_from(f"<{len(exports)}I", elf, chains_off)
        max_bucket = max(gnu_buckets)
        assert symoffset <= max_bucket < nchain, (symoffset, max_bucket, nchain)
        idx = max_bucket
        while (gnu_chains[idx - symoffset] & 1) == 0:
            idx += 1
            assert idx < nchain, (idx, nchain)
        gnu_derived_count = idx + 1
        assert gnu_derived_count == nchain, (gnu_derived_count, nchain)

        defined_names = [symbols[i][0] for i in range(symoffset, nchain)]
        assert set(defined_names) == set(exports), (defined_names, exports)
        for i in range(symoffset, nchain):
            _, st_info, st_other, st_shndx, st_value, st_size = symbols[i]
            assert (st_info, st_other, st_shndx, st_value, st_size) == (0x12, 0, 1, base + 0x210, 6), symbols[i]

        bucket_keys = [gnu_hash(name) % gnu_nbuckets for name in defined_names]
        assert bucket_keys == sorted(bucket_keys), (defined_names, bucket_keys)

        for bucket in range(gnu_nbuckets):
            expected_indices = [symoffset + i for i, key in enumerate(bucket_keys) if key == bucket]
            if not expected_indices:
                assert gnu_buckets[bucket] == 0, (bucket, gnu_buckets[bucket])
                continue
            assert gnu_buckets[bucket] == expected_indices[0], (bucket, gnu_buckets[bucket], expected_indices)
            walked = []
            cur = gnu_buckets[bucket]
            while True:
                walked.append(cur)
                chain_word = gnu_chains[cur - symoffset]
                assert (chain_word & ~1) == (gnu_hash(symbols[cur][0]) & ~1)
                if chain_word & 1:
                    break
                cur += 1
            assert walked == expected_indices, (bucket, walked, expected_indices)

        for name in exports:
            h = gnu_hash(name)
            word = bloom[(h // 64) % bloom_size]
            mask = (1 << (h % 64)) | (1 << ((h >> bloom_shift) % 64))
            assert (word & mask) == mask, (name, hex(word), hex(mask))
            cur = gnu_buckets[h % gnu_nbuckets]
            assert cur >= symoffset, (name, cur, symoffset)
            found = None
            while True:
                chain_word = gnu_chains[cur - symoffset]
                if (chain_word | 1) == (h | 1) and symbols[cur][0] == name:
                    found = cur
                    break
                if chain_word & 1:
                    break
                cur += 1
            assert found is not None and symbols[found][0] == name, (name, found)

            sh = elf_hash(name)
            scur = sysv_buckets[sh % sysv_nbuckets]
            sfound = None
            while scur != 0:
                if symbols[scur][0] == name:
                    sfound = scur
                    break
                scur = sysv_chains[scur]
            assert sfound == found, (name, sfound, found)
        assert all(c == 0 for c in sysv_chains[:symoffset]), sysv_chains[:symoffset]

    rela_bindings = []
    if DT_RELA in tags and tags.get(DT_RELASZ, 0):
        rela_off = file_offset(elf, tags[DT_RELA])
        for off in range(rela_off, rela_off + tags[DT_RELASZ], 24):
            r_offset, r_info, r_addend = struct.unpack_from("<QQq", elf, off)
            r_sym = r_info >> 32
            r_type = r_info & 0xFFFFFFFF
            assert 0 < r_sym < symoffset <= nchain, (r_sym, symoffset, nchain)
            assert r_type == R_X86_64_GLOB_DAT and r_addend == 0, (r_type, r_addend)
            rela_bindings.append((r_offset, r_sym, symbols[r_sym][0]))

    expected_rela_names = ([] if not imports else [imports[1]]) + list(helpers)
    assert [name for _, _, name in rela_bindings] == expected_rela_names, (rela_bindings, expected_rela_names)
    if imports:
        assert rela_bindings[0][:2] == (base + 0x500, 2), rela_bindings[0]
    for idx, (_, r_sym, _) in enumerate(rela_bindings[len(imports[:1]):], 1 + len(imports)):
        assert r_sym == idx, (r_sym, idx, rela_bindings)

    jmprel_bindings = []
    if DT_JMPREL in tags and tags.get(DT_PLTRELSZ, 0):
        jmprel_off = file_offset(elf, tags[DT_JMPREL])
        for off in range(jmprel_off, jmprel_off + tags[DT_PLTRELSZ], 24):
            r_offset, r_info, r_addend = struct.unpack_from("<QQq", elf, off)
            r_sym = r_info >> 32
            r_type = r_info & 0xFFFFFFFF
            assert 0 < r_sym < symoffset <= nchain, (r_sym, symoffset, nchain)
            assert r_type == R_X86_64_JUMP_SLOT and r_addend == 0, (r_type, r_addend)
            jmprel_bindings.append((r_offset, r_sym, symbols[r_sym][0]))

    if imports:
        assert jmprel_bindings == [(base + 0x508, 1, imports[0])], jmprel_bindings
    else:
        assert not jmprel_bindings, jmprel_bindings


def check_structural_exports(relinker, work):
    structural = work / "structural"
    structural.mkdir()
    helper_cases = (
        (("libSceAudioOut.prx",), False, ()),
        (("libc.prx",), True, ("TraceImport_nid_no_patch",)),
        (("libkernel.prx",), False, ("EnterGuestMainStack_nid_no_patch",)),
        (("libkernel.prx", "libc.prx"), True, ("TraceImport_nid_no_patch", "EnterGuestMainStack_nid_no_patch")),
    )
    export_cases = (
        (),
        ("FixtureExport",),
        ("Ae", "Ad", "Ac", "Ab"),
    )
    for exports in export_cases:
        for imports in (("ImportedPlt", "ImportedGlob"), ()):
            for needed, trace, helpers in helper_cases:
                image = build_structural_fixture(exports, imports, needed)
                source = structural / "input.elf"
                source.write_bytes(image)
                option_cases = [([], 0x400000), (["--linux-fixed-base", "0x800000"], 0x800000)]
                if not trace:
                    option_cases.append((["--lazy-binding"], 0))
                for options, base in option_cases:
                    cli = [*options, *(["--trace-imports"] if trace else [])]
                    output = structural / "output.elf"
                    result = subprocess.run(
                        [str(relinker), "--skip-sce-module", *cli, str(source), str(output)],
                        capture_output=True,
                        text=True,
                        timeout=20,
                    )
                    assert result.returncode == 0, (exports, imports, needed, cli, result.stdout, result.stderr)
                    verify_structural_output(output.read_bytes(), exports, imports, helpers, base)
                    output.unlink()


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-main-export-") as directory:
        work = Path(directory)
        check_structural_exports(relinker, work)
        if sys.platform != "linux" or platform.machine() != "x86_64":
            print("Linux executable exports structural tests passed; native execution skipped")
            return
        probe = work / "probe.c"
        probe.write_text(
            "#define _GNU_SOURCE\n#include <dlfcn.h>\n"
            "#include <unistd.h>\n"
            "void check(void) {\n"
            "    int (*function)(void) = dlsym(RTLD_DEFAULT, \"FixtureExport\");\n"
            "    _exit(function && function() == 42 ? 42 : 1);\n"
            "}\n")
        library = work / "probe.so"
        subprocess.run([os.environ.get("CC", "cc"), "-shared", "-fPIC", str(probe), "-o", str(library), "-ldl", "-Wl,-init,check"], check=True)
        image = fixture()
        strings = b"\0FixtureExport#A#B\0".ljust(32, b"\0") + str(library).encode() + b"\0"
        image[0x4600:0x4600 + len(strings)] = strings
        image[0x4210:0x4216] = bytes.fromhex("b82a000000c3")
        struct.pack_into("<IBBHQQ", image, 0x4900, 0, 0, 0, 0, 0, 0)
        struct.pack_into("<IBBHQQ", image, 0x4918, 1, 0x12, 0, 1, 0x210, 6)
        tags = [(5, 0x600), (10, len(strings)), (6, 0x900), (11, 24), (7, 0x700), (8, 0), (9, 24), (0x6100003f, 48), (1, 32), (0, 0)]
        struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x4800, 0x800, 0x800, len(tags) * 16, len(tags) * 16, 8)
        for index, tag in enumerate(tags):
            struct.pack_into("<qQ", image, 0x4800 + index * 16, *tag)
        source = work / "input.elf"
        source.write_bytes(image)
        for options in ([], ["--linux-fixed-base", "0x400000"]):
            output = work / "output.elf"
            result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)], capture_output=True, text=True)
            assert result.returncode == 0, (options, result.stdout, result.stderr)
            output.chmod(0o755)
            executed = subprocess.run([str(output)], capture_output=True)
            assert executed.returncode == 42, (options, executed.returncode, executed.stderr)
        struct.pack_into("<Q", image, 0x4800 + 7 * 16 + 8, 47)
        source.write_bytes(image)
        invalid = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(work / "invalid.elf")], capture_output=True, text=True)
        assert invalid.returncode == 2 and "dynamic symbol table size" in invalid.stderr, invalid.stderr
        multi_probe = work / "multi_probe.c"
        multi_probe.write_text(
            "#define _GNU_SOURCE\n#include <dlfcn.h>\n"
            "#include <unistd.h>\n"
            "void check(void) {\n"
            "    int (*fa)(void) = dlsym(RTLD_DEFAULT, \"Ae\");\n"
            "    int (*fb)(void) = dlsym(RTLD_DEFAULT, \"Ad\");\n"
            "    int (*fc)(void) = dlsym(RTLD_DEFAULT, \"Ac\");\n"
            "    int (*fd)(void) = dlsym(RTLD_DEFAULT, \"Ab\");\n"
            "    _exit(fa && fb && fc && fd && fa() == 42 && fb() == 42 && fc() == 42 && fd() == 42 ? 42 : 1);\n"
            "}\n")
        multi_lib = work / "multi_probe.so"
        subprocess.run([os.environ.get("CC", "cc"), "-shared", "-fPIC", str(multi_probe), "-o", str(multi_lib), "-ldl", "-Wl,-init,check"], check=True)
        multi_image = fixture()
        multi_names = ["Ae", "Ad", "Ac", "Ab"]
        multi_symbols = [(f"{n}#A#B", 0x12, 0, 1, 0x210, 6) for n in multi_names]
        multi_strings = bytearray(b"\0")
        for index, (name, info, visibility, section, value, size) in enumerate(multi_symbols, 1):
            offset = len(multi_strings)
            multi_strings.extend(name.encode() + b"\0")
            struct.pack_into("<IBBHQQ", multi_image, 0x4900 + index * 24, offset, info, visibility, section, value, size)
        multi_strings = multi_strings.ljust(64, b"\0") + str(multi_lib).encode() + b"\0"
        multi_image[0x4600:0x4600 + len(multi_strings)] = multi_strings
        multi_image[0x4210:0x4216] = bytes.fromhex("b82a000000c3")
        multi_tags = [(5, 0x600), (10, len(multi_strings)), (6, 0x900), (11, 24), (7, 0x700),
                      (8, 0), (9, 24), (0x6100003f, (len(multi_symbols) + 1) * 24), (1, 64), (0, 0)]
        struct.pack_into("<IIQQQQQQ", multi_image, 120, 2, 6, 0x4800, 0x800, 0x800, len(multi_tags) * 16, len(multi_tags) * 16, 8)
        for index, tag in enumerate(multi_tags):
            struct.pack_into("<qQ", multi_image, 0x4800 + index * 16, *tag)
        multi_source = work / "multi_input.elf"
        multi_source.write_bytes(multi_image)
        multi_output = work / "multi_output.elf"
        result = subprocess.run([str(relinker), "--skip-sce-module", str(multi_source), str(multi_output)], capture_output=True, text=True)
        assert result.returncode == 0, (result.stdout, result.stderr)
        multi_output.chmod(0o755)
        executed = subprocess.run([str(multi_output)], capture_output=True)
        assert executed.returncode == 42, (executed.returncode, executed.stderr)
        multi_sysv = work / "multi_sysv.elf"
        sysv_data = bytearray(multi_output.read_bytes())
        found_gnu_hash = False
        for i in range(0, len(sysv_data) - 16, 8):
            if struct.unpack_from("<q", sysv_data, i)[0] == 0x6ffffef5:
                struct.pack_into("<q", sysv_data, i, 0x6ffffdf8)
                found_gnu_hash = True
                break
        assert found_gnu_hash
        multi_sysv.write_bytes(sysv_data)
        multi_sysv.chmod(0o755)
        executed_sysv = subprocess.run([str(multi_sysv)], capture_output=True)
        assert executed_sysv.returncode == 42, (executed_sysv.returncode, executed_sysv.stderr)
    print("Linux executable exports test passed")


if __name__ == "__main__":
    main()
