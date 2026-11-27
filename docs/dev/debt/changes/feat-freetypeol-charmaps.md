# libSceFreeTypeOl

## add Functional

- [`FT_CMap_New` and `FT_CMap_Done`](../../../core/libs/prx/libSceFreeTypeOl/Charmap.cpp) bridge guest callbacks and class-sized maps to the bundled [FreeType lifecycle](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftobjs.c#L3844-L3953). Class and SFNT metadata layouts follow [FreeType's class records](https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/sfnt/ttcmap.h). Console-specific layouts and behavior have not been measured. The system-dependency build lacks the private API. A foreign face, unsupported class size, or SFNT metadata exceeding native field widths throws. A native Unicode map passed to a variation callback provides guest-callable index and next methods. No retail-title call trace is available.
