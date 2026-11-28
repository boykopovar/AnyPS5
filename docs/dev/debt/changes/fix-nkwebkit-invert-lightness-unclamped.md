# libSceNKWebKit

## add Functional

- [InvertLightnessFilterOperation colour transforms](../../../core/libs/prx/libSceNKWebKit/src/FilterOperation.cpp) (libSceNKWebKit) - not compared with a console module. `transformColor(SRGBA<float>&)` and `inverseTransformColor(SRGBA<float>&)` are in the 3.20 inventory only and follow [FilterOperation.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/filters/FilterOperation.cpp#L227-L266), whose two bodies are the same on 2020-06-13 and 2020-11-12: a half-turn of the hue through this library's `toHSLA` and `toSRGBA`, and the 5 by 3 matrix. Nothing is clamped and a NaN component is not replaced, unlike the later revision, so a result outside 0 to 1 is returned as computed. Whether the console contracts the matrix sums into fused multiply-adds is unmeasured. Closes when: compared with a console module.
