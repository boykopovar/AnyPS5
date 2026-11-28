# libSceSystemService

## add Functional

- [sceShellCoreUtilIsBgmCpuBudgetAcquired, sceShellCoreUtilIsBgmCpuBudgetAvailable, sceSystemServiceIsBgmCpuBudgetAvailable](../../../core/libs/prx/libSceSystemService/PendingExports.cpp) (libSceSystemService) - both queries report false: AnyPS5 has no BGM core and therefore no BGM CPU budget, as it has no libSceMusicPlayerService for `sceSystemServiceIsBgmPlaying`; `sceSystemServiceIsBgmCpuBudgetAvailable` forwards to the ShellCore query. The console's answers while its BGM core runs, and the acquire and release results, are unmeasured; `sceShellCoreUtilAcquireBgmCpuBudget` and `sceShellCoreUtilReleaseBgmCpuBudget` still throw `NotImplemented_nid_no_patch`.
