# libc

## add Functional

- [libc++ string storage](../../../core/libs/prx/libc/src/LibcxxStringStorage.cpp) (libc) implements the character string's reserve member and base destructor imported by Azahar (#5927). Long strings use the same guest new and delete exports as the caller. Capacity follows the low-bit tag in the default little-endian layout, with a limit of `UINT64_MAX - 17` after 16-byte alignment and the terminator. The one-argument reserve keeps existing capacity when the request fits, matching [LLVM libc++ 23.1.1](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.1/libcxx/include/string#L3010-L3021). The exact console libc++ revision and behavior remain unmeasured. Destroying an unreadable object or an invalid long representation can raise the guest invalid-argument exception.
