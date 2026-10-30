# libSceNpCommerce technical debt

### Silent stubs

- [libSceNpCommerce](../../../core/libs/prx/libSceNpCommerce/Export.cpp) - the PS Store icon show/hide calls and `sceNpCommerceSetPsStoreIconLayout` do nothing

### Unknown function info

- [sceNpCommerceDialogInitializeInternal](../../../core/libs/prx/libSceNpCommerce/Export.cpp) (libSceNpCommerce) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpCommerce.c); the no-parameter prototype follows [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/np/np_commerce/np_commerce.cpp#L61) and delegates to `sceNpCommerceDialogInitialize()`, sharing dialog status and returning `COMMON_DIALOG_ERROR_ALREADY_INITIALIZED` (0x80B80004) when already initialized. Any internal options, parameter structs, or console error returns remain unmeasured on hardware.
- [sceNpCommerceDialogOpen2](../../../core/libs/prx/libSceNpCommerce/Export.cpp): extended content combinations outside content mode 0 and content mode 1 with dialog mode 1 throw when otherwise valid. The native restriction reads process sysctl field 68; its relation to host process metadata is unresolved. Mode-specific meanings of the option and 64-bit argument remain unresolved.

- [sceNpCommerceDialogOpen](../../../core/libs/prx/libSceNpCommerce/Export.cpp): Product copying and service submission remain part of the cancelled UI stub.
