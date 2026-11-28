# libc

## add Functional

- [std::__1::chrono::steady_clock::now](../../../core/libs/prx/libc/src/Time.cpp) (libc) - a named libc++ ABI import of converted cores including Azahar in RetroArch (PPSA99169, #5927). It returns the guest time point as a signed 64-bit count of monotonic nanoseconds, consistent with the [LLVM libc++ POSIX implementation](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/chrono.cpp#L192-L206). On Windows it uses the kernel clock when available, including APS5_TIME_SCALE. Standalone libc uses host monotonic time until libkernel registers its clock. The import is unverified as a firmware export.
