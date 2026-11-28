# libc

## add Functional

- [libc++ bad_weak_ptr](../../../core/libs/prx/libc/src/exception/StandardExceptions.cpp) (libc) provides the destructor, RTTI and vtable imported by RetroArch's Azahar core (PPSA99169, #5927). The type uses the separate libc++ name, has one public std::exception base and reports `bad_weak_ptr` from `what()`, matching [LLVM libc++ 17](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/memory.cpp). The console provider and runtime behavior have not been measured. Other Azahar imports and the Windows PE export limit remain.
