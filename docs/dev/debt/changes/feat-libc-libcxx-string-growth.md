# libc

## add Functional

- [libc++ string growth](../../../core/libs/prx/libc/src/LibcxxStringGrowth.cpp) (libc) - the firmware provider and full title behavior are unmeasured, and the `wchar_t` `__grow_by` import is not provided, since the title does not yet settle its element width. `__grow_by`, copy assignment and `operator+(const char*, const string&)` for `basic_string<char>`, imported by RetroArch Azahar (PPSA99169, #5927), follow LLVM libc++ 17.0.6 on the 24-byte layout observed in the guest.
