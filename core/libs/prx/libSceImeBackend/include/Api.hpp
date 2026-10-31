#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceImeBackendParamInit(void);
int APS5_VABI sceImeBackendOpen(void);
int APS5_VABI sceImeBackendClose(void);
int APS5_VABI sceImeBackendReset(void);
int APS5_VABI sceImeBackendGetStatus(void);
int APS5_VABI sceImeBackendEnterCharacter(void);
int APS5_VABI sceImeBackendBackspaceCharacter(void);
int APS5_VABI sceImeBackendPartConfirm(void);
int APS5_VABI sceImeBackendAllConfirm(void);
int APS5_VABI sceImeBackendAllDeleteConvertString(void);
int APS5_VABI sceImeBackendConvertForward(void);
int APS5_VABI sceImeBackendConvertBackward(void);
int APS5_VABI sceImeBackendConvertCancel(void);
int APS5_VABI sceImeBackendGetString(void);
int APS5_VABI sceImeBackendGetConvertString(void);
int APS5_VABI sceImeBackendGetConfirmString(void);
int APS5_VABI sceImeBackendGetCaretIndex(void);
int APS5_VABI sceImeBackendModeCaretNext(void);
int APS5_VABI sceImeBackendModeCaretPrevious(void);
int APS5_VABI sceImeBackendGetCandidateList(void);
int APS5_VABI sceImeBackendGetCandidateListSize(void);
int APS5_VABI sceImeBackendGetCandidateSelect(void);
int APS5_VABI sceImeBackendGetConvertMode(void);
int APS5_VABI sceImeBackendTemporalConfirm(void);
int APS5_VABI sceImeBackendSetConvertMode(void);
int APS5_VABI sceImeBackendForTestFunction(void);
int APS5_VABI sceImeBackendDeleteCharacter(void);

}
