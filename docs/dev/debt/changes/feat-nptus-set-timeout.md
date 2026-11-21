# libSceNpTus

## remove [sceNpTusSetTimeout]

## remove [sceNpTusChangeModeForOtherSaveDataOwners]

## add Unknown function info

- [sceNpTusSetTimeout and sceNpTusChangeModeForOtherSaveDataOwners](../../../core/libs/prx/libSceNpTus/Export.cpp) (libSceNpTus) - this module was not read: the arguments and checks are those firmware 13.42's `libSceNpScoreRanking.sprx` has for the `sceNpScore` calls of the same names ([SetTimeout](https://github.com/boykopovar/AnyPS5/pull/12611#issuecomment-6494084502), [ChangeMode](https://github.com/boykopovar/AnyPS5/pull/12611#issuecomment-6507773030)). `sceNpTusSetTimeout` refuses a negative retry count, then non-zero timeouts below 1 s (resolve) or 10 s (connect, receive, send) with `INVALID_ARGUMENT`, then an unknown title context or request with `INVALID_ID`; `sceNpTusChangeModeForOtherSaveDataOwners` refuses a mode above 3, then an unknown id. Neither keeps its values, as no request reaches a service. Closes when: compared with the TUS module.
