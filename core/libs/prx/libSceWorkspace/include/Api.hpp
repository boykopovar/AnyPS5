#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI module_start_nid_postfix(void);
int APS5_VABI sceWorkspaceWriteFile(void);
int APS5_VABI sceWorkspaceFinishWrite(void);
int APS5_VABI sceWorkspaceApplyUpdateForFiles(void);
int APS5_VABI sceWorkspaceApplyUpdateForAllFiles(void);
int APS5_VABI sceWorkspaceIsBlockedByDataTransfer(void);
int APS5_VABI sceWorkspaceMirrorBarrier(void);
int APS5_VABI sceWorkspaceGetNumberOfPendingUpdates(void);
int APS5_VABI module_stop_nid_postfix(void);
int APS5_VABI sceWorkspaceUnmount(void);
int APS5_VABI sceWorkspaceApplyUpdate(void);
int APS5_VABI sceWorkspaceDeletePath(void);
int APS5_VABI sceWorkspaceMount(void);
int APS5_VABI sceWorkspaceGetName(void);
int APS5_VABI sceWorkspaceCreateFile(void);
int APS5_VABI sceWorkspaceApplyUpdateForDirectory(void);
int APS5_VABI sceWorkspaceIsUpdatePending(void);
int APS5_VABI sceWorkspaceGetPendingUpdateEntries(void);

}
