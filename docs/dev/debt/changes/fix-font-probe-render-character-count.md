# libSceFont

## add Unknown function info

- [sceFontStringRefersRenderCharacters count](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - the character count, including the 0 of every failure path, is written with `GuestCopyOut_nid_no_patch`; an unwritable count returns null like a null count, because the console's behaviour on unmapped memory is unmeasured.
