# libSceCommonDialog technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceCommonDialog/AdditionalExports.cpp) - 12 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. These NIDs are absent from the pinned public 3.20 catalogue. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `8q7icGBWIrA` |
| `Unknown` | `D-V35OhFeIM` |
| `Unknown` | `QXFsLON5QWw` |
| `Unknown` | `SDpCfY9uB0g` |
| `Unknown` | `Zaa6fhiKQDE` |
| `Unknown` | `aUS4PgJye98` |
| `Unknown` | `afLdI6i0lQw` |
| `Unknown` | `mVRnPerBcK0` |
| `Unknown` | `p9TTq4bLdFU` |
| `Unknown` | `q3-8WWCG2N0` |
| `Unknown` | `txNJzxX6yrA` |
| `Unknown` | `yxjgDvqUbGQ` |

### Unknown function info

- 14 additional pending imports in [InventoryExports.cpp](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) are listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCommonDialog.c). Their signatures and behavior are unverified; each uses the no-parameter unknown-signature convention and throws through `NotImplemented_nid_no_patch`, for import resolution only.

- [`_ZN3sce16CommonDialogUtil12getSelfAppIdEv`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `2RdicdHhtGA`
- [`_ZN3sce16CommonDialogUtil6Client11closeModuleEv`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `I+tdxsCap08`
- [`_ZN3sce16CommonDialogUtil6Client11updateStateEv`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `v4+gzuTkv6k`
- [`_ZN3sce16CommonDialogUtil6Client15launchCmnDialogEv`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `CwCzG0nnLg8`
- [`_ZN3sce16CommonDialogUtil6ClientD0Ev`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `Ib1SMmbr07k`
- [`_ZN3sce16CommonDialogUtil6ClientD1Ev`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `6TIMpGvsrC4`
- [`_ZN3sce16CommonDialogUtil6ClientD2Ev`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `+UyKxWAnqIU`
- [`_ZNK3sce16CommonDialogUtil6Client10isCloseReqEv`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `bUCx72-9f0g`
- [`_ZNK3sce16CommonDialogUtil6Client13getFinishDataEPvm`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `xZtXq554Lbg`
- [`_ZNK3sce16CommonDialogUtil6Client14getClientStateEv`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `C-EZ3PkhibQ`
- [`_ZNK3sce16CommonDialogUtil6Client19isInitializedStatusEv`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `70niEKUAnZ0`
- [`_ZNK3sce16CommonDialogUtil6Client8getAppIdEv`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `mdJgdwoM0Mo`
- [`_ZNK3sce16CommonDialogUtil6Client8isFinishEv`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `87GekE1nowg`
- [`_ZNK3sce16CommonDialogUtil6Client9getResultEv`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) — NID `6ljeTSi+fjs`

### Functional

- [sceCommonDialogInitialize](../../../core/libs/prx/libSceCommonDialog/Export.cpp) (libSceCommonDialog) - returning `ALREADY_SYSTEM_INITIALIZED` (`0x80B80002`) from a repeated call is not measured on a console by the project. A third-party PS5 log ([obSCEne](https://github.com/project-oops/obSCEne/blob/67b4c529eb3c271184117e26bb54330531ef2cf6/reports/hardware/report-20261014-004800.log#L260-L261), firmware 12.40) records two calls, `0x0` and then `0x80b80002`; a third call is not covered. The SDK error names, shadPS4 (a PS4 emulator) and SharpProspero give the same value; PPSA09695 compares the result with `0x80b80002` (call site 0x5F7BD0) and PPSA06327 stops on `0x80b80004`. Closes when: measured on a PS5, including a third call.
- [sceCommonDialogIsUsed](../../../core/libs/prx/libSceCommonDialog/Export.cpp) (libSceCommonDialog) - reports whether a common dialog holds the system, like shadPS4's `g_isUsed`. Only [libSceInvitationDialog](libSceInvitationDialog.md) takes and frees it so far, through `CommonDialogAcquire_nid_no_patch` and `CommonDialogRelease_nid_no_patch`, so a dialog of another library that is in use is not seen. Not measured on a console.
