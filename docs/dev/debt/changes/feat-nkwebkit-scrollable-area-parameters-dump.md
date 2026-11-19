# libSceNKWebKit

## remove `_ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE`

## add Functional

- [WebCore ScrollableAreaParameters dump](../../../core/libs/prx/libSceNKWebKit/src/ScrollableAreaParametersDump.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. The by-value `TextStream` dump follows [ScrollingCoordinator.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/scrolling/ScrollingCoordinator.cpp#L417-L435) of August 2020, whose nine-byte layout and `uint8_t` enums printed as integers match the module exporting only that revision's three `ScrollTypes` dumps; the 11.x source adds overscroll and visibility fields and named enum dumps. True flags are written as `1` and false ones omitted. Closes when: compared with a console module.
