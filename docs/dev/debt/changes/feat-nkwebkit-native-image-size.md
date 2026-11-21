# libSceNKWebKit

## add Unknown function info

- [Native image size](../../../core/libs/prx/libSceNKWebKit/src/NativeImageSize.cpp) (libSceNKWebKit) - Console ABI and behavior are unmeasured. The eight-byte RefPtr and IntSize follow [legacy NativeImageCairo.cpp](https://github.com/WebKit/WebKit/blob/092eb690631dcb874885ac9eb5ac2e395c3ffc83/Source/WebCore/platform/graphics/cairo/NativeImageCairo.cpp) and the image branch of [PS5 CairoUtilities.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/cairo/CairoUtilities.cpp). Non-image surfaces throw. Unreadable RefPtr storage throws while preserving errno; non-null surfaces must be live Cairo backend handles. Fabricated and stale handles are not validated. Closes when: measured on console.
