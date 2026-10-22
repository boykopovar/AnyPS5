# libSceCommonDialog technical debt

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
