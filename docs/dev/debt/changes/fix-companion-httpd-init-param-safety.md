# libSceCompanionHttpd

## add Functional

- [sceCompanionHttpdInitialize](../../../core/libs/prx/libSceCompanionHttpd/Export.cpp) (libSceCompanionHttpd) - optional param pointer is checked with GuestCopyIn_nid_no_patch for host memory safety with caller errno preserved; rejecting an unmapped parameter pointer is host safety fallback with console behavior unmeasured.
