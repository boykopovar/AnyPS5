# libSceUserService

## change [sceUserServiceGetUserName]

- [sceUserServiceGetUserName](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - `0x80960105`, which a PS5 (firmware 13.60) returns for a user id it does not know, has no known name; it is `USER_SERVICE_ERROR_UNKNOWN_USER` here. On the console a registered user who is not logged in still gets their name; here up to 4 users ("Player", "Player2", "Player3", "Player4") are supported; user names "Player2", "Player3" and "Player4" are assumed, not measured. Returning `USER_SERVICE_ERROR_INVALID_ARGUMENT` with caller errno preserved on an unwritable `name` via `GuestCopyOut_nid_no_patch` is a host-safety fallback; native error behavior for an unmapped pointer is unmeasured.
