# libc

## add Functional

- [Windows unwind tables](../../../core/libs/prx/libc/src/exception/Unwind.cpp) (libc) - MinGW GCC on x86-64 emits no `.pdata` entry for a `sysv_abi` function even with unwind tables on, so an exception raised in a callee of a guest export cannot unwind through that frame (#26048); a standalone `sysv_abi` middle frame aborts, while its MS-ABI twin is caught. `-fno-asynchronous-unwind-tables` turns SEH and `__SEH__` off and emits DWARF `.eh_frame` for those frames, which `configure_windows_unwind` renames to `.ehfram` for the guest unwinder; WinLibs GCC 16.2.0 r3 keeps that fallback, because GCC's fix for Win64 `sysv_abi` SEH ([May 2026](https://gcc.gnu.org/pipermail/gcc-patches/2026-May/715961.html)) is not in it. `mingw_unwind_probe` checks the fallback only: a build that selects System V ABI SEH does not compile with that flag. Closes when: a supported toolchain emits `.pdata` for `sysv_abi` frames and #26048 no longer aborts. Labels: windows, code
