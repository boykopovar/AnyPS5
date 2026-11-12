# Relinker usage

**Web converter:** https://boykopovar.github.io/AnyPS5/ (your files stay on your device)

---

## 1. Web converter

### Saving to a folder (Linux output)
- In supported browsers, the converter can save Linux output as an uncompressed folder.
- It will not overwrite an existing destination folder.
- If conversion fails or is cancelled, it stops the current file and tries to delete the new output folder.
- If the browser blocks that cleanup, the error tells you which folder may hold incomplete files.

### Converting many apps at once
- Select or drop several app ZIP archives. They are converted into one ZIP bundle.
- Each app is inspected separately. Its executable, libraries and `app0/` files go in a numbered folder, so archives with the same filename stay separate.
- Batch output always uses ZIP mode. Saving to an uncompressed folder works for one Linux app at a time only.
- The 256 MiB memory-download limit applies to the whole bundle. For larger batches use **Save ZIP to disk**.
- If a batch is cancelled or any app fails, the incomplete bundle is aborted. Any cleanup failure is reported instead of the bundle being offered as finished.

---

## 2. Input and conversion

### Size limit for SELF files
The SELF unwrapper refuses containers that need more than 64 MiB of total alignment padding. It checks this before allocating the output buffer. The limit counts padding across file-backed and empty segments (not segment data). A refusal leaves the input and any existing output file untouched.

### What you need
Use a clean ELF executable. Put its bundled ELF modules in one of these folders beside the executable: `sce_module/`, `sce_modules/` or `prx/`. Or put them under a parent folder you choose with `--sce-module-path <path>`.

- `--sce-module-path` accepts absolute paths, or paths relative to the current working directory. Give the parent folder, not the `sce_module` folder itself.
- Only files directly inside the module folders are processed.
- SELF/fSELF containers are skipped with a warning. Unwrap them first if you need them included. Use `tools/unwrap_self.py <input.self> <output.elf>` for a decrypted container. It places segments at the offsets of the SELF segment records and restores `p_offset % p_align == p_vaddr % p_align`. It refuses a container whose load segments all look encrypted.
- Backup copies are skipped (with a message), on the command line and in the web converter. A module counts as a backup when its filename extends the name of another ELF module in the same folder and both have the same SONAME. Example: a dumper's `libX.prx.esbak` or `libX.prxbak` next to an ELF `libX.prx`. A single backup beside a SELF container is still relinked.
- `prx/` can exist together with `sce_module/` or `sce_modules/`.
- Having both `sce_module/` and `sce_modules/` is an error.
- Having none of the three is an error, unless you give `--module-dir`.

**Deferred module start-up (Linux, macOS, Windows):** if the executable imports `sceKernelLoadStartModule`, a bundled module that neither the executable nor a required module needs is not started at launch. It starts when the title loads it, with the arguments from `sceKernelLoadStartModule`. Without that import, every bundled module starts at launch. On macOS, loading a deferred module this way has not been run.

Layout:

```text
source/
    input.elf
    sce_module/
        <bundled ELF modules>
```

Command:

```text
relinker [options] <input.elf> <output>
```

### Dry run
`relinker --dry-run [options] <input.elf> <output>`

- Runs the whole conversion in memory (bundled modules, extra executables, import analysis, byte patches).
- Lists every planned output: registry files, needed-library files, patch manifests, Windows diagnostics and Linux desktop metadata.
- Creates no folders, writes no files, changes no permissions. Existing outputs are kept.
- Conversion failures still return `2`.
- The output path is still required because it decides the layout.
- It does not check write access to the destination or filesystem aliases of files that don't exist yet. Only a normal conversion checks those.
- `--dry-run` conflicts with `--autorun` and `--verify-patches`.

### Module folders without a standard name
- `--module-dir` works even if the module parent has no `sce_module`, `sce_modules` or `prx` folder.
- For a homebrew bundle with shared libraries next to its executable, use `--module-dir .` (no empty standard folder needed).
- Without an explicit module directory, the standard-folder requirement stays.

### Modules that are needed but not converted
A needed module that wasn't converted is searched for under the module parent directory, outside the module folders. Each SELF container or `ET_SCE_DYNAMIC` ELF found there is reported after conversion if its name matches (up to the `.prx`, `.sprx`, `.suprx` or `.debug_prx` suffix; case ignored with `--windows`). The report shows which `--module-dir` would include it. The output still refers to a host library of that name. Unreadable files are not reported.

### Example commands

Linux:
```sh
relinker source/input.elf app.elf
```

Windows:
```sh
relinker --windows source/input.elf app.exe
```

macOS (x86-64 Mach-O; runs under Rosetta on Apple silicon):
```sh
relinker --macos source/input.elf eboot
```

- Add `--to-intel` for Intel hosts, or `--to-rosetta` for Rosetta. A title that runs under Rosetta on Apple silicon is relinked with `--macos --to-rosetta`.
- `--to-intel` rewrites AMD-specific instructions in the executable and bundled modules. It cannot translate AMD-specific instructions a title generates at runtime, so such titles need an AMD host.
- The output format defaults to Linux ELF whatever the filename is. `.exe` alone does not select Windows.

### Security warning
A converted title is an ordinary program. It runs with your user account's permissions, can read and write whatever your account can, and has none of the console's sandbox. Convert and run only titles and modules you trust. Run them as a separate user, or in a container or VM, to keep them away from your files.

It also has no address space layout randomization (ASLR):
- **Linux:** loaded at `0x400000` or at the `--linux-fixed-base` address (unless the deprecated `--lazy-binding` keeps it position independent).
- **Windows:** the lowest ELF load page goes at the preferred `0x400000` when that range is free, or at the `--windows-fixed-base` address, without `DYNAMIC_BASE`.
- **macOS:** the executable is written without `MH_PIE`.

### CPU requirements
Titles run natively, so your CPU needs the PS5 Zen 2 extensions that compiled code uses without checking, including AVX2, FMA, F16C, BMI1, BMI2, LZCNT and MOVBE. On Linux and Windows a converted title logs the host CPU's extensions at start-up. If one is missing, it stops before running any title code and names the CPU and the missing extensions ([list](../dev/debt/libkernel.md)).

### Output file collisions
The executable, registry, Windows import diagnostics, icon and desktop entry output files must not refer to the input executable or a bundled module being converted. This includes hard links and symbolic links. These collisions fail before anything is written. An existing output file can be replaced if it is separate from those inputs. An output path that is an existing directory also fails before anything is written.

---

## 3. Options

All switches are off by default. `unused-filter` defaults to `0`. `--rpath` defaults to `$ORIGIN/libs`.

### General
| Option | What it does |
|---|---|
| `--help` | Print usage to stdout and exit `0`. No input or output paths needed. |
| `--version` | Print the relinker build revision (from `git describe`) to stdout and exit `0`. No paths needed. |
| `--autorun` | Run the output after conversion, print its exit code, wait for Enter. Needs the target OS and a prepared runtime layout. |
| `--registry` | Write `<output-stem>.registry.json` beside the output executable, and `<output-stem>.<module>.registry.json` for each converted bundled module. |
| `--rpath <path>` | Set the system library search path. Quote `$ORIGIN` so the shell doesn't expand it, e.g. `--rpath '$ORIGIN/libs'` in Bash or PowerShell. **Linux:** every colon-separated entry must be nonempty; with guest modules, each must be absolute or start with `$ORIGIN`. **Windows:** needs a nonempty ASCII path; `$ORIGIN` means the executable's folder. |
| `--omit-unavailable-dependencies` | Linux only. Leave out a bundled module's dependency if no converted module provides it, the module imports nothing from it, and no `--rpath` folder holds it at conversion time. Every run-path folder must exist at that time. Without the switch such a dependency is kept and a note names it. Use only when the run path holds every system library: a library found later through `LD_LIBRARY_PATH` is no longer loaded. |

### Target platform
| Option | What it does |
|---|---|
| `--windows` | Produce a Windows PE executable. |
| `--macos` | Produce an x86-64 Mach-O executable. Bundled modules become Mach-O libraries under `app0/`, each in the folder it was found in (`sce_module`, `sce_modules`, `prx` or a `--module-dir`). Conflicts with `--windows`. |
| `--windows-diagnostics` | Include startup dependency diagnostics. Requires `--windows`. |
| `--windows-gui` | Use the Windows GUI subsystem instead of the console subsystem. Requires `--windows`. |

### Fixed load addresses
| Option | What it does |
|---|---|
| `--windows-fixed-base <address>` | Map the lowest ELF load page at this fixed Windows address instead of `0x400000` (decimal or `0x` hex). Needs `--windows`, a 64 KiB-aligned address from `0x20000` to `0x7fffffff0000`, and a whole image that fits in user address space. The main executable gets no ASLR and no base relocations; if the address is occupied, loading fails. Bundled modules keep normal placement. |
| `--linux-fixed-base <address>` | Load the Linux executable at this fixed address instead of `0x400000` (decimal or `0x` hex). Every virtual address is offset by it, so virtual address 0 lands at that address, like on the console. Needs a 64 KiB-aligned address from `0x10000` to `0x7fffffff0000`, a multiple of every `PT_LOAD` alignment, and an image that ends in user address space. Written as `ET_EXEC` without ASLR. Bundled modules keep normal placement. Conflicts with `--windows`, `--macos` and `--lazy-binding`. |
| `--macos-fixed-base <address>` | Map the lowest ELF load page of the macOS executable at this address (decimal or `0x` hex). Needs `--macos`, a 64 KiB-aligned address from `0x20000` to `0x7fffffff0000`, and an image that ends in macOS user address space. `__PAGEZERO` ends 64 KiB below the address and the Mach-O header occupies those 64 KiB. No ASLR; if the process can't map the address, loading fails. Bundled modules keep normal placement. |

### CPU instruction conversion
| Option | What it does |
|---|---|
| `--to-intel` | Convert supported AMD-only instructions in the executable and bundled modules. Unsupported instructions or unreachable conversion stubs cause an error. |
| `--to-rosetta` | Does what `--to-intel` does, and also lowers what Rosetta lacks: ANDN → scalar NOT/AND; BLSI, BLSMSK, BLSR → scalar LEA/AND/XOR/NEG sequences; SHLX, SHRX, SARX, RORX → flag-preserving scalar shifts/rotates; BZHI, BEXTR → scalar shift-and-mask sequences; RDSEED → RDRAND; CLWB → NOP; XSAVEOPT → XSAVE; RDPID and the ECX of RDTSCP return processor 0. Implies `--to-intel`. Independent of `--macos` (`--macos` alone leaves these instructions as they are). Conflicts with `--windows`. |

### Imports
| Option | What it does |
|---|---|
| `--trace-imports` | Send each function import through the libc.prx call tracer (see "Tracing system library calls"). Windows, Linux, macOS. Conflicts with `--lazy-binding`. |
| `unused-filter=0` | Keep all imported NID references. |
| `unused-filter=1` | Filter unused non-PLT imports using control-flow and GOT access analysis (address-taken code counts as reachable). PLT imports are preserved. |
| `unused-filter=2` | Strict unused-import analysis and PLT compaction. Unsupported analysis cases cause an error. |

Write `unused-filter=0|1|2` **without** `--`, at most once.

### Module and executable selection
| Option | What it does |
|---|---|
| `--sce-module-path <path>` | The **parent** of the module folders, not the module folder itself. Default: the input ELF's parent. Relative input paths resolve from the working directory. Explicit absolute path: used directly. Explicit relative path (including `.` and `..`): relative to the working directory. The output path has no effect. Needs exactly one nonempty value. Repeats, nonexistent or non-directory paths, access failures, and use with `--skip-sce-module` are errors. |
| `--module-dir <dir>` | Also relink the ELF modules in this folder under the module parent, besides `sce_module`/`sce_modules`/`prx`. Outputs keep the relative path. Needed modules are mapped at startup. Repeatable. `.` means the parent itself, without the input ELF. Conflicts with `--skip-sce-module`. Details below. |
| `--executable <path>` | Also convert another executable of the title, which it starts with `sceSystemServiceLoadExec`. The path is relative to the module parent, e.g. `picker/picker.bin`. It is written beside the output, named after the path with `/` replaced by `.` and given the output's extension (`picker.picker.bin.exe`). It uses the same `libs/`, `app0/` and guest modules. It is an error if a guest module converts differently for it. Repeatable. Conflicts with `--skip-sce-module`. |

How `--module-dir` modules start:
- **Linux:** modules in these extra folders initialize only when explicitly loaded, with the arguments from `sceKernelLoadStartModule` (for example Unity plugins in `Media/Plugins`).
- **Windows:** such a module initializes when explicitly loaded, after the modules it needs that initialize the same way (they get the same start arguments). Exception: if a module outside these folders needs it (directly or through other modules), it initializes at startup. The first explicit load of any such module also initializes, best effort, the others that a module depends on.
- **macOS:** a module that another module or the executable needs initializes at startup. Others initialize when loaded.

### Patches
| Option | What it does |
|---|---|
| `--patches <file>` | Apply the byte patches in this file during conversion (see "Game patches"). Overrides `relinker-patches.toml` discovery. Conflicts with `--no-patches`. |
| `--no-patches` | Apply no patches and don't load `relinker-patches.toml` from the input folder. |
| `--verify-patches <file>` | Check a patch file against `<path>` without writing anything (see "Game patches"). Takes one path and no output path. |
| `--verify-output` | Together with `--verify-patches`: treat `<path>` as a converted Linux output instead of an input executable. |

### Advanced / debugging
| Option | What it does |
|---|---|
| `--function-symbol-roots` | Treat defined function symbols as code entry points instead of trusting their `st_size` ranges during export validation and Windows/macOS code analysis. For modules whose symbol sizes describe different code, such as generated loader companions. Function entries must stay executable and file-backed; data, TLS and unwind ranges keep their checks. Original symbol sizes are preserved. |
| `--frame-red-zone` | Windows only. Gives leaf functions that keep values below RSP (the System V red zone) a frame, so an exception Windows delivers on a guest thread can't overwrite them. Covers leaves with a `push rbp; mov rbp, rsp; push ...` prologue, leaves that only push registers (with or without a `sub` allocation after the pushes), and leaves with neither; a prologue or exit shorter than a jump takes in the instructions next to it. In leaves without a frame pointer, RSP is lowered only as far as the leaf reaches below it and their RSP operands move with it (in place, or in a stub if the encoding can't hold the moved displacement). Function bounds come from the executable's `.eh_frame`; the relinker stops with an error if it has none. It prints how many functions it framed and why it skipped the others, and refuses a game patch over a framed site. |

### Deprecated (debugging only)
If the app runs with these enabled, it will be extremely unstable and unsuitable for general use.

| Option | What it does |
|---|---|
| `--skip-sce-module` | Disable bundled module processing and module-directory checks. Conflicts with explicit `--sce-module-path`. |
| `--exclude-sce-module <file>` | Exclude a bundled module by exact filename (not path). Repeat for several; a missing filename is an error. Conflicts with `--skip-sce-module`. |
| `--skip-syscall-check` | Disable syscall scanning in the executable and bundled modules. |
| `--lazy-binding` | Lazy symbol binding instead of eager. Incompatible with bundled ELF modules. The Linux executable stays position independent (`ET_DYN`) instead of being placed at `0x400000`. Windows and Linux output only. |
| `--trap-sdk-syscall-stubs` | Debugging only. Replaces the exact ps5-payload-dev generic and numbered syscall wrapper stubs, the recognized inline mmap/mprotect sequences from SDK libc/mman.c, and the inlined `__syscall6` stubs (`mount`, `nmount`, `unmount`) from SDK libc/mount.c with `UD2`, so unused wrappers don't block relinking. Other syscall instructions still cause an error, except an `int 0x80; ret` between the `int 0x7f; ret` and `int 0x81; ret` entries of a software interrupt table, which becomes `UD2` even without this option. If a trapped wrapper runs, it raises an illegal-instruction exception. It does not implement FreeBSD syscalls or make these apps safe to run. |

### Unknown options and no arguments
Unknown options and extra positional arguments are errors. `relinker --help` prints usage to stdout and exits `0` without converting. Running `relinker` with no arguments prints usage to stderr and exits `1`.

---

## 4. Game patches

A patch file changes bytes in the input executable or in a bundled module before conversion. It's for titles that hard-code a value the host can't meet. The relinker checks the bytes it replaces, so a patch for another version of the title fails instead of corrupting it. With no patch file, the output is unchanged.

### Tools (need Python 3.11 or newer)

Check a local patch catalog against a title:
```sh
python3 tools/suggest_patches.py --catalog <directory> --param-json <sce_sys/param.json> --input <input.elf> --relinker <relinker> [--sce-module-path <directory>] [--module-dir <directory>]... [--executable <path>]... [--skip-sce-module] [--timeout-seconds <seconds>] [--list-verified]
```

Check every entry in a catalog before sharing it (no game files or relinker needed):
```sh
python3 tools/validate_patch_catalog.py --catalog <directory>
```

### What the validator checks
- Index format, duplicate JSON keys, required and optional fields
- Relative patch and target paths
- Patch-file presence, TOML structure, target-hash coverage
- Duplicate `id` entries that could match the same title build. Entries with the same `id` and content version may coexist when a shared target has different hashes (their matches are mutually exclusive).
- To publish a new `revision` for the same build, remove the old entry.
- An empty `entries` array is valid and reports `0 entries valid`.
- It reports each invalid entry and exits `2` if any is invalid. A valid catalog exits `0`.
- It is only a structural check. The relinker accepts a stricter subset of TOML, and only `relinker --verify-patches` with the title files can prove a patch parses and matches. The validator doesn't read target files, download anything or apply patches.

### What the advisor does
- Reads `titleId` and `contentVersion` from the given `param.json`.
- Checks the SHA-256 of every file a candidate patch targets.
- Calls the relinker given by `--relinker` to run `--verify-patches`. A relative path like `--relinker ./relinker` resolves to that local file.
- The explicit metadata path also works when `param.json` and the unwrapped ELF files are stored separately.
- The catalog folder needs an `index.json` with `format: 1` and an `entries` array. Each entry has `titleId`, `contentVersion`, a relative `patch` path, and `targets` mapping every patch target to its SHA-256 (hex digits, case-insensitive).

Example:
```json
{
  "format": 1,
  "entries": [{
    "titleId": "TEST00001",
    "contentVersion": "01.000.000",
    "patch": "patch.toml",
    "targets": {"Media/Modules/module.prx": "<SHA-256 in hexadecimal>"}
  }]
}
```

### Optional catalog entry fields
Catalogs without them behave the same and keep `format: 1`. Older advisor versions reject matching entries that contain these fields; use a newer advisor.

| Field | Value |
|---|---|
| `id` | Stable patch identifier matching `[a-z0-9][a-z0-9_-]*`. May be reused for another content version. Verified entries with the same ID for one title and version are rejected. |
| `name` | User-facing name for the whole patch file. |
| `category` | One of `compatibility`, `fps`, `graphics`, `memory`, `other`. |
| `description` | What the patch file changes. |
| `tested` | Nonempty array of test records (below). |
| `notes` | Limits or instructions for the user. |
| `contributors` | Nonempty array of distinct contributor names. |
| `source` | HTTPS link to the patch's provenance. The advisor only displays it; it doesn't fetch it. |
| `revision` | Positive integer revision of the patch definition (separate from the game's `contentVersion`). |

Notes:
- `category` describes the patch's area, not whether it's a fix or an enhancement. Use `description` to explain the effect.
- `revision` does not pick the newest entry automatically. To replace a patch definition with the same `id` for one title and content version, remove the old entry. If both revisions verify, the advisor rejects both as duplicate IDs.
- `name`, `description` and `notes` are nonempty single-line text when present. They describe the catalog entry as a whole. `name` and `description` inside a patch TOML's `[[patch]]` describe individual byte changes.

Each `tested` record needs:
- `stage`: `verification`, `conversion`, `launch` or `gameplay`
- `result`: `passed`, `failed` or `partial`
- `evidence`: nonempty single-line text
- Optional `environment` (single-line text) and optional `date` (`YYYY-MM-DD`)

Example:
```json
{
  "tested": [{
    "stage": "verification",
    "result": "passed",
    "environment": "CachyOS Linux",
    "date": "2026-10-26",
    "evidence": "Relinker reported 1 ok, 0 not-checked, 0 mismatch"
  }]
}
```

### How the advisor behaves
- It validates these fields for matching entries and reports a malformed entry without stopping the others. It prints metadata only for entries that pass the hash and relinker checks.
- Test records are contributor claims. The advisor doesn't independently confirm launch or gameplay evidence.
- It validates patch files only for entries matching the requested title and version. A malformed matching entry or a relinker verification error is reported and skipped, so another valid entry can still be recommended.
- Verification times out after 120 seconds per entry by default. Change it with `--timeout-seconds <positive integer>`.
- For synthetic inputs without system modules, use `--skip-sce-module` if conversion also uses it. It conflicts with `--sce-module-path`.
- **Exit codes:** no matching entry or no verified patch → `1`. Invalid input, or no verified patch after a relinker error → `2`.
- If several entries verify, the advisor recommends none (it can't choose or combine them) and points to `--list-verified`.
- It doesn't fetch or apply patches. A recommendation prints the `--patches` and module/executable options to use when converting.
- Keep catalog patch files **outside** the input executable's folder to avoid automatic `relinker-patches.toml` discovery.

`--list-verified` prints every entry that passes metadata, hash and relinker checks, in catalog order, with its catalog number, patch file, target hashes and conversion options. It exits `0` if at least one verifies (even if another was rejected); with none verified, the usual `1` or `2` applies. Without this option, a single verified entry is recommended and multiple verified entries stay ambiguous. The listing tells you to choose one patch file. It does not combine or apply patch files. Review the chosen patch before applying it.

### Target hashes and trust
- Target hashes identify the exact unwrapped files a patch changes, which might not include the main executable.
- Different dump tools could produce different bytes for the same title version, so the advisor then rejects a patch even if it would work. It isn't confirmed that all dump tools produce identical files.
- Hashes don't authenticate a catalog, prove a patch is safe, or prove the title is fully playable. Use catalogs from trusted sources and review patches before applying them.
- If a patch targets another title executable, pass `--executable <path>` to the advisor for each additional executable (paths relative to the module parent, e.g. `picker/picker.bin`). The advisor hashes those files and forwards the same options to `relinker --verify-patches`. Use the same `--executable` options when converting with `--patches`.

### Patch files
- `relinker-patches.toml` in the input executable's folder is loaded automatically and announced as `Patches: <path>`.
- `--patches <file>` names another file. `--no-patches` loads none.
- Each applied patch is reported as:

```
Patch: <target> [<name>] va=0x<address> off=0x<offset> <old> -> <new> (<description>)
```

The file is a subset of TOML (anything else is an error that names the line):
- UTF-8, with blank lines, `#` comments outside strings, `[[patch]]` entries and `key = value` lines
- Integers are decimal or `0x` hex
- Strings are double-quoted, with no escapes
- Keys match `[A-Za-z0-9_]+`

```toml
format = 1

[[patch]]
name        = "pool_size"
target      = "eboot.elf"
va          = 0x1234
type        = "i32"
expected    = 0x10000000
value       = 0x18000000
max         = 0x7f000000
align       = 0x1000
description = "pool size"
```

Top-level keys:
- `format = 1` is required.
- `relinker_version`, `relinker_commit`, `config`, `config_sha256`, `main_target`, `linux_base`, `settings` and `settings_sha256` are accepted in any file. Only `main_target` and `linux_base` are used (by `--verify-output`).
- Any other top-level key is an error.

Keys inside each `[[patch]]` entry:

| Key | Meaning |
|---|---|
| `name` | Optional. A setting name of `[a-z0-9_]`, unique in the file and not one of the reserved top-level names (`format` and the eight above). Shown in the progress line, the manifest and the verify report. |
| `target` | Required. The main executable's file name. For `--executable` files and bundled modules, the path relative to the module parent, e.g. `sce_module/libc.prx`. |
| `va` | Required. The ELF virtual address as a disassembler shows it. |
| `type` | Optional: `u8 u16 u32 u64 i8 i16 i32 i64`. Then `expected`, `value`, `min`, `max`, `align` are integers, range-checked and written little-endian. Without it, `expected` and `value` are hex byte strings of the same length. |
| `expected` | Required. The bytes the input must hold. |
| `value` | Required. The bytes to write. |
| `min`, `max`, `align` | Optional, typed entries only. Checked against `value`. |
| `description` | Optional. Reported and copied to the manifest. |

Tip: x86 `mov r/m64, imm32` and `add`/`sub`/`cmp r/m64, imm32` sign-extend their immediate. To patch a 64-bit field through an imm32, use `type = "i32"`, and add `min` to keep negative values out.

When patches are applied:
- Entries apply in file order.
- A patch range must lie in the file bytes of one `PT_LOAD` and outside the ELF header and program header table.
- Conversion fails before writing any output if: bytes differ from `expected`; a range is invalid; two entries overlap; a target matches no file or more than one file; or a range overlaps bytes the relinker itself rewrites (`--to-intel` replacements and trampolines, relinker patches, syscall traps) or the 8-byte target of a relocation.

### Patch manifest
When at least one patch is applied, `<output-stem>.patches.toml` is written beside the output. It lists every applied entry for every converted file, with:
- `main_target`
- `linux_base` (the address a Linux `ET_EXEC` output is placed at)
- `config` (the patch file path with forward slashes; reduced to the file name if the path contains a `"`, backslash or control character; conversion fails before writing any output if even that can't be held)

Typed entries keep their `type`, integers and `min`, `max`, `align`. `--patches` accepts the manifest to reproduce the conversion. The manifest must not replace the patch file it was read from.

### Verifying patches
`--verify-patches <file> <path>` writes nothing and exits `0` when no entry is a mismatch.

**Without `--verify-output`:**
- `<path>` is the input executable of any type.
- Modules are found as in a conversion (module options and `--executable` apply).
- Each entry is `applicable` (input holds `expected`) or `mismatch`.
- A target that matches nothing is an error suggesting `--verify-output`.

**With `--verify-output`:**
- `<path>` is a converted Linux ELF. PE and Mach-O outputs are refused.
- The manifest must have `main_target`, so verify the generated manifest, not a hand-written file.
- Entries are checked at their `va` plus the manifest's `linux_base` in an `ET_EXEC` output (which requires `linux_base`). An `ET_DYN` output (from `--lazy-binding`) requires a manifest without it.
- The entry whose target is `main_target` is checked against `<path>`. Entries for the `--executable` strings you give are checked against the output file derived as in a conversion. An entry holding `value` is `applied`.
- Every other entry is `not-checked (module, or executable not named with --executable)`, including a mistyped module target.
- Two targets with the same name (such as an `--executable` equal to `main_target`) give an `ambiguous patch target` error.
- Module discovery options are rejected with `--verify-output`.

Verify mode rejects conversion options (`--windows`, `--macos`, `--to-intel`, `--rpath`, `--patches`, `--no-patches` and the like) and a second path.

---

## 5. Runtime layout

### Fixed base addresses per platform
- **Windows:** the executable has no `DYNAMIC_BASE` and prefers its lowest ELF load page at `0x400000` (PE headers occupy the preceding 64 KiB; preferred base `0x3f0000`). Ruffle (PPSA68091), VLC (PPSA85300) and XPSemu (PPSA97358) expect their image there when they read their `app0/imports.txt`. Relocations are kept, so Windows can relocate it if that range is taken. If the executable needs an exact address, use `relinker --windows --windows-fixed-base <address> source/input.elf app.exe` (no relocations, so an occupied address fails to load). Modules keep `DYNAMIC_BASE`. This option doesn't change macOS output.
- **macOS:** for an executable that assumes its image starts at `0x400000` (the same three titles), use `relinker --macos --macos-fixed-base 0x400000 source/input.elf app`. Use it only when the executable requires a fixed address; otherwise macOS output places the lowest ELF load page at `0x140010000`.
- **Linux:** virtual address 0 goes at `0x400000` with no option, inside the 40-bit window a title's own `sceLibcMspaceCreate` checks. `--linux-fixed-base` selects another address. With `--lazy-binding` the executable stays position independent.
- The same base is applied to executables selected with `--executable`. Bundled PRX modules are not rebased.

### Folder layout
Paths are relative to the output executable:

```text
app.elf (Linux) or app.exe (Windows)
libs/
    *.prx
app0/
    <app resources>
    eboot.bin
    sce_module/
        <converted modules>
```

- Use `sce_modules/` or `prx/` instead of `sce_module/` if that was the input folder name. The relinker keeps each module's folder under `app0/` and prints the exact path.
- Put app resources in `app0/` yourself, including the title's original `eboot.bin`. Titles test `/app0/eboot.bin` to find their own folder, as it's always present on the console. (ProsperoTV, ProsperoRadio, ProsperoLight and ProsperoLichess fall back to an empty `/data/homebrew/<title id>` and fail to load their fonts without it.)
- Only the converted executable runs.
- **Linux symlinks:** a symbolic link inside the `app0/` tree is listed with the type of the file or directory it points to, so a linked directory is walked like a real one. Links inside a linked directory whose target is outside `app0/` keep the link type. For nested trees, link only top-level entries, or use copies or hard links.
- Copy the built system libraries from `build/core/libs/libs/*.prx` into `libs/`. Use libraries built for the target OS. A custom `--rpath` changes the system library location.

### param.json / param.sfo
Some apps need their original `app0/sce_sys/param.json` at runtime. AnyPS5 reads it for the title ID and display name.
- It must have a non-empty `titleId` string and a non-empty `localizedParameters` object with a language entry that has a non-empty `titleName`.
- It selects the entry named by `defaultLanguage` (inside `localizedParameters` or at the top level). If that entry is missing, it tries `en-US`, then the first object-valued language entry.
- Keep the values from the title's own metadata, and put the file under the same `app0/` folder as the other resources.
- At startup, its presence in the working directory creates `data/`. A non-zero `downloadDataSize` also creates `download0/`.
- Without `param.json`, AnyPS5 reads `app0/sce_sys/param.sfo` instead (`TITLE`, `TITLE_ID`, `DOWNLOAD_DATA_SIZE`), with the same effect on `data/` and `download0/`. Without either file, neither folder is created.
- If both are missing, or the one read is invalid, metadata calls such as title/window setup can fail with a `param.json` or `param.sfo` error.

### app0.zar archive
Resources can be packed into `app0.zar` beside `app0/`. It's a [ZArchive](https://github.com/Exzap/ZArchive) whose root is the `app0/` folder.
- A file present in `app0/` is read from there; any other file comes from the archive.
- The executable, `sce_module/` and `sce_sys/` must stay in `app0/`.
- The archive is read only through `sceKernelOpen`/`open`, the read/seek/stat calls on its descriptors, and `sceKernelStat`. Other calls on an archived path, listing a directory that the archive also contains, and opening a file in it for writing fail as described in [technical debt](../dev/debt/libkernel.md).
- An archive that can't be opened stops the title at its first file access.

### Bundled title modules and libs/
- For bundled title modules, use the `.guest.prx` files the relinker writes into `app0/` (listed in the layout it prints).
- `libs/` is for AnyPS5 system libraries, **not** the original PS5 `.prx` files. On Windows, an original PS5 module in `libs/` makes Windows try to load it as a DLL and can fail with error 193 (not a valid Win32 application).
- A module the title loads at runtime that wasn't relinked (e.g. one in a folder not given to `--module-dir`) is not loaded: the title gets an error and continues, and the runtime prints `[module] dlopen: <path> was not relinked: ...` once per file. Relink that folder with `--module-dir <dir>`.

### Windows: extra DLLs and mimalloc
- A self-built `libs/` also needs `libgcc_s_seh-1.dll`, `libstdc++-6.dll` and `libwinpthread-1.dll` from the `mingw64/bin` folder of the toolchain that built the libraries. Release archives already include them. Libraries are loaded without searching `PATH`, so a copy elsewhere isn't used; a library that needs one of them fails with error 126 (module not found) even though its `.prx` file is there.
- With `APS5_MIMALLOC` set, the title loads `mimalloc.dll` and `mimalloc-redirect.dll` from `libs/` and allocates through mimalloc. Copy both from `build/core/libs/libs` (release archives contain them). Without them the title keeps the C runtime's heap and prints nothing.

### macOS and Linux: mimalloc
- **macOS:** with `APS5_MIMALLOC` set, the title restarts at once with `libmimalloc.dylib` from `libs/` loaded. The self-built `build/core/libs/libs` already has it. Without the file it keeps the system allocator and prints nothing.
- **Linux:** with `APS5_MIMALLOC` set, the title restarts at once with `libmimalloc.so` from `libs/` in `LD_PRELOAD`. Release archives and `build/core/libs/libs` contain it. Without the file it keeps the system allocator and prints nothing.
  - A `libs/` path with a space or colon can't be listed in `LD_PRELOAD`: the title prints one line and keeps the system allocator.
  - After the restart, `APS5_MIMALLOC` and `LD_PRELOAD` stay set, so the title's own environment and every process it starts carry them.
  - mimalloc keeps more memory than the system allocator: in PPSA21564 about 0.9 GiB less host memory was available with it.

### Windows: direct memory and commit limit
Direct memory (`sceKernelAllocateDirectMemory`, up to 13824 MiB per title) is committed when the title **maps** it, not when it allocates it or first uses its pages. The system commit limit (installed memory plus page file size; the second value of Committed in Task Manager) must cover mapped direct memory plus all other committed memory. Otherwise `sceKernelMapDirectMemory` throws `replace guest placeholder with resident direct memory` with the Windows error (`commit direct memory pages` when a page is mapped at a second address, `keep unmapped direct memory` when it is unmapped). Fix: enlarge the page file or close other applications.

### Linux: /dev/udmabuf access
When the Vulkan driver imports dma-buf memory (not the NVIDIA proprietary driver), shared direct memory is imported through `/dev/udmabuf`, and the user running the game needs read-write access to it.
- Many distros create it as `root:kvm` with mode `0660`: add the user to the `kvm` group and log in again. (An ACL such as `setfacl -m u:$USER:rw /dev/udmabuf` lasts until the next reboot.)
- Without access, startup prints `[gpu] open /dev/udmabuf: Permission denied`, those ranges are copied instead of imported, and GPU stores to them through FLAT/GLOBAL addresses fail with `BDA access failed`.
- A range larger than 64 MiB is imported as one sparse buffer over 64 MiB udmabuf buffers when the device binds sparse memory (RADV, ANV).
- Without sparse binding, or when the device's sparse block size is larger than 64 KiB, ranges above udmabuf's `size_limit_mb` (64 MiB by default) are copied too.

### Running the converted game

Linux:
```sh
./app.elf
```
The relinker writes `app.elf` and each `Executable file:` it lists with execute permission. A copy that lost its file mode (e.g. a browser download) needs `chmod +x` on each of them first. Otherwise a title that starts another executable stops with `sceSystemServiceLoadExec: execv failed: Permission denied`.

Windows PowerShell:
```powershell
.\app.exe
```
A Windows executable started without a standard output or error handle (such as a `--windows-gui` build started from Explorer) writes that output to `anyps5.log` beside it, replacing the previous run's file. Without write access to that folder it runs without the log.

---

## 6. Runtime settings (environment variables)

Most settings below are environment variables set before launching the game. Unless stated otherwise, an invalid value stops the game with an error naming the variable.

### Display resolution and window size
- The game chooses its video-output resolution, and AnyPS5 renders at that. The window can be resized or made fullscreen, which scales the presented image. Changing the window size does not change the game's render resolution.
- **F11** switches between window and fullscreen and returns the window to its earlier size.
- `ANYPS5_FULLSCREEN`: `1` or `borderless` opens fullscreen at desktop resolution from the start. `0` or `windowed` (default) keeps it windowed. Any other non-empty value (`true`, `yes`) still opens fullscreen, with a warning on stderr naming the value. Fullscreen goes to the display the window is on (the one `ANYPS5_DISPLAY_INDEX` selected at start). The mouse cursor is hidden in fullscreen unless the game has opened a mouse.
- `APS5_OUTPUT_RESOLUTION=<width>x<height>` (e.g. `1920x1080`): the game's video output status reports the 1080p class when width < 3840 and height < 2160, otherwise the 2160p class, instead of the class of the display's current mode. Numbers must be positive integers separated by `x` or `X`. Empty = unset. Any other value stops the game. This is the only host setting that changes the resolution a game reads. Otherwise use a game's own display setting.
- `ANYPS5_DISPLAY_INDEX=<n>`: open the window centred on display `n` (counted from 0 in SDL's order). Unset = first display. A value that isn't a connected display's index (including empty) stops the game at start.

### VR compatibility
AnyPS5 does not support PS VR2 headsets or PC VR headsets through OpenXR. No headset tracking, stereoscopic output or reprojection. A VR title may skip headset setup when no device is detected, but that doesn't mean its VR gameplay works.

### Controller input for test runs
Set `APS5_PAD_INPUT_FILE` to an **absolute path** before launching to enable controller pulses from a command file. Once the app is reading controller input, send buttons with the helper from the repository root (run it in a second terminal):

```sh
APS5_PAD_INPUT_FILE=/tmp/anyps5-pad ./app.elf
python3 tools/pad_input.py /tmp/anyps5-pad cross
python3 tools/pad_input.py /tmp/anyps5-pad down --hold-ms 500
python3 tools/pad_input.py /tmp/anyps5-pad release
python3 tools/pad_input.py /tmp/anyps5-pad release --left-stick 128 0 --hold-ms 2000
python3 tools/pad_input.py /tmp/anyps5-pad cross --left-stick 255 128 --right-stick 0 128
python3 tools/pad_input.py /tmp/anyps5-pad touch-right
python3 tools/pad_input.py /tmp/anyps5-pad l1+r1
```

For timed button sequences see [Controller timelines](INPUT_MAPPING.md#controller-timelines).

How it works:
- Commands are polled at most every 100 ms during controller reads. Their buttons add to physical controller input.
- A press lasts 300 ms by default (starting when the app reads it) and releases automatically. `--hold-ms` accepts 1–5000 ms.
- `--left-stick X Y` and `--right-stick X Y` hold the sticks for the same time. Each axis is 0–255, 128 is centre, X grows right, Y grows downward. While such a command is active, both sticks come from it instead of the physical controller; a stick you didn't name stays centred.
- `touch-left` and `touch-right` press the touchpad with a finger on its left or right half, like the `TouchLeft`/`TouchRight` keys. In the command file they are bits `0x200000` and `0x400000` of the button field.
- Join button names with `+` (e.g. `l1+r1`) to press them together. `release` can't be joined.
- Wait for the app to observe a release before sending another press. The file holds only the latest command, so rapid commands can replace one another.
- On POSIX, replacing an existing command file keeps its permission mode; a privileged helper also keeps owner and group so the game user can still read it.
- Commands left over when the app first reads the controller are ignored.
- Without the environment variable, file input is disabled.

`APS5_NO_PAD_INPUT` (any value, even empty): the window ignores keyboard and mouse events. Keyboard/mouse bindings, the mouse stick and the window keys (F10, F11, F12 and others) do nothing. Controllers and `APS5_PAD_INPUT_FILE` commands still work.

`APS5_NO_WGI` (Windows, any value): turn off SDL's Windows Gaming Input controller backend. AnyPS5 always turns off DirectInput there. Read once when the video output driver is created.

### Host microphone selection
- By default `libSceAudioIn` captures from SDL's default input device.
- Set `ANYPS5_AUDIO_IN_DEVICE` to an exact capture-device name reported by SDL to pick another microphone. Unset, empty or `default` keeps SDL's default.
- If the name is missing, appears more than once, or can't be opened, the game's AudioIn open call returns the invalid-device error `0x8026010F`. AnyPS5 does not silently switch microphones. (SDL2 identifies capture devices by name, so duplicate names can't be selected individually.)
- Read each time the game opens an AudioIn port.

```sh
ANYPS5_AUDIO_IN_DEVICE="USB Microphone" ./app.elf
```

`ANYPS5_AUDIO_IN_MUTE_ON_FOCUS_LOSS=1`: return silent microphone samples to the game while the window has no focus. Capture stays open in SDL, but queued samples are cleared when focus changes so speech captured while muted isn't delivered after focus returns. `0` or unset: unchanged. Invalid value stops the game.

### Controller vibration strength
`ANYPS5_RUMBLE` = whole number 0–100 scales vibration strength.
- `100` (default when unset) passes vibration exactly as the game asks.
- `0` turns vibration off: motors never get a nonzero request, and a running rumble is stopped.
- Values in between scale motor strength by that percentage, for every connected controller.
- Covers rumble motors, impulse-trigger rumble, and vibration a game plays as audio (both on a DualSense's own sound card and when turned into motor rumble).
- Does **not** change DualSense adaptive trigger effects (resistance and trigger vibration profiles); those reach the controller as the game sends them.
- An empty, signed, fractional or above-100 value stops the game at start.
- It's a host-side comfort setting, not console behaviour. Read once at start. No hotkey or overlay entry. Not a model of any console setting.

```sh
ANYPS5_RUMBLE=50 ./app.elf
ANYPS5_RUMBLE=0 ./app.elf
```

### Vibration while the window has no focus
By default the controller stops vibrating while the window has no focus, and vibrates again with the next pulse after focus returns. `ANYPS5_RUMBLE_ON_FOCUS_LOSS=0` keeps vibration on in the background. Only the vibration motors are affected (not the light bar or controller state), and identify pulses still work while the window has focus. Any value other than `0` or `1` stops the game at start.

### Controller connection and user 0
By default, port 0 stays connected and the first user stays logged in even if the only controller is removed, so the game keeps reading the last state and never learns the pad is gone.

`ANYPS5_PAD_PORT0_STATE=controller` makes the game see the controller instead:
```sh
ANYPS5_PAD_PORT0_STATE=controller ./app.elf
```
- Removing the controller bound to Player 1 makes the game see a disconnected pad with zeroed input, and user 0 logs out. Plugging it back in reconnects the pad and logs the user in.
- A change applies only after the controller has stayed away or present for 500 ms, so a cable that drops and returns doesn't make the game flap.
- Keyboard and mouse don't count as a controller in this mode: while no controller is bound to Player 1, their input isn't given to the game.
- `default` or unset keeps the default behaviour. Any other value (including empty) stops the game when video output starts.
- Only port 0 and user 0 follow the controller. Players 2–4 keep the state they had.

### Controller identification pulse
`ANYPS5_PAD_IDENTIFY` helps you feel which controller is which player:
- `off` (default): sends nothing.
- `connect`: a short pattern when a controller is bound to a slot: one pulse for player 1, two for player 2, up to four for player 4.
- `always`: also repeats the pattern every 30 seconds while the game isn't rumbling.
- The pattern starts once the game hasn't rumbled for 500 ms. If the game starts rumbling, the rest of the pattern stops and the game's vibration is sent as usual.
- It is scaled by `ANYPS5_RUMBLE`; `0` sends nothing.
- A controller without rumble support logs one line and gets no pulse. Keyboard and mouse never get a pulse.
- Any other value (including empty) stops the game at start.

```sh
ANYPS5_PAD_IDENTIFY=connect ./app.elf
```
Host-side convenience, not console behaviour. Timing and strength were chosen by AnyPS5 and not measured against real controllers.

### Video-out and pad telemetry
Set `APS5_VIDEOOUT_TELEMETRY_DIR` to a folder to write the run's presents and pad changes to `<folder>/videoout.jsonl` (one JSON object per line; appended if the file exists). Every line starts with `"v":1`, the host's steady-clock nanoseconds in `t_ns`, and the event name in `ev`.

- **`contract`** (first line of a run): `{"v":1,"t_ns":<ns>,"ev":"contract","contract":"v1-pad","commit":"<12 hex>","pid":<pid>,"prx":"libSceVideoOut"}`. `commit` = source commit the libraries were built from (`unknown` if Git wasn't found at configuration). `pid` tells apart runs that appended to the same file.
- **`pad`**: written when the pad state the title reads changes in buttons, sticks or triggers: `{"v":1,"t_ns":<ns>,"ev":"pad","n":<present>,"buttons":<bits>,"sticks":[<left x>,<left y>,<right x>,<right y>],"triggers":[<L2>,<R2>]}`. `n` = presents completed when the change was published (the count in the window title). `buttons` = bit field in decimal. Stick axes 0–255 (128 = centre). Triggers 0–255. Motion, touch and device kind aren't compared, so a run that only moves the controller writes no `pad` line. It reports controller and keyboard/mouse input read through SDL. Input fed by `APS5_PAD_INPUT_FILE` is applied later in `libScePad`, so a scripted run writes no `pad` line.
- **`present`**: written after the first present and every 30 presents after: `{"v":1,"t_ns":<ns>,"ev":"present","n":<present>,"fps":<rate>}` (count and rate as the window title shows).
- **`drops`**: a background thread writes the lines and the title isn't waited for. If it falls behind, the line is dropped and `{"v":1,"t_ns":<ns>,"ev":"drops","dropped":<count>}` is written every ten seconds while the count grows, and once at normal exit, only if lines were dropped. An exit that skips static destructors writes no last line. A pad line the full queue refused counts as dropped even though a later publish writes the change.

Without the variable no file is opened and no thread starts. Empty = unset. A folder that can't be written stops the title when video output starts, with an error naming the file.

### Controller-speaker fallback audio
`APS5_PAD_SPEAKER_FALLBACK_DB` changes the gain for AudioOut2 controller-speaker ports that are folded into the main stereo output when no controller speaker is present.
- Read once when a title creates its AudioOut2 context.
- Default `0` dB (mix unchanged). Accepts a number from `-60` to `0`, or `mute`, `off` or `-inf` for silence. Surrounding whitespace is ignored.
- Any other value (empty, out of range, non-finite, trailing text) stops the title with an error naming the variable and value.
- Doesn't affect main-mix ports, a real controller speaker, or the legacy AudioOut controller-speaker port.
- A title that plays a sound on both a main port and a controller-speaker port keeps the main-port copy when the fallback is muted. That copy follows the title's own volume settings; the controller-speaker copy may not.

### System fonts
Games that open the console's system font sets (`sceFontOpenFontSet`) need font files in an `anyps5-fonts/` folder beside the output executable. Set `ANYPS5_SYSTEM_FONTS` to use another folder.

- Files dumped from the console are used under their own names (`SST-Roman.otf`, `SST-Bold.otf`, `SSTJpPro-Regular.otf`, ...).
- Without them, these openly licensed substitutes are used when present:
  - `NotoSans-{Light,Regular,Medium,Bold}.ttf` and `NotoSans-{LightItalic,Italic,MediumItalic,BoldItalic}.ttf` (Latin and Vietnamese)
  - `NotoSansMono-{Light,Regular,Medium,Bold}.ttf` (typewriter)
  - `NotoSansThai-{Light,Regular,Medium,Bold}.ttf` (Thai)
  - `NotoSansArabic-{Light,Regular,Medium,Bold}.ttf` (Arabic)
  - `NotoSansCJK-{Light,Regular,Medium,Bold}.ttc` (Japanese, Chinese, Korean)
- A set that covers several scripts also opens the files of its other scripts (`SSTThai-*.otf`, `SSTArabic-*.otf`, `SSTJpPro-*.otf`, `DFHEI5-SONY.ttf`, `SCEPS4Yoongd-*.otf` or their substitutes) for characters its main file lacks. A missing one leaves those characters without a glyph.
- With neither, opening a system font set fails (the log shows `sceFontOpenFontSet` with `file=(none)`). What happens next depends on the game: some show no text in those fonts, others stop at start-up because they assert the open succeeded.
- The substitutes come from the [Noto project](https://github.com/notofonts/notofonts.github.io) under the SIL Open Font License (Noto Sans files in `fonts/NotoSans/full/ttf`, Arabic in `fonts/NotoSansArabic/full/ttf`). Copy the files named above into `anyps5-fonts/`.
- A Linux distro's Noto package can be used without copying: point `ANYPS5_SYSTEM_FONTS` at the folder that holds `NotoSans-Regular.ttf` (e.g. `/usr/share/fonts/noto` from Arch's `noto-fonts`). That folder holds only one package's files, and the variable names a single folder. Arch installs the `NotoSansCJK-*.ttc` files from `noto-fonts-cjk` in `/usr/share/fonts/noto-cjk`, so the Japanese, Chinese and Korean sets have no file there. To cover them, link or copy both packages' files into one folder and point the variable at it.

### Console language
Games ask for the console language with `sceSystemServiceParamGetInt`. It's English (United States) unless `ANYPS5_LANGUAGE` is set to another number:

| # | Language | # | Language | # | Language |
|---|---|---|---|---|---|
| 0 | Japanese | 11 | Chinese (Simplified) | 22 | French (Canada) |
| 1 | English (US) | 12 | Finnish | 23 | Czech |
| 2 | French | 13 | Swedish | 24 | Hungarian |
| 3 | Spanish | 14 | Danish | 25 | Greek |
| 4 | German | 15 | Norwegian | 26 | Romanian |
| 5 | Italian | 16 | Polish | 27 | Thai |
| 6 | Dutch | 17 | Portuguese (Brazil) | 28 | Vietnamese |
| 7 | Portuguese (Portugal) | 18 | English (UK) | 29 | Indonesian |
| 8 | Russian | 19 | Turkish | 30 | Ukrainian |
| 9 | Korean | 20 | Spanish (Latin America) | | |
| 10 | Chinese (Traditional) | 21 | Arabic | | |

Example: `ANYPS5_LANGUAGE=21` selects Arabic.

`ANYPS5_LANGUAGE=auto` (any letter case) takes the language from the host OS:
- **Linux:** the first of `LC_ALL`, `LC_MESSAGES`, `LANG` that is set and not empty, with the entries of `LANGUAGE` first when that locale isn't `C` or `POSIX`.
- **Windows:** the user's locale name.
- **macOS:** the preferred language list.
- Regional variants the console has are kept: `en-GB` → English (UK), `fr-CA` → French (Canada), `es-MX` → Spanish (Latin America), `pt-PT` → Portuguese (Portugal), `zh-TW`, `zh-HK`, `zh-Hant` → Chinese (Traditional), `zh-CN`, `zh-Hans` → Chinese (Simplified). A language without an entry for the region uses its main one.
- A host language the console doesn't have, and the `C`/`POSIX` locales, give English (US) and print one stderr line naming the locale.
- The host is read once per process; every query then gets the same language.
- Empty = unset. Any other value stops the game at its first language query.

### Save data free space
By default a game's save data is never refused for lack of disk space, and the free blocks it sees are those of its own save.
- `ANYPS5_FREE_SPACE=host`: a new save refuses to be created when the volume holding save data has less room than the save's size (so the game can report a full disk). A save of 16384 blocks (the default size when the game asks for none) needs 1 GiB. Free blocks a game reads are then limited to the room its volume has left.
- `fixed` (default when unset) keeps the default behaviour.
- Any other value (including empty) stops the game at its next save mount or save search that finds a save. On Windows, an empty value counts as unset (because `set ANYPS5_FREE_SPACE=` in `cmd` and `$env:ANYPS5_FREE_SPACE = ""` in PowerShell remove the variable).
- Existing saves open as before in both modes.
- Whether a console refuses a save the same way is not measured; see the [technical debt](../dev/debt/libSceSaveData.native.md) of `libSceSaveData.native`.

### Host game mode
`APS5_GAME_MODE=on` asks the host for its game/performance mode while the title runs. Default `off` (nothing loaded or requested).
- **Linux:** goes to [Feral GameMode](https://github.com/FeralInteractive/gamemode). AnyPS5 loads `libgamemode.so.0` (then `libgamemode.so`) with `dlopen` at start, asks `gamemoded` to start game mode, and ends the request on exit. What `gamemoded` changes (CPU governor, process priority, GPU settings) comes from its own `gamemode.ini`, not AnyPS5.
- **macOS:** holds a user-initiated, latency-critical `NSProcessInfo` activity, which keeps App Nap and timer coalescing away.
- **Windows:** nothing to request; AnyPS5 already asks Windows not to throttle process timers when it loads.
- `auto` makes the same request but says nothing if the host can't take it (no GameMode library, no Foundation). With `on`, a missing library is logged once as `[host-performance]`. In both modes a request the daemon refuses (e.g. `gamemoded` not running) is logged once, and the title keeps running.
- Any other value stops the title at start with an error naming `APS5_GAME_MODE`.

### Add-ons (DLC)
Games check whether an add-on is installed with `sceNpEntitlementAccessGetAddcontEntitlementInfo` and list installed ones with `sceNpEntitlementAccessGetAddcontEntitlementInfoList`.
- An add-on counts as installed only if its entitlement label is listed in `anyps5-entitlements.ini` beside the output executable, or in the file `ANYPS5_ENTITLEMENTS` names.
- Format: one label per line, up to 16 characters; `#` or `;` starts a comment.
- Every other label is reported as not installed. The first query for each prints one line such as `[entitlements] the title asked for the add-on EXAMPLEADDON0000, which anyps5-entitlements.ini does not list, so it is reported as not installed`. The line names the file as `anyps5-entitlements.ini`, or by the path `ANYPS5_ENTITLEMENTS` gives. Bytes of a label outside printable ASCII, and the backslash, are written as `\xNN`. After 64 such labels, one more line says further ones are reported without a line.
- Some games wait at start-up for an add-on they look for instead of going on without it. A title (PPSA26344, version 02.002.000) stays on its loading spinner until the add-on label it asks for is listed.

### Opening links in the host browser
A game asking for a web page with `sceSystemServiceLaunchWebBrowser` gets a failure and nothing opens, unless `ANYPS5_OPEN_URLS` is set:
- `ask`: shows the whole address in a message box with Cancel as the default button, and opens it in the default browser only if you press Open. With no display to show the box, nothing opens.
- `always`: opens it without asking. **Not recommended**, because the game picks the address. Unlike `ask`, it opens any `http`/`https` host the game names, including `localhost` and private-network addresses like a router's web page.
- `never`, empty and unset: keep the failure.
- Any other value stops the game at its first browser request.

Rate limits (all addresses and all game threads together): a request that arrives while a box or launch is still active is refused at once; at most three boxes or launches happen in any 60 seconds; after you press Cancel, no further box or launch for 10 minutes. Refused requests fail like the default.

Address rules: only `http://` and `https://` addresses of at most 2048 bytes open. Refused: a user name or password, a space, a control character, a non-ASCII character (use punycode and percent escapes), a malformed or control-character percent escape, an empty host, a port outside 1–65535, and `psno:` addresses. The browser is started without a shell: `xdg-open` on Linux, `open` on macOS, `ShellExecuteW` on Windows.

### Console date and time format
Games ask for date and time format with `sceSystemServiceParamGetInt`. Default: date = day/month/year, time = 24-hour.
- `ANYPS5_DATE_FORMAT`: `0` = year, month, day; `1` = day, month, year; `2` = month, day, year.
- `ANYPS5_TIME_FORMAT`: `0` = 12-hour; `1` = 24-hour.
- Example: `ANYPS5_DATE_FORMAT=2` with `ANYPS5_TIME_FORMAT=0` gives month/day/year and a 12-hour clock.
- Either also accepts `auto` (any letter case), taking the choice from the host's short date and time patterns: on Linux the locale named by the first of `LC_ALL`, `LC_TIME`, `LANG` that is set and not empty (`LANGUAGE` and `LC_MESSAGES` don't count); on Windows the user's short date/time formats from Region settings; on macOS the current locale. Only the order of day/month/year and 12/24-hour are used; separators and names aren't.
- When the host has no preference (nothing set, `C`/`POSIX`, an uninstalled locale) or its date pattern has an order the console has no format for, the value stays `1` and one stderr line names the variable.
- The host is read once per process. Time zone and summer time already follow the host.
- Empty = unset. Any other value stops the game at its first query of that parameter.

### Trophies
**Unlocking.** A game unlocks a trophy by posting a `_UnlockTrophy` event with an int32 `_trophy_id` property to `libSceNpUniversalDataSystem`, or through statistics rules (below).
- Unlocks are stored in `_trophy/<user id>_<service label>.txt` in the folder the game runs in (the one holding `app0/`), one `<trophy id> <unlock time>` line per trophy. Time = RTC tick (microseconds since 0001-01-01 UTC). Delete the file before starting to lock the trophies again.
- Each new unlock prints one line, e.g. `[trophy] PPSA01556 unlocked trophy 12 for user 268435456`. Unlocking again prints nothing.
- `sceNpTrophy2GetTrophyInfo`, `sceNpTrophy2GetTrophyInfoArray` and `sceNpTrophy2GetGameInfo` report stored unlocks and times to the game.
- When trophy data is loaded (below), a posted unlock of a trophy the data doesn't have, of the platinum, or of a progressive trophy is not stored and prints the reason, e.g. `[trophy] ignored the posted unlock of trophy 0 for user 268435456: it is the platinum, which unlocks with the last trophy that names it`. The platinum unlocks with the last trophy that names it; a progressive trophy through its statistic.
- Without trophy data, every posted id of 0 or more is stored, and the platinum is not unlocked when the others are.

**Trophy data.** Comes from the game's own `app0/sce_sys/trophy2/trophyNN.ucp` (`NN` = service label, `00` for most games).
- Trophy queries report names, descriptions and rewards in the console language (`ANYPS5_LANGUAGE`), or English (US) or the data's default language if it has no text in that language. Also grades, groups, hidden flags, number of trophies and grade counts.
- A platinum trophy unlocks as soon as every trophy that names it in `platinumTrophyId` is unlocked.
- The first trophy query or event prints what was read, e.g. `[trophy] /app0/sce_sys/trophy2/trophy00.ucp: 52 trophies, 1 group, texts in en-US`.

**Statistics.** Many games unlock trophies through statistics: `app0/sce_sys/uds/udsNN.ucp` names, for each event, the property whose value becomes a statistic, and a trophy's `unlockCondition` compares a statistic with a target.
- The latest value of each statistic is stored in `_trophy/<user id>_<service label>.stats.txt`, one `<stat id> <value>` line each, and kept across runs. Delete it together with the unlock file to start over.
- A progressive trophy reports its statistic as progress.
- Only these are applied: statistics that keep the latest (`"aggregation":"latest"`) or the largest value (`"max"`) as `uint32`, `uint64` or `int32`; and rules that copy one property of an event chosen by name (`$.name` or `$.object.name`), optionally only when one property equals an integer (`_UpdateTrophyProgress` by its `_trophy_id`). Others are listed in `[trophy]` lines when the file is read.

**Tracing.** `APS5_TRACE_UDS=1` prints every posted event (e.g. `[uds] context 1 posted _UnlockTrophy {"_trophy_id":12}`), every statistic an event sets (e.g. `[uds] stat 10 of user 268435456 is 10 (COLLECT $.counter)`), and a `[uds]` line for a missing `trophyNN.ucp` or `udsNN.ucp`. An unused event prints the reason in a `[trophy] ignored` line: an `_UnlockTrophy` event without an int32 `_trophy_id` of 0 or more, or posted to a context that doesn't exist; and, once per rule, an event whose property is missing, negative, not an integer or outside the statistic's range.

**Missing or bad data.**
- A missing or malformed `trophyNN.ucp` or `udsNN.ucp` turns off only what it provides, and a malformed one prints the reason once. Without trophy data, queries report placeholders and statistics unlock nothing. Without statistics rules, events change no statistic.
- Icons are not read: `sceNpTrophy2GetGameIcon`, `sceNpTrophy2GetGroupIcon` and `sceNpTrophy2GetTrophyIcon` stop the game with an error, and `sceNpTrophy2GetRewardIcon` returns `SCE_NP_TROPHY2_ERROR_ICON_FILE_NOT_FOUND`.
- A damaged unlock or statistics file in `_trophy/` (a line that isn't a new `<id> <value>` pair, such as one an edit left blank or ended with a carriage return), or one that can't be read or written, does the same (stops the game with an error).
- No notification is shown, and callbacks registered with `sceNpTrophy2RegisterUnlockCallback` are not called.

### PlayGo dumps without metadata
When a dump has neither `sce_sys/playgo-chunk.dat` nor `sce_sys/playgo-scenario.json`, AnyPS5 can't tell which nonzero PlayGo chunk IDs exist. Requests for those IDs stop with an error by default. Set `APS5_PLAYGO_UNLISTED_CHUNKS`:

| Value | Answer for an unlisted chunk ID | Observed title that needs it |
|---|---|---|
| `installed` | Report it as installed (`LOCAL_FAST`). | High on Life (PPSA15388) needs this to load its saves. |
| `absent` | Return `SCE_PLAYGO_ERROR_BAD_CHUNK_ID`. | Sifu (PPSA03001) and Demon Slayer: Kimetsu no Yaiba - The Hinokami Chronicles (PPSA03490) use this to end their chunk-ID search. With `installed`, PPSA03490 keeps asking about higher chunk IDs forever, its chunk list grows until startup fails with `Ran out of memory allocating 5565054976 bytes`. |
| Comma-separated IDs, e.g. `0,1` | Report those IDs as installed (`LOCAL_FAST`) and every other unlisted ID as absent (`SCE_PLAYGO_ERROR_BAD_CHUNK_ID`). | The Medium (PPSA03718) needs chunks 0 and 1; with `installed` it asks about every ID and UE4 runs out of memory. |

- Leave unset to keep the explicit error. A list holds decimal chunk IDs 0–65535 and is added to chunk 0. It only applies when the dump has neither file. Any other value also stops with an error.
- Dumps that include `playgo-chunk.dat` or `playgo-scenario.json` are unaffected.
- These title-specific results come from emulator runs and haven't been verified against a console.

### Screenshots
- Press **F12** in the game window to save the next displayed frame as a PNG at the resolution of the game's display buffer (no window scaling, letterbox or performance overlay). A BT.2100 PQ display buffer is converted to SDR, also while shown through HDR output.
- Files are named `<title ID>-<YYYYMMDD-HHMMSS>.png`, written to an `anyps5-screenshots/` folder beside the output executable (created when needed). Set `ANYPS5_SCREENSHOTS` to use another folder.
- The saved path is printed to stderr. Presses while the previous screenshot is still being written are merged into one.
- The key is the `Screenshot` action of the [input mapping](INPUT_MAPPING.md).
- A screenshot that can't be written stops the game with the error.

### Host volume and mute
- **F5** lowers, **F6** raises, **F8** mutes the host's output volume for the game window. Change keys with `VolumeDown`, `VolumeUp`, `ToggleMute` in [the input mapping](INPUT_MAPPING.md). It's a host control, like a media player's volume: not console behaviour, the title doesn't see it, and its own volume settings aren't changed.
- Volume goes 0–100 in steps of 5, starting at 100 or at `ANYPS5_VOLUME` (whole number 0–100). Output amplitude is the square of the volume as a fraction of 100, so 50 is a quarter of the amplitude.
- Mute is separate from volume: unmuting brings the volume back, and a volume key while muted unmutes. Gain changes move over 10 ms instead of jumping. Each press prints one stderr line with the new level or mute; no on-screen indicator.
- At volume 100 and not muted, audio passes to the host device unchanged, with no processing.
- It applies to what the title plays through `libSceAudioOut` and `libSceAudioOut2`, including what `libSceAudio3d` plays through them. The controller's own sound card and haptics, and the microphone, are not affected.
- `ANYPS5_MUTE_ON_FOCUS_LOSS=1` mutes audio while the window has no focus and restores it when focus returns. `0` or unset leaves audio alone. It doesn't change the mute set with the key (a window you muted stays muted when it regains focus). Only focus events count, so a window minimized without losing focus stays audible, and a window that opens without focus starts muted.
- An invalid value of either variable (including empty) stops the game with an error naming it.
- The keys work while the display window has focus. Not measured on a real audio device or on a console.

---

## 7. GPU, caches and performance

### GPU selection
- The game runs on the first Vulkan 1.1 device with graphics and compute queues and swapchain presentation, preferring a discrete GPU over an integrated one.
- `ANYPS5_GPU`: set to part of a device name (case-insensitive) to run on another device. Names are printed at start-up in the `Physical device candidate` lines. If no usable device contains the text, start fails and the error lists device names.

### Present mode
- The window presents with vertical sync (`fifo`). `ANYPS5_PRESENT_MODE` can be `fifo_latest_ready`, `mailbox` or `immediate` so the game doesn't wait for the display:
  - `fifo_latest_ready` and `mailbox` still show whole frames at the display's refresh and drop the ones in between.
  - `immediate` presents at once and can tear.
- Start fails on any other value, and when the surface doesn't offer the mode (`fifo_latest_ready` needs `VK_KHR_present_mode_fifo_latest_ready` or its EXT form).
- Flips stay paced at 59.94 Hz in every mode.
- A Wayland compositor can stop delivering frames to a window it doesn't show (e.g. one on another workspace). On Hyprland with the NVIDIA driver a `fifo` swapchain then presents about once per second, while `fifo_latest_ready` and `immediate` keep the game at its normal rate.

### FPS limit
`ANYPS5_FPS_LIMIT` = frames per second, from 0.5 to 1000000 with at most three decimals (`30`, `59.94`).
- Each window flip waits for its turn and the title's flip completes then, so the title is paced as by a display of that rate. The title isn't told: its vblank events and refresh rate stay those of the output.
- `0`, empty and unset mean no limit. Any other value stops the game at its first flip.
- The limit applies on top of the flip pacing, which holds flips to the output's refresh (59.94 Hz, or 119.88 Hz in the output mode of that name; every second or third vblank when the title sets a flip rate of 1 or 2) in every present mode. It lowers the rate below that and does nothing above it. A title already slower than the limit isn't delayed.
- A stall isn't made up with a burst: a frame late by under one period is made up by the next, and after a longer stall at most one frame is presented early.
- The wait sleeps until shortly before its deadline and spins for the rest. Window events are handled between flips, so under a low limit the window answers about once per frame.
- With `fifo` the display still shows a frame at each refresh, so a limit that doesn't divide the refresh rate shows some frames for one refresh and some for two.
- The cap is a host choice; the console has none.

### Windows: Mesa Dozen (D3D12 through Vulkan) crash
On Windows, the OpenCL, OpenGL, and Vulkan Compatibility Pack can install Mesa Dozen, which exposes D3D12 devices through Vulkan. Some D3D12 drivers can crash while the Vulkan loader enumerates adapters, even when AnyPS5 would pick a native Vulkan device. If that happens and you have another Vulkan driver, disable Dozen for the current PowerShell session:

```powershell
$env:VK_LOADER_DRIVERS_DISABLE = '*dzn*'
.\app.exe
```

- Don't disable Dozen if it's your only Vulkan driver.
- `VK_LOADER_DRIVERS_DISABLE` matches Vulkan driver manifest filenames and needs a Vulkan loader built with headers 1.3.234 or later ([loader docs](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderInterfaceArchitecture.md#filter-environment-variables)).
- Loaders up to 1.4.365 ignore it in an elevated process (e.g. PowerShell opened with **Run as administrator**).

### Windows: NVIDIA driver 610.47 crash
NVIDIA driver 610.47 can crash in its shader compiler while pipelines are created: the log ends in `FATAL: unhandled exception 0xc0000005` with `nvgpucomp64.dll` in `rip`, on a driver thread. PPSA25344 on an RTX 3060 stopped this way in 3 of 3 New Game loads, also with `APS5_NO_PIPELINE_LTO=1`. With driver 617.42, the same build and scene ran for 40 minutes. **Update the driver** if the crash is in `nvgpucomp64.dll`.

### Host report (for bug reports)
`ANYPS5_HOST_REPORT=1` prints what a bug report needs to stderr once, when the game's Vulkan device has been chosen. Attach the whole block to the issue. `0`, empty or unset prints nothing; any other value stops the start with an error naming the variable.

Fixed sections in a fixed order (two runs on one machine print the same text):
- `[system]`: commit, OS (kernel release and machine type on Linux/macOS; version and build on Windows), CPU model, hardware threads, physical memory (MiB), and the CPU check
- `[sdl]`: SDL version linked at run time and the one built against
- `[gpu]`: device name and type, PCI vendor/device ids, Vulkan version, driver name and info from `VK_KHR_driver_properties`, raw driver version, subgroup size, largest device-local heap, every Vulkan device found, the surface's present modes and the one `ANYPS5_PRESENT_MODE` asked for
- `[vulkan]`: each extension and feature the game needs and the optional extensions it uses, marked `present` or `missing`
- `[shader_cache]`: `disabled`, `absent`, or `present` with its size in MiB
- `[environment]`: the `APS5_`, `ANYPS5_`, `VK_` and `SDL_` variables that are set

Notes:
- A fact that couldn't be read prints `unavailable` and is explained in a final `[unavailable]` section.
- The `cpu_gate` line says `passed` whenever the report appears (a host CPU lacking a required extension stops the game before the Vulkan device is chosen). It says `unavailable` where that check doesn't run, such as macOS.
- The shader cache size counts at most 200000 files.
- **Privacy:** no user names, host names, serial numbers or MAC addresses. Any word that looks like a path (starts with `/` or `~`, contains a backslash, or starts with a drive letter) prints as `<path>`; URLs print as `<url>`; MAC addresses as `<redacted>`; account name, host name and home directory name are replaced wherever they appear as a whole word. Check the text before posting anyway, since the values of the listed variables are printed as set.

**Missing GPU features.** When the chosen GPU lacks an extension or feature the game requires, the start fails with one message naming the GPU, its driver and Vulkan version, listing every missing item, and pointing to the other Vulkan devices `ANYPS5_GPU` can select. Required items: `VK_KHR_8bit_storage`, `VK_KHR_buffer_device_address`, `VK_KHR_shader_float_controls`, `storageBuffer8BitAccess`, `bufferDeviceAddress`, `shaderInt64`, `shaderSignedZeroInfNanPreserveFloat32`, `vertexPipelineStoresAndAtomics`, `fragmentStoresAndAtomics`, `samplerAnisotropy`, `textureCompressionBC`, and `VK_KHR_swapchain` for a window. With `ANYPS5_HOST_REPORT=1` the report is printed first, so a failed start still has it.

### HDR output
- **Windows only.** Titles see HDR output when: the monitor showing the window runs in HDR mode (Settings > Display > Use HDR); the window's Vulkan surface offers an HDR10 (`VK_COLOR_SPACE_HDR10_ST2084_EXT`) swapchain in a 10-bit format; the instance has `VK_EXT_swapchain_colorspace`; and the device has `VK_EXT_hdr_metadata`.
- Monitor luminance comes from DXGI. A monitor that reports no peak luminance is taken as 1000 nits peak and full-frame, 0 nits black.
- On Linux and other hosts output is always SDR, because the surface's formats don't show whether the display is in HDR. A title's BT.2100 PQ display buffers then reach the display unchanged; without HDR output they are converted to SDR.

### Shader cache
- Recompiled shaders and the Vulkan pipeline cache are kept in a `shader_cache/` folder beside the output executable, so later runs skip recompilation.
- `ANYPS5_SHADER_CACHE_DIR` uses another folder. `ANYPS5_NO_SHADER_CACHE` set to any non-empty value other than `0` disables both caches.
- Shaders are stored in a subfolder named after the recompiler's source version. Subfolders of other versions unused for 14 days are removed the first time a run looks up a shader.
- The pipeline cache is one `vk-pipelines-*.bin` file per GPU and driver version in the same subfolder, so it goes with the shaders it was built from.
- Entries of the current version are never removed and the folder has no size limit, so a title that compiles many shaders can add several GB within minutes. Deleting the folder is safe; it's rebuilt on the next run.

**Very long first-run compile.** A first run can stop at one very large compute shader while the GPU driver compiles its pipeline: no new frames, and the driver's compiler keeps several CPU threads busy. This can last minutes to hours for shaders of several million SPIR-V words.
- With `APS5_PROFILE_DRAW=1` the log names such a compile before it starts (`[dispatch] creating a pipeline for N SPIR-V words`, printed above 100000 words) and its time when it ends (`[dispatch] created a pipeline for N SPIR-V words in N ms`).
- The title's committed memory can keep growing during the compile, so the system commit limit needs room for it. On Windows, enlarge the page file before such a run.
- Later runs can reuse the driver's pipeline cache, and the pipeline journal rebuilds recorded pipelines on background threads, which can avoid the wait. Neither guarantees the driver skips the compile, and a driver update or another GPU starts a separate cache.

### Pipeline journal
Besides recompiled shaders, the shader cache folder holds `pipelines.journal`: every Vulkan graphics pipeline the pipeline store kept and every compute pipeline a run built, with the specialized shader modules they were built from. The next run builds those pipelines on background threads while it loads, so a draw or dispatch the earlier run already made doesn't compile its pipeline when first reached.
- It belongs to the GPU and driver that wrote it (like the driver's pipeline cache file next to it). A run on another GPU or driver version starts it over. The driver's own pipeline cache decides how fast each pipeline is rebuilt.
- Two runs sharing the folder take turns on `pipelines.journal.lock`. A run that can't lock it records nothing.
- `APS5_NO_PIPELINE_JOURNAL=1` disables recording and replay.
- `APS5_PIPELINE_JOURNAL_THREADS`: replay threads (half the CPU threads by default; `0` records without replaying).
- `APS5_PIPELINE_JOURNAL_ENTRIES`: how many pipelines are replayed (4096, the last recorded).
- `APS5_PIPELINE_STORE_ENTRIES`: how many graphics pipelines stay resident (by default the store grows from 1024 up to 4096 to hold the replay and the pipelines that keep coming back).
- `APS5_PIPELINE_JOURNAL_MAX_MIB` (1024): file size at which recording stops.
- Deleting the file loses nothing but the head start.
- With `APS5_PROFILE_DRAW=1`, a `[pipeline-journal]` line reports what was loaded, replayed and recorded.

### Draw pipeline
`APS5_DRAW_PIPELINE=1` (off by default) moves more of each draw off the thread that reads the title's command buffers: a second thread finishes each draw while the next is read; the recording thread skips work whose inputs didn't change; and checks that a draw's resources are still current are reused until the title or GPU writes memory. Meant to be used with `APS5_PIPELINED_DRAWS=32 APS5_PIPELINE_DISPATCH=1`.
- The picture doesn't change. If it does, that's a bug; a run without the switch shows whether it's the cause.
- `APS5_VERIFY_DRAW_PIPELINE=1` runs the full checks next to the reused ones (slower) and prints `[epoch-proofs]` lines with how many differed (any nonzero count is a bug).
- `APS5_PROFILE_DRAW=1` finishes every draw on the reading thread, so a run with it doesn't measure this switch.
- To see the hand-off without that, set `APS5_PIPELINE_STATS` (any value; `1` also turns on Vulkan pipeline statistics, which cost time). Every 10 s a `[finisher]` line counts draws handed to the second thread and those finished on the reading thread, and `[finisher-held]` lines count draws held back by why their writes aren't known.

### Texture cache budget
- Image and buffer memory allocations retry after releasing unused pooled buffers and reclaimable texture cache entries. Recovery preserves storage images with pending GPU results and resources retained by recorded work. `[bufferpool]` recovery and failure lines report the requested allocation, memory released and retry count. An allocation still fails if recovery can't free enough.
- When the `VK_EXT_memory_budget` budget of the device-local heap has no room for an image, device buffer or resident heap mirror slab, the texture caches first release reclaimable entries for the allocation and for what the heap is over budget. The allocation goes to host memory only if they release less than that and the budget, read again, still has no room.
- Sampled textures and storage images stay in device memory in two caches that evict their least recently used entries when they grow past their budgets.
  - **Storage image budget:** what the budget of the largest device-local heap leaves after the heap's use other than the cached storage images themselves and an eighth of the budget; capped at half of that budget or half of the heap's size (whichever is smaller); never below 512 MiB.
  - **Sampled texture budget:** what the reported budget leaves after the heap's use other than cached textures, a quarter of the heap's size (at least 2 GiB) and an eighth of the budget; never below 2 GiB.
  - As the heap's other use grows, the sampled budget falls first (to its floor), then the storage budget. The storage budget also gives back what the live sampled textures take as they grow. Each cache trims at its next insert.
  - Both budgets are re-read every second. `[gpu] sampled texture cache budget` lines report them at start-up and when either moves by more than a tenth, at most every 10 s.
- `APS5_TEXTURE_CACHE_MIB=<n>` fixes both budgets at `n` MiB (and the lines are then not printed).
- A fixed budget above the automatic one can exceed what the card holds next to the rest of the frame. Allocations then go to host memory (`[gpu] ... placed in host memory` lines), where the GPU reads them over PCIe. Example: PPSA16064 on an RTX 4060 8 GB (automatic storage budget 1797 MiB under the earlier rule) placed 868 and 878 MiB in host memory with `APS5_TEXTURE_CACHE_MIB=3072` and walked through its first level at 4.46 and 4.83 FPS; with the variable unset it placed none and ran the same walk at 6.48 and 6.07 FPS. **Leave it unset** unless a measurement on your card shows otherwise.

### Storing GPU results at each flip
GPU results in storage images and render targets stay on the GPU until something reads their memory. A title that frees such an image after waiting for its GPU work and reuses the memory can have its new data overwritten when the results are stored later ([technical debt](../dev/debt/libSceAgcDriver.md)).
- `APS5_STORE_AT_FLIP=1` stores every pending result when a display buffer is flipped (one write-back per pending image and frame). Any other value stops the title at its first flip.
- A title's own CPU reads of that memory aren't seen, so a title that waits for a GPU label and then reads the results reads stale memory. `APS5_STORE_AT_LABEL=1` stores every pending result before each label the GPU writes (`RELEASE_MEM`, `WRITE_DATA`, `DMA_DATA`), at the cost of a write-back of every pending image per label. Any other value stops the title at its first label.

### Upscaling (FSR)
The window normally shows the display buffer scaled to fit with bilinear filtering. `ANYPS5_UPSCALE=fsr` scales it with AMD FidelityFX Super Resolution 1.0 instead.
- When the fitted image is larger than the display buffer on either axis (e.g. a 1920x1080 buffer in a 2560x1440 or 3840x2160 window), EASU upscales it and RCAS sharpens the result.
- EASU is designed for up to 2x the buffer's size on each axis. Larger factors (e.g. 1280x720 in a 3840x2160 window) still work but look softer.
- When the fitted image is the same size or smaller, FSR stays off for that frame and the frame is presented exactly as without `ANYPS5_UPSCALE` (RCAS alone would cost frame rate there without adding detail).
- `ANYPS5_FSR_SHARPNESS` sets RCAS sharpness in stops, from `0.0` (sharpest) to `2.0` (weakest). Default `0.2`. No effect without `ANYPS5_UPSCALE=fsr`. RCAS runs only on frames FSR upscales, and there it always sharpens: `2.0` is its weakest setting, not off.
- RCAS's approximate reciprocal darkens flat areas by a fixed fraction: 0.17% at the default `0.2` (a flat 0.5 becomes 0.4992), 0.32% at `0.0`, 0.07% at `2.0`. That's at most one 8-bit level for SDR, and at most three 10-bit code values for HDR10.
- FSR works on SDR and HDR10 alike, on the values the display receives (sRGB-encoded for SDR, PQ code values for HDR10). Sharpening never takes them out of the signal range.
- While EASU runs, each presentation in flight keeps two more images (one the display buffer's size, one the fitted image's size), and each frame takes two full-window passes.
- Empty = unset. The start fails on any `ANYPS5_UPSCALE` value other than `fsr`, and on an `ANYPS5_FSR_SHARPNESS` that isn't a number from 0.0 to 2.0.

---

## 8. Reports, tracing and diagnostics

### Benchmark reports
To measure how a title runs, start it with `APS5_TRACE_SUMMARY_EVERY=30 APS5_TRACE_BENCH=1`, play the scene for a few minutes and quit.
- Every 30 seconds the runtime rewrites `aps5-bench-<commit>-<pid>.json.gz` in `APS5_TRACE_DIR` (or the working directory) and appends a line to `aps5-summary.jsonl`.
- The file holds the time between every two presents, the commit, GPU, driver, OS, CPU name, thread count and physical memory, and (when `app0/sce_sys/param.json` is present) the title ID, name and content version. It holds no game data.
- The first interval includes shader compilation; compare later intervals. The interval still open when the title exits isn't written.
- Fields are described under [summaries for a long session](../dev/BUILD.md#summaries-for-a-long-session).

### Run report
Every run of a converted game writes `anyps5-report.json` when it ends through `exit`: what the game was, which build ran it, how far it got, frames presented, shader and missing-function counts. It stays on your machine; nothing is uploaded. Fields are in [the run report](run-report.md); `docs/run-report.schema.json` is the exact format.

- On by default. `--aps5-report=off` on the launch command line or `APS5_REPORT=0` turns it off. `--aps5-report=on` or `APS5_REPORT=1` turns it on for a process that isn't a converted game. The flag wins over the variable. The game sees the flag in its own arguments. With the report off, nothing is collected or written.
- Written next to the game's executable. If that folder can't be written, it goes to `reports/<run id>/anyps5-report.json` in the data folder (`$XDG_DATA_HOME/AnyPS5` or `~/.local/share/AnyPS5` on Linux, `%LOCALAPPDATA%\AnyPS5` on Windows, `~/Library/Application Support/AnyPS5` on macOS).
- On Windows only the guest's `exit` and `_exit` write a report; a return from the entry writes none. A run that is killed, crashes or aborts writes no report.

Which milestones are recorded:

| System | `loaded`, `started` | `firstDraw`, `firstPresent` | `stage`, frames, frame rate |
|---|---|---|---|
| Linux | not recorded | recorded | recorded |
| Windows | not recorded | recorded | recorded |
| macOS | recorded | recorded | recorded |

A milestone missing from the report is either one the run didn't reach or one this system doesn't record. On Linux and Windows, `loaded` and `started` are never recorded, so their absence says nothing; `stage` and `framesPresented` say how far the run was seen to get. The Windows and macOS code has not been run.

To share a run, attach `anyps5-report.json` to the title's discussion. The runtime filters user names, home folders, host names, addresses and account numbers out of the report and never writes save data. That's a filter, not a guarantee, so read the file before sharing.

### Tracing system library calls
Windows, Linux, macOS. An executable converted with `--trace-imports` records calls to the libraries named in `APS5_TRACE_CALLS` (comma-separated list such as `libkernel,libSceSaveData`, or `all`). Without the variable nothing is recorded.
- If an import has no library label, the tracer uses the filename of the host module containing its function. Select that name (such as `libSceAvPlayer.native`), not a guest alias.
- Crash and abort reports then end with the last 64 calls (on macOS, which has no crash report, only an abort prints them): sequence number, thread, library, import name as the executable imports it (a NID) and the caller's address. Then the last 32 calls made by the reporting thread (taken from the 65536 most recent calls, so a thread whose calls were pushed out of the last 64 by a busier one still shows them). Then the 32 most-called imports since start-up, with their call counts.

```
last traced system library calls (APS5_TRACE_CALLS):
  #1 thread 9384 libkernel.prx k+AXqu2-eBc from 0x0000000000400006 app.exe+0x10006
last traced calls of thread 9384:
  #1 libkernel.prx k+AXqu2-eBc from 0x0000000000400006 app.exe+0x10006
most called traced imports (1 calls in total):
             1 libkernel.prx k+AXqu2-eBc
```

To list every import a title calls during a run that ends without a crash, also set `APS5_TRACE_CALLS_FILE` to a file path. Every 10 seconds the file is rewritten with one line per import called so far, most called first (call count, library, import name). It survives a run that is killed, up to its last rewrite.

```
245557 libc.prx gQX+4GDQjpM#guest
3866 libSceAgc.prx 57labkp+rSQ
```

Limits are in the [libc](../dev/debt/libc.md) and [relinker](../dev/debt/relinker.md) technical debt.

### Performance overlay
**F10** cycles an overlay over the presented image: off → page 1 → page 2 → off. `ANYPS5_OVERLAY=1` shows page 1 from the first frame, `ANYPS5_OVERLAY=2` shows page 2 (F10 then continues from there). `ANYPS5_OVERLAY=0` = unset. `ANYPS5_OVERLAY_POS` puts it in a corner: `tl` (default), `tr`, `bl`, `br`. For either variable, empty = unset, and any other value stops the game at its first frame. Change the key with `TogglePerformanceOverlay` in the [input mapping](INPUT_MAPPING.md). Text scales with window height at a whole multiple of its 5x7 pixel font. On an HDR10 swapchain the overlay is drawn at 203 nits.

**Page 1:**

| Line | Meaning |
|---|---|
| `PROFILED` | First line while `APS5_PROFILE_GPU=1` or `APS5_PROFILE_DRAW=1` is set. Those add timestamp queries to every batch and timers to every packet, so the numbers aren't comparable with an unprofiled run. |
| `FPS` | Frames presented per second over the last second. Below 1 fps (fewer than two presents in the last second) it shows the rate of the last frame (1000 ÷ its time in ms); a frame every 2 s shows `0.50`. Every present counts, including frames a title presents cleared (e.g. while loading). |
| `FRAME` | Time between the last two presented frames, in ms. |
| graph | The last 120 frame times, newest on the right. Full height = the longest of them or twice their average, whichever is larger. |
| `MIN/AVG/MAX` | Shortest, average, longest of the 120 frame times, in ms. |
| `GPU` | GPU time per frame over the last second (or of the last frame below 1 fps), in ms: the span of each recorded batch from its first to last command, from `APS5_PROFILE_GPU=1` timestamp queries. Only shown with that variable. Batches are counted when they complete, so the line trails a sudden change by up to a second. Batches that overlap on different queues each count in full, so it can exceed `FRAME`. |
| `DRIVER` | CPU time per frame over the last second (or of the last frame below 1 fps), in ms, that packet workers spent in dispatch and draw packets (the `dispatch` and `draw` time of the `[gpu] worker` line of `APS5_PROFILE_DRAW=1`). Only shown with that variable. Every worker's time counts in full, so it can exceed `FRAME`. |
| `DRAWS` | Draw packets per frame over the last second (or last frame below 1 fps), then dispatch packets per frame (`DISPATCH`), as the packet workers execute them. Packets predication skips aren't counted. Shown once the title has submitted either. |
| `SHADERS` | Pipelines created in the last second (or during the last frame below 1 fps), then since start (`TOTAL`): a graphics pipeline for each draw state the pipeline cache hadn't seen, and a compute pipeline for each new dispatch shader. A rise while the game stutters points at pipeline compilation. Shown once a pipeline was created. |
| `VRAM` | Usage and budget of the GPU's largest device-local memory heap from `VK_EXT_memory_budget`, read once a second. Not shown if the driver lacks the extension. |
| `RAM` | The process's private memory, read once a second: `PrivateUsage` of `GetProcessMemoryInfo` on Windows, `RssAnon` plus `VmSwap` of `/proc/self/status` on Linux. The title's direct memory is shared memory on both and isn't included. |
| `RES` | The display buffer's size, then the window's swapchain size, e.g. `RES: 1920X1080 / 2560X1440`. Not shown for frames without a display buffer. |

Format: times show one decimal below 100 and whole numbers above; anything above 9999 shows as 9999 (so `FRAME` and `MIN/AVG/MAX` read 9999 for frames of 10 s or more). `FPS` has the same format, with two decimals below 1. Counts are whole numbers, in thousands (`12K`) from 10000 and millions (`3M`) from a million. Memory is in GB of 2^30 bytes.

Cost: while the overlay is hidden, presentation records nothing for it and the image is unchanged. While shown, one small render pass draws it over each presented frame. Each draw and dispatch packet and each pipeline creation is counted even while hidden, with one increment of a counter belonging to the packet worker (or a shared one for pipelines). Memory and counters are read only while the overlay is shown.

**Page 2** is wider (46 characters) with no graph. It reads its sources once a second while shown, so its rates cover the last second and appear a second after the page does. A line whose source isn't available is left out, and so is a part of a line.

| Line | Meaning |
|---|---|
| `FPS`, `FRAME` | As page 1. |
| `BUSY %` | Share of the last second each queue's packet worker (`Q0`, `Q1`, ...) spent on packets, suspends, ends of submissions, rewinds and completions (`worker_busy_pct` of the trace summary). From the trace totals. |
| phases | The 5 phases with the most CPU time per frame over the last second, in ms, summed over the driver's threads and named as in the trace summary's `top_phases_ms_per_frame`: a timed scope, or a scope and one of its stages. A scope's time includes scopes and stages within it, and threads that work at once each count, so phases can add up to more than `FRAME`. From the trace totals. |
| `Q0 WAIT MS/FRAME` | Time per frame queue 0's packet worker waited, in ms: `MEM` in packets that wait for guest memory or for rendering to finish, `LOCK` for the GPU lock, `GPU` in the recorder's syncs and serial waits and in command batches' fence waits. A memory wait that syncs the GPU counts in both `MEM` and `GPU`. From the trace totals. |
| `RENDER PASSES/FRAME` | Render passes begun per frame (a draw that continues the open pass begins none). From the trace totals. Not shown while an `APS5_TRACE_*` variable is set. |
| `PIPELINES` | `COMPILING`: shader preparations and pipeline-library prebuilds queued or running. `NEW/S`: pipelines created in the last second, as `SHADERS` on page 1. |
| `HIT %` | `DRAW CACHE`: share of the draw cache's lookups in the last second that found an entry whose stages all matched. `PIPELINES`: share of pipeline cache lookups that found the pipeline. Each is left out for a second without lookups. |
| `FLIPS/S`, `PRESENTS/S` | The title's flips per second vs frames presented per second. |
| `DROPPED/S`, `REPEATED/S` | Per second: flips not presented because no swapchain image was free (the `dropped` count of the `[present]` line of `APS5_PROFILE_DRAW=1`), and presents of the same display buffer as the present before. |
| `COMMIT` | System commit charge against its limit, in GB: `CommitTotal` and `CommitLimit` of `GetPerformanceInfo` on Windows; `Committed_AS` and `CommitLimit` of `/proc/meminfo` on Linux (the limit is enforced only with `vm.overcommit_memory=2`). Then `FAULTS/S`: the process's page faults per second, soft ones included (`PageFaultCount` of `GetProcessMemoryInfo` on Windows; minor plus major faults of `getrusage` on Linux and macOS). macOS reports no commit charge, so only `FAULTS/S` shows there. |
| `HOST IMPORTS` | Host memory imports in use, then their size against the import budget, in GB. Not shown on a device that can't import host memory. |
| `CPU COPIES` | Guest memory copied per second by address-based resource builds, for ranges no host import or mirror serves, in MB of 2^20 bytes. `CAP`: the largest of those builds in the last second against the copy cap `APS5_ADDRESS_COPY_MAX_MIB` (64 MiB unless set), past which a build stops the game. |
| `UNIMPLEMENTED` | Calls per second to functions AnyPS5 doesn't implement, then the total since start. Such a call throws an error, so a count above 0 means the title caught it and went on. |
| `REFUSED SHADERS` | Shaders whose draws are skipped. Only shown with `APS5_SKIP_REFUSED_DRAWS=1`; without it a refused shader stops the game. |
| `TITLE`, `PRESENT` | The title ID from `param.json`, and the swapchain's present mode as `ANYPS5_PRESENT_MODE` names it. |
| `FPS LIMIT` | The value of `ANYPS5_FPS_LIMIT` on its own line after `TITLE` and `PRESENT`, formatted like `FPS` (`59.94` reads `59.9`), in thousands from 10000. Not shown without a limit. |

How page 2 gets its data: busy shares, phases, queue 0 waits and render passes come from the totals the trace keeps per thread. Showing page 2 turns those totals on (without the event buffers of an `APS5_TRACE_FRAMES` trace): each timed scope on the driver's threads reads the clock twice and adds its time to its thread's totals, and each render pass begun is counted. Leaving page 2 turns them off. A run with an `APS5_TRACE_*` variable keeps its trace as configured; page 2 then shows those lines only while that trace records. While the overlay is hidden or on page 1, page 2 adds only a check per render pass begun (the same check each timed scope already makes), a per-thread count of bytes each address-based build copies, a count of unimplemented-function calls, and the display buffer's address per present. Its sources are read only while shown.

### Saving the log to a file
Set `ANYPS5_LOG_FILE=<path>` in the environment of a converted executable to keep its log in a file you can attach to a bug report:

```
ANYPS5_LOG_FILE=run.log ./app.elf
```

- The executable still prints everything on the terminal. The file receives a copy of what it writes to stdout and stderr: the runtime's log lines, `[tag]` messages, what the title prints there, and crash and abort reports.
- `APS5_LOG_TIMESTAMPS` stamps the file's lines as it does on the terminal.
- Not included: the relinker, trace files of `APS5_TRACE_*` variables, and output of programs that don't write to those two streams.
- The file is opened when the executable loads and created if missing. A relative path is resolved against the working directory.
- It is **appended to, never truncated**, so a title that starts another converted executable keeps writing to the same file, and a second run continues below the first. Each start writes one line `[log] ANYPS5_LOG_FILE: appending to this file, process <id>` to the file only. Delete the file first for a fresh log. No `{pid}` or date expansion in the path.
- Empty = unset. A path that can't be opened (missing folder, a folder, no permission) stops the executable before it runs, with status 1 and a message naming `ANYPS5_LOG_FILE`, the path and the OS error.
- A line produced by one `write` call is written whole: a stdout line never lands inside a stderr line. Two threads writing lines longer than the pipe's atomic size (512 bytes on macOS, 4096 on Linux) to the same stream can still interleave, as on a terminal. The two streams are copied by two threads, so lines of one stream can come out a few lines before or after nearly simultaneous lines of the other. A last line without a newline gets one in the file when the process ends.
- Crash/abort reports, and the lines before them, are written to the file before the process ends. A process ended from outside (`SIGKILL`, `TerminateProcess`) loses what it wrote in the last milliseconds.
- No size limit; it grows as long as the title logs.
- If the file can no longer be written (e.g. disk full), the executable prints one line saying so and continues with terminal output only.
- While set, the executable runs two extra threads and holds the standard streams in two pipes. A child process the title starts keeps its output in the same pipes, so output it writes after the title exits is lost, and on Linux/macOS the `SIGPIPE` of that write ends it unless it ignores the signal. When the title replaces itself with another executable through `sceSystemServiceLoadExec`, the real streams are handed over first.
- With no console (e.g. a Windows executable started without one), only the file receives the log.
- On Windows the file was checked with a console, with output redirected to a file or read through a pipe, and with no console; the crash report path and the handover to a replacement executable are run by the tests.

### Controller status panel
A small panel over the presented image lists the controllers of the four player slots: one line per slot that has had a controller since the game started (with `ANYPS5_PAD_STATUS` unset or 0, since the first F9 press, because controllers aren't tracked before that). Examples: `P1 DUALSENSE  WIRED`, `P2 XBOX  BATTERY MEDIUM`, `P3 DISCONNECTED`. With no controller seen yet it reads `NO CONTROLLER`. It's never permanent: it disappears 4 seconds after the last thing that showed it.

- **F9** shows it, in the game window. Pressing F9 while shown restarts the 4 seconds. The key is `ShowPadStatus` in the [input mapping](INPUT_MAPPING.md) and accepts only keyboard keys.
- `ANYPS5_PAD_STATUS=1`: it also appears by itself when a controller connects (including one already connected at game start), disconnects, is replaced in its slot, or its battery enters a lower class (full, medium, low, empty). Each new event restarts the 4 seconds.
- `ANYPS5_PAD_STATUS=0` = unset: only F9 shows it. Empty = unset. Any other value stops the game when video output starts.
- **A line** = player number (the controller's slot; a controller that reconnects takes the lowest free slot, which can be another player), controller type, and power.
  - Type: `DUALSENSE`, `DUALSHOCK 4`, `DUALSHOCK 3`, `XBOX` or `SWITCH`. For any other controller it's the name SDL reports, reduced to the overlay font's characters and cut to 12 characters with a final dot, or `PAD` when SDL gives no name.
  - Power: `WIRED` or one of SDL's battery classes: `BATTERY EMPTY` (up to 5%), `BATTERY LOW` (up to 20%), `BATTERY MEDIUM` (up to 70%), `BATTERY FULL`. A controller whose driver or connection reports no level reads `BATTERY UNKNOWN`. What's reported depends on SDL, the OS and the controller.
- The panel uses the font and scale of the performance overlay and sits in the corner mirrored horizontally from `ANYPS5_OVERLAY_POS` (top right by default), so it never covers that overlay. While the system on-screen keyboard is shown the panel isn't drawn, and its 4 seconds keep running. Screenshots don't include it.
- While hidden (always, unless F9 or `ANYPS5_PAD_STATUS=1` shows it), nothing is drawn, no render pass or memory is allocated for it, and the image is unchanged. Still running: a lock and clock read in each present and video-out loop pass to see whether the panel is wanted. With `ANYPS5_PAD_STATUS` unset or 0 and no F9 press, no controller name is copied and no battery level is read; they're read every loop pass with `ANYPS5_PAD_STATUS=1`, and while the panel is shown after F9.

### PSN-only dialogs
Some system dialogs need PlayStation Network: profile, friend list, store, invitation dialog, Facebook link, authorized-apps dialog. AnyPS5 doesn't provide PSN, so these dialogs finish at once with the same answer as a cancel, and nothing is shown. The web browser dialog isn't known to need PSN, so its notice only says the system feature isn't available.

`ANYPS5_PSN_DIALOGS=show` shows a notice on the game's screen when one of these dialogs opens. The notice has one OK button and takes the controller until answered. It isn't shown again within two seconds of the previous one, and not while another dialog or notice is showing. The game's result is the same either way. `silent`, unset and empty show nothing. Any other value stops the game at its first such dialog.

### Linux crash report
On Linux x86-64, a `SIGSEGV`, `SIGBUS`, `SIGILL` or `SIGFPE` that nothing else handles prints a report to stderr (and to the log file when `ANYPS5_LOG_FILE` is set) before the process ends with that signal. It contains:
- the signal, its `si_code`, the fault address and the thread ID
- `rip`, as an address and as the mapped file plus offset in that file (e.g. `app.elf+0xc77cda` or `libkernel.prx+0xc7e30`)
- `guest abort()` for `int 0x45`, or `guest assert/break` for `int 0x41`, when the kernel's general-protection context and readable instruction bytes identify that interrupt
- the sixteen general registers
- the return addresses of the `rbp` frame chain, up to 32
- up to 24 stack words within 32 KiB above `rsp` that point at executable mappings

With `--trace-imports` and `APS5_TRACE_CALLS`, the last traced calls follow, as in the abort report. An offset in a guest module or the executable can be disassembled from the converted file at that file offset.

### Windows crash diagnostics
- **Text in registers:** when a register points at readable memory that starts with at least four printable ASCII characters (as UTF-16LE or single bytes), the crash report prints that text after the register memory dump, as `[r13] u"..."` (UTF-16LE) or `[rdi] "..."` (single bytes). The text ends at a NUL, at the first character that isn't printable ASCII, a tab, a line feed or a carriage return, after 512 characters, or at the end of readable memory. Tabs, line breaks, quotes and backslashes are escaped; `...` after the closing quote means no NUL ended the text within those limits.
- `APS5_CRASH_INDIRECT_MEMORY=1`: include up to 64 bytes from each pointer found in the register memory dump. Each block is labelled with its register, pointer offset and target address. Inaccessible, incomplete and executable targets are skipped. Disabled when unset or empty.
- `APS5_CRASH_STACK_BYTES=<n>`: after the stack return address candidates, print the faulting thread's stack words from `rsp` for `n` bytes (at most 1048576; decimal or `0x` hex), four words per line with their offset from `rsp`. Below them the report prints eight words at every distinct stack word that points at readable, non-executable memory outside the thread's stack, labelled with the word's offset and the address it holds, up to 256 addresses. This shows objects the caller frames kept (such as the `this` of a destructor or a container's buffer). The dump stops at the first unreadable stack page. Nothing is printed when unset, empty or 0.
- **Other threads:** the report lists the other threads of the process, up to 128 (a line says how many were left out), with their `rip`, `rsp`, `rbp` and the return addresses of their `rbp` frame chain. A thread whose chain yields fewer than two frames (e.g. waiting inside `ntdll.dll` or in code built without frame pointers) also gets up to 24 stack return address candidates: words in the 32 KiB above its `rsp` that point at executable memory, read while the thread is suspended. Like the faulting thread's candidates, they're a scan, not an unwind, so stale return addresses of finished calls can appear.

### GPU submission rings and stall dumps
Each GPU queue keeps its last 16 command buffer submissions: when each was submitted, its serial, size and guest address, and a copy of its first 512 dwords after indirect buffers are inlined.
- `APS5_PM4_RING=<n>` keeps the last `n` instead (0–4096; 0 keeps none). Any other value stops the title at its first submission.
- The rings are printed to stderr (one line per submission and one per decoded packet) when the Windows crash report runs: an unhandled exception, `abort()`, an invalid CRT parameter, or the timed dump of `APS5_HANG_DUMP_SECS`.
- `APS5_STALL_WATCHDOG_SECS=<n>`: a queue that has waited `n` seconds without progress in a `WAIT_REG_MEM`, a `REWIND` or behind queue 0 prints the rings once for that wait, and the title keeps running. A `WAIT_REG_MEM` progresses while any other packet runs or completes (as for `APS5_GPU_WAIT_TIMEOUT_MS`). `n` is 0–86400; `0` (default) disables the watchdog. Any other value stops the title at its first submission, as for `APS5_PM4_RING`. While the rings are printed for a queue held behind queue 0, new submissions and the other queues wait.
- `ANYPS5_STALL_PROMPT=1` (with the watchdog): the first time a watched wait expires, the host shows a message box. `Wait` keeps the title running; `Quit` ends it with exit code 124 after the rings are printed. After `Wait`, the box comes back one period later, at most three times for the same wait. The rings are printed once per wait in any case. Only one box is open at a time; a wait that finds it open is asked after that box is answered. If no message box can be shown, the rings are printed and the title keeps running. Unset or `0` (default): no box. Any value other than `0` or `1` stops the title at its first submission. Without the prompt, the watchdog never ends a title.

---

## 9. Exit codes
- `0`: conversion succeeded, or help/version was displayed.
- `1`: invalid arguments, or a `--verify-patches` entry that is a mismatch.
- `2`: conversion failed (error printed to stderr).
- With `--autorun`, a successful conversion returns the launched application's exit code.

---

## 10. Import audit

[`tools/import_audit.py`](../../tools/import_audit.py) lists the system functions a converted game imports and whether the built libraries provide them, before you launch the game. Needs Python 3 and nothing else.

```sh
relinker --registry source/input.elf app.elf
python3 tools/import_audit.py app.registry.json app.libc.prx.guest.prx.registry.json --libs build/core/libs/libs --modules source/sce_module
```

- `--registry` writes `app.registry.json` for the executable and one `app.<module>.registry.json` per converted bundled module. Pass all of them.
- `--libs` is the folder the `libs` target fills ([build instructions](../dev/BUILD.md)). Exports are read from the built `.prx` files, so the result matches what the loader finds.
- Imports are counted once per NID and library, and every reference lands in exactly one class:

| Class | Meaning |
|---|---|
| `implemented` | A built library exports the NID and its function isn't a throwing stub. |
| `partial` | Exported, but the function throws for inputs whose behaviour is unknown. |
| `stub` | Exported, but the whole function calls `NotImplemented_nid_no_patch`: the game loads and throws when it calls it. |
| `absent` | No built library the loader searches exports the NID: the loader fails. |
| `module` | The import names a title library found in `--modules` that exports the symbol or its NID. |

### Name mismatches
An import that names a library requires the literal export name. If its implementation is found only by hashing the import name, it still counts as `implemented` or `stub`, but the report lists the name mismatch, sets `symbol_mismatch` on the JSON import record, counts it in the summary, and exits with code 1. A strong import with a name mismatch is a current stop. Imports without a library keep the audit's name-to-NID fallback.

### Weak imports
Module registries mark ELF weak imports with `"weak": true`, so the audit's current-stop selection can skip them. Strong imports omit the field. Older module registries don't preserve weak bindings; regenerate them to tell optional imports apart.

### Auditing modules
- ELF modules in `--modules` are audited regardless of filename: their undefined symbols are classified against the built libraries and the other modules' exports, with the module file named in the report. The libraries they need must have a file in `--libs` or `--modules`.
- Ordinary non-module sidecars are ignored. Malformed ELF, SELF containers, and non-ELF files with module extensions remain errors. Converted `*.guest.prx` PE modules of a Windows build are accepted; any other PE file is an error.
- The audit reads dynamic tables from ELF section headers or, for sectionless files, from `PT_DYNAMIC` and a SysV or GNU hash table.

### Companion `.needed.json` files
Each registry has a companion with the same prefix and the suffix `.needed.json`, listing its host library dependencies, including declared `DT_NEEDED` entries with no imports. Dependencies supplied by converted bundled modules (including aliases) are omitted. The executable's companion includes host dependencies of its bundled modules. The audit reads these automatically; keep them beside the registries. Older registries without companions still work but can't report dependencies that have no imports; regenerate them with `--registry`.

The report also lists needed libraries with no file in `--libs` or `--modules` (including zero-import dependencies). These don't increase import or reference counts. Windows can skip a missing dependency when no import needs it; Linux still needs the file. It also lists imports exported only by a library other than the one they name: those resolve on Linux and can fail on Windows.

### Windows builds with companions
An import without a library counts only what the entry stub searches: the title's modules, the libraries the companions list, the kernel family when one of them is a kernel library, and `libc.prx` when one is a kernel library or `libSceLibcInternal.prx`.
- An import that only other built libraries export is `absent`. Its JSON import record lists them in `unsearched_providers`, and the summary and report count them per library.
- Companions of all registries passed are pooled, so pass the registries of one executable and its modules together, with every folder holding the title's converted modules in `--modules`. An import that only a title module provides counts as `absent` if that module's folder is missing.
- When no companion lists a library, and on a Linux build, every built library still counts.

### Host dependency check
The audit also walks the built libraries' own host dependencies: each `.prx` under `--libs` has its PE imports (or ELF `DT_NEEDED` entries) checked for `.prx` names with no file in `--libs` or `--modules`, following present libraries transitively. Gaps fail the audit and are reported as `Importer: <root> Chain: <root> -> ... -> <missing>` lines, since the loader fails on them even when every game import resolves.

### Audit options
| Option | Effect |
|---|---|
| `--libs <dir>` | Built `.prx` folder. Required; repeat for several. |
| `--modules <dir>` | Folder of the title's own modules; repeat for several. |
| `--source <dir>` | `core/libs/prx` tree searched for throwing stubs. Defaults to this repository's. |
| `--strict-stubs` | Also exit `1` when an import has class `stub` (weak imports and imports of `--modules` files included). `partial` imports don't count. |
| `--strict-providers` | Also exit `1` when an import that names a library is exported only by another library. `module` imports and imports that `libc.prx` exports don't count. |
| `--names <file>` | NID and name per line, as in `aerolib.csv` from [`tools/nid_names.py`](../../tools/nid_names.py). Names the absent imports. Never downloaded. A name that doesn't hash to its NID is marked. |
| `--json <file>` | Full result, stamped with the repository's commit and whether its tree was modified. |

Registry `nid` fields may hold either encoded NIDs or ordinary ELF symbol names such as `strcmp`. Imports are matched against literal exports first, then against their computed NIDs when the literal lookup misses. Reports keep the original import name. Export availability doesn't prove every platform loader supports that lookup path.

**Audit exit codes:**
- `0`: nothing blocks loading.
- `1`: there are `absent` imports, import name mismatches, needed libraries or library host dependencies without a file, or (with `--strict-stubs` / `--strict-providers`) `stub` imports or imports exported only by another library.
- `2`: an input couldn't be read; the message names the file and the value.

A module registry lists `nid`, `library` and `targetOffset` for each relocation against a function that no other bundled module exports, without the `#` suffix of the executable's NIDs.

---

## 11. Patching imports of native applications

`nid_patcher --exec <file> [file2 ...]` patches imports in place and preserves the application's exports. It detects PE, ELF64 and Mach-O64 from file contents. No library name is required. Build a separate executable for each target platform.

**Default naming rules** (same as library export naming):
- `_nid_no_patch` and `SDL_*` stay unchanged.
- `_nid_no_patch_cut` is removed without hashing.
- `_nid_postfix` and numeric `_nid_disambig` suffixes are removed before computing the NID.
- When both `foo` and `foo_nid_postfix` are imported, `foo` stays unchanged.
- These rules also apply to host imports unless explicitly preserved.

**Modes:**
- `nid_patcher --exec --only-sce <file>`: restrict renaming to names starting with `sce` (case-insensitive) or ending with `_nid_postfix`.
- Both modes accept `--preserve-exports <unpatched-library>` or `--preserve-export-list <export-list>` for explicit exclusions.
- The existing `nid_patcher <library_name> <file>` mode is unchanged.

Unsupported layouts fail before the input file is written.
