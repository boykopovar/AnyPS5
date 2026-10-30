# libSceMouse technical debt

### Unknown function info

- [libSceMouse additional exports](../../../core/libs/prx/libSceMouse/InventoryExports.cpp) - these name/NID pairs are present in the 13.60 export inventory. Parameter lists, return types and behavior are unverified. Parameterless `int` declarations are import-resolution placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `sceMouseExtensionGetCursor` | `UBO+tFHvcyE` |
| `sceMouseExtensionRead` | `doRbAW89lZE` |
| `sceMouseExtensionReadState` | `CvUnq1gsWMg` |
| `sceMouseExtensionScalePosition` | `GE+ZU2Y9pEs` |
| `sceMouseExtensionSetConfig` | `T4BHUjH773k` |
| `sceMouseExtensionSetCursor` | `2KVa2-HqIRs` |
| `sceMouseGetHandle` | `P+9UQOe1F2g` |

- [sceMouseConnectPort, sceMouseDebugGetDeviceId, sceMouseDeviceOpen, sceMouseDisconnectDevice, sceMouseDisconnectPort, sceMouseGetDeviceInfo, sceMouseMbusInit, sceMouseSetHandType, sceMouseSetPointerSpeed and sceMouseSetProcessPrivilege](../../../core/libs/prx/libSceMouse/Export.cpp) (libSceMouse) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceMouse.c); the no-argument prototypes follow [shadPS4's PS4 stubs](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/mouse/mouse.cpp#L34-L146), not measured on a console. These declarations have no parameters solely to resolve known imports and make no ABI claim.
- [sceMouseReadAbsolutePosition, sceMouseSetAbsoluteHighPrecisionMode and sceMouseSetAbsoluteVirtualScreenSize](../../../core/libs/prx/libSceMouse/Export.cpp) (libSceMouse) - the names match unresolved NIDs reported by a title, but their signatures and behavior are not verified. These no-argument declarations only resolve imports; the exports throw if called.

### Functional

- [sceMouseResetAbsolutePosition](../../../core/libs/prx/libSceMouse/Export.cpp) (libSceMouse) - the position is not stored because the host reports relative motion only. Native input widths and validation are confirmed, including the default 1920 by 1080 bounds; other native coordinate modes and absolute reports remain unimplemented
