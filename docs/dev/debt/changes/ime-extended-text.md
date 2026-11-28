# libSceVideoOut

## change [ImeTextInput]

- [ImeTextInput](../../../core/libs/prx/libSceVideoOut/src/ImeTextInput.cpp) (libSceVideoOut) has unverified preedit offsets for non-BMP cursors, other Linux engines, Wayland, Windows and macOS. X11 IBus/libpinyin reports a BMP cursor with length -1; fcitx5 reports selection ranges or start 0, length -1, leaving character versus UTF-16 counts unsettled. `SDL_SetTextInputRect` is not called: `sceImeOpen` supplies a panel position and alignment, not a verified field rectangle, and `sceImeSetTextGeometry` discards field geometry. Candidate placement, including letterbox/pillarbox and HiDPI conversion, remains unimplemented. SDL text input stays on outside dialogs; gating against keyboard-to-pad bindings is unverified. Closes when: field geometry and non-BMP offsets are measured across Linux input methods, Windows and macOS

# libSceAgcDriver

## add Functional

- [KeyboardOverlay](../../../core/libs/prx/libSceAgcDriver/Execution/src/KeyboardOverlay.cpp) (libSceAgcDriver) has no CJK or supplementary-plane glyphs in its built-in font, so those characters render as fallback boxes even when the complete preedit reaches the keyboard view. Closes when: CJK and supplementary-plane preedits render with their own glyphs
