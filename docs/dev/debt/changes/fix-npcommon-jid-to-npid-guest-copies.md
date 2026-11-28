# libSceNpCommon

## add Functional

- [sceNpUtilJidToNpId guest record](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - the `NpJid` is copied in once and its user, domain and resource are read through checked copies in page-bounded chunks; an unreadable record returns `NP_UTIL_ERROR_INVALID_ARGUMENT` and an unreadable string `NP_UTIL_ERROR_INVALID_JID`, the null results, with `errno` kept. A host-safety fallback, not measured on the console.
