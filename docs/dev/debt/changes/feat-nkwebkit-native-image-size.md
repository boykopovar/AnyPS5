# libSceNKWebKit

## add Unknown function info

- [WebCore::nativeImageSize](../../../core/libs/prx/libSceNKWebKit/src/NativeImageCairo.cpp) (libSceNKWebKit) - not compared with a console module. Follows pinned WebKit [NativeImageCairo.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/cairo/NativeImageCairo.cpp) and [CairoUtilities.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/cairo/CairoUtilities.cpp): null `RefPtr` and non-image Cairo surface types return `{0, 0}`, and `CAIRO_SURFACE_TYPE_IMAGE` surfaces query `libcairo` width and height. Null or unreadable `RefPtr` and `CairoSurface` pointers throw via `NotImplemented_nid_no_patch` instead of faulting. Closes when: compared with a console module.
