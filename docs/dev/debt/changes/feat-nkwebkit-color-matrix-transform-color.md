# libSceNKWebKit

## add Functional

- [BasicColorMatrixFilterOperation::transformColor](../../../core/libs/prx/libSceNKWebKit/src/ColorMatrixFilter.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. It follows [FilterOperation.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/filters/FilterOperation.cpp#L90-L115) and the [ColorMatrix.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/ColorMatrix.h#L57-L126) matrices of August 2020: grayscale and sepia clamp the amount to 0..1, saturate and hue-rotate do not, alpha is kept, other types return false. It reads the same layout as BasicComponentTransferFilterOperation. Closes when: compared with a console module.
