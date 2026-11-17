# libSceNpCppWebApi

## add Unknown function info

- [sce::Np::CppWebApi::Leaderboards::V1 mode and temporary rank fields](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - 6 model accessors read fields the PPSA21564 and PPSA01341 modules measure: `RecordScoreResponseBody::{getTmpRank,getTmpSerialRank}` at +0x30 and +0x34, 32-bit each, and `GetBoardDefinitionResponseBody::{getSortMode,sortModeIsSet,getUpdateMode,updateModeIsSet}` at +0x6c (32-bit), +0x70 (byte), +0x74 (32-bit) and +0x78 (byte). Those six bodies are byte-identical in both modules. Model construction, the fields not listed here and any console value are unverified.

## change [sce::Np::CppWebApi::Activities::V1]

- [sce::Np::CppWebApi::Activities::V1](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - `ParameterToGetUserActivities::initialize` ignores the account-id vector, `setlimit` and `setnpServiceLabel` store their value in a host block that nothing reads, and `getUserActivities` returns `SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN`. `UserActivities::getActivities` returns the handle at +0x30, the offset both measurable builds use; `GetUsersActivitiesResponse::getUsers` copies the `IntrusivePtr` at +0x30 with its reference count, the PPSA05347 build keeping the handle at +0x10. The 0x10-byte handle and the lifecycle codes come from the module bundled with PPSA21564/PPSA23732 (builds `J03093737`/`J03341606`, addresses `0x3e7a10`, `0x3e7a70`, `0x3e7cb0`, `0x3e7e50`, `0x3e7ec0`, `0x3e7f20`); the module's setters write no return value, so they stay `void`. Nothing here is measured on a console. PPSA22408 (Hot Wheels Infinite Rush) imports the 15 NIDs
