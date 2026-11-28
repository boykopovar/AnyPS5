# libSceFreeTypeOt

## add Pending dependency exports

- [FT_Face_GetCharVariantIndex](../../../core/libs/prx/libSceFreeTypeOt/ForwardExports.cpp) (NID `dPcZ76eZb64`) has an unverified console ABI and behavior. Windows forwards to the [libSceFreeType adapter](../../../core/libs/prx/libSceFreeType/Export.cpp), which passes the low 32 bits of the character and selector to bundled FreeType. Format 14 default and non-default mappings were compared with the bundled driver.
