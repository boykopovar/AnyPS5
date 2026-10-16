# libSceConvertKeycode technical debt

### Unknown function info

- [sceConvertKeycodeGetCharacter](../../../core/libs/prx/libSceConvertKeycode/Export.cpp) (libSceConvertKeycode) - the signature `(uint16_t keycode, uint32_t, uint32_t keyboardType, char16_t* character)` is inferred from PPSA12361, which passes a 16-bit keycode, 0, the type from `sceConvertKeycodeGetImeKeyboardType` and a pointer it reads one UTF-16 unit from when the result is 0. The meaning of the second argument is unknown. PRAGMATA (PPSA02530, #5726) and Onimusha: Way of the Sword (PPSA27836) pass `0x4000000` as a constant: Onimusha calls it for every keycode from 0 to 0x8F to build a table of key names, upper-cases `a` to `z` itself and skips keycodes that fail or return 0, so that bit is ignored and the unshifted character is returned as for 0. Any other bit throws, and so does a keyboard type other than 0. The returned character for HID usage `0x64` has not been measured on a console; `\` is an unverified US-layout guess based on the Windows VK mapping, which does not establish the console result.

### Functional

- [sceConvertKeycodeGetCharacter](../../../core/libs/prx/libSceConvertKeycode/Export.cpp) (libSceConvertKeycode) takes the keycode as a USB HID keyboard usage and returns the unshifted character of the US layout for letters, digits, punctuation, space and the keypad. A keycode without a character returns 0 with the character 0; whether the console returns an error instead is not measured.
