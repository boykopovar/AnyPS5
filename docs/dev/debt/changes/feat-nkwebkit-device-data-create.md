# libSceNKWebKit

## remove `_ZN7WebCore16DeviceMotionData6createEv`

## add Functional

- [WebCore device motion and orientation data](../../../core/libs/prx/libSceNKWebKit/src/DeviceData.cpp) (libSceNKWebKit) - not compared with a console module. `DeviceMotionData::create` (both overloads) and the non-iOS `DeviceOrientationData::create(alpha, beta, gamma, absolute)` follow [DeviceMotionData.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/dom/DeviceMotionData.cpp) and [DeviceOrientationData.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/dom/DeviceOrientationData.cpp), whose `WTF::Optional` matches the export names. They `fastMalloc` the release-build `RefCounted` object, move the `RefPtr` arguments in, and take each bool-first `WTF::Optional` by hidden reference. Closes when: compared with a console module.
