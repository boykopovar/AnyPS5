# Reading titles and libraries

Many exports have no source that gives their signature or behaviour. Two places still hold them: the code of a title
that calls the export (its call sites) and the code of the library itself. This page covers how to read both, what each
one settles, and how to cite it.

## Sources, strongest first

1. **A console measurement**: a homebrew payload that calls the function and records what it returns, with the
   firmware version (for example `sceVoiceQoSEnd`, #1407).
2. **The library's own code**: a firmware module, or a copy of an SDK module that a title ships in `sce_module/`. It gives
   behaviour, error codes, layouts and validation order ([libSceJobManager](debt/libSceJobManager.md),
   [libSceRtc](debt/libSceRtc.md), [libSceHttp](debt/libSceHttp.md)). A module a title ships is not reimplemented
   ([CONTRIBUTING](../../CONTRIBUTING.md)); it can still be read to learn how a system library behaves.
3. **Other implementations and SDK bindings**: KytyPS5, shadPS4, fpPS4, SharpProspero, Orbital. PS4 sources may differ
   from the PS5 ABI.
4. **A title's call sites**: the arguments, their widths, buffer sizes, which pointers are outputs and which results the
   title checks. They show what the caller expects, not what the library does.

When sources disagree, say so in the pull request and in the debt entry.

[prosper](https://github.com/mattias800/prosper)'s console oracle has published such measurements for libSceRtc, libc, libSceHttp, libSceNet, libSceUserService, libkernel and libSceAgc's packet builders. `python3 tools/library-oracle/library_oracle.py` replays them against the built modules, after `cmake --build build --target library_oracle_replay`: it downloads the measurements from a pinned commit, checks their digest and fails on a result that is not in [known.tsv](../../tools/library-oracle/known.tsv). A pull request that fixes a listed case removes its line, and one that adds a difference explains it there.

## Public source baselines

Some libraries are builds of public projects. The library's debt file records which release its entries follow,
and an entry cites the file at that release. This can differ from the submodule the repository bundles, which some
adapters call into.

| Library | Project | Entries follow | Bundled submodule |
| --- | --- | --- | --- |
| [libwebrtc](debt/libwebrtc.md) | WebRTC | code that is the same at M127 and M134 (its Source revision entry) | none |
| [libSceLibreSsl](debt/libSceLibreSsl.md) | LibreSSL | 2.5.4 | `3rdparty/libressl-openbsd` (OpenBSD 7.0) |
| [libSceLibreSsl3](debt/libSceLibreSsl3.md) | LibreSSL | 4.0.0 | `3rdparty/libressl-openbsd` (OpenBSD 7.0) |
| [libSceFreeType](debt/libSceFreeType.md), [libSceFreeTypeOt](debt/libSceFreeTypeOt.md) | FreeType | 2.13.3 | `3rdparty/freetype` (2.15.0) |
| [libfreetype](debt/libfreetype.md) | FreeType | 2.15.0 | `3rdparty/freetype` (2.15.0) |
| [libSceIcu](debt/libSceIcu.md) | ICU | 68 for names (`icu_68`), 78.1 for behaviour | `3rdparty/icu` (78.1) |
| [libharfbuzz](debt/libharfbuzz.md) | HarfBuzz | 10.0.1 | `3rdparty/harfbuzz` (10.0.1) |
| [libfontconfig](debt/libfontconfig.md) | Fontconfig | 2.15.0 | `3rdparty/fontconfig` (2.15.0) |
| [libSceJxr](debt/libSceJxr.md) | jxrlib | v2019.10.9 | `3rdparty/jxrlib` (v2019.10.9) |
| [libSceNKWebKit](debt/libSceNKWebKit.md) | WebKit | the PS5 firmware 11.x WebKit source | none |
| [libc](debt/libc.md), [libkernel](debt/libkernel.md), [libkernel_sys](debt/libkernel_sys.md), [libSceNet](debt/libSceNet.md), [libScePosixForWebKit](debt/libScePosixForWebKit.md), [libSceUsbd](debt/libSceUsbd.md) | FreeBSD | 11.4 | none |
| [libSceLibcInternal](debt/libSceLibcInternal.md) | FreeBSD, Microsoft STL (Dinkumware) | FreeBSD 11.4 for C functions, the Microsoft STL for C++ helpers | none |

The guest `std::string` is the 0x28-byte object in [GuestLocale.hpp](../../core/libs/prx/libc/include/GuestLocale.hpp),
not libc++'s 24-byte one. The layouts of the other standard containers are not settled yet (#18830).

## What a call site settles

The PS5 uses the System V AMD64 calling convention: integer and pointer arguments in `rdi`, `rsi`, `rdx`, `rcx`, `r8`,
`r9`, then the stack; floating-point arguments in `xmm0`-`xmm7`; the result in `rax` or `xmm0`.

| Seen at the call site | Settles |
| --- | --- |
| Registers written right before the `call` | The argument count. A register set long before the call, or for another use, is not an argument; check several call sites |
| `mov %esi` / `mov %rsi`, `movzwl`, `movsbl` | The argument width and signedness |
| `lea -0x2030(%rbp), %rsi` | A pointer to a caller buffer; the stack layout gives its size |
| A value stored at the pointer before the call and read after it | An in/out argument (capacity in, size out) |
| `test %eax, %eax` / `cmp`, `js` after the call | An `int32_t` result and which values the title treats as failure |
| The result is never read | Nothing: the return type is not established ([libSceAgc](debt/libSceAgc.md)) |
| A log string that names the function (`"sceXxx() failed. Error code %#x"`) | Which import the call is, and that failures are SCE error codes |
| A first call with a null buffer, then a second one with a buffer of the returned size | A size query ([libSceNpTrophy2](debt/libSceNpTrophy2.md)) |

A call site doesn't settle what the library writes, which error codes it returns, its side effects, or the values a
title happens to accept. "The title continues if it gets 0" is not "the console returns 0". Everything that the call
site and the other sources leave open throws (`NotImplemented_nid_no_patch`), as in
[libSceNpManager](debt/libSceNpManager.md): the signature came from a call site, what the library writes into the
buffer did not, so the export still throws. A silent stub is allowed only when it affects the UI
([CONTRIBUTING](../../CONTRIBUTING.md)): a read that always reports no data, or a write that drops its data, is not UI.

## Finding the call sites

1. Unwrap the title's decrypted `eboot.bin` with `tools/unwrap_self.py`
   ([GAME_TESTING](GAME_TESTING.md)).
2. Relink it with `--registry` ([USAGE](../user/USAGE.md)). `app.registry.json` lists every import of the executable
   with its `nid`, `relocationType`, `targetOffset` (the GOT slot) and `callSites` (virtual addresses). Bundled modules'
   registries have no `callSites`. `tools/nid_names.py --name sceXxx` gives the NID of a name, and `--nid` the name of a
   NID (without the `#` suffix).
   - `R_X86_64_GLOB_DAT`: each call site is an instruction that reads the slot: a `call *slot(%rip)`, a tail call
     `jmp *slot(%rip)`, their far forms `lcall` and `ljmp`, a `push slot(%rip)`, or a 64-bit `mov slot(%rip), %reg`
     that loads the address for a later indirect call or a function pointer. Each of them may also carry the
     address-size prefix (`0x67`) and read the slot as `slot(%eip)`. At a `call` or a tail-call `jmp` the argument
     registers hold the import's arguments. A `push` or a `mov` only copies the address; the arguments are the
     registers at the instruction that later transfers control to it.
   - `R_X86_64_JUMP_SLOT`: the call site is the PLT stub (`jmp *slot(%rip)`); the callers are the `call <stub>`
     instructions.
3. Disassemble around the site. The ELF has no section headers, so `objdump -d` prints nothing; take the executable
   `LOAD` segment from `readelf -lW`, cut it out at its file offset and disassemble it at its virtual address:

   ```sh
   readelf -lW eboot.elf                      # executable LOAD: offset 0x4000, vaddr 0x0, filesz 0x1f36afc
   tail -c +$((0x4000 + 1)) eboot.elf | head -c $((0x1f36afc)) > text.bin
   objdump -D -b binary -m i386:x86-64 --adjust-vma=0x0 text.bin > text.dis
   grep -nE 'call +0x1f2e480$' text.dis       # callers of the JUMP_SLOT stub at 0x1f2e480
   ```

   For a relinked module (`*.guest.prx`), `tools/annotate_module.py <module> --out <prefix>` does this cut and
   disassembly for every executable segment and writes `<prefix>.asm`, `<prefix>.imports.txt` and
   `<prefix>.exports.txt`. It names each import call, PLT stub, GOT reference and export entry from the module's NIDs
   through the database of `tools/nid_names.py`, which is downloaded on first use unless `--db` names a CSV or
   `--no-names` prints the NIDs instead. It needs binutils `objdump`.

   To name symbols in Ghidra without disassembling or downloading a database:

   ```sh
   python3 tools/annotate_module.py module.guest.prx --symbols-ghidra --db aerolib.csv > module.symbols
   ```

   Save the ASCII output without a BOM. Load the same relinked ELF at its ELF virtual addresses and run
   [ImportSymbolsScript.py](https://github.com/NationalSecurityAgency/ghidra/blob/ac245cfdbd035c14180bb7a470e39348511b4ba2/Ghidra/Extensions/Jython/ghidra_scripts/ImportSymbolsScript.py)
   from Ghidra's Jython extension with this map. Function exports inside executable file-backed segments get `f`;
   aliases at the same address get `l`, keeping one function name. Import slots are `imp_` labels and other exports are labels too. TLS exports and
   unallocated common symbols are omitted because their values are not addresses. Names use ASCII letters, digits and
   underscores: other characters become underscores, a leading digit gets an underscore prefix, and names are limited
   to Ghidra's 2000 characters. Collisions get `_2`, `_3`, and so on within that limit.
   This mode reads only a local NID database or cache, prints to stdout and cannot be combined with `--symbols-json`
   or `--out`. Without a local database it uses NIDs; `--no-names` skips the database entirely.

   A disassembler that loads the program headers (Ghidra, IDA) does the same segment loading.
4. Log strings near the call confirm which import it is: `strings -t x eboot.elf`, then the `lea` that loads that address.

The same steps work on a module: the code of a firmware module or of one in `sce_module/` gives the library's side of
the contract.

## Citing it

In the pull request and in the library's [debt](debt/) entry, say:

- Where it came from: the title ID and the call-site addresses (`PPSA03393, 0x76D132`), or the module and firmware
  version (`libSceRtc.sprx, firmware 13.60`).
- What each site showed: "`rsi` points to a 0x2000-byte stack buffer and `rdx` to its capacity, which the title reads
  back as the size".
- What was not verified: "read from disassembly, not measured on a console", "the return value is not read".

Quote only the instructions that show the claim, not whole functions. The code stays generic: a title ID belongs in the
debt entry, never in code or test names.

## Rules

- Work only from dumps of titles and firmware that you own. Never add, attach or link game files, firmware, decrypted
  binaries or keys in the repository, a pull request or an issue.
- Write the implementation from the contract you read (arguments, sizes, values, order, errors), not by translating the
  disassembly instruction by instruction or copying its structure. Rebuild data tables from a public source when one
  exists ([libSceIme](debt/libSceIme.md) rebuilds its codepage checks from Unicode's tables).
- The goal is interoperability: what a title needs from a system library to run. Nothing here is for bypassing
  licence, entitlement or online checks.
