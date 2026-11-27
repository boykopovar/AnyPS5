# libSceUserService

## change [sceUserServiceGetNpAccountId]

- [sceUserServiceGetNpAccountId](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the account id for a logged-in user with no linked PSN account is assumed to be 0 with success; libSceNpManager's `sceNpGetAccountIdA` answers the same question with `SCE_NP_ERROR_SIGNED_OUT`. Console measurement on PS4 Pro firmware 10.71 confirms a user that is not logged in returns `0x80960105` (`USER_SERVICE_ERROR_UNKNOWN_USER`) and leaves the output untouched, differing from `sceUserServiceGetUserName`. Firmware 13.42's `libSceUserService.sprx` returns `0x80960002` while it is not initialized and `0x80960005` for a null output before it looks at the user, both modelled here ([disassembly and a native run with stubbed imports](https://github.com/boykopovar/AnyPS5/pull/12165#issuecomment-6512375278), not measured on a console).
