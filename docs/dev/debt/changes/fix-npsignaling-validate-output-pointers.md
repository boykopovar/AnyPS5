# libSceNpSignaling

## add Functional

- [sceNpSignalingCreateContext](../../../core/libs/prx/libSceNpSignaling/Export.cpp) (libSceNpSignaling) - destination context id pointer is validated with GuestWritable_nid_no_patch upfront before allocating context id or mutating context registry; rejecting an unmapped pointer is host safety fallback with console behavior unmeasured.
- [sceNpSignalingGetConnectionStatus](../../../core/libs/prx/libSceNpSignaling/Export.cpp) (libSceNpSignaling) - destination status pointer is validated with GuestWritable_nid_no_patch upfront before context lookup; rejecting an unmapped pointer is host safety fallback with console behavior unmeasured.
