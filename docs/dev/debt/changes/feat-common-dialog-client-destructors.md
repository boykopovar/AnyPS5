# libSceCommonDialog

## add Functional

- [`sce::CommonDialogUtil::Client::~Client` (`D0`, `D1`, `D2`) and `getSelfAppId`](../../../core/libs/prx/libSceCommonDialog/InventoryExports.cpp) (libSceCommonDialog) - from the firmware 13.42 disassembly (#21860), `D1` and `D2` resolve to the same destructor, which skips implementation teardown when the pointer at `this+0x08` is null; only that null-implementation return is implemented (without rewriting the vtable at `this+0x00` or the module's current-client state, as no active client is modelled), while a non-null `this+0x08` throws through `NotImplemented_nid_no_patch` and an unreadable `this` throws `std::invalid_argument` preserving `errno` as a host-safety fallback. `D0` (`ud2` in firmware 13.42) and `getSelfAppId` (which queries `sceLncUtilGetAppStatus`) still throw.
