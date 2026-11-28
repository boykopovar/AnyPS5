# libSceIcu

## add Functional

- [Enumeration reset and default next helpers](../../../core/libs/prx/libSceIcu/Enumeration.cpp) have no console ABI or behavior verification. `uenum_reset`, `uenum_nextDefault` and `uenum_unextDefault` were throwing stubs in `LegacyExports.cpp`; they follow ICU 68.2 [uenum.cpp](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/uenum.cpp), which is the same in the pinned 78.1 apart from the `NULL` and `UChar` spellings and casts. The default helpers convert through the 4-byte length header plus data buffer that ICU keeps in `baseContext`, growing it with 8 bytes of padding. The buffer is allocated with the guest `realloc` because `uenum_close` frees `baseContext` with the guest `free`. A null enumeration, null status, or null `uenum_nextDefault` length throws where ICU dereferences it. Closes when: compared with the console.
