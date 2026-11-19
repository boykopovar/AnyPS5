# libSceNKWebKit

## remove `_ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE`

## add Functional

- [WebCore Color text dump](../../../core/libs/prx/libSceNKWebKit/src/ColorDump.cpp) (libSceNKWebKit) - not compared with a console module. `operator<<(TextStream&, const Color&)` writes `serializationForRenderTreeAsText(color)` to the stream, as the pinned [Color.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/Color.cpp#L224-L227) does (WebKit [73a35b43](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/Color.cpp#L163-L166) has the same body). It inherits that export's gaps: out-of-line colors and colors flagged `UseColorFunctionSerialization` throw before anything is written. Closes when: compared with a console module.
