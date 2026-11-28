# libSceFreeTypeOt

## add Pending dependency exports

- [FT_Face_GetCharVariantIsDefault](../../../core/libs/prx/libSceFreeTypeOt/ForwardExports.cpp) (NID `Vrab2WK1mgk`) has an unverified console ABI and behavior. Windows forwards to the [libSceFreeType adapter](../../../core/libs/prx/libSceFreeType/Export.cpp), which passes the low 32 bits of both inputs to bundled FreeType. The signed results for default, non-default and absent mappings were compared with the bundled format 14 driver.
