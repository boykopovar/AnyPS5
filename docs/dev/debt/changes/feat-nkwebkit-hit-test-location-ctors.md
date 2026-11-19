# libSceNKWebKit

## remove `_ZN7WebCore15HitTestLocationC1ERKS0_`

## remove `_ZN7WebCore15HitTestLocationC1Ev`

## remove `_ZN7WebCore15HitTestLocationC2ERKS0_`

## remove `_ZN7WebCore15HitTestLocationC2Ev`

## add Functional

- [WebCore::HitTestLocation default and copy constructors](../../../core/libs/prx/libSceNKWebKit/src/WebCoreHitTestLocation.cpp) (libSceNKWebKit) - not compared with a console module. They follow WebKit 73a35b43 [HitTestLocation.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/rendering/HitTestLocation.h), whose `IntRect` bounding box and `HitTestLocation(const FloatPoint&)` match the export names; the pinned 11.x source uses a `LayoutRect` box instead. The 68-byte layout is `LayoutPoint`, `IntRect`, `FloatPoint`, `FloatQuad`, then `m_isRectBased` (false) and `m_isRectilinear` (true). A null object or source throws. Closes when: compared with a console module.
