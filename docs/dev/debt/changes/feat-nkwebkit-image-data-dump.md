# libSceNKWebKit

## remove `_ZN7WebCorelsERN3WTF10TextStreamERKNS_9ImageDataE`

## add Functional

- [WebCore ImageData dump](../../../core/libs/prx/libSceNKWebKit/src/ImageDataDump.cpp) (libSceNKWebKit) - not compared with a console module. `operator<<(TextStream&, const ImageData&)` writes the address of the `Uint8ClampedArray` pixel data through the `TextStream` pointer writer, as the pinned [ImageData.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/html/ImageData.cpp#L168-L172) does. The `Ref` is read at offset 16, after the release-build `RefCounted` count and the `IntSize`. Closes when: compared with a console module.
