# libSceMouse technical debt

### Unknown function info

- [libSceMouse additional exports](../../../core/libs/prx/libSceMouse/InventoryExports.cpp) - these name/NID pairs are present in the 13.60 export inventory. Parameter lists, return types and behavior are unverified. Parameterless `int` declarations are import-resolution placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `sceMouseDisconnectAll` | `8fjFFh4zZXU` |
| `sceMouseExtensionGetCursor` | `UBO+tFHvcyE` |
| `sceMouseExtensionRead` | `doRbAW89lZE` |
| `sceMouseExtensionReadState` | `CvUnq1gsWMg` |
| `sceMouseExtensionScalePosition` | `GE+ZU2Y9pEs` |
| `sceMouseExtensionSetConfig` | `T4BHUjH773k` |
| `sceMouseExtensionSetCursor` | `2KVa2-HqIRs` |
| `sceMouseGetHandle` | `P+9UQOe1F2g` |
| `sceMouseVirtualDeviceAddDevice` | `JB6ycme-yJ8` |
| `sceMouseVirtualDeviceDeleteDevice` | `HN2s86AOUiM` |
| `sceMouseVirtualDeviceGetCursor` | `rdrYJpbUJyA` |

- [sceMouseDisconnectAll, sceMouseVirtualDeviceAddDevice, sceMouseVirtualDeviceDeleteDevice and sceMouseVirtualDeviceGetCursor](../../../core/libs/prx/libSceMouse/InventoryExports.cpp) - the names and NIDs are recorded in the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/e495d3cf24aea17cefda59decdfda9e3e4e3e5ff/data/firmware-13.60/libscemouse-acdd5b5e23.jsonl). Signatures and behavior remain unknown; the parameterless `int` declarations only resolve imports and every call throws. Not measured on a console.

- [sceMouseConnectPort, sceMouseDebugGetDeviceId, sceMouseDeviceOpen, sceMouseDisconnectDevice, sceMouseDisconnectPort, sceMouseGetDeviceInfo, sceMouseMbusInit, sceMouseSetHandType, sceMouseSetPointerSpeed and sceMouseSetProcessPrivilege](../../../core/libs/prx/libSceMouse/Export.cpp) (libSceMouse) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceMouse.c); the no-argument prototypes follow [shadPS4's PS4 stubs](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/mouse/mouse.cpp#L34-L146), not measured on a console. These declarations have no parameters solely to resolve known imports and make no ABI claim.

### Functional

- [Publish](../../../core/libs/prx/libSceMouse/src/mouse_impl.cpp) (libSceMouse) - consecutive motion-only reports with the same buttons are summed into the newest unread entry, so a title that reads a few entries per frame gets current motion instead of a full queue of old reports. Reports with no motion and no button, wheel, tilt or connection change are dropped. Per-report granularity and the console's own report rate are not reproduced

- [sceMouseResetAbsolutePosition, sceMouseSetAbsoluteVirtualScreenSize, sceMouseSetAbsoluteHighPrecisionMode, sceMouseReadAbsolutePosition](../../../core/libs/prx/libSceMouse/src/mouse_impl.cpp) (libSceMouse) - follow firmware 13.42 [run natively](https://github.com/boykopovar/AnyPS5/issues/11477#issuecomment-6502712241) with the driver stubbed: checks and order, screen sizes and rescale, the mode byte, the curve with its fraction, the 40-byte record. Host choices, after [XatiX's title-tested version](https://github.com/boykopovar/AnyPS5/issues/11477#issuecomment-6503782633): each host event is one sample with unscaled counts, in its own queue, so `sceMouseRead`'s motion merging does not feed the curve. A read takes all pending samples; past `num`, same-button neighbours share a record, each still moving the pointer, and the last `num` records are kept. Speed is taken as 4, the hand swap and flagged bit never apply, and a disconnected mouse stands in for a zero driver status
