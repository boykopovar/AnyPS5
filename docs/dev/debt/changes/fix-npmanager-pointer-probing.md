# libSceNpManager

## add sce2np value classes

- [NpOnlineId::SetNpOnlineIdStr, User::Clear, and NpTitleToken::GetStr guest pointer probing](../../../core/libs/prx/libSceNpManager/ValueClasses.cpp) (libSceNpManager) - `NpOnlineId::SetNpOnlineIdStr` and `User::Clear` write guest object fields through `GuestCopyOut_nid_no_patch` and `NpTitleToken::GetStr` probes `data` through `GuestCopyIn_nid_no_patch`, preserving host `errno`. Returning `SCE_NP_ERROR_INVALID_ARGUMENT` (`0x80550003`) from `NpOnlineId::SetNpOnlineIdStr`, ignoring invalid `self` in `User::Clear`, and returning `nullptr` from `NpTitleToken::GetStr` on null or unmapped guest pointers is a host-safety fallback; console behaviour on invalid `self` pointers is unmeasured.
