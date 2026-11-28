# libSceIcu

## add Functional

- [`ucnv_getStandard`](../../../core/libs/prx/libSceIcu/ConverterStandards.cpp) forwards the indexed standards-table lookup to bundled ICU 78.1 with the upstream `uint16_t` index and `UErrorCode*` contract. The guest status is copied through checked guest-memory access while preserving `errno`; a preexisting failure returns immediately. The reduced ICU data contains no converter alias table, so it can report no standards and `U_MISSING_RESOURCE_ERROR` rather than the names available from a full ICU data package. The ABI and results are not verified on a PS5. Closes when: compared with the console and the matching converter alias data is bundled.
