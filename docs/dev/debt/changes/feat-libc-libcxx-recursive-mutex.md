# libc

## add Functional

- [std::__1::recursive_mutex constructor, destructor, lock, try_lock and unlock](../../../core/libs/prx/libc/src/LibcxxMutex.cpp) (libc) - implements the five imports listed for Azahar in RetroArch (PPSA99169, #5927). [LLVM libc++ 17's pthread backend](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/include/__threading_support) initializes a recursive pthread mutex and forwards lock operations to pthread; [FreeBSD 11.4](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/sys/_pthreadtypes.h) represents its mutex as one pointer-sized field. This bridge uses the existing guest pthread mutex and attribute exports in libkernel, so the native handle shares guest pthread ownership and recursive behavior. The console provider, firmware class layout, guest C++ exception ABI on error paths, and destruction while another thread owns the mutex have not been measured.
