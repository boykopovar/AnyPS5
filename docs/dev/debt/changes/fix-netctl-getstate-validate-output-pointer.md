# libSceNetCtl

## add Functional

- [sceNetCtlGetState](../../../core/libs/prx/libSceNetCtl/Export.cpp) (libSceNetCtl) - destination state pointer is validated with GuestWritable_nid_no_patch upfront before querying network interfaces with caller errno preserved; rejecting an unmapped pointer is host safety fallback with console behavior unmeasured.
- [sceNetCtlGetStateV6](../../../core/libs/prx/libSceNetCtl/Export.cpp) (libSceNetCtl) - destination state pointer is validated with GuestWritable_nid_no_patch upfront before querying IPv6 interfaces with caller errno preserved; rejecting an unmapped pointer is host safety fallback with console behavior unmeasured.
