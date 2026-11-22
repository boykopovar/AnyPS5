# libSceVrTracker2

## add Silent stubs

- [SceVrTracker2DepthCubeMapInit](../../../core/libs/prx/libSceVrTracker2/Export.cpp) - throwing for an unwritable output destination via `GuestCopyOut_nid_no_patch` is a host-safety fallback preserving host errno; console behavior for this pointer is unmeasured.
