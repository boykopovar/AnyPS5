# libSceIcu

## change [Enumeration dispatch]

- [Enumeration dispatch](../../../core/libs/prx/libSceIcu/Enumeration.cpp) is unverified on a console. `uenum_close`, `uenum_count`, `uenum_next` and `uenum_unext`, which firmware 13.42 exports ([#11013](https://github.com/boykopovar/AnyPS5/pull/11013#issuecomment-6401310351)), follow ICU 78.1 [uenum.cpp](https://github.com/unicode-org/icu/blob/release-78.1/icu4c/source/common/uenum.cpp) over the 56-byte `UEnumeration` of [uenumimp.h](https://github.com/unicode-org/icu/blob/release-78.1/icu4c/source/common/uenumimp.h), calling its callbacks with the guest ABI. `uprv_free` is the guest `free`, as ICU's default allocator is `free` and 13.42 exports no `u_setMemoryFunctions`. Native-backed numbering enumerations use guest-ABI callbacks that release native ownership without passing native storage to guest free. A null or unreadable status with a nonnull enumeration throws where ICU dereferences it. Closes when: compared with the console.
