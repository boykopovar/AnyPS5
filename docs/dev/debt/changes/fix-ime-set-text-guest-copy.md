# libSceIme

## add Functional

- [sceImeSetText](../../../core/libs/prx/libSceIme/Export.cpp) (libSceIme) - reads guest UTF-16 input up to `min(length, max_text_length)` or the first NUL unit via `GuestCopyIn_nid_no_patch`, preserving host `errno` and existing session text, caret and revision on failure and returning `0x80BC0031` (`ErrorInvalidAddress`) on unmapped or unreadable guest memory (returning 0 and clearing text when `length` is 0 with a non-null pointer). Console fault behaviour on bad memory is unmeasured.
