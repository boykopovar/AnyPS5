# libSceLibcInternal

## add Functional

- [Dinkum thread errors](../../../core/libs/prx/libSceLibcInternal/DinkumThreadErrors.cpp) are unverified on a console and on SDK versions newer than 10.00. SDK 10.00 `libc.prx` establishes the 48-byte guest-string layout and distinct runtime-error-derived RTTI: `_Throw_lock_error` at `0xc6d60`, `_Throw_resource_error` at `0xc6e50`, constructors at `0xc6db0` and `0xc6ea0`, complete destructors at `0xc6e40` and `0xc6f30`, and deleting destructors at `0xc6fd0` and `0xc7090`. Its vtable relocations at `0x18f810` and `0x18f840` use `runtime_error::what` at `0x14420` and the empty `exception::_Doraise` at `0x17140`. These exports do not repair the different message layout used by the existing generic `runtime_error` exports; an inline guest base-class operation that bypasses the Dinkum vtable can still encounter that gap.
