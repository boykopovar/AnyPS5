# libSceNpAuth

## change [sceNpAuthGetAuthorizedAppCode]

- [sceNpAuthGetAuthorizedAppCode](../../../core/libs/prx/libSceNpAuth/Export.cpp): returning `ORBIS_NP_AUTH_ERROR_INVALID_ARGUMENT` (0x80550301) for unmapped or unreadable parameter pointers is a defensive host-safety fallback (console unmeasured). The five-argument ABI and 40-byte request parameter are recovered. Synchronous requests return `SCE_NP_ERROR_SIGNED_OUT` while asynchronous requests complete successfully and report `SCE_NP_ERROR_SIGNED_OUT` through `sceNpAuthPollAsync` and `sceNpAuthWaitAsync`, matching the request lifecycle of `sceNpAuthGetAuthorizationCodeV3` and `sceNpAuthGetIdTokenV3`. Prior processed or aborted requests return `ORBIS_NP_AUTH_ERROR_INVALID_ARGUMENT` and `ORBIS_NP_AUTH_ERROR_ABORTED`. Client-ID storage is copied as 136 bytes by the firmware; its internal fields and the complete output-code format remain unresolved.
