# libSceFreeTypeOt

## add Functional

- [FT_Select_Metrics, FT_Request_Metrics and FT_Match_Size guest records](../../../core/libs/prx/libSceFreeTypeOt/src/SizeMetrics.cpp) (libSceFreeTypeOt) - the face record, its size record and the size request are read once per call through GuestCopyIn_nid_no_patch, the strike table in page-bounded chunks before any host table is allocated, and the size metrics are written back once through GuestCopyOut_nid_no_patch; throwing std::invalid_argument from FT_Select_Metrics and returning Invalid_Argument from FT_Request_Metrics and FT_Match_Size on unreadable or unwritable guest memory, with errno preserved, is host safety fallback with console behavior unmeasured.
