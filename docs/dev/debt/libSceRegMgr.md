# libSceRegMgr technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceRegMgr/InventoryExports.cpp) - 8 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `JfNNpUQdhQw` |
| `Unknown` | `UuGXdpQj1Aw` |
| `Unknown` | `YtmvqKL9PEo` |
| `Unknown` | `csy-cEETOKg` |
| `Unknown` | `iHi1Omn25M4` |
| `Unknown` | `wpJW0oIM6rg` |
| `Unknown` | `x0YJQilTJDo` |
| `Unknown` | `x9WN+tFTY34` |

### Functional

- [Registry store](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - entries are queried through an in-memory process-local store without persistence. Callable prototypes for `sceRegMgrGetInt`, `sceRegMgrGetStr`, and `sceRegMgrGetBin` derive from [regmgr.h](https://github.com/ps5-payload-dev/regdump/blob/883524cc48c8497267ba7fc94ff0ac58880c4227/regmgr.h#L3713-L3729). Implemented getters return error codes for invalid parameters (`0x80820001`), missing keys (`0x80820002`), buffer sizing (`0x80820004`), and type mismatches (`0x80820006`) as unverified host implementation assumptions, as the cited regmgr.h does not establish error codes or validation behavior. Queries with non-positive keys `0` and `-1` take the missing-key path without key-range validation, pinning the unverified host assumption that non-positive keys are not rejected as invalid parameters. Setters (`sceRegMgrSetInt`, `sceRegMgrSetStr`, `sceRegMgrSetBin`, `sceRegMgrSetbin`), reset (`sceRegMgrResetVal`), and zero-length getter paths remain unimplemented throwing `NotImplemented_nid_no_patch` until callable contracts and error codes are supported by evidence.
- [Unverified registry APIs](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - export names and NIDs for initial-value getters, version queries, region queries and conversions, error checks, control/backup routines, and `sceRegMgrNonSys*` variants are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c) and [NID database](https://github.com/zecoxao/sce_symbols/blob/9620e03b2964dbf0cfc1cb143ab24a65125091de/aerolib.csv), but their callable contracts and runtime behavior remain unverified and retain `NotImplemented_nid_no_patch`.

### Unknown function info

- [sceRegMgrCheckError](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrCheckIntVal](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrGetBinInitVal](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrGetIntInitVal](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrGetStrInitVal](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrGetVersion](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrIsChange](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrEvtGetCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrEvtGetRegId](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrResetVal](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSetInitLevel](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvCnvRegionInt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvCnvRegionStr](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvGetMachineType](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvGetQAFforReg](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvGetRealMachineType](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvGetRegion](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvGetRegionStr](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrBackupNeedMem](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrBackupPullData](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrBackupPushData](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrBackupPullDataGkp](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrBackupPushDataGkp](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrCntlStart](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrCntlDeleteReg](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrCntlGetFilesCount](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrCntlGetFileName](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrCntSetInitReinst](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrIsInitOK](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrGetInitError](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrRecoverRegNvs](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrNonSysCheckError](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrNonSysGetBin](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrNonSysGetStr](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrNonSysSetBin](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrNonSysSetInt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrNonSysSetStr](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrGetVersionForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrBackupNeedMemForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrBackupPullDataForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrBackupPushDataForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrEvtGetCntForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrEvtGetRegIdForPS4](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrStrncpy](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrMemset](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSetbin](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrToolGetUpdateCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrToolGetEntryCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrToolDataCheckGet](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrDrvGetUpdateCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrDrvGetEntCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrDrvDataOpen](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrDrvDataCheckGet](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrDrvDataClose](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrToolGetInfo](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrLogPull](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrCntlStopHdd](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrPrintInfo](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrLogStart](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature; the name is the PS5 3.20 import whose NID the export carries ([PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs))
