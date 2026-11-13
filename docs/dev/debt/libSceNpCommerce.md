# libSceNpCommerce technical debt

### Functional

- [sceNpCommerceDialogOpen, sceNpCommerceDialogOpen2](../../../core/libs/prx/libSceNpCommerce/Export.cpp) - returning `0x80B8000D` (`COMMON_DIALOG_ERROR_ARG_NULL`) for an unmapped or unreadable parameter pointer is a host-safety fallback; whether console hardware faults or returns an argument error on unmapped memory is unmeasured.

### Silent stubs

- [sceNpCommerceDialogGetResult](../../../core/libs/prx/libSceNpCommerce/Export.cpp) (libSceNpCommerce) - Firmware 13.42 checks only for null and accesses the structure directly (read from disassembly, not measured on a console), so the console faults on an inaccessible non-null pointer; AnyPS5 returns COMMON_DIALOG_ERROR_INVALID_ARGUMENT instead.
- [sceNpCommerceDialogOpen, sceNpCommerceDialogOpen2](../../../core/libs/prx/libSceNpCommerce/Export.cpp) (libSceNpCommerce) - with `ANYPS5_PSN_DIALOGS=show`, a successful open shows AnyPS5's notice that the feature needs PlayStation Network; the console's own display without PlayStation Network is not measured, and the notice wording is AnyPS5's.
- [sceNpCommerceShowPsStoreIcon, sceNpCommerceHidePsStoreIcon, sceNpCommerceSetPsStoreIconLayout](../../../core/libs/prx/libSceNpCommerce/Export.cpp) (libSceNpCommerce) - return 0 and show, hide or lay out no icon. Of the 38 titles relinked here, four import them: Ghost of Tsushima (PPSA02225) sets layout 2 once and then shows the icon at position 0, and Borderlands 4 (PPSA01494), MXGP 26 (PPSA30839) and Super Meat Boy 3D (PPSA31162) pass a position read from a structure field and set no layout; each of the four calls `sceNpCommerceHidePsStoreIcon` once

### Unknown function info

- [sceNpCommerceDialogInitializeInternal](../../../core/libs/prx/libSceNpCommerce/Export.cpp) (libSceNpCommerce) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpCommerce.c); the no-parameter prototype follows [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/np/np_commerce/np_commerce.cpp#L61) and delegates to `sceNpCommerceDialogInitialize()`, sharing dialog status and returning `COMMON_DIALOG_ERROR_ALREADY_INITIALIZED` (0x80B80004) when already initialized. Any internal options, parameter structs, or console error returns remain unmeasured on hardware.
- [sceNpCommerceDialogOpen2](../../../core/libs/prx/libSceNpCommerce/Export.cpp): extended content combinations outside content mode 0 and content mode 1 with dialog mode 1 throw when otherwise valid. The native restriction reads process sysctl field 68; its relation to host process metadata is unresolved. Mode-specific meanings of the option and 64-bit argument remain unresolved.

- [sceNpCommerceDialogOpen](../../../core/libs/prx/libSceNpCommerce/Export.cpp): Product copying and service submission remain part of the cancelled UI stub.
