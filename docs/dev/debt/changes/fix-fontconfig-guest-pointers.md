# libfontconfig

## add Functional

- [Pattern and configuration query guest pointer checks](../../../core/libs/prx/libfontconfig/Objects.cpp) (libfontconfig) - `FcPatternGet`, `FcPatternGetInteger`, `FcPatternGetBool`, `FcPatternGetString`, `FcPatternGetFTFace`, `FcPatternGetMatrix`, `FcPatternGetCharSet`, `FcPatternGetLangSet`, `FcPatternAddMatrix`, `FcFontSetMatch`, `FcFontMatch`, and `FcFontSort` transfer guest `FcValue`, scalar/pointer outputs, `FcMatrix` inputs, and `FcResult` outputs through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting invalid or unwritable pointers via `NotImplemented_nid_no_patch`; console behavior on faulted pointers is unverified.
