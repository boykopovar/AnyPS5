# libSceNKWebKit

## add Functional

- [WebCore::Cursor(Image*, const IntPoint&) constructors and setAsPlatformCursor](../../../core/libs/prx/libSceNKWebKit/src/Cursor.cpp) (libSceNKWebKit) - not compared with a console module. Reuses the `NKWebKit::WebCoreAbi::Cursor` model from `CursorAbi.hpp` and adds to it `OptionalIntPoint` (the guest `std::optional<IntPoint>` return, flag-first) and `ImageOrientation`. The guest `Image` is not modelled: the constructor refs it (`m_refCount` at offset 8) and computes the hot spot as [determineHotSpot](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/Cursor.cpp#L37-L54) does, calling its vtable slots 19 (`size`) and 22 (`hotSpot`). `setAsPlatformCursor` is a no-op, as upstream `Cursor.cpp` on `!HAVE(NSCURSOR)`. A null image, this, hot spot, vtable or slot throws where the console faults.

## remove | `_ZN7WebCore6CursorC1EPNS_5ImageERKNS_8IntPointE` | `m38rIXXKp48` |

## remove | `_ZN7WebCore6CursorC2EPNS_5ImageERKNS_8IntPointE` | `JAEgKfRXTnY` |

## remove | `_ZNK7WebCore6Cursor19setAsPlatformCursorEv` | `wewK46-v8RA` |
