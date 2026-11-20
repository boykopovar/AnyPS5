# libSceAppContent

## change [The entitlement-label and mount-point exports of libSceAppContent]

- [The entitlement-label and mount-point exports of libSceAppContent](../../../core/libs/prx/libSceAppContent/Export.cpp) - returning `SCE_APP_CONTENT_ERROR_PARAMETER` for an unreadable parameter, entitlement label or mount point, or an unwritable output, via `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch` / `CopyGuestString_nid_no_patch`, is a host-safety fallback preserving host `errno`; console behavior on unmapped guest memory is unmeasured.
