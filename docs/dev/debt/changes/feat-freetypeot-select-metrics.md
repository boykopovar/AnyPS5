# libSceFreeTypeOt

## change [FT_Select_Metrics, FT_Request_Metrics, FT_Match_Size]

- [FT_Select_Metrics, FT_Request_Metrics, FT_Match_Size](../../../core/libs/prx/libSceFreeTypeOt/src/SizeMetrics.cpp) (libSceFreeTypeOt) - console prototypes and FreeType version are unverified. The provider is dated to 2.4.11-2.4.12 [ftobjs.c](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftobjs.c#L2490-L2765), but all three follow bundled FreeType 2.15.0. In 2.4.12 `FT_Request_Metrics` returns nothing, handles zero dimensions and oversized ppem differently and applies the CELL rule only when both sizes are set; `FT_Match_Size` accepts a zero size. Null or unreadable pointers and overflow throw. `FT_Select_Metrics` works with bundled and system dependencies, reads only the selected strike and writes only the guest size metrics. With `ANYPS5_USE_SYSTEM_DEPS=ON`, the other two exports throw because the private FreeType functions are unavailable.
