# libSceUserService

## change [sceUserServiceIsKratosUser, sceUserServiceIsKratosPrimaryUser and sceUserServiceGetKratosPrimaryUser]

- [sceUserServiceIsKratosUser, sceUserServiceIsKratosPrimaryUser and sceUserServiceGetKratosPrimaryUser](../../../core/libs/prx/libSceUserService/Export.cpp) - `sceUserServiceIsKratosUser` throws because the account-state backend and Cronos behavior are unavailable. `sceUserServiceIsKratosPrimaryUser` returns false for every user ID, and `sceUserServiceGetKratosPrimaryUser` returns 0x80960006 without writing its output, as measured on a PS5 13.40 in a retail title for the initial user and -1. Whether a console in another state answers differently is unknown, and a null output to `sceUserServiceGetKratosPrimaryUser` throws because it was not measured.

# libSceSystemService

## add Functional

- [sceSystemServiceGetAppType, sceSystemServiceGetAppFocusedAppStatus and sceSystemServiceGetAppCategoryType](../../../core/libs/prx/libSceSystemService/PendingExports.cpp) (libSceSystemService) - app queries. Measured on a PS5 13.40 in a retail title, `sceSystemServiceGetAppFocusedAppStatus`, `sceSystemServiceGetAppIdOfRunningBigApp`, `sceSystemServiceGetAppId` for the title's own ID, and `sceSystemServiceGetAppType` and `sceSystemServiceGetAppCategoryType` for app IDs -1 and 0x8094000f return 0x8094000f without writing. Here the first and the last two for any app ID other than 1 return it, but the app ID lookups still return 1, as the running application ID entry says, so `sceSystemServiceGetAppType` for app ID 1 throws instead of guessing a type. A null output throws, except the unmeasured 0x80A10003 of `sceSystemServiceGetAppCategoryType`.
- [sceSystemServiceGetDbgExecutablePath](../../../core/libs/prx/libSceSystemService/PendingExports.cpp) (libSceSystemService) - returns 0x80A10005 without writing for every selector, as measured on a PS5 13.40 in a retail title for selectors 0 and -1. A development unit's answer is unknown, and a null path throws.
- [sceSystemServiceGetPlatformPrivacyDefinitionVersion and sceSystemServiceGetPlatformPrivacyDefinitionData](../../../core/libs/prx/libSceSystemService/PendingExports.cpp) (libSceSystemService) - an empty key returns 0x80A10001 without writing, as measured on a PS5 13.40 in a retail title. Any other key, a null or unreadable key and a null output throw, because the definitions and their layouts are unknown.
