# libSceVideoOut

## change [ImeTextInput]

- [ImeTextInput](../../../core/libs/prx/libSceVideoOut/src/ImeTextInput.cpp) (libSceVideoOut) forwards SDL_TEXTEDITING and, with `SDL_HINT_IME_SUPPORT_EXTENDED_TEXT` set, SDL_TEXTEDITING_EXT for preedits longer than 32 bytes, with `start` and `length` read as counts of characters. `SDL_SetTextInputRect` is not called so the system candidate window is not placed at the field, and SDL's own text input, started when video starts, is left on outside dialogs. How each platform reports `start` and `length` (characters or UTF-16 units, cursor or selection), and whether each IME sends the extended event, is not measured. Closes when: measured on Linux (ibus and fcitx)
