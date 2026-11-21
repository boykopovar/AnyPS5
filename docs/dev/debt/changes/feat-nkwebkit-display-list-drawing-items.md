# libSceNKWebKit

## add Functional

- [WebCore display list drawing items](../../../core/libs/prx/libSceNKWebKit/src/DisplayListDrawingItems.cpp) (libSceNKWebKit) - not compared with a console module; `apply` and `StrokeEllipse::localBounds` throw `NotImplemented_nid_no_patch` because `GraphicsContext` is not implemented. The 3.20 constructors and destructors of the transparency layer, rect, line, text line, marker, ellipse, fill, stroke and clear items and their `localBounds` follow [DisplayListItems.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/displaylists/DisplayListItems.cpp#L743-L1325) at `c55364d53f97`, keeping its float operation order. Null arguments throw. Closes when: compared with a console module.
