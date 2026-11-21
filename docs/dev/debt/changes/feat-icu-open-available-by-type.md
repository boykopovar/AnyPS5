# libSceIcu

## add Functional

- [uloc_openAvailableByType](../../../core/libs/prx/libSceIcu/AvailableLocales.cpp) - the PS5 ABI and behavior are not measured, and the reduced bundled ICU data omits `res_index.res`, so valid `ULocAvailableType` values return `U_MISSING_RESOURCE_ERROR` and no enumeration until a guest supplies ICU data through `udata_setCommonData` rather than fabricating an empty enumeration. Null, unreadable or newly unwritable status pointers throw `std::invalid_argument` with `errno` preserved instead of faulting. The export follows pinned ICU 78.1 and wraps the host enumeration in a guest-callable `UEnumeration` adapter. Closes when: measured on a PS5.
