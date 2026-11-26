# libSceNpManager

## add Functional

- [_ZN3sce2np10NpOnlineId16SetNpOnlineIdStrEPKc, _ZN3sce2np4User5ClearEv, and _ZNK3sce2np12NpTitleToken6GetStrEv](../../../core/libs/prx/libSceNpManager/ValueClasses.cpp) (libSceNpManager) - probing and copying `self` via `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch` while preserving `errno` (returning `NP_ERROR_INVALID_ARGUMENT` in `SetNpOnlineIdStr` and faulting via `NotImplementedAt_nid_no_patch` in `User::Clear` and `NpTitleToken::GetStr`) is a host-safety fallback; console behaviour on invalid `self` pointers is unmeasured.
