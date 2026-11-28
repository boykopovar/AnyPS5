# libSceNetCtl

## add Functional

- [sceNetCtlGetState](../../../core/libs/prx/libSceNetCtl/Export.cpp) (libSceNetCtl) - validates destination state pointer upfront with `GuestArena::HostWrite` paired with `GuestWritable_nid_no_patch` before querying network interfaces while preserving caller `errno`; rejecting an unmapped pointer is a host-safety fallback with console behavior unmeasured. Closes when: measured on hardware. Labels: console, tested.
- [sceNetCtlGetStateV6](../../../core/libs/prx/libSceNetCtl/Export.cpp) (libSceNetCtl) - validates destination state pointer upfront with `GuestArena::HostWrite` paired with `GuestWritable_nid_no_patch` before querying IPv6 interfaces while preserving caller `errno`; rejecting an unmapped pointer is a host-safety fallback with console behavior unmeasured. Closes when: measured on hardware. Labels: console, tested.
