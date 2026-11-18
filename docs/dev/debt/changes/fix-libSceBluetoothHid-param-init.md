# libSceBluetoothHid

## change [sceBluetoothHidParamInitialize, sceBluetoothHidThreadParamInitialize, sceBluetoothHidRegisterCallback]

- [sceBluetoothHidParamInitialize, sceBluetoothHidThreadParamInitialize, sceBluetoothHidRegisterCallback](../../../core/libs/prx/libSceBluetoothHid/Export.cpp) - validates writability and readability of parameter pointers upfront with `GuestWritable_nid_no_patch` and `GuestCopyIn_nid_no_patch`, returning `0x81540001` (`InvalidArgument`) while preserving host errno; whether console hardware faults or returns an argument error on unmapped memory is unmeasured.
