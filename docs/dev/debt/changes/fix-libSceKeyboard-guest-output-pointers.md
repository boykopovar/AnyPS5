# libSceKeyboard

## add Functional

- [sceKeyboardGetConnection](../../../core/libs/prx/libSceKeyboard/src/keyboard_impl.cpp) (libSceKeyboard) - isConnected output pointer is written through GuestCopyOut_nid_no_patch with caller errno preserved; returning KEYBOARD_ERROR_INVALID_ARG on unwritable pointer is host safety fallback with console behavior unmeasured.
- [sceKeyboardDebugGetDeviceId](../../../core/libs/prx/libSceKeyboard/src/keyboard_impl.cpp) (libSceKeyboard) - deviceId output pointer is written through GuestCopyOut_nid_no_patch with caller errno preserved; returning KEYBOARD_ERROR_INVALID_ARG on unwritable pointer is host safety fallback with console behavior unmeasured.
- [sceKeyboardGetKey2Char](../../../core/libs/prx/libSceKeyboard/src/keyboard_impl.cpp) (libSceKeyboard) - charData output pointer is written through GuestCopyOut_nid_no_patch with caller errno preserved; returning KEYBOARD_ERROR_INVALID_ARG on unwritable pointer is host safety fallback with console behavior unmeasured.
