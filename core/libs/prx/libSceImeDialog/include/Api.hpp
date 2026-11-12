#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceImeDialogAbort(void);
int APS5_VABI sceImeDialogGetPanelPositionAndForm(PositionAndForm* form);
int APS5_VABI sceImeDialogGetPanelSize(const Param* param, uint32_t* width, uint32_t* height);
int APS5_VABI sceImeDialogGetPanelSizeExtended(const Param* param, const ExtendedParam* extended, uint32_t* width, uint32_t* height);
int APS5_VABI sceImeDialogGetResult(Result* result);
int APS5_VABI sceImeDialogGetStatus(void);
int APS5_VABI sceImeDialogInit(const Param* param, const ExtendedParam* extended);
int APS5_VABI sceImeDialogTerm(void);
int APS5_VABI sceImeDialogForTestFunction();
int APS5_VABI sceImeDialogForceClose(void);
int APS5_VABI sceImeDialogGetCurrentStarState(int64_t state);
int APS5_VABI sceImeDialogInitInternal(const Param* param, const ExtendedParam* extended);
int APS5_VABI sceImeDialogSetPanelPosition(int32_t, int32_t);
int APS5_VABI sceImeDialogInitInternalWithSubInfo();

}
