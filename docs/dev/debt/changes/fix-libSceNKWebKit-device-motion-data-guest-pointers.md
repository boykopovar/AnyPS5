# libSceNKWebKit

## add Functional

- [WebCore::DeviceOrientationData and DeviceMotionData guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/DeviceMotionData.cpp) - console behaviour on invalid, unmapped, or read-only guest pointers passed to `DeviceOrientationData::create` and `DeviceMotionData::create` is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null pointers) without mutating guest `RefPtr` slots or leaking allocations. Closes when: console behaviour on invalid guest pointers in these exports is measured.
