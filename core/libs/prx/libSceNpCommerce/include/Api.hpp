#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpCommerceDialogInitialize();
int APS5_VABI sceNpCommerceDialogOpen(const NpCommerceDialogParam* param);
int APS5_VABI sceNpCommerceDialogUpdateStatus(void);
int APS5_VABI sceNpCommerceDialogGetStatus(void);
int APS5_VABI sceNpCommerceDialogGetResult(NpCommerceDialogResult* result);
int APS5_VABI sceNpCommerceDialogClose(void);
int APS5_VABI sceNpCommerceDialogTerminate();
int APS5_VABI sceNpCommerceHidePsStoreIcon(void);
int APS5_VABI sceNpCommerceShowPsStoreIcon(int pos);
int APS5_VABI sceNpCommerceSetPsStoreIconLayout(int layout);
int APS5_VABI sceNpCommerceDialogOpen2(const NpCommerceDialogParam2* param);
int APS5_VABI sceNpCommerceDialogInitializeInternal();

}
