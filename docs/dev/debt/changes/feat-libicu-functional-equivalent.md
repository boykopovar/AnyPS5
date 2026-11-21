# libSceIcu

## add Functional

- [ures_getFunctionalEquivalent](../../../core/libs/prx/libSceIcu/LegacyExports.cpp) (libSceIcu) - not checked against `libSceIcu.sprx` or on a console. Resolves a null `locid` through `Icu::DefaultLocaleName()`, validates writable guest pointers (`status`, `isAvailable`, `result`) instead of dereferencing arbitrary guest pointers, guards null/zero-capacity error paths against upstream ICU writing `result[0] = 0`, and forwards to ICU's `ures_getFunctionalEquivalent` under `UCONFIG_NO_FILE_IO=1`.
