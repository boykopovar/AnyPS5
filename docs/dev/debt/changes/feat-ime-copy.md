# libSceIme

## add Functional

- [the IME clipboard copy](../../../core/libs/prx/libSceIme/Export.cpp) (libSceIme) - only with `ANYPS5_IME_COPY=1` and not on Apple builds; the host copy is made by libSceVideoOut through SDL on the thread that calls SDL_SetClipboardText (on Apple, the main thread), and a copy the host refuses is logged to stderr. No console behaviour was measured: the copy of the confirmed text is an AnyPS5 convenience, decided by the project, and it is not part of the IME contract.
