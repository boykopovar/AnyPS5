# libSceKeyboard

## change initialization, output length and reserved bytes remain unverified

- [sceKeyboardGetKey2Char](../../../core/libs/prx/libSceKeyboard/src/keyboard_impl.cpp): output length and reserved bytes remain unverified on a console; unconverted keys retain the host's length of 0. One title (PPSA15246) disassembled for this entry calls it after clearing the first 20 bytes of its output buffer (16-byte store 0x69942d, 32-bit store 0x699435; no size argument), uses a character only when the result is 0 and the first output byte is nonzero, and then reads the 16-bit value at offset 8. Which other output fields it reads and what its other arguments mean are not shown. Its checks run in firmware 9.20's order ([reading](https://github.com/boykopovar/AnyPS5/issues/21849#issuecomment-7074475968), disassembly only): initialization, the key code (`0x04` to `0xE7`), the arrangement (above 1 refused), a null output, then the handle.
