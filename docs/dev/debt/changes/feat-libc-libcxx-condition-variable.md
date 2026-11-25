# libc

## add Functional

- [std::__1::condition_variable notify_all, notify_one, timed wait, wait and destructor](../../../core/libs/prx/libc/src/LibcxxCondition.cpp) (libc) - implements the five imports listed for Azahar in RetroArch (PPSA99169, #5927). [LLVM libc++ 17](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/condition_variable.cpp) forwards these operations to pthread condition variables; [FreeBSD 11.4](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/sys/_pthreadtypes.h) represents the condition and mutex handles as pointer-sized fields. This bridge uses the guest pthread condition and mutex exports in libkernel and converts a system-clock nanosecond time point to an absolute guest timespec. The console provider, firmware class layout, guest C++ exception ABI on error paths, destruction with active waiters, and title-level behavior have not been measured.
