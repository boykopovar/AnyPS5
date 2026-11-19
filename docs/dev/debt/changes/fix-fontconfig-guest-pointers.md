# libfontconfig

## add Functional

- [Pattern and configuration query guest pointer checks](../../../core/libs/prx/libfontconfig/Objects.cpp) (libfontconfig) - `FcPatternGet`, `FcPatternGetInteger`, `FcPatternGetBool`, `FcPatternGetString`, `FcPatternGetFTFace`, `FcPatternGetMatrix`, `FcPatternGetCharSet`, `FcPatternGetLangSet`, `FcPatternAddMatrix`, `FcFontSetMatch`, `FcFontMatch`, and `FcFontSort` transfer guest `FcValue`, scalar/pointer outputs, `FcMatrix` inputs, and `FcResult` outputs through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting invalid or unwritable pointers with `std::runtime_error`, or through `NotImplemented_nid_no_patch` in `FcFontSetMatch`, `FcFontMatch` and `FcFontSort`; console behavior on faulted pointers is unverified.
