# libc

## add Functional

- [std::exception_ptr copy, destructor and rethrow](../../../core/libs/prx/libc/src/exception/ExceptionPointer.cpp) (libc) - implements the three exact Azahar imports recorded for RetroArch (PPSA99169, #5927). [LLVM libc++ 17](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/support/runtime/exception_pointer_cxxabi.ipp) stores one exception pointer, retains it on copy, releases it on destruction and rethrows through `__cxa_rethrow_primary_exception`. The bridge uses AnyPS5's guest exception reference counting and the Itanium indirect argument for the non-trivial by-value rethrow parameter. The console provider and firmware exception ABI have not been measured.
