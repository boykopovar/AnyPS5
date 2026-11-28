# libc

## add Functional

- [std::__1::chrono::system_clock::now](../../../core/libs/prx/libc/src/Time.cpp) (libc) - a named libc++ ABI import used by converted cores including Azahar in RetroArch (PPSA99169, #5927). It returns the guest time point as a signed 64-bit count of microseconds since the Unix epoch, following the [LLVM libc++ POSIX implementation](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/chrono.cpp#L105-L124). The import is unverified as a firmware export.
