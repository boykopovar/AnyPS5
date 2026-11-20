# libSceFiber

## add Functional

- [sceFiberGetInfo](../../../core/libs/prx/libSceFiber/Export.cpp) (libSceFiber) - probes the destination size field using `GuestCopyIn_nid_no_patch` before validating length and copying out context information; returning `SCE_FIBER_ERROR_INVALID` for an unmapped or unwritable pointer is a defensive host-protection fallback.
