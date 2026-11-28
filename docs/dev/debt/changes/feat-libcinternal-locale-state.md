# libc

## add Functional

- [Unversioned global locale state and _Locimp vtable](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc) stores `g_globalLocale` starting at `g_classicLocale`, so `_ZNSt6locale5_InitEv` (`9rMML086SEE`) and `_ZNSt6locale16_GetgloballocaleEv` (`hEQ2Yi4PJXA`) observe `libSceLibcInternal`'s `locale::global` instead of hardcoding `g_classicLocale`. The console's unversioned `_Locimp` vtable and `facets` ownership were not disassembled; the vtable uses `RetainFacet`/`ReleaseFacet` and frees only the `facets` vector via `ApplicationHeapFree_nid_no_patch` without releasing facets (`_Locimp::_Addfac` does not retain them). The transparent fallback retains the global `_Locimp` through a `GlobalLocaleHold` for as long as the caller uses the facet; `_Getgloballocale` itself still returns a borrowed pointer, as the console export does.

# libSceLibcInternal

## add Functional

- [_ZNSt6locale6globalERKS_, _ZNSt6locale7_LocimpC2Eb and _ZNSt8_LocinfoC1EiPKc](../../../core/libs/prx/libSceLibcInternal/LocaleObjects.cpp) (libSceLibcInternal, NIDs `ptwhA0BQVeE`, `zrmR88ClfOs` and `L1Ze94yof2I`) were written against Microsoft STL (`165a67afdd`) `xlocale`, `xlocinfo` and `locale0.cpp`, `[locale.statics]`, and the repo's `GuestLocale::Implementation` (`0x30` bytes, `const char*` name) and `LocinfoStorage` (`64` bytes) ABIs rather than console binary disassembly. The category argument is validated as an STL mask, but its effect is not independently observable while only `"C"` is supported. `locale::global` uses `LibcSwapGlobalLocale_nid_no_patch` under `g_localeInitMutex` instead of the console's `_Setgloballocale` stub and rejects non-`"*"` names failing `setlocale`; `_LocimpC2Eb` installs `LibcLocimpVtable_nid_no_patch()`; `_LocinfoC1EiPKc` accepts category masks `0..0x3f` and only `"C"`.

## remove [_ZNSt6locale6globalERKS_]

## remove [_ZNSt6locale7_LocimpC2Eb]

## remove [_ZNSt8_LocinfoC1EiPKc]
