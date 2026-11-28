# libSceAudio3d

## add Functional

- [sceAudio3dReportRegisterHandler](../../../core/libs/prx/libSceAudio3d/Export.cpp) (libSceAudio3d) - reports the same pointer-sized slot handle and measured full-list sentinel `0xFFFFFFFF` for valid outputs, using `GuestCopyOut_nid_no_patch` while preserving caller `errno`. An unwritable non-null handle throws `std::runtime_error`, with a newly allocated slot rolled back before the report lock is released, instead of crashing the host or retaining a handler the caller cannot unregister. The console behavior for an inaccessible guest handle has not been measured; this is a host-safety fallback, not a claimed console error ABI. A null optional output retains the original registration behavior.
