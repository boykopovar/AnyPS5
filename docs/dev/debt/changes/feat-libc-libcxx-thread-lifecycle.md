# libc

## add Functional

- [std::__1::thread join, detach and destructor](../../../core/libs/prx/libc/src/Threads.cpp) (libc) - provides the three imports listed for Azahar in RetroArch (PPSA99169, #5927). [LLVM libc++ 17](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/thread.cpp) joins or detaches the pthread handle, clears it only on success, and terminates if a thread is still joinable at destruction. The guest thread object is assumed to hold one pointer-sized pthread handle, as in the libc++ pthread backend. The console provider, exact firmware class layout, guest C++ exception ABI on error paths, process termination behavior on a joinable destructor, and title-level behavior have not been measured.
