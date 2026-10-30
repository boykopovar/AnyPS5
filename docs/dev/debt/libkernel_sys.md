# libkernel_sys technical debt

- [sceKernelGetCpuUsage, sceKernelGetThreadName, sceKernelLwfsAllocateBlock, sceKernelLwfsLseek, sceKernelLwfsSetAttribute, sceKernelLwfsWrite, sceKernelSetCompressionAttribute and sceKernelSetFsstParam](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - throw through `NotImplemented_nid_no_patch`; behaviour and the PS5 prototypes are unmeasured. The parameter lists come from a single source, the PS4 OpenOrbis `libkernel.h`, with no second reference: `off_t` and `ssize_t` are taken as 64-bit, the `Lwfs*` and `Set*` parameters are unnamed there (the names here are guesses), and the `Proc_Stats` of `sceKernelGetCpuUsage` is declared as `void*` because its layout is unknown.
### Silent stubs
- [fchmodat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - `AT_SYMLINK_NOFOLLOW` on a symbolic link (on Windows and Linux)

### Unknown function info

- [libkernel_sys additional exports](../../../core/libs/prx/libkernel_sys/AdditionalExports.cpp) - 328 callable exports have import-resolution declarations only. Parameter lists, return types and behavior remain unverified. Parameterless `int` declarations are placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`. Unresolved names use literal NID aliases; no runtime functionality or data objects are fabricated. Existing providers and exports supplied by open pull requests are excluded. Kernel variant imports already resolve through `libkernel`, then `libc`; existing providers remain unchanged. Six native data exports (`errno`, `__stack_chk_guard`, `_sigintr`, `__progname`, `environ`, and NID `AxgW94umKqU`) require separate storage, layout, TLS and ownership validation and are not added as callable aliases or fabricated globals.

| Name | NID |
| --- | --- |
| `__cfi_slowpath` | `Bq2a8HmmUoY` |
| `__cfi_slowpath_diag` | `-cbfK7bCKQU` |
| `__freeze` | `nSSPVGJLMjE` |
| `__sceKernelGetGPI` | `3JYhkiIHkJc` |
| `__sys_debug_init` | `zdaF5N-Xe2M` |
| `__sys_dl_get_info` | `YbAunrti+54` |
| `__sys_dl_get_info_2` | `lCp17SqY3Nc` |
| `__sys_dl_get_list` | `XujojypwYYc` |
| `__sys_dl_get_metadata` | `7OpNDDNMJyo` |
| `__sys_dynlib_get_info2` | `nTc+tFajGqQ` |
| `__sys_dynlib_get_info_for_libdbg` | `K7xiuldOPKw` |
| `__sys_dynlib_get_list2` | `P0jjY6bxakI` |
| `__sys_dynlib_get_list_for_libdbg` | `vpo3SbGFuEk` |
| `__sys_dynlib_load_prx` | `nZHk+lpqwVQ` |
| `__sys_dynlib_unload_prx` | `GQli4UAXTfQ` |
| `__sys_get_proc_type_info` | `nG-FYqFutUo` |
| `__sys_is_development_mode` | `O-hEvSnv2o4` |
| `__sys_kqueueex` | `2M+dFM8Wmq4` |
| `__sys_namedobj_create` | `3CNY4Z0Luc8` |
| `__sys_namedobj_delete` | `E7CmfLfeSuQ` |
| `__sys_nanosleep` | `msfSXXy-8H8` |
| `__sys_netabort` | `UTR6wAkajxk` |
| `__sys_netcontrol` | `zsTvhCDrOjQ` |
| `__sys_netgetiflist` | `X86Q0qQJ1m0` |
| `__sys_netgetsockinfo` | `Wx2+2t1mX1E` |
| `__sys_opmc_disable` | `iZFJYJJoZS8` |
| `__sys_opmc_enable` | `MGrj20+EK+U` |
| `__sys_opmc_get_ctr` | `h+4DJpAXs4I` |
| `__sys_opmc_get_hw` | `IglowNcOePQ` |
| `__sys_opmc_set_ctl` | `vS-RnoD1BSY` |
| `__sys_opmc_set_ctr` | `ztNqqNCl1Tw` |
| `__sys_opmc_set_hw` | `PIt9WYaoBqU` |
| `__sys_osem_close` | `raO+bB7q2cY` |
| `__sys_osem_open` | `kPGXeSQeoWc` |
| `__sys_randomized_path` | `n0ErFZ2hmKs` |
| `__sys_rdup` | `9Iem9Wk07xU` |
| `__sys_regmgr_call` | `7NwggrWJ5cA` |
| `__sys_resume_internal_hdd` | `NTNdYQluihk` |
| `__sys_resume_process` | `6wXisPohifE` |
| `__sys_set_uevt` | `ByogDrS-Xw8` |
| `__sys_socketclose` | `t+rTx5NW+qU` |
| `__sys_suspend_process` | `-rXkJO1hOk0` |
| `__sys_test_debug_rwmem` | `EeVthQocE3Y` |
| `__sys_workaround8849` | `jTPE1AS7uak` |
| `__ubsan_handle_cfi_check_fail` | `K4M0IhFb88Q` |
| `__ubsan_handle_cfi_check_fail_abort` | `sPU6b2LaPqg` |
| `_dup2` | `W8f1adVl+48` |
| `_execve` | `-3nj+K1elI0` |
| `_execvpe` | `ScfDTOqLOMw` |
| `_wait4` | `RFlsu7nfopM` |
| `adjtime` | `2t3V7tfWY5s` |
| `mmap2` | `tGtH2GTWbyE` |
| `pthread_setschedother_np` | `ymCrwbFD4fk` |
| `sceKernelAddDynlibEvent` | `dGFwUPT+BK8` |
| `sceKernelAddFGCInfo` | `M+vuTJQ3F8Y` |
| `sceKernelCallIndirectBuffer3` | `x+JkNr3eoxs` |
| `sceKernelCreateUuidV1` | `8PS6UDzj-xY` |
| `sceKernelCreateUuidV7` | `Nr+3bRBygw0` |
| `sceKernelDeleteDynlibEvent` | `0GquoxOD3Aw` |
| `sceKernelDeleteFGCInfo` | `ZGfk6FwQozw` |
| `sceKernelEnableDmemAliasing3` | `RbhD-epf6c0` |
| `sceKernelExtendMaximumModuleNumber` | `ClMdHuu+R1A` |
| `sceKernelGetAioDebugRequestInfo` | `iWe19fLnD0Y` |
| `sceKernelGetAioDebugSubmitInfo` | `uepHNAxXJ6s` |
| `sceKernelGetAmprCounter` | `6bX1434czj4` |
| `sceKernelGetAppCategoryTypeByAppId` | `fh-Bb3wl0Vs` |
| `sceKernelGetAppInfo2` | `d7xu4QDJw8Q` |
| `sceKernelGetAppInfoListForMinisyscore` | `OeLAM9FL10U` |
| `sceKernelGetCpuCoreClock` | `8Ejov5Yq8ck` |
| `sceKernelGetGenuineProsperoSystemSwVersion` | `IrjEm08+qoY` |
| `sceKernelGetNandInfo` | `W+BEVqLrDXo` |
| `sceKernelGetPackageInfo` | `4kJiTvD4tsA` |
| `sceKernelGetProsperoCompiledSdkVersionByPid` | `3vD7TSeebMs` |
| `sceKernelGetProsperoCompiledSdkVersion` | `GGeRJk1XdWc` |
| `sceKernelGetPs4CompiledSdkVersionByPid` | `Ha-dfYigiss` |
| `sceKernelGetPs4CompiledSdkVersion` | `sp+h-CJV1Ns` |
| `sceKernelGetResidentFmemCount2` | `zx9n-9bt7IA` |
| `sceKernelGetToolMemoryRange` | `dkBx0YqFQ+Y` |
| `sceKernelGetUpdatemode` | `NMf5gHfsrYs` |
| `sceKernelHasTrinityMode` | `yu17wG8L5FI` |
| `sceKernelIccClearEmcCrashReport` | `qq-gRlUwTPk` |
| `sceKernelIccDisableExtBDDriveOperation` | `pv+JNjlZJgI` |
| `sceKernelIccGetBDDriveChuckingState` | `-uPq82VbRMI` |
| `sceKernelIccGetBuzzerDimSetting` | `8uGeBlkKoDE` |
| `sceKernelIccGetBuzzerOffSetting` | `qbx9P0mc2Bk` |
| `sceKernelIccGetDynamicLedDimSetting` | `LdcAla8x5QI` |
| `sceKernelIccGetEmcBtDownload` | `hhzesncETPw` |
| `sceKernelIccGetEmcCrashNumOfBlocks` | `G0KaQ+fpNWY` |
| `sceKernelIccGetEmcCrashReport` | `4NEVlgkAjNk` |
| `sceKernelIccGetFloydVersion` | `N8YJA7a+T2U` |
| `sceKernelIccGetWlanBtGpio` | `Ck1ezcV7c-Y` |
| `sceKernelIccIsExtBDDriveAttached` | `1ns3nDImmno` |
| `sceKernelIccIsThermalAlert` | `Mrc6Jc4kC1A` |
| `sceKernelIccSetBootTimestamp` | `Ht23vmmuP0Q` |
| `sceKernelIccSetBuzzerDimSetting` | `UhDIvuRtbHc` |
| `sceKernelIccSetBuzzerOffSetting` | `3Fj1v0puUz4` |
| `sceKernelIccSetDynamicLedDimSetting` | `SOWF3EWyJlo` |
| `sceKernelIccSetEmcBtDownload` | `AJjZOx0GwYU` |
| `sceKernelIccSetPriorityBuzzerEx` | `PGdGn+-opZg` |
| `sceKernelIccSetPriorityBuzzer` | `eV6oaavMeI0` |
| `sceKernelIccSetWlanBtGpio` | `9D07JRs47Fg` |
| `sceKernelIccSetWlanBtManuTestMode` | `7Ud-rED9ooU` |
| `sceKernelIsAuthenticTrinity` | `X0HkB92+NRE` |
| `sceKernelIsCloudDevkit` | `XyW5rUlKP1w` |
| `sceKernelIsCloudkit` | `rm1EC-NwS6A` |
| `sceKernelIsCronos` | `nQ5AHlyJghQ` |
| `sceKernelIsEcoChargeAvailable` | `a9683+DNj84` |
| `sceKernelIsSpmemAvailable` | `QEvYMQNJGMU` |
| `sceKernelKernelHeapGetCount` | `rd23OgUSfZQ` |
| `sceKernelKernelHeapUsage2` | `pQiLz-exzR4` |
| `sceKernelMapFlexibleMemoryInternal2` | `dYK287SsmDs` |
| `sceKernelMapFlexibleMemoryInternal` | `z68xkNPqNRk` |
| `sceKernelMapNamedFlexibleMemoryInternal2` | `n7I71nQgO3M` |
| `sceKernelMapNamedSystemFlexibleMemory2` | `QS0AbROQuJM` |
| `sceKernelMapperGetIndirectBufferInfo` | `kz6S+-e2e74` |
| `sceKernelMapperGetUsageStatsData` | `CEbYeyYuiPQ` |
| `sceKernelMapperSetPageTablePoolOccupancyNotificationThreshold` | `V-DUYmkNleQ` |
| `sceKernelPltTrap` | `cMs2CQrqAN0` |
| `sceKernelSetAcKeyRegResult` | `mpTUqam2SgY` |
| `sceKernelSetAppInfoByAppId` | `fIyST5BItRg` |
| `sceKernelSetUpdatemodeDirect` | `MsmPuiJdZ1Q` |
| `sceKernelTitleWorkaroundIsEnabled2` | `TUZlvaB9rlg` |
| `sceKernelTraceAutoCaptureCreate` | `A3MDcIJe1tg` |
| `sceKernelTraceAutoCaptureDelete` | `4uDeONzVc04` |
| `sceKernelTraceAutoCaptureMap` | `0t79VtA5FwY` |
| `sceKernelTraceAutoCaptureStart` | `SoN+jQ6T788` |
| `sceKernelTraceAutoCaptureStop` | `TqLPe9eIbng` |
| `sceKernelTraceAutoCaptureStore` | `51iRQnpGof0` |
| `sceKernelTraceAutoCaptureUnmap` | `stvymc0mgcY` |
| `sceKernelUseCronosKura` | `7lZJYy0+evk` |
| `sceKernelUseCronosStreaming` | `ind6aqzel+Y` |
| `sceKernelWriteUnmapToPrtCommand` | `vnKYv2NuO5U` |
| `scePthreadRegisterThreadDeathHook` | `DlCg0QhcgYQ` |
| `Unknown` | `+1mj9RYU1rs` |
| `Unknown` | `+MZfwzH65JQ` |
| `sceKernelGetAmprSysInfo` | `+ooJjRJ+xR4` |
| `Unknown` | `-PNB3tfnD8c` |
| `sceKernelGetAppLibcData` | `-QipytgE4x0` |
| `Unknown` | `-sYN8ucoAWw` |
| `Unknown` | `0CV40L3tznI` |
| `Unknown` | `1WXQlxMpnNA` |
| `Unknown` | `1v23w7Rnkic` |
| `Unknown` | `1v6DjTPunjs` |
| `Unknown` | `2+KZ4mZKp6E` |
| `sceKernelInternalCreateMapperSharedMemory` | `2ICp74QsULo` |
| `Unknown` | `2XwZJG3Dkss` |
| `Unknown` | `2YWCKF9-3gM` |
| `Unknown` | `2fPLezHAV2E` |
| `Unknown` | `2wKjID0iGVM` |
| `Unknown` | `2xg4JXvaJ6k` |
| `Unknown` | `3BwLcdWeZLs` |
| `Unknown` | `3YkQPLocySc` |
| `Unknown` | `3jw-vq4gYLs` |
| `Unknown` | `4AMTAyz32fk` |
| `sceKernelIsCloudkitForDecid` | `5EThc7n+PpM` |
| `Unknown` | `5KHFhRhBR78` |
| `sceKernelIccGetButtonStatus` | `5uP89EdDbB0` |
| `sceKernelAbortWithContext` | `6nv1h9JH1Lk` |
| `sceKernelGetCanvasHandleByPid` | `6yzl0ibATw0` |
| `Unknown` | `7BUHmsuhcq4` |
| `Unknown` | `7FNl8FfSGNw` |
| `Unknown` | `7Sd1vnnqEqU` |
| `sceKernelIccReadWlanArea` | `8B-r5ubQLZs` |
| `Unknown` | `8U0nlN0TpDU` |
| `Unknown` | `8YkbXeSRxnM` |
| `Unknown` | `940JfV9yJto` |
| `sceKernelGetAppMemoryInfo` | `9KpeTiVh5Hk` |
| `Unknown` | `9ncS0kCRRSA` |
| `Unknown` | `A+S5E4RCh7Y` |
| `Unknown` | `AGHqSV1gdIY` |
| `Unknown` | `AhBZlHQYeA8` |
| `Unknown` | `ApbTseKUKCI` |
| `Unknown` | `B70jHzr4Jj4` |
| `Unknown` | `BJvWKKfmj4w` |
| `Unknown` | `BjmZn9pYSuk` |
| `Unknown` | `C3ToW2CA+hM` |
| `Unknown` | `C3xgqN95IoE` |
| `Unknown` | `C49jelxiaVE` |
| `sceKernelSwitchToTrinityMode` | `Cz48pcLnLCY` |
| `sceKernelIsGenuineCronosCex` | `DB0muDALi4Y` |
| `Unknown` | `DWWduhQYA3I` |
| `Unknown` | `Dv8In6FvzuM` |
| `Unknown` | `E58GHGcQT7I` |
| `Unknown` | `EIz+fSlYCn8` |
| `sceKernelTraceMarkerBookmark` | `EKMWpdzszVo` |
| `Unknown` | `EaepepH-LmI` |
| `Unknown` | `FIUPjhnoI+o` |
| `Unknown` | `FR0odl35yQY` |
| `sceKernelIccGetExtBDDriveStatus` | `GADbvgNm4tY` |
| `Unknown` | `GTyjVeZcdRM` |
| `Unknown` | `Gtncco+mxio` |
| `Unknown` | `HRrfx1wytn0` |
| `Unknown` | `HnWsxkpJya0` |
| `Unknown` | `HruYl9uBnNo` |
| `Unknown` | `IJQeoUQ3pb4` |
| `sceKernelInternalUnlockMapperMemory` | `IJnZ6pAGPcA` |
| `Unknown` | `Ipau2b5eaHM` |
| `Unknown` | `Ishr1Sdu024` |
| `sceKernelIdTableGetStats` | `J2dqh+RUwEY` |
| `Unknown` | `JFpAREOJnag` |
| `Unknown` | `JN1D3WtbDPs` |
| `sceKernelIccNotifyPowerState` | `JcG0kUg4TrE` |
| `Unknown` | `KoDy4Jazz40` |
| `Unknown` | `KoSCciXA1N0` |
| `Unknown` | `L4GARHYRW0U` |
| `Unknown` | `LPXIR37gw3c` |
| `Unknown` | `LVRydXoXt7U` |
| `Unknown` | `LwGBaXgvFaY` |
| `Unknown` | `MJ-err8l-XY` |
| `Unknown` | `MKlp-bW1bz8` |
| `Unknown` | `N94a0T4F4+U` |
| `Unknown` | `NhFlyw-77cY` |
| `Unknown` | `OZBJnWxt7d4` |
| `Unknown` | `PBq-OUg5A5s` |
| `Unknown` | `PTDuAbCYfm0` |
| `Unknown` | `QTGIeqEjeFc` |
| `Unknown` | `Qk38ESxhDEQ` |
| `Unknown` | `R3QLLyp9MEU` |
| `Unknown` | `RA5mXLPd5V4` |
| `Unknown` | `RDjusrvQ-44` |
| `Unknown` | `RfzgytiWey0` |
| `Unknown` | `SEKlG1AMRGQ` |
| `sceKernelGetContentVersion` | `SP37+6t8PUw` |
| `Unknown` | `Sesgl6Zl8YI` |
| `sceKernelTraceMarkerPop` | `SgxG1rkVqu8` |
| `sceKernelInternalLockMapperMemory` | `TC+IFjP+pJc` |
| `Unknown` | `TVB77yVa+RU` |
| `Unknown` | `Tca8tetUZCw` |
| `sceKernelAddExternalTriggerEvent` | `U-zqHonxxm4` |
| `Unknown` | `U9B58Hc5C+8` |
| `Unknown` | `V4f3CEBQyuM` |
| `Unknown` | `VU1YkdCmYto` |
| `Unknown` | `W1Cg5WPD4IQ` |
| `Unknown` | `W7Vpt62Xtf4` |
| `Unknown` | `WLC94v0XY+0` |
| `Unknown` | `WtA8Aq-0cyY` |
| `Unknown` | `WyZULCQhjdY` |
| `Unknown` | `X4R-jzzFYac` |
| `Unknown` | `Xj9nET9tctI` |
| `Unknown` | `XvONqaRmAPE` |
| `Unknown` | `YP7qj+uSJwo` |
| `Unknown` | `Z+8fq02NpM0` |
| `Unknown` | `ZFhVeS+BQJs` |
| `Unknown` | `aH3bEULQH54` |
| `Unknown` | `aJK9UjBtwi4` |
| `Unknown` | `aL9DwvoSDzs` |
| `Unknown` | `aekYw+lydSg` |
| `Unknown` | `aj7wRdAWFeM` |
| `Unknown` | `ak6t3JzhOtg` |
| `sceKernelGetSandboxConfig` | `b3Q5U48bJLI` |
| `Unknown` | `bQ+8uJVyxII` |
| `sceKernelIccResetBootTimestamp` | `c1xs7KJ+uf0` |
| `Unknown` | `cH6vM6sF5YA` |
| `Unknown` | `cReByBlM5XE` |
| `Unknown` | `cafDxa64sO4` |
| `Unknown` | `ciYlptMdvhc` |
| `sceKernelIccWriteWlanArea` | `cp8qj9i8zMM` |
| `Unknown` | `culoiinnxDA` |
| `sceKernelCheckProsperoSdkVersion` | `cvFtKaAYbHE` |
| `Unknown` | `dnzOA7Vx1AE` |
| `Unknown` | `drv94FQk-Qw` |
| `Unknown` | `e7oZmNPlBbU` |
| `Unknown` | `eSgYzXvTPQI` |
| `sceKernelTraceMarkerPlot` | `eu1IgkLPYh8` |
| `Unknown` | `ftHBlCfIEQo` |
| `Unknown` | `g7MGwZ5UxBE` |
| `Unknown` | `gBiaAGKEvHM` |
| `Unknown` | `gJ5gpW45mU0` |
| `Unknown` | `gSIiye3ERqo` |
| `Unknown` | `gpgi-GwE2Is` |
| `Unknown` | `guq+otVpgj0` |
| `Unknown` | `gvq6XRoDogE` |
| `Unknown` | `h26nEKnW8VI` |
| `Unknown` | `h35fpAz5tf4` |
| `sceKernelTraceMarkerPush` | `hYEEJrcB3wU` |
| `Unknown` | `hmqw8GlN+tI` |
| `sceKernelGetCanvasHandle` | `i9ghW7uPndU` |
| `sceKernelSetSandboxId` | `iZEvDaPnNL4` |
| `Unknown` | `ilQLkGlAjWA` |
| `Unknown` | `j6bGtwZkxOQ` |
| `Unknown` | `jESbu+uFJYU` |
| `Unknown` | `jMuNoBRCPUg` |
| `Unknown` | `jkLbtOA587w` |
| `Unknown` | `k+J3KBjfkNk` |
| `Unknown` | `km8Fr2Jn4Gg` |
| `Unknown` | `l-QGmU3VU+Q` |
| `Unknown` | `l67TRPy55-U` |
| `sceKernelDeleteExternalTriggerEvent` | `lRD35KDvDzE` |
| `Unknown` | `mIkJUpP5vKo` |
| `Unknown` | `ncrollDo5u4` |
| `Unknown` | `nsq826qQ2EI` |
| `Unknown` | `o141x2VDeVc` |
| `sceKernelGetMainSocSubId` | `oSQQA8I595Y` |
| `sceKernelInternalUnmapMapperSharedMemory` | `pAI+17im1o0` |
| `Unknown` | `pCbBddNl66Y` |
| `Unknown` | `pOClGbxrx94` |
| `sceKernelSetAppLibcData` | `pn68p6w2eK0` |
| `Unknown` | `pp+G+UZhiqY` |
| `Unknown` | `q-2Uxlg-faU` |
| `Unknown` | `qGzuTSD9cPQ` |
| `Unknown` | `qHVs+dxCumg` |
| `Unknown` | `qSleeVC6lwI` |
| `Unknown` | `qe5JmcCj4zg` |
| `Unknown` | `qtHOLtFWq7Y` |
| `Unknown` | `r6qUkFmQFr0` |
| `Unknown` | `rC7zr-oAHCo` |
| `Unknown` | `rD4PS3f+jKw` |
| `Unknown` | `rLBfsmdAuvo` |
| `sceKernelIccDisableButton` | `rqiBT75A1po` |
| `Unknown` | `sFW+Ilmw+58` |
| `Unknown` | `tkZiGUoRJok` |
| `Unknown` | `u9lxqerTKeA` |
| `Unknown` | `uV+YilcHqAs` |
| `Unknown` | `ulVjluL9TzM` |
| `Unknown` | `v+qTlNjF6kE` |
| `Unknown` | `vfYaxkxnJXQ` |
| `Unknown` | `vq2+HyP5U5I` |
| `sceKernelSetCanvasHandle` | `wXnNof3LrrU` |
| `Unknown` | `wn0XzOue4EM` |
| `Unknown` | `wwWv+0MzhGM` |
| `sceKernelDebugPauseWithDebugContext` | `x0DQK0TXDk0` |
| `Unknown` | `xpB6yv8NhhE` |
| `Unknown` | `yWOJkfd-DhA` |
| `Unknown` | `yn0b42DApAI` |
| `Unknown` | `yzvrk3YrPlE` |
| `Unknown` | `z+e0vV-tnH4` |
| `Unknown` | `z0qKN5kkVj0` |
| `Unknown` | `zAloppPJxy0` |

- [sceKernelGetHwModelName, sceKernelIsProspero](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - both throw. On a CFI-2014 PS5 (firmware 13.60) a payload process got 0 from `sceKernelGetHwModelName(char*)`, which wrote "CFI-2014 A01X" and a terminator (14 bytes), and 0 from `sceKernelIsProspero()`. The model string belongs to one console and the buffer size and null behaviour are not known, and 0 from `sceKernelIsProspero` on a PS5 suggests it answers something other than the model, so neither is implemented
- [sceKernelGetCompiledSdkVersion](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - which SDK version the console returns is unmeasured. The [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk/blob/a7685ce8e97ce1f8a4f38253a41d54241c9124f1/crt/kernel.c#L257-L275) declares the process parameters with a 32-bit PS4 SDK version at offset `0x10` and a 32-bit PS5 SDK version at `0x14`; this export returns the field at `0x10`, the one [libSceHmd2](../../../core/libs/prx/libSceHmd2/Export.cpp) reads and the one [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/615bcba5e21b82dc59588575f7ecbce8da09ca2c/src/emulator.cpp#L110-L138) (PS4, a single 64-bit `sdk_version` there) reads from the eboot. The `int (int32_t*)` signature and `EINVAL` (`0x80020016`) for a null output follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/615bcba5e21b82dc59588575f7ecbce8da09ca2c/src/core/libraries/kernel/process.cpp#L56-L62). Closes when: measured on a PS5
- [scePthreadCondattrGetclock](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - the `int (const PthreadCondattr*, KernelClockid*)` signature and checks follow FreeBSD 11.4 libthr's [`_pthread_condattr_getclock`](https://github.com/freebsd/freebsd-src/blob/releng/11.4/lib/libthr/thread/thr_condattr.c#L82-L90) with the error code of libkernel's `scePthreadCondattrSetclock`: a null or destroyed attribute returns `SCE_KERNEL_ERROR_EINVAL`, and a null clock pointer throws, since libthr writes through it unchecked. Console behaviour is unverified

### Functional

- [sceKernelGetUpdVersion](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - runs the same code as `sysKernelGetUpdVersion` on firmware 13.42 ([disassembly](https://github.com/boykopovar/AnyPS5/pull/10671#issuecomment-6398286973)), so it forwards there: a null output is refused with `SCE_KERNEL_ERROR_EINVAL`, and otherwise the 4 bytes come from `machdep.upd_version`, whose retail value is unmeasured (see libkernel's entry)
- [kenv](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) follows FreeBSD 11.4's [`sys_kenv`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_environment.c#L87-L204) over an in-memory table that starts empty, since the console's boot environment is unmeasured, so a variable the system sets at boot reads as `ENOENT`. `KENV_SET` and `KENV_UNSET` always pass the privilege check, as the guest is uid 0. A name or value is read with the guest path copy, which reads up to 1024 bytes, so an unterminated string followed by an unreadable page fails with `EFAULT` where FreeBSD's bounded `copyinstr` gives `ENAMETOOLONG`. A 513th variable is dropped silently, as in FreeBSD. Console behaviour is unverified
- [sched_rr_get_interval](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) accepts pid 0 or the host-backed guest pid and reports the FreeBSD 11 ULE default time slice for the usual AMD64 clock rates (94 ms); other pids return `ESRCH`, and a null interval returns `EFAULT`. AnyPS5 does not run a guest ULE scheduler or track a per-process configured quantum, so this reports the modeled default and does not reflect runtime scheduling. Console-specific clock rates and quantum configuration are unverified
- [fchdir](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) changes the guest working directory to the descriptor's directory: on Windows through the path recorded when the descriptor was opened, on Linux through the `/proc/self/fd` link target. It returns `EBADF` for a descriptor that is not open, `EINVAL` for a pipe or socket, as FreeBSD `getvnode` does for a descriptor without a vnode, and `ENOTDIR` for any other descriptor that is not a directory. Outside Windows it checks search permission on the directory with `faccessat`, which fails with `EACCES` as FreeBSD `change_dir` does; on Windows that check is not modelled. A directory renamed after it was opened is still reached through the recorded path. Console behaviour is unverified
- [openat, _openat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) pass `AT_FDCWD` (-100) and absolute paths to `open`. Any other relative path is joined to the descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to it, and a `..` component is not kept inside the guest root. They return `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. As with `open`, `O_NONBLOCK` is ignored. PPSA99003 opens directory entries through `openat`. Console behaviour is unverified
- [fchmodat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) passes `AT_FDCWD` (-100) and absolute paths to `chmod`; any other relative path is joined to the descriptor's host directory the same way as for `openat`, so guest path aliases and case-insensitive name matching do not apply to it and a `..` component is not kept inside the guest root. A flag other than `0` or `AT_SYMLINK_NOFOLLOW` (0x200) returns `EINVAL`; the mode is masked to `07777`. With `AT_SYMLINK_NOFOLLOW` a symbolic link returns success silently on Windows and Linux, since the host cannot change a link's own mode, while a regular file is changed either way. The mode maps to the host file mode, which on Windows is only the CRT's read-only bit. PPSA99621 calls it through `std::filesystem::permissions`. Console behaviour is unverified
- [lchmod](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) throws for a symbolic link on Windows and Linux, whose hosts cannot change a link's own mode; on macOS the link's mode is changed. Anything else goes to `fchmodat(AT_FDCWD, path, mode, AT_SYMLINK_NOFOLLOW)`, as FreeBSD 11.4's [`sys_lchmod`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/vfs_syscalls.c#L2490-L2496) does, so a file or directory is changed as by `chmod`, with `fchmodat`'s path lookup and Windows mode mapping. On Windows any reparse point counts as a link. Console behaviour is unverified
- [pathconf, fpathconf, _fpathconf](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) answer every file and directory as the console's UFS mount `/system_data` does in a payload process on a PS5 (firmware 13.60). Other mounts differ: `/data` and `/user` give `_PC_LINK_MAX` 1, `_PC_SYMLINK_MAX` 0, `_PC_NO_TRUNC` `EINVAL` and 255 for `_PC_MAX_CANON`, `_PC_MAX_INPUT` and `_PC_VDISABLE`; `/`, `/system`, `/hostapp` and `/update` answer only four names; tmpfs gives `_PC_FILESIZEBITS` 0 and `_PC_SYNC_IO` 1. What a title sees under `/app0`, `/savedata0`, `/temp0` and `/download0` is not known. `fpathconf` and `_fpathconf` answer `_PC_ASYNC_IO` 200112 and, for a pipe, only `_PC_PIPE_BUF` 512, as FreeBSD 11's `kern_fpathconf` does; a guest socket fails `fstat` with `EBADF`
- [mkdirat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) passes `AT_FDCWD` (-100) and absolute paths to `mkdir`. Any other relative path is joined to the descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to it, and a `..` component is not kept inside the guest root. It returns `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. On Windows, `mode` is ignored. Console behaviour is unverified
- [readlink](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) on Windows reads a symbolic link's target from its reparse data and returns it with `/` separators, only when the target is relative, as `symlink` creates them. A symbolic link with an absolute target returns its print name, or its substitute name when the print name is empty, without a `\??\` or `\\?\` prefix. A directory junction returns the same kind of name when it is a drive path (`X:\...`), using the print name when that is a drive path and the substitute name otherwise. A mounted volume (`\??\Volume{GUID}\`), any other name surrogate and every reparse point without the name surrogate bit (WOF compression, deduplication, cloud files) return `EINVAL`, as FreeBSD does for a file that is not a link and as CPython's `os.readlink`, which reads only symbolic links and junctions, does
- [aio_read, aio_write, aio_fsync, aio_error, aio_return, aio_suspend, aio_waitcomplete, aio_cancel](../../../core/libs/prx/libkernel_sys/Aio.cpp) (libkernel_sys) - a request with `SIGEV_SIGNAL`, `SIGEV_THREAD_ID` or `SIGEV_KEVENT` completion, or on a socket, throws, because delivering a guest signal or kevent and socket aio are not modelled. One worker thread runs the process's requests one at a time in submission order, so a request is cancellable only until that thread takes it. A queued request keeps the descriptor number, not the open file: closing or reusing the descriptor before it runs changes what it reaches. Pipes fail with `EOPNOTSUPP`, as FreeBSD 11 does with `vfs.aio.enable_unsafe` 0; the console's setting is unverified. Waits are not interrupted by signals (`EINTR`), and timeouts under 1 ms round up to 1 ms (hz 1000). Otherwise the rules follow FreeBSD 11 `vfs_aio.c`
- [readlinkat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) passes `AT_FDCWD` (-100) and absolute paths to `readlink`. Any other relative path is joined to the descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to it, and a `..` component is not kept inside the guest root. It returns `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. Console behaviour is unverified
- [link](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) on Windows resolves a symbolic link source through the host, which follows longer chains than FreeBSD: a chain of 40 links is linked, where FreeBSD 11.4 returns `ELOOP` after 32 (`MAXSYMLINKS`). A loop returns `ELOOP` as on FreeBSD
- [pthread_attr_setcreatesuspend_np, pthread_resume_np, pthread_resume_all_np, pthread_multi_np, scePthreadAttrSetcreatesuspend, scePthreadResume, scePthreadResumeAll and scePthreadMulti](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - when a thread created suspended stops relative to its signal mask is unmeasured on a console. Return values and errors follow FreeBSD 11.4 libthr ([`thr_attr.c`](https://github.com/freebsd/freebsd-src/blob/8c639945/lib/libthr/thread/thr_attr.c#L369-L381), [`thr_resume_np.c`](https://github.com/freebsd/freebsd-src/blob/8c639945/lib/libthr/thread/thr_resume_np.c#L47-L97), [`thr_multi_np.c`](https://github.com/freebsd/freebsd-src/blob/8c639945/lib/libthr/thread/thr_multi_np.c#L40-L51)) and the code of firmware 13.42's `libkernel_sys.sprx` ([#9573](https://github.com/boykopovar/AnyPS5/pull/9573#issuecomment-6393877843)), where the sce forms add `0x80020000` to a non-zero errno
- [pthread_attr_setstackaddr and pthread_attr_getsolosched_np](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - firmware 13.60 `libkernel_sys.sprx` (SHA-256 `60921322b155ba649cfa29ea5e6d358ca0229424b4f73fd5c30ae7f7ce7c304c`) places `pthread_attr_setstackaddr` (NID `suCrEbr0xIQ`) at function address `0x4ed0`: it returns `EINVAL` for a null attribute pointer, null attribute handle or null stack address, and stores a valid address at attribute offset `0x18`. `pthread_attr_getsolosched_np` (NID `ApMLfJ527Ag`) is at `0x51b0`: it returns `EINVAL` for a null attribute pointer, null handle or null output, otherwise writes the attribute field at `+0x10` masked with `0x10`. These details are from local firmware disassembly; console runtime behavior is not independently tested
- [sceKernelGetCpumodeGame and sceKernelSetCpumodeGame](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - the setter takes the mode as its only argument and the getter writes it through an `int*`, as [ProsperoEden (PPSA99008) declares and calls them](https://github.com/mihawk-99/PS5_ProsperoEden/blob/a0b65fe50dd48d4064e2f58fa9c8d44665478878/headless/performance.cpp#L1030-L1062); its getter faulted when called without one, which throws here. The mode starts at 1, 0 and 1 are accepted, and 2 to 7 return `EINVAL` (0x80020016), as it [measured](https://github.com/mihawk-99/PS5_ProsperoEden/blob/a0b65fe50dd48d4064e2f58fa9c8d44665478878/docs/PERFORMANCE_FINDINGS.md#L113-L114) on a PS5; other modes were not tried and throw. That the getter reports the last accepted mode is inferred from the probe. The mode changes nothing else here: on the console 0 gave system CPU mode 1 and 1 mode 5, while `sceKernelGetCpumode` still returns 0.
- [fstatat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) passes `AT_FDCWD` (-100) and absolute paths to `stat`, or to `lstat` with `AT_SYMLINK_NOFOLLOW` (0x200). Any other relative path is joined to the descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to it, and a `..` component is not kept inside the guest root. It returns `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory, and flags other than `AT_SYMLINK_NOFOLLOW` return `EINVAL`. PPSA74126 imports it. Console behaviour is unverified
- [uuidgen](../../../core/libs/prx/libkernel_sys/Uuid.cpp) (libkernel_sys) - the node is 48 random bits with the multicast bit set, chosen once per process, where the console's kernel uses the address of its first Ethernet interface; that the PS5 kernel registers one is assumed, not measured. Otherwise it follows FreeBSD 11's [`kern_uuidgen` and `sys_uuidgen`](https://github.com/freebsd/freebsd-src/blob/53c4471833e8e5e57b4d1b501b1a52b343bb0451/sys/kern/kern_uuid.c#L129-L198): version 1 UUIDs with consecutive timestamps from the guest `CLOCK_REALTIME`, a random 14-bit clock sequence that is incremented when the clock has not passed the last batch, and `EINVAL` for a count outside 1 to 2048. A null store returns `EFAULT` after the state advances, as `copyout` fails there; other invalid addresses are not detected
- [unlinkat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) passes `AT_FDCWD` (-100) and absolute paths to `rmdir` (with `AT_REMOVEDIR`, 0x800) or `unlink`. Any other relative path is joined to the descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to it, and a `..` component is not kept inside the guest root. It returns `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. Removing a directory without `AT_REMOVEDIR` fails with `EPERM`, removing `.` or `..` with `AT_REMOVEDIR` fails with `EINVAL`, and flags other than `AT_REMOVEDIR` fail with `EINVAL`. Console behaviour is unverified
- [renameat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) passes `AT_FDCWD` (-100) and absolute paths to `rename`. Any other relative path is joined to its descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to relative descriptors, and a `..` component is not kept inside the guest root. It returns `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. Console behaviour is unverified
- [fork](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) throws: creating a child process is not supported. PPSA19111 imports it and fails to load without it. Its engine calls `fork` in `Posix_Daemonize`, only with `-daemonize`, and in `Platform_ShellExecute` before `execvp("xdg-open")`, which the main menu's Previews button reaches, so pressing Previews ends in this throw. [setsid](../../../core/libs/prx/libkernel_sys/Export.cpp) fails with `EPERM`, as FreeBSD's does for a process group leader, since the guest is taken to lead its own process group (see `kill`). [umask](../../../core/libs/prx/libkernel_sys/Export.cpp) keeps the guest's mask (initially 022, masked to 07777) and returns the previous one, Linux named shared-memory creation applies it, but ordinary files are still created with the host process's mask
- [sched_setscheduler](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) follows FreeBSD 11.4's [`sys_sched_setscheduler`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/p1003_1b.c#L198-L225) and [`ksched_setscheduler`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/ksched.c#L169-L205): a null parameter returns `EFAULT`, a pid other than 0 and the guest's own returns `ESRCH`, and a policy other than `SCHED_FIFO` (1), `SCHED_OTHER` (2) and `SCHED_RR` (3) returns `EINVAL`. Pid 0 sets the calling thread through `scePthreadSetschedparam`, with the ranges it accepts on a PS5; the values are only recorded. Values it rejects throw (FreeBSD answers `EPERM` or `EINVAL`, the console is unverified), and so does the guest's own pid, whose first thread is not tracked. Whether a title holds FreeBSD's `PRIV_SCHED_SET` is unverified
- [sigreturn](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) follows FreeBSD 11.4's amd64 [`sys_sigreturn`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/amd64/amd64/machdep.c#L473-L576) on `setcontext`'s `ucontext_t`: a null context returns `EFAULT`, and `mc_flags` outside `_MC_FLAG_MASK` or an `rflags` differing from the current flags outside `PSL_USERCHANGE` return `EINVAL`; `mc_len` is not checked. As in [`set_fpcontext`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/amd64/amd64/machdep.c#L2349-L2370), `_MC_FPFMT_NODEV` keeps the FPU state, `_MC_FPOWNED_NONE` loads the initial one, the FPU and PCB owners load `mc_fpstate`, and other values return `EINVAL`. The rest is restored as by `setcontext`, which keeps the `%fs` and `%gs` bases FreeBSD would load. A `%cs` that is not a user selector (FreeBSD also raises `SIGBUS`) and `_MC_HASFPXSTATE` throw
- [sceKernelGetModuleInfoInternal, sceKernelGetModuleListInternal and sceKernelLoadStartModuleForSysmodule](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) answer for every loaded module, system modules included. `sceKernelGetModuleInfoInternal` fills the 0x1A8-byte `SceKernelModuleInfoEx` of a module handle the way `sceKernelGetModuleInfoFromAddr` does for an address, with [shadPS4's checks](https://github.com/shadps4-emu/shadPS4/blob/a808e76/src/core/libraries/kernel/process.cpp#L243-L258) in their order: `EFAULT` for a null pointer, `EINVAL` when `st_size` is not 0x1A8, `ESRCH` for an unknown handle. `sceKernelGetModuleListInternal` returns the same list as `sceKernelGetModuleList`, as [fpPS4](https://github.com/red-prig/fpPS4/blob/04cefd4/kernel/ps4_libkernel.pas#L852-L858) does; fpPS4 marks that as a guess, and neither is checked on a PS5. `sceKernelLoadStartModuleForSysmodule` forwards to `sceKernelLoadStartModule`
- [chroot](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) follows FreeBSD 11.4's [`sys_chroot`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/vfs_syscalls.c#L776-L807) and [`pwd_chroot`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/sys/kern/kern_descrip.c#L3059-L3085) for a root caller: `EFAULT` for a null path, `ENOENT` for an empty or missing one, `ENAMETOOLONG` for a path of 1024 bytes or more or a component over 255, and `ENOTDIR` for a path that is not a directory or passes through a file. A directory that is the guest root (`/`, `/..`, or `.` after `fchdir` to a descriptor of `/`) succeeds and changes nothing, as the root stays the same directory; the guest is taken never to have changed its root, so open directory descriptors do not make it fail with `EPERM` (`kern.chroot_allow_open_directories` 1). Any other directory throws, naming the path: the guest path resolver has a fixed root and mount aliases (`/app0`, `/savedata0`) that do not move with it. On the console a title starts in a sandbox root, where `pwd_chroot` refuses with `EPERM` while a directory is open; whether that holds for titles, and whether a title holds `PRIV_VFS_CHROOT`, are unverified. Porpoise (PPSA99764) weak-imports it and calls it only as `fchdir` to its saved descriptor of `/` then `chroot(".")`, to put its sandbox root back after a jailbreak daemon replaced it ([`put_root_back`](https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5/blob/2ed1ab5a06995aa198f7bccd4da0f90c3fc29d39/src/porpoise_jailbreak.cpp#L303-L331))
- [Console identity](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - `get_authinfo` errors and capabilities are unverified on PS5, returning `EFAULT` when passed an unwritable destination pointer and following [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/396c2a09a38d22eaa02aab32c1967263dceab95f/src/core/libraries/kernel/kernel.cpp) with `EPERM` for null, and zero authority and credential words. The host models a retail unit (`IsCEX` 1, `IsGenuineCEX` 1, `IsDevKit` 0, `IsGenuineDevKit` 0, `IsTestKit` 0, `IsGenuineTestKit` 0) and Prospero (`IsProspero` 1, `IsKratos` 0).
- [sceKernelIsPs4Process and System OpenPSID](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - `sceKernelIsPs4Process` returns 1 when the title ID starts with `CUSA` and 0 otherwise, as `libSceRtc` and `libSceConvertKeycode` tell PS4 titles apart; this mapping follows the PS4 title-ID convention and is unmeasured on a console, since BunnyFl's [13.42 report](https://github.com/boykopovar/AnyPS5/pull/10959#issuecomment-6404011201) only mentions the export returning 0 in passing. System OpenPSID reuses the ordinary ID; firmware 13.42 reads a separate `machdep.openpsid_for_sys` of unknown value. Null returns positive 22 per the [module report](https://github.com/boykopovar/AnyPS5/pull/10524#issuecomment-6393566914).
- [kldfind, kldfirstmod, kldnext and kldunload](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) follow FreeBSD 11.4's linker calls ([sys/kern/kern_linker.c](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_linker.c)): kldfind validates the module name with CopyPath (EFAULT on null or unreadable pointers, ENAMETOOLONG for paths of 1024 bytes or more) and returns ENOENT for any module name; on FreeBSD the kernel itself is linker file 1, and AnyPS5 models an empty file list, so kldnext(0) returns 0 and kldfind("kernel") fails; kldnext returns ENOENT for negative or positive fileids; kldfirstmod returns ENOENT for any fileid; and kldunload returns EPERM as sandboxed/jailed titles lack PRIV_KLD_UNLOAD. Dynamic kernel linker module lookup and management remain unverified on hardware
- [kldstat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) never succeeds: no linker file is listed here, as for `kldnext`, so the kernel's own file (id 1 on FreeBSD) and the modules a console preloads are missing, every file id gets `ENOENT` and the structure is never written. The checks before the lookup follow FreeBSD 11.4's [`sys_kldstat`](https://github.com/freebsd/freebsd-src/blob/releng/11.4/sys/kern/kern_linker.c#L1228-L1250): only the `version` field is read (`EFAULT` when it cannot be), and a version other than the size of `struct kld_file_stat_1` (1056 bytes on amd64) or `struct kld_file_stat` (2080) gets `EINVAL`. Console behaviour is unverified
- [fchown](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - ownership is not modelled: the guest's own owner and group (0) and the unchanged value (-1) are accepted, a descriptor that is not open fails with `EBADF`, and any other owner or group throws, naming it. The check reads the descriptor with `fstat`, ignoring the CRT's invalid-parameter handler on Windows as `DescriptorControl` does. The console's ownership rules are unverified. Morrowind (`mshivam019/morrowind-ps5`) imports it
- [chown, lchown and fchownat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - ownership is not modelled, as for `fchown`: owner and group 0 and the unchanged value (-1) are accepted, and any other owner or group throws, naming it. The path is looked up first, as in FreeBSD 11's `kern_fchownat`, with `stat`, with `lstat` for `lchown`, and with `fstatat` for `fchownat`, so a lookup error is returned before the owner is checked and nothing is changed on the host. `fchownat` flags other than `AT_SYMLINK_NOFOLLOW` (0x200) return `EINVAL`, as in FreeBSD 11. The console's ownership rules are unverified
- [sceKernelIsNeoMode](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - not measured on a console, and the PS5 module's body is not read. A title with `param.json` throws: what a PS5 application gets is unknown. For a title with `param.sfo` it returns bit 23 (0x00800000) of `ATTRIBUTE`, the title's Neo-mode support, and 0 without the key, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/3eecea94ddaca360b19f4b800523b609a8b2ffcc/src/core/libraries/kernel/process.cpp#L29-L35) answers on a Neo console ([bit 23](https://github.com/shadps4-emu/shadPS4/blob/3eecea94ddaca360b19f4b800523b609a8b2ffcc/src/common/elf_info.h#L50-L51)), which `sceKernelHasNeoMode` and `sceKernelIsAuthenticNeo` already report. A base PS4 (firmware 10.71) answers 0 ([discussions/7941](https://github.com/boykopovar/AnyPS5/discussions/7941))
- [linkat](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) resolves `AT_FDCWD` (-100) and absolute paths as `link` does, and flags other than `AT_SYMLINK_FOLLOW` (0x400) return `EINVAL`, as in FreeBSD 11. Any other relative path is joined to the descriptor's host directory, found the same way as for `fchdir`, so guest path aliases and case-insensitive name matching do not apply to it, and a `..` component is not kept inside the guest root. It returns `EBADF` for a descriptor that is not open and `ENOTDIR` for one that is not a directory. Without `AT_SYMLINK_FOLLOW` a symbolic link as the source is linked itself, as in FreeBSD: on Linux and macOS through the host's `linkat` with flag 0, and on Windows through `CreateHardLinkW`, which links the link's reparse point rather than its target, dangling or not. Windows cannot hard-link a directory, so there a symbolic link or junction to a directory as the source throws. Console behaviour is unverified
- [pthread_set_defaultstacksize_np and scePthreadSetDefaultstacksize](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) set the stack size that `scePthreadAttrInit` and a thread created without attributes use, as [fpPS4's `pthread_set_defaultstacksize_np`](https://github.com/red-prig/fpPS4/blob/04cefd4/kernel/ps4_pthread_attr.pas#L89-L102) does: a size of 16384 bytes or less fails with `EINVAL` (`0x80020016` from the sce form) and there is no upper limit, so a default the host cannot allocate fails only when a thread is created. Attributes initialised earlier keep their size. Not checked on a PS5
- [pthread_mutexattr_getgen_np, pthread_mutexattr_setgen_np, scePthreadMutexattrGetgen and scePthreadMutexattrSetgen](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) were reconstructed from the firmware 13.60 exports: generation is a one-bit attribute, set accepts only 0 or 1 and otherwise returns `EOPNOTSUPP`, and the `sce` forms convert the POSIX error to `0x80020000 | errno`. The flag is retained in AnyPS5's attribute state; whether it changes mutex initialization is unmeasured. Runtime behaviour on a PS5 is not independently validated
- [sceKernelTitleWorkaroundIsEnabled](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - not measured on a console; the `{version, align, ids[2]}` layout, the bit test and the limit of 0x3A bits follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/a808e76/src/core/libraries/kernel/kernel.cpp), which notes that the limit changes between firmwares. A negative bit is `EINVAL` here; shadPS4 compares it signed and reads outside `ids`. Titles usually pass the `title_workaround` block of `sceKernelGetAppInfo`, which is written as zeros here; where `title_workaround` sits in the 0x58-byte structure is unmeasured, and with the shadPS4 offsets (0x30 to 0x48) it falls in the zeroed bytes, so its bits read 0; the console's values in a game are unmeasured
- [sceKernelGetProcessType](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - not measured on a console; as in shadPS4, the title's own pid, -1 and 0 are type 0 (big application) and any other pid is `ESRCH`
- [cpuset, cpuset_getid, cpuset_setid](../../../core/libs/prx/libkernel_sys/Cpuset.cpp) (libkernel_sys) - console root IDs and inheritance are unverified; non-process scopes (TID, CPUSET, JAIL) throw as unmodelled as per-thread cpusets and hierarchy visibility are not tracked. CPU set assignment and queries follow FreeBSD 11.4 [kern_cpuset.c](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_cpuset.c): cpuset allocates sequential IDs from 2; cpuset_setid supports CPU_WHICH_PID for known IDs (or root 0, default 1); dynamic sets are single-process referenced and unlinked when vacated, returning ESRCH on lookup; cpuset_getid returns root 0 for CPU_LEVEL_ROOT and the assigned ID for CPU_LEVEL_CPUSET. Output writes use GuestCopyOut_nid_no_patch (EFAULT on bad pointers), with level/which/id errors returned before copyout; levels outside 1 to 3 return EINVAL where FreeBSD returns 0 with the base set ID; other which return EINVAL, unknown pids return ESRCH
- [sceKernelSettimeofday](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - forwards to `settimeofday` and returns its result, or `0x80020000` plus `errno` for a failure, as shadPS4's [`sceKernelSettimeofday`](https://github.com/shadps4-emu/shadPS4/blob/7ea677e56d75bf3d90ace2b0a5144a665d1f38b6/src/core/libraries/kernel/time.cpp#L565-L571) maps its call through `ErrnoToSceKernelError`; that branch passes the call's return value, and shadPS4's own `settimeofday` never fails, so the errno form the other sce wrappers here use is taken. Nothing is measured on the console, and a valid time inherits `settimeofday`'s refusal to set the host clock

### Unknown function info

- [scePthreadBarrier*, scePthreadBarrierattr*, scePthreadGetcpuclockid, scePthreadGetconcurrency and sceKernelMsync](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) call libkernel's `pthread_barrier*`, `pthread_getcpuclockid`, `pthread_getconcurrency` and `msync` and turn a POSIX error into `0x80020000 | errno`; that the console's `sce` forms do the same is not measured. Signatures follow the [Orbital type database](https://github.com/AlexAltea/orbital/blob/master/tools/ida/db_types.json); `scePthreadBarrierInit` ignores its name, and `scePthreadBarrierWait` returns -1 to the serial thread, both as in [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/kernel/threads/barrier.cpp) (PS4). `scePthreadGetconcurrency` returns the level unchanged
- [scePthreadAttrGetscope, scePthreadAttrSetscope, scePthreadAtfork and scePthreadSetconcurrency](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) call libkernel's `pthread_attr_getscope`, `pthread_attr_setscope`, `pthread_atfork` and `pthread_setconcurrency` and turn a POSIX error into `0x80020000 | errno`, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/15abf68c9b52c4b04d094f2837719fdb871021f3/src/core/libraries/kernel/threads/pthread_attr.cpp#L392) exports `scePthreadAttrGetscope` as `ORBIS(posix_pthread_attr_getscope)`; shadPS4 has no export for the other three, whose names and NIDs are in the [symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) and whose signatures follow the same POSIX forms. Not measured on a console. As in libkernel, the scope is 0 (process) or 2 (system), and `scePthreadAtfork` handlers are only recorded, as no process forks
- [scePthreadMutexInitForInternalLibc, scePthreadMutexattrInitForInternalLibc](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - not measured on a console. They call `pthread_mutex_init` and `pthread_mutexattr_init` as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/ce74386/src/core/libraries/kernel/threads/mutex.cpp#L541-L557) and [ChonkyStation4](https://github.com/liuk7071/ChonkyStation4/blob/c101a23/ChonkyStation4/OS/Libraries/Kernel/Kernel.cpp#L97-L108) (PS4) do, with shadPS4's `0x80020000 | errno` errors; a mutex without attributes checks errors. FreeBSD's malloc-internal [`_pthread_mutex_init_calloc_cb`](https://github.com/freebsd/freebsd-src/blob/releng/11.4/lib/libthr/thread/thr_mutex.c#L401-L419) disagrees, making a normal mutex that defers signals. A name argument, signal deferral and allocation are unknown; the [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk/blob/a7685ce/sce_stubs/libkernel_sys.c#L4382-L4452) only lists the exports
- [AMM kernel write commands](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - signatures of the map, map direct, unmap, remap, multimap and protection writers and their GPU mask id forms taken from [ampr_emu's libkernel hook targets](https://github.com/drakmor/ampr_emu/blob/cfa85df379f6eeeb165d7badf9b648e266fe77b7/include/ampr_libkernel_hook_targets.h), which forward to the real kernel; the measure mode (`dst` null reports the size, `outSize` null skips the report) follows ampr_emu's measure-then-record calls, not confirmed on a title. Each writer builds its record with the encoder of its `libSceAmpr` command buffer sibling, so it applies the sibling's checks and writes its bytes. The GPU mask id is ignored, as by the siblings, and a protection or mask with bits above 32 is rejected, as by `sceKernelWriteModifyMtypeProtectCommand`. A wider `type` is cut to the record's 32 bits, as by that writer; the console's handling is not measured
- [sceKernelWaitCommandBufferCompletion](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - signature taken from [ampr_emu's libkernel hook targets](https://github.com/drakmor/ampr_emu/blob/cfa85df379f6eeeb165d7badf9b648e266fe77b7/include/ampr_libkernel_hook_targets.h), where `sceAmprAmmWaitCommandBufferCompletion` forwards to it; like that sibling it waits for an AMM submission and returns `SCE_KERNEL_ERROR_ESRCH` for an id that was never submitted, not confirmed on a title
- [kldsym](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) follows FreeBSD 11.4's [`sys_kldsym`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_linker.c) in the order it checks its arguments: `EFAULT` for an unreadable lookup structure, `EINVAL` for a `version` other than the structure size (32 bytes on amd64) or a command other than `KLDSYM_LOOKUP`, `EFAULT` or `ENAMETOOLONG` for the symbol name, `ENOENT` for a file id that no file has. The linker file list is empty here, as in `kldnext` and `kldfind`, so every file id other than 0 gets `ENOENT`; modules a console preloads are not modelled. A lookup in id 0 (every file) throws, as no symbol table is modelled. Whether a title may query kernel symbols is unverified

#### Inventory identity evidence

The NID set is from the inspected `libkernel_sys` ELF function-export table (defined `STT_FUNC` entries), not the older 3.20 inventory. The module-parameter words at `0x10` and `0x14` are `0x13590001` and `0x13600007`; the earlier 13.60 label refers to SDK-formatted metadata, not independently authenticated retail firmware or an introduction date. The [reproducible static audit](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298) finds 1551 unique function NIDs with set digest `5f1668cc9c69e2950722a83151f31cf7aa94fb1552ab883e5067f43c94d63b16` and confirms all 328 proposed entries. The digest hashes sorted unique ASCII NIDs, each followed by LF. Named declarations are candidate spellings whose repository-computed NID exactly matches an observed function NID; the hashed table establishes neither plaintext spelling nor C ABI.

The 35 formerly literal aliases are named by the [repository-pinned aerolib table](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv). There are now 168 named declarations and 160 unresolved NID aliases; every one of the 328 entries remains a throwing placeholder.

The following eight candidate spellings are absent from that aerolib revision. Their name/NID pairs occur in the [public ps5rs identity database at commit `2304be5131221bae0dac24b9767cabffda33f747`](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv) and independently recompute to the function NIDs in the inspected table. This database supports candidate spelling only; it does not establish firmware membership, introduction version or ABI.

| Candidate spelling | NID |
| --- | --- |
| `__sys_dynlib_unload_prx` | `GQli4UAXTfQ` |
| `mmap2` | `tGtH2GTWbyE` |
| `pthread_setschedother_np` | `ymCrwbFD4fk` |
| `sceKernelEnableDmemAliasing3` | `RbhD-epf6c0` |
| `sceKernelGetResidentFmemCount2` | `zx9n-9bt7IA` |
| `sceKernelIsCloudDevkit` | `XyW5rUlKP1w` |
| `sceKernelKernelHeapUsage2` | `pQiLz-exzR4` |
| `sceKernelSetAppInfoByAppId` | `fIyST5BItRg` |
