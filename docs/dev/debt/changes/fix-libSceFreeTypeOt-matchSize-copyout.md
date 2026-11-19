# libSceFreeTypeOt

## add Functional

- [FT_Match_Size_nid_postfix size index output](../../../core/libs/prx/libSceFreeTypeOt/src/SizeMetrics.cpp) (libSceFreeTypeOt) - a successful match writes the size index once through GuestCopyOut_nid_no_patch; errors and failed matches leave it untouched. Returning Invalid_Argument when the matched output is unwritable is a host safety fallback with console behaviour unmeasured.
