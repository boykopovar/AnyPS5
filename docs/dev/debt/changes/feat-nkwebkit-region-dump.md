# libSceNKWebKit

## remove `_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE`

## add Functional

- [WebCore region dump](../../../core/libs/prx/libSceNKWebKit/src/RegionDump.cpp) (libSceNKWebKit) - not compared with a console module. `operator<<(TextStream&, const Region&)` writes a newline, then, one indent level deeper, a `(rect <IntRect>)` line for each rectangle of `Region::rects()`, as the pinned [Region.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/Region.cpp#L670-L680) does, reusing the existing `IntRect` dump and `TextStream::writeIndent` exports. The indent is restored before returning. Closes when: compared with a console module.
