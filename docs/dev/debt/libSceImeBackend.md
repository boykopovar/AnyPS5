# libSceImeBackend technical debt

### Unknown function info

- [sceImeBackendParamInit, sceImeBackendOpen, sceImeBackendClose, sceImeBackendReset, sceImeBackendGetStatus, sceImeBackendEnterCharacter, sceImeBackendBackspaceCharacter, sceImeBackendPartConfirm, sceImeBackendAllConfirm, sceImeBackendAllDeleteConvertString, sceImeBackendConvertForward, sceImeBackendConvertBackward, sceImeBackendConvertCancel, sceImeBackendGetString, sceImeBackendGetConvertString, sceImeBackendGetConfirmString, sceImeBackendGetCaretIndex, sceImeBackendModeCaretNext, sceImeBackendModeCaretPrevious, sceImeBackendGetCandidateList, sceImeBackendGetCandidateListSize, sceImeBackendGetCandidateSelect](../../../core/libs/prx/libSceImeBackend/Export.cpp) (libSceImeBackend) - declared without parameters: their signatures and behaviour are not established, so every call throws. PPSA05110 imports them and does not load without them.
