#include "prx/libSceShareUtility/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceShareUtilityAdvanceFromFileWithContentIdList() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceFromFileWithTitleIdList() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceGetServiceInfo() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceQueryServiceInfo() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceQueryServiceInfoA() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceResetPlayingContent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceSetPlayingContent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceSetUploadContentData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceValidateContents() {
    return static_cast<int>(0x81170001u);
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenu() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuBroadcast() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuDefault() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuForShareContent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityInitializeEx2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityOpenShareMenu() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityOpenShareMenuBroadcast() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityOpenShareMenuDefault() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityOpenShareMenuForShareContent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShareUtilityTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
