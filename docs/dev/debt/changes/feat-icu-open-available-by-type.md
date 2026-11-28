# libSceIcu

## add Functional

- [uloc_openAvailableByType](../../../core/libs/prx/libSceIcu/AvailableLocales.cpp) - the PS5 ABI and behavior are not measured, and valid `ULocAvailableType` values enumerate the `res_index` locales of the bundled ICU data package. Null, unreadable or newly unwritable status pointers throw `std::invalid_argument` with `errno` preserved instead of faulting. The export follows pinned ICU 78.1 and wraps the host enumeration in a guest-callable `UEnumeration` adapter. Closes when: measured on a PS5.
