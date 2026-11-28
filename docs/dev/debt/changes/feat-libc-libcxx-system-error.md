# libc

## add Functional

- [libc++ system_error](../../../core/libs/prx/libc/src/exception/StandardExceptions.cpp) (libc) - the `std::__1` `generic_category`, `system_error` constructors, destructors, RTTI and vtable, `__throw_system_error` and `error_category` base destructor and RTTI imported by RetroArch Azahar (PPSA99169, #5927) follow the [LLVM libc++ 17 system_error.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/libcxx/src/system_error.cpp) layout and the Azahar relocations, not a console module; no firmware provider was measured. `ELAST` is taken as FreeBSD 11's 96 (`sys_nerr` - 1), so from 97 the categories report `unspecified generic_category error` or `unspecified system_category error` and the system category keeps the value; the ELAST of the guest's libc++ build is unverified, and the guest `strerror` names some errnos above 96. Messages below that come from the guest `strerror_r`. A null `what` argument or error category throws.
