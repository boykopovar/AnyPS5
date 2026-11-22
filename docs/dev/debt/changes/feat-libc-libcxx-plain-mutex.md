# libc

## add Functional

- [std::__1::mutex lock, unlock, try_lock and destructor](../../../core/libs/prx/libc/src/LibcxxPlainMutex.cpp) (libc) - implements the four imports listed for Azahar in RetroArch (PPSA99169, #5927). [LLVM libc++ 17](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/mutex.cpp) forwards these operations to pthread; [FreeBSD 11.4](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/sys/_pthreadtypes.h) represents the mutex as a pointer-sized handle with a null static initializer. This bridge uses the existing guest pthread mutex exports in libkernel, so native-handle ownership is shared. Unlock and destruction ignore pthread errors to preserve libc++'s non-throwing behavior. The console provider, firmware class layout and guest C++ exception ABI on lock errors have not been measured.
