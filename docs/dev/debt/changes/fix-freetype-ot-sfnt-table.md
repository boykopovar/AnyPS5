# libSceFreeTypeOt

## add Pending dependency exports

- [FT_Get_Sfnt_Table](../../../core/libs/prx/libSceFreeTypeOt/ForwardExports.cpp) (NID `omDz0x8lyHk`) has an unverified console ABI and behavior. Windows forwards to the [libSceFreeType adapter](../../../core/libs/prx/libSceFreeType/src/SfntTable.cpp), which returns a face-owned LP64 snapshot refreshed on lookup. Guest writes to that snapshot do not affect FreeType. The `hhea` and `vhea` internal metric pointers retain their native representation. The bundled `head` timestamps are unsigned words and `OS/2` includes optical size fields absent from FreeType 2.4.11.
