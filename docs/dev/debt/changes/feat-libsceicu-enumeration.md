# libSceIcu

## add Functional

- [Enumeration openers and defaults](../../../core/libs/prx/libSceIcu/Enumeration.cpp) (libSceIcu) - `uenum_openCharStringsEnumeration` and `uenum_openUCharStringsEnumeration` wrap the host enumeration through the same `Icu::OpenHostEnumeration` helper the converter name exports use; `uenum_reset` calls the enumeration's own reset function or answers `U_UNSUPPORTED_ERROR`; `uenum_nextDefault` and `uenum_unextDefault` convert through a scratch buffer kept in the enumeration's `baseContext`, growing it with an eight byte pad exactly as ICU 68.2's `_getBuffer` does. `uenum_openFromStringEnumeration` stays a stub: it needs the `UStringEnumeration` model.
