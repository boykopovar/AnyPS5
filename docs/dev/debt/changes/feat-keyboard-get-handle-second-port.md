# libSceKeyboard

## change [Keyboard handle query]

- [Keyboard handle query](../../../core/libs/prx/libSceKeyboard/src/keyboard_impl.cpp) (libSceKeyboard) - `sceKeyboardGetHandle` queries the handle for the open keyboard assigned to `userId` on primary port (`type == 0`, `index == 0`), returning `KEYBOARD_ERROR_NO_HANDLE` (`0x80da0008`) if unassigned or if type/index differ from default port 0; for `index == 1` it returns handle 2 while that user has the second port open (entry below). Both the error constant `0x80da0008` and the type/index constraint are inferred from user service conventions and remain unmeasured on a retail console.

## change [sceKeyboardOpen with index 1]

- [sceKeyboardOpen with index 1](../../../core/libs/prx/libSceKeyboard/src/keyboard_impl.cpp): what the console's second keyboard port answers is unverified, and so is the range of `index`; the library's code was not read. PPSA15508 opens index 0 and then index 1 for the same user, with type 0 and a null parameter, and asserts unless each result is above 0 (call sites `0xd76403` and `0xd76427`). Index 1 gives a second handle, 2, for a port with no keyboard: `sceKeyboardRead` returns no samples, `sceKeyboardReadState` a disconnected sample of length 1, `sceKeyboardGetConnection` false, `sceKeyboardDebugGetDeviceId` `INVALID_HANDLE` and `sceKeyboardGetHandle` that handle for its user; the host keyboard stays on index 0. Any other index keeps `INVALID_ARG`.
