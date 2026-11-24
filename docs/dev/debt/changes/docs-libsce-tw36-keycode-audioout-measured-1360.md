# libSceConvertKeycode

## change [sceConvertKeycodeGetCharacter, sceConvertKeycodeGetCharacterFromKeyboardData, sceConvertKeycodeGetVirtualKeycode](../../../core/libs/prx/libSceConvertKeycode/Export.cpp)

- [sceConvertKeycodeGetCharacter, sceConvertKeycodeGetCharacterFromKeyboardData, sceConvertKeycodeGetVirtualKeycode](../../../core/libs/prx/libSceConvertKeycode/Export.cpp) - returning `0x80BC0031` for unwritable output pointers is a host-safety fallback; measured on a PS5 Pro payload process (13.60): `sceConvertKeycodeGetCharacter` writing to an unwritable non-null output pointer faults the calling process - the module stores through it directly - so the returned error code here deviates from the console on purpose.

## change [sceConvertKeycodeGetCharacter](../../../core/libs/prx/libSceConvertKeycode/Export.cpp): a PS5 payload process

- [sceConvertKeycodeGetCharacter](../../../core/libs/prx/libSceConvertKeycode/Export.cpp): a PS5 payload process (firmware 11.60, reconfirmed on a PS5 Pro payload process on 13.60) returned `0x80BC001D` for layout 0 and a character for layouts 1 to 10, 31, 38 and 39 - the 13.60 session saw keycode 0x04 give `a` for layouts 1, 2, 31, 38 and 39 and `q` for layouts 9 and 10, layout 0 leaving its output untouched - so none of the SDK-version rules for 31 and up was seen on either firmware; whether a title process, with its SDK version, differs was not measured. Keycode 0x04 gave `a`, or `q` for layouts 9 and 10, and six keycodes gave the same characters for layouts 1 and 9 in six raw states; Shift, Caps Lock and Num Lock were not measured.

# libSceAudioOut

## change [sceAudioOutMasteringTerm](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Mastering.cpp) (libSceAudioOut)

- [sceAudioOutMasteringTerm](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Mastering.cpp) (libSceAudioOut) - returns 0 once and clears the state `sceAudioOutMasteringInit(0)` set; a Term without a matching Init returns `0x80260203`, as on a PS5 (firmware 11.60, payload process, after `sceAudioOutInit`). Measured on a PS5 Pro payload process (13.60) that never called `sceAudioOutInit`: the first Term answers `0x80260203`, `sceAudioOutMasteringInit(0)` fails there with `0x8026000f` and both following Terms keep answering `0x80260203` - mastering initialization needs an audio-out context the uninitialized payload process does not have, so the 11.60 answer is confirmed on 13.60 only for the no-state path. There is no mastering stage to tear down

## change [sceAudioOutSetMainOutput](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut)

- [sceAudioOutSetMainOutput](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - returns 0 and does nothing, as the console function does on firmware 12.70: [PS5-Audio-Passthrough-Research](https://github.com/sainsaji/PS5-Audio-Passthrough-Research/blob/cf3c921d79bf00db4847f31b68a7bd95a8037cd7/docs/FINDINGS.md#L25) reads that export as `xor eax,eax; ret`. The export reads no argument, so its parameters stay unknown; measured on a PS5 Pro payload process (13.60): three arbitrary arguments still give 0, agreeing with the 12.70 read
