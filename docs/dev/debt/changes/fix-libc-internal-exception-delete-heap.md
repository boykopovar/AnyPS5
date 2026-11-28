# libSceLibcInternal

## change [_ZNSt18bad_variant_accessD1Ev and _ZNSt18bad_variant_accessD0Ev]

- [_ZNSt18bad_variant_accessD1Ev and _ZNSt18bad_variant_accessD0Ev](../../../core/libs/prx/libSceLibcInternal/ExceptionDestructors.cpp) (libSceLibcInternal, NIDs `U3b5A2LEiTc` and `8DNJW5tX-A8`): [LLVM libc++](https://github.com/llvm/llvm-project/blob/llvmorg-18.1.8/libcxx/include/variant#L270-L273) derives `bad_variant_access` from `exception` with no member and only a `what()` override, so `D1` runs the `libc` export `_ZNSt9exceptionD2Ev`, which frees nothing, and `D0` runs it and then `_ZdlPv`, as the [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#definitions) deleting destructor does. The console's `libSceLibcInternal.sprx` was not read and no title was seen calling them
