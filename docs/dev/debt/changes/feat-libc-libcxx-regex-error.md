# libc

## add Functional

- [libc++ regex_error](../../../core/libs/prx/libc/src/exception/StandardExceptions.cpp) (libc) supplies the constructor, destructor and RTTI imported by RetroArch Azahar (PPSA99169, #5927). The guest allocates 24 bytes and passes the libc++ ordinal; the object keeps the runtime_error base, stores the ordinal at byte 16, and selects the [LLVM libc++ 18 diagnostic](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/regex.cpp). The firmware provider and on-console behavior are unmeasured. Remaining core imports and the Windows PE export limit are separate.
