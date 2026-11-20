# libSceNKWebKit

## add Functional

- [WebCore display list state items](../../../core/libs/prx/libSceNKWebKit/src/DisplayListStateItems.cpp) (libSceNKWebKit) - not compared with a console module; `apply` throws `NotImplemented_nid_no_patch` because `GraphicsContext` is not implemented. The 3.20 constructors and destructors of `Item`, `DrawingItem` and the save, restore, transform, line style, shadow, scale factor and clip items follow [DisplayListItems.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/displaylists/DisplayListItems.cpp#L53-L460) at `c55364d53f97`, the first revision whose `PutImageData` matches the exports. Layouts and vtable order were measured on that source. A `DashArray` copy keeps the source capacity, as `Vector` did then. Null arguments throw. Closes when: compared with a console module.
