# libSceFreeTypeOl

## add Functional

- [sceFreeTypeOlAlloc_nid_postfix](../../../core/libs/prx/libSceFreeTypeOl/ModuleWrappers.cpp) (libSceFreeTypeOl) - returning 6, freeing the allocated block via `memory->free` if copying out fails, and preserving caller `errno` when `output` is unwritable via `GuestCopyOut_nid_no_patch` is a host-safety fallback with console behavior unmeasured. Closes when: measured on hardware. Labels: console, tested.
