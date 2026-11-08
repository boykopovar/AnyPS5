#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSlimglCompositorSetInvisibleCanvasCommand();
int APS5_VABI sceSlimglCompositorSetMemoryCommand();
int APS5_VABI sceSlimglCompositorSetPostEventCommand();
int APS5_VABI sceSlimglCompositorWaitEndOfRendering();
int APS5_VABI sceSlimglCompositorWaitPostEvent();
int APS5_VABI sceSlimglIPCEnd();
int APS5_VABI sceSlimglIPCSetup();


int APS5_VABI libSceGLSlimVSHInventory_9iW854bwvmc_nid_no_patch();
int APS5_VABI libSceGLSlimVSHInventory_DVNKmWX1jOU_nid_no_patch();
int APS5_VABI libSceGLSlimVSHInventory_JjyNMbSAgLk_nid_no_patch();
int APS5_VABI libSceGLSlimVSHInventory_LLDLZ_minus_mQK_plus_Q_nid_no_patch();
int APS5_VABI libSceGLSlimVSHInventory_Yf0LATiXPsg_nid_no_patch();
int APS5_VABI libSceGLSlimVSHInventory_ZLvm5korFRA_nid_no_patch();
int APS5_VABI libSceGLSlimVSHInventory_qSuVNKHENqs_nid_no_patch();

int APS5_VABI glOrbisMapTextureResourceSCE_nid_postfix();

int APS5_VABI glOrbisTexImageCanvas2DSCE_nid_postfix();

int APS5_VABI glOrbisTexImageResourceSCE_nid_postfix();

int APS5_VABI glOrbisUnmapTextureResourceSCE_nid_postfix();

int APS5_VABI scePigletAllocateSystemMemoryEx();

int APS5_VABI scePigletAllocateVideoMemoryEx();

int APS5_VABI scePigletGetConfigurationVSH();

int APS5_VABI scePigletSetConfigurationVSH();

int APS5_VABI scePigletReleaseSystemMemoryEx();

int APS5_VABI scePigletReleaseVideoMemoryEx();

int APS5_VABI sceSlimglClientStartIPC();

int APS5_VABI sceSlimglCompositorAllocateIndex();

int APS5_VABI sceSlimglCompositorCreateIndirectRenderTarget();

int APS5_VABI sceSlimglCompositorDeleteIndirectRenderTarget();

int APS5_VABI sceSlimglCompositorFlush();

int APS5_VABI sceSlimglCompositorFlushWithRepeat();

int APS5_VABI sceSlimglCompositorGetCanvasHandle();

int APS5_VABI sceSlimglCompositorGetError();

int APS5_VABI sceSlimglCompositorReleaseIndex();

int APS5_VABI sceSlimglCompositorSetEndOfFrameCommand();

int APS5_VABI sceSlimglCompositorSetIndirectCanvasCommand();

int APS5_VABI sceSlimglCompositorSetIndirectCompositionCommand();

int APS5_VABI sceSlimglCompositorSetIndirectRenderTargetConfigCommand();

}
