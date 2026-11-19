# libSceNKWebKit

## remove `_ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE`

## add Functional

- [WebCore FloatRoundedRect dump](../../../core/libs/prx/libSceNKWebKit/src/FloatRoundedRectDump.cpp) (libSceNKWebKit) - not compared with a console module. `operator<<(TextStream&, const FloatRoundedRect&)` writes the rectangle through the existing `FloatRect` dump, then one `top-left`, `top-right`, `bottom-left` and `bottom-right` property group per corner radius through the `FloatSize` dump, as the pinned [FloatRoundedRect.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/FloatRoundedRect.cpp#L210-L218) does. Closes when: compared with a console module.
