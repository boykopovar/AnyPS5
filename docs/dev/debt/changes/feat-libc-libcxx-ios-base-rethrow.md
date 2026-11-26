# libc

## add Functional

- [std::__1::ios_base::__set_badbit_and_consider_rethrow](../../../core/libs/prx/libc/src/LibcxxIosBase.cpp) (libc) - the console provider is unverified, and the `ios_base` layout (`__rdstate_` at +32, `__exceptions_` at +36, `badbit` 0x1) comes from [LLVM libc++ `<ios>`](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/include/ios), not from a console object. It sets `badbit` and rethrows the active exception when `badbit` is in the exception mask, as [LLVM libc++](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/ios.cpp) does. This is 1 of the stream imports listed for Azahar in RetroArch (PPSA99169, #5927); the rest of that stream group stays unresolved.
