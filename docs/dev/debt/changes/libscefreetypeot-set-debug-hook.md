# libSceFreeTypeOt

## add Unknown function info

- [FT_Set_Debug_Hook](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) (libSceFreeTypeOt) - a non-null hook at index 0 (`FT_DEBUG_HOOK_TRUETYPE`) throws: FreeType [calls it in place of the bytecode interpreter](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/truetype/ttobjs.c#L1020-L1028) with an execution context that the bundled 2.15.0 lays out differently. Indices 1 to 3 are stored and never read; 2.4.12 [reads index 1](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/truetype/ttobjs.c#L624-L645) only when built without the bytecode interpreter, which its default configuration includes. The provider's configuration and prototype are unverified.
