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

- [Registry store](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - entries are queried through an in-memory process-local store without persistence. Callable prototypes for `sceRegMgrGetInt`, `sceRegMgrGetStr`, and `sceRegMgrGetBin` derive from [regmgr.h](https://github.com/ps5-payload-dev/regdump/blob/883524cc48c8497267ba7fc94ff0ac58880c4227/regmgr.h#L3713-L3729). Implemented getters return error codes for invalid parameters (`0x80820001`), missing keys (`0x80820002`), buffer sizing (`0x80820004`), and type mismatches (`0x80820006`) as unverified host implementation assumptions, as the cited regmgr.h does not establish error codes or validation behavior. Queries with non-positive keys `0` and `-1` take the missing-key path without key-range validation, pinning the unverified host assumption that non-positive keys are not rejected as invalid parameters. `sceRegMgrSetbin` and zero-length getter paths remain unimplemented throwing `NotImplemented_nid_no_patch`.
- [Registry write paths](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - `sceRegMgrSetInt` (`khaYelw1Ytc`), `sceRegMgrSetStr` (`ffInidSqRss`), `sceRegMgrSetBin` (`eXxrXkrLXa4`) and `sceRegMgrResetVal` (`eL7sMZaIdac`) follow the [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) PS4 HLE, whose IDA notes record PS4 forwarding through `__sys_regmgr_call` (cmd 1=SetInt, 3=SetStr, 5=SetBin, 11=ResetVal) with per-call result 0 and byte lengths stored verbatim. The names resolve to the same NIDs in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c); behavior is unmeasured on PS5. Setters succeed without validation, null or zero-length stores clear entry content, and reset erases the entry: Set→ResetVal→Get returns `0x80820002` here where the cited source returns success with the default.
- [Initial values](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - the initial-value getters and `sceRegMgrSrvGetQAFforReg` (`wQlp94zzrWg`) follow the [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) PS4 HLE, whose IDA-verified SceShellCore caller shapes record `GetIntInitVal(reg_id, int*, a3)` and siblings returning the factory default with success; same NIDs in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c), unmeasured on PS5. voldemort's DefaultInt returns -1 for key `0x07410000` and 1 for key `0x14190b00`; those per-key defaults are deliberately dropped under the generic-behaviour rule, so InitVal reports zero/empty for every key and QAF reports 0 as the retail default.
- [Empty-store getter edges](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - after a null or zero-length `sceRegMgrSetBin`, a following `sceRegMgrGetBin` copies nothing and leaves the caller buffer unwritten, where the cited [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) GetBin zero-fills first; the divergence is accepted because the getters' provenance is the prototype-only PS5 regmgr.h, not voldemort. Sibling edge: `sceRegMgrGetStr` with a length equal to the stored size returns `0x80820004` where voldemort succeeds.
- [Change tracking](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - `sceRegMgrIsChange` (`xhrI4zhlBuA`, same NID in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c)) follows the [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) PS4 HLE, whose notes on PS4 FW7 kernel registry service record that the second argument is a consumer id: valid ids are `0x0d010001..0x0d010020` bits of mask `0x8000808b` plus `0x0d890080`, the low byte selects dirty bits, writes set all bits, and a query consumes only that consumer's bits. Unmeasured on PS5; the consumer-id table is PS4 FW7 state.
- [Unverified registry APIs](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - export names and NIDs for version queries, region queries and conversions, error checks, control/backup routines, and `sceRegMgrNonSys*` variants are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c) and [NID database](https://github.com/zecoxao/sce_symbols/blob/9620e03b2964dbf0cfc1cb143ab24a65125091de/aerolib.csv), but their callable contracts and runtime behavior remain unverified and retain `NotImplemented_nid_no_patch`.

### Unknown function info

- [sceRegMgrCheckError](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrCheckIntVal](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrGetVersion](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - known contract, value unmeasured: [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) IDA notes record the PS4 export forwarding `__sys_regmgr_call(cmd=0, arg=1, ...)` and returning the registry format version (NID `q+lPgqZniW4`, same NID in the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c)); the numeric value is unmeasured on PS4 and PS5, so the export retains `NotImplemented_nid_no_patch`
- [sceRegMgrEvtGetCnt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrEvtGetRegId](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSetInitLevel](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvCnvRegionInt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvCnvRegionStr](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvGetMachineType](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvGetRealMachineType](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature
- [sceRegMgrSrvGetRegion](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - known contract, value unmeasured: [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) IDA notes record the PS4 export forwarding `__sys_regmgr_call(cmd=18,...)` and returning the console region code, which the PS4 SceShellCore caller accepts as any non-negative value (NID `sywg-RnhZMA`, same NID in the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c)); the region value is per-console state, unmeasured and without a documented value mapping, so the export retains `NotImplemented_nid_no_patch`
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
- [sceRegMgrNonSysGetInt](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - the NID (`dKeshzt29G4`) is recorded by the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c), which is NID-to-name only; the signature `(int64 key, int32 unk, int32* data)` comes from the [ChonkyStation4](https://github.com/liuk7071/ChonkyStation4/blob/master/ChonkyStation4/OS/Libraries/SceRegMgr/SceRegMgr.cpp) PS4 emulator and matches the pre-existing in-tree declaration. Behavior is unknown: no source establishes the meaning of `unk`, the key domain, or result values, and ChonkyStation4 returns per-key constants, which is title-specific guessing, not evidence; retains `NotImplemented_nid_no_patch`
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
- [sceRegMgrStrncpy](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - name suggests a libc `strncpy` host mapping, but no source provides a parameter list or return type: the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c) resolves the NID (`BnSHkcDlM+8`) to the name only, and the OpenOrbis PS4 header omits the function. A host-API mapping is blocked on a signature source, so the export retains `NotImplemented_nid_no_patch`
- [sceRegMgrMemset](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - name suggests a libc `memset` host mapping, but no source provides a parameter list or return type: the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c) resolves the NID (`7WBGJ8aph1M`) to the name only, and the OpenOrbis PS4 header omits the function. A host-API mapping is blocked on a signature source, so the export retains `NotImplemented_nid_no_patch`
- [sceRegMgrSetbin](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - unknown signature; the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c) resolves it to NID `LhiUdPKY2qo`, distinct from `sceRegMgrSetBin` (`eXxrXkrLXa4`), and no source establishes a parameter list or proves it an alias, so assuming `sceRegMgrSetBin` semantics is unfounded and the export retains `NotImplemented_nid_no_patch`
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
