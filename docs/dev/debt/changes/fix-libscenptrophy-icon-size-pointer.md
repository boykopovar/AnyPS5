# libSceNpTrophy

## add Functional

- [sceNpTrophyGetGameIcon, sceNpTrophyGetGroupIcon, sceNpTrophyGetTrophyIcon](../../../core/libs/prx/libSceNpTrophy/Export.cpp) (libSceNpTrophy) - returning 0x80551604 (NP_TROPHY_ERROR_INVALID_ARGUMENT) with errno preserved for a non-null size pointer that cannot be read or written is a host-safety fallback; whether console hardware faults or returns an argument error on such a pointer is unmeasured.
