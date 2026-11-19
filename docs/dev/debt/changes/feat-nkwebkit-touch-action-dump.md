# libSceNKWebKit

## remove `_ZN7WebCorelsERN3WTF10TextStreamENS_11TouchActionE`

## add Functional

- [WebCore touch action dump](../../../core/libs/prx/libSceNKWebKit/src/TouchAction.cpp) (libSceNKWebKit) - not compared with a console module. `operator<<(TextStream&, TouchAction)` writes the CSS keyword of each of the six one-bit values of the 8-bit enum in the pinned [TouchAction.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/TouchAction.h#L32-L39), as the pinned [EventRegion.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/rendering/EventRegion.cpp#L623-L641) does. Any other value, such as a combination of bits, writes nothing, as a release build of that switch does; the console's behaviour for those values is unmeasured. Closes when: compared with a console module.
