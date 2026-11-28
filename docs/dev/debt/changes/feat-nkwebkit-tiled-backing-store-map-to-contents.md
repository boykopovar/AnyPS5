# libSceNKWebKit

## add Functional

- [WebCore::TiledBackingStore::mapToContents](../../../core/libs/prx/libSceNKWebKit/src/TiledBackingStore.cpp) (libSceNKWebKit) - not compared with a console module; only `m_contentsScale` is read, at offset 124 of the 136-byte `TiledBackingStore` laid out from the pinned [TiledBackingStore.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/texmap/coordinated/TiledBackingStore.h#L79-L98) with an 8-byte release `HashMap`, which is unverified. Follows [TiledBackingStore.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/texmap/coordinated/TiledBackingStore.cpp#L363-L369): each `IntRect` field is divided by the scale in `float` and the result goes through the existing `enclosingIntRect(FloatRect)`. A null store or rect throws `std::invalid_argument` instead of crashing. Closes when: compared with a console module.

## remove | `_ZNK7WebCore17TiledBackingStore13mapToContentsERKNS_7IntRectE` | `dEydpmJiJ+0` |
