# NID patching

A title's `.prx` imports system library functions by 11-character names (NIDs). The host libraries are built with readable C names, and `nid_patcher` rewrites the linked binaries so their exports and imports match what a title expects. The rules below are read from the cited sources; [CONTRIBUTING](../../CONTRIBUTING.md) states which suffix each case requires.

## The NID hash

`ComputeNid` ([NidCompute.cpp](../../core/libs/nid/src/NidCompute.cpp)):

1. SHA-1 of the symbol name's UTF-8 bytes followed by the fixed 16-byte suffix `51 8D 64 A6 35 DE D8 C1 E6 B0 39 B1 C3 E5 52 30`.
2. Take the first 8 digest bytes and reverse their order.
3. Encode 6 bits at a time over the alphabet `A-Z a-z 0-9 + -`: the first 6 bytes give 8 characters, the remaining 2 give 3. The NID is the resulting 11 characters.

The `libraryName` argument is not used; the NID is a function of the symbol name alone. The C++ implementation and [tools/nid_names.py](../../tools/nid_names.py) agree with each other and with the SCE symbol database the tool downloads (aerolib): 159,030 of 159,042 entries hash to their listed NID, and the 12 exceptions are four `sceBgft*` names repeated across modules.

`tools/nid_names.py` also scans the `APS5_EXPORT(...)` stubs for unknown NIDs, checks candidate real names against the database, and flags a row when the name does not hash back to the NID.

## The patcher

`nid_patcher <library_name> [--preserve-exports <unpatched-library>] <file>...` ([main.cpp](../../core/libs/nid/src/main.cpp)).

[core/libs/CMakeLists.txt](../../core/libs/CMakeLists.txt) builds every library into `libs/unpatched` and then patches a copy into `libs`: `libc` is patched without a reference, and every other library with `--preserve-exports` pointing at the unpatched `libc.prx`. `--preserve-exports` reads the reference's export names (PE or ELF), strips a trailing `_nid_postfix` from each, and uses the result as the exclusion set ([ExportExclusions.cpp](../../core/libs/nid/src/ExportExclusions.cpp)).

- **PE** ([PeNidPatcher.cpp](../../core/libs/nid/src/PeNidPatcher.cpp)): rewrites the export name table in place (repacking the name strings, sorting the name/ordinal arrays, failing on a duplicate result name), rewrites imports by name, and renames the `.eh_frame` section to `.ehfram`.
- **ELF** ([ElfNidPatcher.cpp](../../core/libs/nid/src/ElfNidPatcher.cpp)): rewrites defined and undefined `.dynsym` names, rebuilds `.dynstr` (deduplicating suffixes, and the result must fit the original section), remaps `DT_NEEDED` and `.gnu.version_r` name offsets, and rebuilds `.gnu.hash` (reordering `.dynsym` and fixing up relocation symbol indices). A `.gnu.version_d` section is a hard error.

## Export rules

`ResolveNids` ([NidResolver.cpp](../../core/libs/nid/src/NidResolver.cpp)) applies the first matching rule per export name:

| Condition | Result |
| --- | --- |
| The name, with a trailing `_nid_postfix` stripped, is in the `--preserve-exports` reference | kept verbatim |
| ends in `_nid_no_patch_cut` | exported with the suffix removed |
| ends in `_nid_no_patch`, or starts with `SDL_` | kept verbatim |
| both `X` and `X_nid_postfix` are exported | `X` is kept verbatim |
| otherwise | renamed to `NID(X)`, where `X` is the name with a trailing `_nid_postfix` and `_nid_disambig<digits>` removed |

So `std_execute_once_nid_postfix` exports `NID(std_execute_once)`, `fstat_nid_disambig1_nid_postfix` exports `NID(fstat)`, `rWSuTWY2JN0_nid_no_patch_cut` exports `rWSuTWY2JN0`, and `SceNpUtilityModuleLoaded_nid_no_patch` is unchanged.

## Import rules

An import is rewritten only when it is an undefined, non-local, unversioned symbol (ELF: no `.gnu.version` entry, or index at most 1) whose name ends in `_nid_postfix` or starts with `sce` (case-insensitive). The replacement comes from the same naming rules: `_nid_no_patch_cut` is cut, `_nid_no_patch` and `SDL_` are kept, everything else becomes `NID(X)` with `X` the name with the suffixes removed. On PE the rewrite is in place and must not be longer than the original name.

The suffix test is against the literal dynamic symbol name: a mangled C++ name does not end in `_nid_postfix`, so it is never rewritten. Cross-library calls must therefore be `extern "C"`, as in [GuestArena.hpp](../../core/libs/prx/libc/include/GuestArena.hpp).

## Suffixes

- **(none)** - the default for SCE names that can be used as-is: `sceKernelOpen` ([File.hpp](../../core/libs/prx/libkernel/File/include/File.hpp)), `sceVideoOutOpen` ([Output.hpp](../../core/libs/prx/libSceVideoOut/include/Output.hpp)). The export is renamed to the NID of the full name.
- **`_nid_postfix`** - the SCE symbol's real name cannot be used as a plain C identifier (POSIX names, C++ runtime names), so the identifier carries the suffix. The patcher strips it before hashing, and on the import side it is the marker that makes the import rewrite to the same NID. `extern "C"` is required. Examples: `access_nid_postfix` ([Filesystem.cpp](../../core/libs/prx/libc/src/Filesystem.cpp)), `std_execute_once_nid_postfix` and `_ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_postfix` ([libc/Export.cpp](../../core/libs/prx/libc/Export.cpp)), `clock_gettime_nid_postfix` ([Common.hpp](../../core/libs/prx/libkernel/Pthread/Posix/Common.hpp)), `__error_nid_postfix` ([SystemConfiguration.cpp](../../core/libs/prx/libkernel/System/src/SystemConfiguration.cpp)), `GuestArenaPinWritable_nid_postfix` ([GuestArena.hpp](../../core/libs/prx/libc/include/GuestArena.hpp)).
- **`_nid_disambig<digits>`** - goes between the base name and `_nid_postfix`; the marker and its digits are removed before hashing, so the export still resolves to the base NID. Used for the `fstat` and `pwrite` overloads in libkernel ([Stdio.cpp](../../core/libs/prx/libkernel/File/src/Stdio.cpp)).
- **`_nid_no_patch`** - helper symbols that are referenced by name across the codebase; the name keeps the suffix in the final module on both sides. Unimplemented exports call `NotImplemented_nid_no_patch(__func__)` ([CONTRIBUTING](../../CONTRIBUTING.md), [libSceAudioIn/Export.cpp](../../core/libs/prx/libSceAudioIn/Export.cpp)); other examples: `LibcHeapTraceInfo_nid_no_patch` ([libc/Export.cpp](../../core/libs/prx/libc/Export.cpp)), `Aps5FiberSwitchStack_nid_no_patch` ([libSceFiber/Export.cpp](../../core/libs/prx/libSceFiber/Export.cpp)), the module-loaded flag `int SceNpUtilityModuleLoaded_nid_no_patch = 1` ([libSceNpUtility/Export.cpp](../../core/libs/prx/libSceNpUtility/Export.cpp)).
- **`_nid_no_patch_cut`** - not written by hand. The `APS5_EXPORT("NID", func)` macro ([ExportMacros.hpp](../../core/libs/prx/libc/include/general/ExportMacros.hpp)) exports an alias named `<NID>_nid_no_patch_cut`, which the patcher cuts to the bare NID string. Used for unknown NIDs whose real name is not known: `APS5_EXPORT("rWSuTWY2JN0", libcCyberUnknown18)` ([libc/Export.cpp](../../core/libs/prx/libc/Export.cpp)), `APS5_EXPORT("X+4jdIS75P0", sceAudioInUnknown_X4jdIS75P0)` ([libSceAudioIn/Export.cpp](../../core/libs/prx/libSceAudioIn/Export.cpp)).
- **`SDL_`** - SDL is a host library and is imported by name, so its exports are kept verbatim.

## The failure mode to check

Tests run against the unpatched libraries, so every name binds; a title loads the patched ones. A cross-library import that the export side renames but the import rule does not rewrite therefore links, passes CI, and then fails to load for a title - on Windows the module load fails with `GetLastError: 127`. [#1613](https://github.com/boykopovar/AnyPS5/issues/1613) is the example: `libScePosixForWebKit.prx` imports `_ZN12GuestSockets6IsOpenEi` from libkernel. [#1827](https://github.com/boykopovar/AnyPS5/issues/1827) records the rule and its consequence; [#1828](https://github.com/boykopovar/AnyPS5/pull/1828) adds a test that loads every patched library with its imports bound.

Written with AI assistance.
