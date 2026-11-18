#include "prx/libSceWorkspace/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI module_start_nid_postfix(void) {
    return 0;
}

int APS5_VABI sceWorkspaceWriteFile(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWorkspaceFinishWrite(void) {
    return 0;
}

int APS5_VABI sceWorkspaceApplyUpdateForFiles(void) {
    return 0;
}

int APS5_VABI sceWorkspaceApplyUpdateForAllFiles(void) {
    return 0;
}

int APS5_VABI sceWorkspaceIsBlockedByDataTransfer(void) {
    return 0;
}

int APS5_VABI sceWorkspaceMirrorBarrier(void) {
    return 0;
}

int APS5_VABI sceWorkspaceGetNumberOfPendingUpdates(void) {
    return 0;
}

int APS5_VABI module_stop_nid_postfix(void) {
    return 0;
}

int APS5_VABI sceWorkspaceUnmount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWorkspaceApplyUpdate(void) {
    return 0;
}

int APS5_VABI sceWorkspaceDeletePath(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWorkspaceMount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWorkspaceGetName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWorkspaceCreateFile(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWorkspaceApplyUpdateForDirectory(void) {
    return 0;
}

int APS5_VABI sceWorkspaceIsUpdatePending(void) {
    return 0;
}

int APS5_VABI sceWorkspaceGetPendingUpdateEntries(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
