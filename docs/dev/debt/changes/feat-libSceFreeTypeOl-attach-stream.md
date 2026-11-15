# libSceFreeTypeOl

## change [`FT_Attach_Stream`, `FT_CMap_New`, and `FT_CMap_Done`]

- [`FT_CMap_New` and `FT_CMap_Done`](../../../core/libs/prx/libSceFreeTypeOl/Export.cpp) (`wLqQQWyOdpc`, `h0n4Fb9j2R8`): direct native calls bypass the existing guest face/charmap layout, cmap class callback ABI, and lifecycle ownership. These require a dedicated guest bridge before implementation.

## add Signature and layout provenance

- [FT_Attach_Stream](../../../core/libs/prx/libSceFreeTypeOl/AttachStream.cpp) (libSceFreeTypeOl) - the console prototype and FreeType version are unverified; `ZqvTX8u1bEc` is the NID of `FT_Attach_Stream` and the arguments follow bundled FreeType 2.15.0 [FT_Attach_Stream](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftobjs.c#L2903-L2942), identical in 2.13.3. More than one source flag throws: 2.13.3 and later return Invalid_Argument, while [2.4.12](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftobjs.c#L137-L202) picks memory, then path, then stream. A null path and a path that is not a regular file throw. A bridged guest stream's `memory` field is not set to the library's memory record as FreeType does. Only the face flags, bounding box, ascender and descender are copied back to the guest face, the fields the bundled Type 1 attach hook changes.
