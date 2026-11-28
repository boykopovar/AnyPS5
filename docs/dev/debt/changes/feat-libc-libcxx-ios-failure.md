# libc

## add Functional

- [libc++ ios_base::failure](../../../core/libs/prx/libc/src/exception/StandardExceptions.cpp) (libc) - the `std::__1` `iostream_category`, `ios_base::failure` constructors, destructors, RTTI and vtable imported by RetroArch Azahar (PPSA99169, #5927) follow [LLVM libc++ 17 ios.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/ios.cpp), not a console module; no firmware provider was measured. `iostream_category` reports `unspecified iostream_category error` for 1 (`io_errc::stream`) and from `sys_nerr` on, with the same unverified ELAST of 96 as the `system_error` categories. A null message, error code or category throws.
