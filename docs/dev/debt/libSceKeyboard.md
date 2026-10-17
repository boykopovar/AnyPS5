# libSceKeyboard technical debt

### Functional

- [sceKeyboardGetKey2Char](../../../core/libs/prx/libSceKeyboard/src/keyboard_impl.cpp) (libSceKeyboard) translates the 101-key (US) arrangement and the alphanumeric layer of the 106-key (Japanese) one, which follows the JIS layout of the HID usages: the Yen (0x89) and Ro (0x87) keys return `\` (0x5C) unshifted, as JIS X 0201 places the yen sign there; whether the console returns U+00A5 instead is unverified. With the Kana LED (bit 4, the HID position) set the 106-key arrangement throws, as the kana the console returns are unknown. Ctrl and Alt do not change the character.

### Unknown function info

- [sceKeyboardConnectPort, sceKeyboardDebugGetDeviceId, sceKeyboardDeviceOpen, sceKeyboardDisconnectAll, sceKeyboardDisconnectDevice, sceKeyboardDisconnectPort, sceKeyboardGetConnection, sceKeyboardGetDeviceInfo, sceKeyboardGetHandle, sceKeyboardMbusInit, sceKeyboardPadEmulateClose, sceKeyboardPadEmulateOpen, sceKeyboardSetProcessFocus, sceKeyboardSetProcessPrivilege](../../../core/libs/prx/libSceKeyboard/Export.cpp) (libSceKeyboard) - throwing stubs declared without parameters from the firmware export database (aerolib.csv); their guest signatures are unverified on hardware
