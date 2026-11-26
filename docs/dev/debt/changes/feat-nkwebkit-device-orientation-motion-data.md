# libSceNKWebKit

## add Functional

- [DeviceOrientationData::create and DeviceMotionData::create](../../../core/libs/prx/libSceNKWebKit/src/DeviceMotionData.cpp) (libSceNKWebKit) - the overloads with arguments, not compared with a console module. Only in the 3.20 inventory; they follow the 2020 [DeviceOrientationData.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/dom/DeviceOrientationData.cpp) and [DeviceMotionData.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/dom/DeviceMotionData.cpp). The objects are laid out for the caller's inline accessors (reference count at 0, then the members in order; `WTF::Optional` is a flag followed by the value) and the `Optional` arguments are taken by address; both are derived from the pinned headers, not measured. A disengaged member has a zero value where the source leaves it unset. Closes when: compared with a console module.
