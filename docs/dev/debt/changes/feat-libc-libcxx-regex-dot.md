# libc

## add Functional

- [libc++ narrow regex dot matcher](../../../core/libs/prx/libc/src/RuneLocale.cpp) (libc) has an unverified firmware provider. The match-state layout, actions, and narrow-character behavior follow the [LLVM libc++ 18.1.8 regex implementation](https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/regex.cpp) in the public PS5 Payload SDK v0.42. The Azahar ELF in RetroArch PPSA99169 (#5927) binds this named import in the matcher vtable at `0x28f5ca8`.
