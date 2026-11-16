# libfreetype

## add Functional

- [Multiple masters arrays](../../../core/libs/prx/libfreetype/src/MultipleMasters.cpp) (libfreetype) - `FT_Get_MM_Blend_Coordinates` and `FT_Get_MM_WeightVector` write the caller's array through checked copies in chunks of 4096 values, so host memory doesn't follow the guest count; `*length` and the `FT_Set_MM_WeightVector` weights go through checked copies. An unreadable or unwritable pointer returns `FT_Err_Invalid_Argument` with `errno` kept; FreeType faults there. A host-safety fallback, not measured on the console.
- [Palette outputs](../../../core/libs/prx/libfreetype/src/Color.cpp) (libfreetype) - `FT_Palette_Data_Get` and `FT_Palette_Select` write their record or pointer through one checked copy; an unwritable output returns `FT_Err_Invalid_Argument` with `errno` kept. A host-safety fallback, not measured on the console.
