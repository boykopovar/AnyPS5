# libSceBluetoothHid technical debt

### Silent stubs

- [libSceBluetoothHid](../../../core/libs/prx/libSceBluetoothHid/Export.cpp): the host still has no device backend or event worker. Registration retains only lifecycle state and never delivers the recovered 104-byte callback events. Custom worker scheduling throws; device ioctl failures and resource exhaustion are not modeled.

### Unknown function info

- [sceBluetoothHidGetDeviceName and sceBluetoothHidGetDeviceInfo](../../../core/libs/prx/libSceBluetoothHid/Export.cpp): output pointers are forwarded to device ioctls without accessing the buffers. Their output sizes and layouts remain unresolved. Report APIs use 64-bit device handles; device-info and disconnect APIs use 32-bit device IDs.
- [sceBluetoothHidDebugGetVersion](../../../core/libs/prx/libSceBluetoothHid/Export.cpp): returns a constant without reading arguments, so the supplied implementation does not establish whether the public prototype has unused parameters.
