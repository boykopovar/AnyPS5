# libSceNKWebKit

## add Unknown function info

- [WebCore::DisplayList::DrawGlyphs](../../../core/libs/prx/libSceNKWebKit/src/DisplayListDrawGlyphs.cpp) (libSceNKWebKit) - console ABI and behaviour are unmeasured. The [pinned item](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WebCore/platform/graphics/displaylists/DisplayListItems.h#L1046) and its constructors give the `static_assert`ed 1400-byte layout, including the 128-element inline buffers of both vectors. The constructor moves them: a heap buffer changes owner and the source keeps its inline buffer at size 0, while an inline payload is copied into the item. `m_bounds` stays empty because `computeBounds()` needs `Font::fontMetrics`, so `localBounds()` returns the block location with zero size. The font is not `Ref`-counted, because its console ref count offset is unmeasured. Null arguments and vectors outside the size/capacity/inline invariant throw. Closes when: measured on console.
