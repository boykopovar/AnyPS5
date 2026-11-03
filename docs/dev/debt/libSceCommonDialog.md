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

### Silent stubs

- [libSceCommonDialog](../../../core/libs/prx/libSceCommonDialog/Export.cpp)

### Unknown function info

- 14 additional pending imports in [InventoryExports.cpp](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) are listed in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCommonDialog.c). Their signatures and behavior are unverified; each uses the no-parameter unknown-signature convention and throws through `NotImplemented_nid_no_patch`, for import resolution only.

- `_ZN3sce16CommonDialogUtil12getSelfAppIdEv` — NID `2RdicdHhtGA` — [`sceCommonDialogPending_001_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZN3sce16CommonDialogUtil6Client11closeModuleEv` — NID `I+tdxsCap08` — [`sceCommonDialogPending_002_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZN3sce16CommonDialogUtil6Client11updateStateEv` — NID `v4+gzuTkv6k` — [`sceCommonDialogPending_003_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZN3sce16CommonDialogUtil6Client15launchCmnDialogEv` — NID `CwCzG0nnLg8` — [`sceCommonDialogPending_004_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZN3sce16CommonDialogUtil6ClientD0Ev` — NID `Ib1SMmbr07k` — [`sceCommonDialogPending_005_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZN3sce16CommonDialogUtil6ClientD1Ev` — NID `6TIMpGvsrC4` — [`sceCommonDialogPending_006_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZN3sce16CommonDialogUtil6ClientD2Ev` — NID `+UyKxWAnqIU` — [`sceCommonDialogPending_007_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZNK3sce16CommonDialogUtil6Client10isCloseReqEv` — NID `bUCx72-9f0g` — [`sceCommonDialogPending_008_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZNK3sce16CommonDialogUtil6Client13getFinishDataEPvm` — NID `xZtXq554Lbg` — [`sceCommonDialogPending_009_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZNK3sce16CommonDialogUtil6Client14getClientStateEv` — NID `C-EZ3PkhibQ` — [`sceCommonDialogPending_010_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZNK3sce16CommonDialogUtil6Client19isInitializedStatusEv` — NID `70niEKUAnZ0` — [`sceCommonDialogPending_011_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZNK3sce16CommonDialogUtil6Client8getAppIdEv` — NID `mdJgdwoM0Mo` — [`sceCommonDialogPending_012_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZNK3sce16CommonDialogUtil6Client8isFinishEv` — NID `87GekE1nowg` — [`sceCommonDialogPending_013_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)
- `_ZNK3sce16CommonDialogUtil6Client9getResultEv` — NID `6ljeTSi+fjs` — [`sceCommonDialogPending_014_nid_no_patch`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp)

### Functional

- [sceCommonDialogInitialize](../../../core/libs/prx/libSceCommonDialog/Export.cpp) (libSceCommonDialog) - returning `ALREADY_SYSTEM_INITIALIZED` (`0x80B80002`) from a repeated call is not measured on a console by the project. A third-party PS5 log ([obSCEne](https://github.com/project-oops/obSCEne/blob/67b4c529eb3c271184117e26bb54330531ef2cf6/reports/hardware/report-20261014-004800.log#L260-L261), firmware 12.40) records two calls, `0x0` and then `0x80b80002`; a third call is not covered. The SDK error names, shadPS4 (a PS4 emulator) and SharpProspero give the same value; PPSA09695 compares the result with `0x80b80002` (call site 0x5F7BD0) and PPSA06327 stops on `0x80b80004`. Closes when: measured on a PS5, including a third call.
- [sceCommonDialogIsUsed](../../../core/libs/prx/libSceCommonDialog/Export.cpp) (libSceCommonDialog) - reports whether a common dialog holds the system, like shadPS4's `g_isUsed`. Only [libSceInvitationDialog](libSceInvitationDialog.md) takes and frees it so far, through `CommonDialogAcquire_nid_no_patch` and `CommonDialogRelease_nid_no_patch`, so a dialog of another library that is in use is not seen. Not measured on a console.
