# libSceMouse technical debt

### Unknown function info

- [sceMouseConnectPort, sceMouseDebugGetDeviceId, sceMouseDeviceOpen, sceMouseDisconnectDevice, sceMouseDisconnectPort, sceMouseGetDeviceInfo, sceMouseMbusInit, sceMouseSetHandType, sceMouseSetPointerSpeed and sceMouseSetProcessPrivilege](../../../core/libs/prx/libSceMouse/Export.cpp) (libSceMouse) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceMouse.c); the no-argument prototypes follow [shadPS4's PS4 stubs](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/mouse/mouse.cpp#L34-L146), not measured on a console. These declarations have no parameters solely to resolve known imports and make no ABI claim.

### Functional

- [Publish](../../../core/libs/prx/libSceMouse/src/mouse_impl.cpp) (libSceMouse) - consecutive motion-only reports with the same buttons are summed into the newest unread entry, so a title that reads a few entries per frame gets current motion instead of a full queue of old reports. Reports with no motion and no button, wheel, tilt or connection change are dropped. Per-report granularity and the console's own report rate are not reproduced

- [sceMouseResetAbsolutePosition](../../../core/libs/prx/libSceMouse/Export.cpp) (libSceMouse) - the position is not stored because the host reports relative motion only. Native input widths and validation are confirmed, including the default 1920 by 1080 bounds; other native coordinate modes and absolute reports remain unimplemented
