# libSceFreeTypeOt

## add Pending dependency exports

- [FT_Get_Multi_Master](../../../core/libs/prx/libSceFreeTypeOt/ForwardExports.cpp) (NID `pORpGtdlfqQ`) has an unverified console ABI and return codes. Windows forwards to the [libSceFreeType adapter](../../../core/libs/prx/libSceFreeType/src/Variations.cpp). FreeType 2.4.11 and the bundled FreeType agree on Adobe Multiple Master metadata, and axis names are borrowed from the face. The bundled version rejects a null output with Invalid_Argument, including when the face is also null, where 2.4.11 returns Invalid_Face_Handle.
