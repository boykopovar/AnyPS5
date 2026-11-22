# libfontconfig

## add Functional

- [Pattern string, object-set and format guest pointer checks](../../../core/libs/prx/libfontconfig/Objects.cpp) (libfontconfig) - `FcLangSetAdd`, `FcLangSetHasLang`, `FcPatternFilter`, `FcPatternDel`, `FcPatternRemove`, `FcPatternGet*`, `FcPatternAdd*`, and `FcPatternFormat` copy guest C-strings and `FcObjectSet` headers and entry pointers through `GuestCopyIn_nid_no_patch` while preserving `errno`, rejecting invalid pointers and negative or null-backed object sets via `NotImplemented_nid_no_patch`; console behavior on faulted pointers is unverified.
