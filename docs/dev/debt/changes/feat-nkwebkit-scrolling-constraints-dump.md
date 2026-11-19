# libSceNKWebKit

## remove `_ZN7WebCorelsERN3WTF10TextStreamERKNS_27AbsolutePositionConstraintsE`

## remove `_ZN7WebCorelsERN3WTF10TextStreamERKNS_32FixedPositionViewportConstraintsE`

## remove `_ZN7WebCorelsERN3WTF10TextStreamERKNS_33StickyPositionViewportConstraintsE`

## add Functional

- [WebCore scrolling constraints dumps](../../../core/libs/prx/libSceNKWebKit/src/ScrollingConstraintsDump.cpp) (libSceNKWebKit) - not compared with a console module. The `TextStream` dumps of `AbsolutePositionConstraints`, `FixedPositionViewportConstraints` and `StickyPositionViewportConstraints` write one property group per field through the existing `FloatPoint`, `FloatRect` and `FloatSize` dumps, in the order of the pinned [ScrollingConstraints.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/scrolling/ScrollingConstraints.cpp#L119-L144). Closes when: compared with a console module.
