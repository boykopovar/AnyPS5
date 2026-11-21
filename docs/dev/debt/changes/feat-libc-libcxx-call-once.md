# libc

## add Functional

- [std::__1::__call_once](../../../core/libs/prx/libc/src/CxxAbiSupport.cpp) (libc) - the libc++ ABI import listed for Azahar in RetroArch (PPSA99169, #5927). It follows [LLVM libc++'s once-flag states and exception retry behavior](https://github.com/llvm/llvm-project/blob/main/libcxx/src/call_once.cpp), using a 64-bit guest flag, one process-wide mutex and condition variable, and checked guest memory access. The firmware provider and behavior have not been verified on a console. Guest memory becoming unwritable while a callback runs can prevent publishing the completed or reset state.
