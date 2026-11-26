# libSceLibcInternal

## change u6UfGT9+HEA

- [Additional 3.20 imports](../../../core/libs/prx/libSceLibcInternal/InventoryExports2.cpp) (libSceLibcInternal) - `_ZNKSt19istreambuf_iteratorIcSt11char_traitsIcEE5equalERKS2_` (NID `u6UfGT9+HEA`), `_ZNKSt19istreambuf_iteratorIwSt11char_traitsIwEE5equalERKS2_` (NID `jZmLD-ASDto`); signatures and behavior are unverified and calls throw through `NotImplemented_nid_no_patch`.

## add Additional PS5 3.20 import declarations

- [_ZNSt13bad_exceptionD0Ev and _ZNKSt13bad_exception8_DoraiseEv](../../../core/libs/prx/libSceLibcInternal/ExceptionVirtuals.cpp) (libSceLibcInternal, NIDs `lX9M5u0e48k` and `kTlQY47fo88`): `D0` only calls `_ZdlPv`, since `D1` is the bare `ret` noted under Empty facet and exception members. `_Doraise` throws a new 8-byte `std::bad_exception` derived from `St9exception` through `libc`, so a guest handler for either type catches it; a null object throws `std::invalid_argument`. Its `what()` is `bad exception`, the default message in [Microsoft STL](https://github.com/microsoft/STL/blob/54d74d7a58e828857a73f979eccc2ba1c02e256d/stl/inc/exception#L138); the console string is unverified. `libc` does not export the type's `typeinfo` or vtable, since which library does is unknown. The console module bodies were not read and no title was seen calling either
