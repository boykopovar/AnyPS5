# libSceIme

## change [sceImeGetPanelSize, sceImeGetPanelPositionAndForm, sceImeKeyboardGetResourceId, sceImeParamInit]

- [sceImeGetPanelSize, sceImeGetPanelPositionAndForm, sceImeKeyboardGetResourceId, sceImeParamInit](../../../core/libs/prx/libSceIme/Export.cpp) (libSceIme) have unmeasured console fault behaviour on bad memory. An unmapped or unwritable destination returns `0x80bc0031` (`ErrorInvalidAddress`), or returns early in `sceImeParamInit`, as a host-safety fallback. A concurrent mapping change can still leave `width` written if the `height` copy fails in `sceImeGetPanelSize`.
