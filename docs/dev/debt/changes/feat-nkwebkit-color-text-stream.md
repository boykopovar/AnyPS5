# libSceNKWebKit

## add Functional

- [WebCore::operator<<(TextStream&, const Color&)](../../../core/libs/prx/libSceNKWebKit/src/ColorTextStream.cpp) (libSceNKWebKit) - not compared with a console module. It follows the pinned [Color.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/Color.cpp#L224-L227) and [ColorSerialization.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/ColorSerialization.cpp#L176-L181): an inline color is decoded from the 8-byte `m_colorAndFlags` (RGBA in the low 32 bits, flags at bit 48, as `WebCoreColor.cpp` decodes it; unverified) and written as `#RRGGBB` or `#RRGGBBAA` through the `SRGBA<uint8_t>` render-tree serialization and `TextStream::operator<<(const String&)`. Out-of-line colors and colors flagged `UseColorFunctionSerialization` throw through `NotImplemented_nid_no_patch`. Closes when: compared with a console module.

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_5ColorE` | `MkKcrZ9pT0Q` |
