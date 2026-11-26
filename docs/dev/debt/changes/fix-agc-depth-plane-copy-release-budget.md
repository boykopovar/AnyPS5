# libSceAgcDriver

## add Functional

- [Depth plane copy budget and release](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - the default per-device budget when `APS5_DEPTH_PLANE_COPY_MIB` is unset (a sixteenth of the device-local heap budget on discrete GPUs or a sixty-fourth on integrated/unified heaps, with a 256 MiB floor) is an unmeasured choice, and single-slice resident depth-surface views and the disabled texture cache are still not marked released in `SharedResourceCache`. Evicted or replaced `DepthPlaneCopy` views are marked released (`NoteReleasedTexture`) and swept from `SharedResourceCache` immediately (keeping displaced sets on any active recorder) instead of waiting for the next `ResourceCache::Insert`.
