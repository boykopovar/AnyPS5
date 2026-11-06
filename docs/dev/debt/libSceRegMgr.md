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
- [Registry write paths](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - `sceRegMgrResetVal` (`eL7sMZaIdac`) follows the [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) PS4 HLE, whose IDA notes record PS4 forwarding through `__sys_regmgr_call` cmd 11 resetting an entry to its factory default; the same NID resolves in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c), unmeasured on PS5. Reset erases the entry, so Set→ResetVal→Get returns `0x80820002` here where the cited source returns success with the default. `sceRegMgrSetInt`, `sceRegMgrSetStr` and `sceRegMgrSetBin` are reverted to `NotImplemented_nid_no_patch` because open PR #15057 implements them against the same store, with independently recorded operation codes 1, 3 and 5.
- [Key widening](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - `sceRegMgrResetVal` and `sceRegMgrIsChange` take 32-bit keys and zero-extend them into the store's positive `int64_t` domain, the same resolution as the setter wrappers in open PR #15057, whose tests pin `0xC0808000` round-tripping with the `std::int64_t` getters. A key with bit 31 set therefore matches the getters while its upper 32 bits are zero; getter keys below 0 or at/above 2^32 can never be reset or consumed, which is consistent because the store cannot hold them.
- [Initial values](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - firmware 13.42 disassembly (sha256 591b1205…) shows the module forwarding `sceRegMgrGetIntInitVal` as (key, &result, command 7) to the regmgr call at 0x26b0 and returning its result: the factory defaults live in the system registry, the module has no table or zero fallback of its own, and for most keys the default is not 0. GetIntInitVal reads only the key and output registers (`edi`, `rsi`); the Str and Bin shapes are unverified. Zero/empty defaults previously returned here were invented and are reverted; the three retain `NotImplemented_nid_no_patch` until a defaults source exists. `sceRegMgrSrvGetQAFforReg` (`wQlp94zzrWg`) stays implemented, returning 0 as the retail default after the [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) PS4 HLE, unmeasured on PS5.
- [Getter length edges](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - `sceRegMgrGetStr` requires a length strictly greater than the stored size (`len == size` returns `0x80820004`) where the [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) PS4 GetStr succeeds, and `sceRegMgrGetBin` errors when the buffer is smaller than the stored size where voldemort copies the prefix; both divergences are accepted because the getters' provenance is the prototype-only PS5 regmgr.h, not voldemort.
- [Change tracking](../../../core/libs/prx/libSceRegMgr/Export.cpp) (libSceRegMgr) - `sceRegMgrIsChange` (`xhrI4zhlBuA`, same NID in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/cf6eede3f6588d779664e01874638f09484324b7/libSceRegMgr.c)): the PS5 module forwards to a kernel regmgr call with command `0x30` and the consumer id, so change detection is decided in the kernel. The consumer whitelist (`0x0d010001..0x0d010020` bits of mask `0x8000808b` plus `0x0d890080`) and the 8-bit dirty mask implemented here are a host model taken from the [voldemort](https://github.com/exhaustiblewizard/voldemort/blob/master/src/libs/libRegMgr.cpp) PS4 HLE, not module-checked behaviour; with no store writer in this PR every query reports 0. Unmeasured on PS5.
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
