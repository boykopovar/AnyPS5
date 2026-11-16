# libSceNpCommon

## add Functional

- [sceNpUtilXmlEscape and sceNpUtilXmlUnescape guest outputs](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - each piece, the terminator and the required length are written with GuestCopyOut_nid_no_patch in the same order as before, so a buffer that turns out too small still holds the pieces written before; returning `0x80550601` (the null-argument error) for an unwritable buffer or length is a host-safety fallback with console behaviour unmeasured. The source text is still read in place.
