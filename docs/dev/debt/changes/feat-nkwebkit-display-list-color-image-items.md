# libSceNKWebKit

## add Functional

- [WebCore display list color and image items](../../../core/libs/prx/libSceNKWebKit/src/DisplayListColorImageItems.cpp) (libSceNKWebKit) - not compared with a console module; `apply` throws `NotImplemented_nid_no_patch` because `GraphicsContext` is not implemented. The 3.20 constructors and destructors of the color fill, rounded rect, focus ring and image items follow [DisplayListItems.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/displaylists/DisplayListItems.cpp#L592-L1097) at `c55364d53f97`. Colors use that revision's encoding (a pointer to a reference-counted `ExtendedColor` when the two low bits are clear), not the 11.x one of the `WebCore::Color` exports. The last release of a color or image frees it. Null arguments throw. Closes when: compared with a console module.
