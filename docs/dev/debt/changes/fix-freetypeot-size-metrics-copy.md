# libSceFreeTypeOt

## add Functional

- [FT_Select_Metrics, FT_Request_Metrics and FT_Match_Size guest records](../../../core/libs/prx/libSceFreeTypeOt/src/SizeMetrics.cpp) (libSceFreeTypeOt) - `FT_Select_Metrics` and `FT_Request_Metrics` copy the face, size and request records and the complete strike table through the guest-copy helpers. `FT_Match_Size` reads the face and request once and each strike only as the ordered search reaches it; returning Invalid_Argument for unreadable records is a host safety fallback with console behavior unmeasured.
