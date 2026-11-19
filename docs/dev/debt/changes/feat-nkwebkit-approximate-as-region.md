# libSceNKWebKit

## add Functional

- [WebCore::approximateAsRegion](../../../core/libs/prx/libSceNKWebKit/src/RoundedRectRegion.cpp) (libSceNKWebKit) - not compared with a console module. `approximateAsRegion(const RoundedRect&, unsigned stepLength)` follows the pinned [RoundedRect.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/RoundedRect.cpp#L314-L382): the enclosing `IntRect`, then up to 20 corner rectangles per rounded corner subtracted through the existing `Region` exports, with `LayoutUnit` saturation and truncation reproduced. The sample points use the host `cos` and `sin`, which can differ from the console's libm in the last bit. A step length of 0 on a non-empty rounded rectangle divides by zero upstream; it throws here. Closes when: compared with a console module.
