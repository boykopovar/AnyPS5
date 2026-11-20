# libSceNpAuth

## add Functional

- [sceNpAuthGetAuthorizationCode, sceNpAuthGetAuthorizationCodeA, sceNpAuthGetAuthorizationCodeV3, sceNpAuthGetIdToken, sceNpAuthGetIdTokenA, sceNpAuthGetIdTokenV3, sceNpAuthCreateAsyncRequest](../../../core/libs/prx/libSceNpAuth/Export.cpp) (libSceNpAuth) - returning `ORBIS_NP_AUTH_ERROR_INVALID_ARGUMENT` (0x80550301) for unmapped parameter or output pointers, or an unmapped `issuer_id` when non-null, is a defensive host-safety fallback; console behavior with invalid pointers is unmeasured.
