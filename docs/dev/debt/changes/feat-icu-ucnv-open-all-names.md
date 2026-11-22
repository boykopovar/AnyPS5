# libSceIcu

## add Functional

- [ucnv_openAllNames](../../../core/libs/prx/libSceIcu/ConverterAvailableNames.cpp) follows pinned ICU 78.1 and returns a guest-callable `UEnumeration` adapter whose count, narrow and Unicode iteration, reset and close operations forward to the host enumeration. The PS5 ABI and behavior are unmeasured. Null, unreadable or newly unwritable status pointers throw `std::invalid_argument` with `errno` preserved instead of reproducing upstream undefined memory access. The reduced bundled ICU data omits `cnvalias.icu`, so opening currently returns ICU's missing-resource error and no enumeration until converter alias data is supplied; it does not fabricate an empty enumeration.
