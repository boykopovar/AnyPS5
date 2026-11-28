# libfontconfig

## add Unknown function info

- [FcLangSetCreate, FcLangSetDestroy, FcLangSetAdd, FcLangSetHasLang, FcPatternCreate, FcPatternDestroy, FcPatternReference, FcPatternDuplicate, FcPatternFilter, FcPatternEqual, FcPatternDel, FcPatternRemove, FcPatternGet, FcPatternAddCharSet, FcPatternAddLangSet, FcPatternAddMatrix, FcFontRenderPrepare](../../../core/libs/prx/libfontconfig/Objects.cpp) (libfontconfig) - unknown or unmapped `FcPattern*` and `FcLangSet*` handles are rejected via host handle registries (`NotImplemented_nid_no_patch`), `FcCharSet*` handles are checked for alignment/readability/writability before retaining, and guest reads/writes use `GuestArenaPrepareHostRead_nid_no_patch` / `GuestArena::HostWrite` while preserving `errno`; console fault behavior for forged or unmapped handles is not measured.
