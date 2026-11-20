# libSceNKWebKit

## add Functional

- [enclosingIntRect, enclosingLayoutRect and encloseRectToDevicePixels](../../../core/libs/prx/libSceNKWebKit/src/WebCoreEnclosingRect.cpp) (libSceNKWebKit) - not compared with a console module. The four free functions follow the pinned [FloatRect.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/platform/graphics/FloatRect.cpp#L234-L246) and [LayoutRect.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/platform/graphics/LayoutRect.cpp#L178-L193) on the 1/64 `LayoutUnit` layout, with `clampToInteger` and saturated `LayoutUnit` sums and differences. Closes when: compared with a console module.

## remove `_ZN7WebCore16enclosingIntRectERKNS_10LayoutRectE`

## remove `_ZN7WebCore16enclosingIntRectERKNS_9FloatRectE`

## remove `_ZN7WebCore19enclosingLayoutRectERKNS_9FloatRectE`

## remove `_ZN7WebCore25encloseRectToDevicePixelsERKNS_9FloatRectEf`
