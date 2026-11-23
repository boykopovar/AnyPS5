# libSceNKWebKit

## add Functional

- [HitTestLocation(const FloatPoint&) (3.20)](../../../core/libs/prx/libSceNKWebKit/src/HitTestLocation.cpp) (libSceNKWebKit) - not compared with a console module. Only in the 3.20 inventory; it follows the 2020 [HitTestLocation.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/rendering/HitTestLocation.cpp): the point floored to a layout unit, a one-pixel bounding box at its floored integer position, the transformed point taken from the floored point and the transformed quad from the bounding box. The object has the layout of the pinned header (68 bytes), derived, not measured. A NaN coordinate becomes the smallest layout unit, as the conversion gives on x86-64, and null arguments throw `std::invalid_argument`. Closes when: compared with a console module.
