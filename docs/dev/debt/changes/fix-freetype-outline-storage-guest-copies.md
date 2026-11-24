# libSceFreeType

## change [FT_Outline_New]

- [FT_Outline_New](../../../core/libs/prx/libSceFreeType/src/OutlineStorage.cpp) (libSceFreeType) - console signature, errors and allocator are unverified; the adapter follows bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftoutln.c) with the guest LP64 outline layout. The arrays are zero-initialised from the host C heap, not from a guest or FreeType allocator, because the guest points are 64 bits wide. The outline is written through checked copies; an unwritable one returns `FT_Err_Invalid_Argument`, as a null one does

## change [FT_Outline_Done]

- [FT_Outline_Done](../../../core/libs/prx/libSceFreeType/src/OutlineStorage.cpp) (libSceFreeType) - console signature, errors and allocator are unverified; the adapter follows bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftoutln.c) with the guest LP64 outline layout. The point, tag and contour arrays of an outline with the owner flag are released with the host C heap, so a title that fills an owning outline with arrays from another allocator is not supported. The outline is read and cleared through checked copies; an unreadable or unwritable one returns `FT_Err_Invalid_Outline`, as a null one does, before anything is released
