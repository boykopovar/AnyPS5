# libSceBluetoothHid technical debt

### Silent stubs

- [libSceBluetoothHid](../../../core/libs/prx/libSceBluetoothHid/Export.cpp): the host still has no device backend or event worker. Registration retains only lifecycle state and never delivers the recovered 104-byte callback events. Custom worker scheduling throws; device ioctl failures and resource exhaustion are not modeled.

### Unknown function info

- [sceBluetoothHidGetDeviceName and sceBluetoothHidGetDeviceInfo](../../../core/libs/prx/libSceBluetoothHid/Export.cpp): output pointers are forwarded to device ioctls without accessing the buffers. Their output sizes and layouts remain unresolved. Report APIs use 64-bit device handles; device-info and disconnect APIs use 32-bit device IDs.
- [sceBluetoothHidParamInitialize](../../../core/libs/prx/libSceBluetoothHid/Export.cpp) follows firmware 13.42's module as BunnyFl ran it natively ([comment](https://github.com/boykopovar/AnyPS5/pull/11124#issuecomment-6416053242); not a console, and neither the kernel side of `/dev/bt` nor the event thread's work is covered): it writes its second argument at offset 0 of the 16-byte block, clears the 32-bit word at offset 8, leaves bytes 12 to 15 and returns 0, and a null block returns `0x81540001`. [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/Bluetooth/SceBluetoothHid.cs#L21-L49) types that argument as a 64-bit value; it is the address of a `BluetoothHidThreadParam`, since `sceBluetoothHidRegisterCallback` reads the event thread's priority and affinity through it. The same run covers `sceBluetoothHidThreadParamInitialize`, still unimplemented here
- [sceBluetoothHidDebugGetVersion](../../../core/libs/prx/libSceBluetoothHid/Export.cpp): returns a constant without reading arguments, so the supplied implementation does not establish whether the public prototype has unused parameters.
