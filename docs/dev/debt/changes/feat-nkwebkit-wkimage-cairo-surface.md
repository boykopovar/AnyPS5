# libSceNKWebKit

## change [WKImage size handling]

- [WKImage and Cairo surface backing](../../../core/libs/prx/libSceNKWebKit/src/Image.cpp) (libSceNKWebKit) - console module behavior is unmeasured, `options` (`kWKImageOptionsShareable` shared-memory backing) is not distinguished from local `ImageBuffer` backing, and null or non-image inputs that upstream dereferences throw `std::invalid_argument`. `WKImageCreate`, `WKImageGetSize`, `WKImageCreateCairoSurface` and `WKImageCreateFromCairoSurface` otherwise follow [WKImageCairo.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/Shared/API/c/cairo/WKImageCairo.cpp), `WKImage.cpp` and `ImageBufferCairoImageSurfaceBackend.cpp` at that revision over `CAIRO_FORMAT_ARGB32` `libcairo` surfaces, returning null from `WKImageCreate` for non-positive dimensions or sizes above Cairo's 32767-pixel limit. Closes when: compared with a console module.

## remove | `WKImageCreateCairoSurface` | `1QfyjHj40es` |

## remove | `WKImageCreateFromCairoSurface` | `jJePHioTWw0` |
