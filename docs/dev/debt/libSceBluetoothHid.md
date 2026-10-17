# libSceBluetoothHid technical debt

### Silent stubs

- [libSceBluetoothHid](../../../core/libs/prx/libSceBluetoothHid/Export.cpp) implements only the no-device path: initialization and callback and device registration succeed, the callback is never called because no device connects, and calls that need a connected device throw

### Unknown function info

- [libSceBluetoothHid](../../../core/libs/prx/libSceBluetoothHid/Export.cpp) (libSceBluetoothHid) - signatures taken from PPSA01317: `sceBluetoothHidInit()` and `sceBluetoothHidUnregisterCallback()` without arguments, `sceBluetoothHidRegisterCallback(callback, 0, param)` with a 16-byte param, and `sceBluetoothHidRegisterDevice` / `sceBluetoothHidUnregisterDevice(vendorId, productId)`; the report and device name exports are declared without parameters
- [sceBluetoothHidDebugGetVersion, sceBluetoothHidDisconnectDevice, sceBluetoothHidGetDeviceInfo, sceBluetoothHidGetFeatureReport, sceBluetoothHidGetReportDescriptor, sceBluetoothHidInterruptOutput, sceBluetoothHidParamInitialize, sceBluetoothHidSetFeatureReport, sceBluetoothHidThreadParamInitialize](../../../core/libs/prx/libSceBluetoothHid/Export.cpp) (libSceBluetoothHid) - throwing stubs declared without parameters from the firmware export database (aerolib.csv); their guest signatures are unverified on hardware
