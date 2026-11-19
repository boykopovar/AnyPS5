# libSceNpCommon

## add Unknown function info

- [sceNpUtilJsonUnescape guest outputs](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - each decoded piece, the required length and the terminator are written with `GuestCopyOut_nid_no_patch` in the same order as before; after an unwritable piece nothing more is written, and a non-null unwritable buffer or length returns `0x80550601` (the null-argument error), which is a host-safety fallback. The source text is still read in place, and console behaviour on unmapped memory is unmeasured.
