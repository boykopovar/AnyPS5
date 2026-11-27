# libc

## add Functional

- [libc++ regex character classes](../../../core/libs/prx/libc/src/RuneLocale.cpp) (libc) have an unverified firmware provider. The masks follow the [FreeBSD configuration in LLVM libc++ 18.1.8](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/include/__locale) and the [regex lookup](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/regex.cpp). Azahar in RetroArch PPSA99169 (#5927) passes a C string and a Boolean at `0x4931f0` and reads the 64-bit mask.
- [libc++ regex collating names](../../../core/libs/prx/libc/src/RuneLocale.cpp) (libc) have an unverified firmware provider. The ASCII names and empty or one-byte results follow [LLVM libc++ 18.1.8](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/regex.cpp). Azahar in RetroArch PPSA99169 (#5927) consumes a 24-byte libc++ result at `0x490cee` and `0x492aee`.
