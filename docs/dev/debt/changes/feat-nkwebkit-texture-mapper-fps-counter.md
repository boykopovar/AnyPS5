# libSceNKWebKit

## add Functional

- [WebCore::TextureMapperFPSCounter constructors](../../../core/libs/prx/libSceNKWebKit/src/TextureMapperFPSCounter.cpp) (libSceNKWebKit) - not compared with a console module. Follow [TextureMapperFPSCounter.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/texmap/TextureMapperFPSCounter.cpp): guest `WEBKIT_SHOW_FPS` is read as Latin-1 and parsed with `String::toDouble`, and a nonzero result turns the counter on and stamps `MonotonicTime::now`. The 32-byte layout comes from TextureMapperFPSCounter.h and is not measured. A null object throws.

## remove `_ZN7WebCore23TextureMapperFPSCounterC1Ev`

## remove `_ZN7WebCore23TextureMapperFPSCounterC2Ev`
