# libSceNKWebKit

## change [WebCore color space conversions and serialization]

- [WebCore color space conversions and serialization](../../../core/libs/prx/libSceNKWebKit/src/WebCoreColorSpaces.cpp) (libSceNKWebKit) - not checked against a console module. Out-of-line colors and colors flagged `UseColorFunctionSerialization` throw in the `Color` overloads of `serializationForCSS` and `serializationForRenderTreeAsText`, which decode the 11.x packing `WebCoreColor.cpp` uses; `Color::colorSpaceAndComponents` throws. The rest follows WebKit c55364d53f97 [ColorConversion.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/ColorConversion.cpp) and [ColorSerialization.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/ColorSerialization.cpp). Matrix products are not fused into FMA, so a console build that fuses them may differ in the last bit.

## remove `_ZN7WebCore19serializationForCSSERKNS_5ColorE`

## remove `_ZN7WebCore32serializationForRenderTreeAsTextERKNS_5ColorE`
