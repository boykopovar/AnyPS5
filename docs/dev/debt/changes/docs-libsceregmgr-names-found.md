# libSceRegMgr

## change [sceRegMgrGetInt, sceRegMgrGetStr and sceRegMgrGetBin]
- [sceRegMgrGetInt, sceRegMgrGetStr and sceRegMgrGetBin](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - returning `0x80820001` (`SCE_REG_MGR_ERROR_INVALID_PARAM`) for an unwritable non-null output pointer via `GuestCopyOut_nid_no_patch` with errno preserved after key lookup and type check is a host-safety fallback with console behavior unmeasured. All 3 names of this entry were found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called.
## change [sceRegMgrNonSysGetInt, sceRegMgrNonSysSetInt, sceRegMgrNonSysGetStr, sceRegMgrNonSysSetStr, sceRegMgrNonSysGetBin, sceRegMgrNonSysSetBin, sceRegMgrCheckError, sceRegMgrNonSysCheckError and sceRegMgrIsInitOK]
- [sceRegMgrNonSysGetInt, sceRegMgrNonSysSetInt, sceRegMgrNonSysGetStr, sceRegMgrNonSysSetStr, sceRegMgrNonSysGetBin, sceRegMgrNonSysSetBin, sceRegMgrCheckError, sceRegMgrNonSysCheckError and sceRegMgrIsInitOK](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - the 11.00 wrappers show the NonSys calls take a 64-bit key, a 32-bit type and an int value or a buffer plus length; the getters reject a null output (`0x800D0202`), and the string/binary calls reject a length above `0x800` (`0x800D0208`). The host reuses the same in-memory store as the system getters and setters, ignores the type word because the console schema is unavailable, and returns the store's `0x808200xx` codes for missing keys, type mismatches and short buffers; a zero-length getter returns `0x80820004`. `sceRegMgrCheckError` and `sceRegMgrNonSysCheckError` report no error and `sceRegMgrIsInitOK` reports the store ready, because the in-memory model has no service init. Not measured on a PS5. All 9 names of this entry were found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called.
## change [sceRegMgrCheckIntVal]
- [sceRegMgrCheckIntVal](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrGetBinInitVal]
- [sceRegMgrGetBinInitVal](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrGetStrInitVal]
- [sceRegMgrGetStrInitVal](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrEvtGetCnt]
- [sceRegMgrEvtGetCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrEvtGetRegId]
- [sceRegMgrEvtGetRegId](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrSetInitLevel]
- [sceRegMgrSetInitLevel](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrSrvCnvRegionInt]
- [sceRegMgrSrvCnvRegionInt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrSrvCnvRegionStr]
- [sceRegMgrSrvCnvRegionStr](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrSrvGetMachineType]
- [sceRegMgrSrvGetMachineType](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrSrvGetQAFforReg]
- [sceRegMgrSrvGetQAFforReg](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - the [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) PS4 HLE reports the retail QA flag as 0; no PS5 source exists and the value is unmeasured on PS5, the same class of unmeasured default that keeps the InitVal getters unimplemented, so the export retains `NotImplemented_nid_no_patch`. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called.
## change [sceRegMgrSrvGetRealMachineType]
- [sceRegMgrSrvGetRealMachineType](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrSrvGetRegionStr]
- [sceRegMgrSrvGetRegionStr](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrBackupNeedMem]
- [sceRegMgrBackupNeedMem](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrBackupPullData]
- [sceRegMgrBackupPullData](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrBackupPushData]
- [sceRegMgrBackupPushData](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrBackupPullDataGkp]
- [sceRegMgrBackupPullDataGkp](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrBackupPushDataGkp]
- [sceRegMgrBackupPushDataGkp](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrCntlStart]
- [sceRegMgrCntlStart](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrCntlDeleteReg]
- [sceRegMgrCntlDeleteReg](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrCntlGetFilesCount]
- [sceRegMgrCntlGetFilesCount](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrCntlGetFileName]
- [sceRegMgrCntlGetFileName](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrCntSetInitReinst]
- [sceRegMgrCntSetInitReinst](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrGetInitError]
- [sceRegMgrGetInitError](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrRecoverRegNvs]
- [sceRegMgrRecoverRegNvs](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrGetVersionForPS4]
- [sceRegMgrGetVersionForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrBackupNeedMemForPS4]
- [sceRegMgrBackupNeedMemForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrBackupPullDataForPS4]
- [sceRegMgrBackupPullDataForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrBackupPushDataForPS4]
- [sceRegMgrBackupPushDataForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrEvtGetCntForPS4]
- [sceRegMgrEvtGetCntForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrEvtGetRegIdForPS4]
- [sceRegMgrEvtGetRegIdForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrSetbin]
- [sceRegMgrSetbin](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature; the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c) resolves it to NID `LhiUdPKY2qo`, distinct from `sceRegMgrSetBin` (`eXxrXkrLXa4`), and no source establishes a parameter list or proves it an alias, so assuming `sceRegMgrSetBin` semantics is unfounded and the export retains `NotImplemented_nid_no_patch`. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called.
## change [sceRegMgrToolGetUpdateCnt]
- [sceRegMgrToolGetUpdateCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrToolGetEntryCnt]
- [sceRegMgrToolGetEntryCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrToolDataCheckGet]
- [sceRegMgrToolDataCheckGet](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrDrvGetUpdateCnt]
- [sceRegMgrDrvGetUpdateCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrDrvGetEntCnt]
- [sceRegMgrDrvGetEntCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrDrvDataOpen]
- [sceRegMgrDrvDataOpen](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrDrvDataCheckGet]
- [sceRegMgrDrvDataCheckGet](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrDrvDataClose]
- [sceRegMgrDrvDataClose](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrToolGetInfo]
- [sceRegMgrToolGetInfo](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrLogPull]
- [sceRegMgrLogPull](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrCntlStopHdd]
- [sceRegMgrCntlStopHdd](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrPrintInfo]
- [sceRegMgrPrintInfo](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature. Found by `dlsym` in `libSceRegMgr.sprx` on a PS5 (firmware 11.60, payload process; a title process may differ); nothing was called, so the signature stays unverified.
## change [sceRegMgrLogStart]
- [sceRegMgrLogStart](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature; the name is the PS5 3.20 import whose NID the export carries ([PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs)). On a PS5 (firmware 11.60, payload process; a title process may differ) `sceRegMgrLogStart` did not resolve by `dlsym` in `libSceRegMgr.sprx` under the bare name (two runs), with a leading underscore or with the suffixes `_40` to `_90`, while 55 other names of the module and a control name resolved; a lookup by NID was not tried, so this does not show that the export is absent.
