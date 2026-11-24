#include "prx/libSceCamera/include/Api.hpp"
#include <cstdint>
#include <cerrno>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include "prx/libSceCamera/include/Camera.hpp"

namespace {

int CheckAudioDataBuffer(const void* data) {
    if (!data) return SCE_CAMERA_ERROR_PARAM;
    std::uint8_t probe = 0;
    const bool readable = GuestCopyIn_nid_no_patch(&probe, data, sizeof(probe));
    return readable ? 0 : SCE_CAMERA_ERROR_PARAM;
}

}

extern "C" {

int APS5_VABI sceCameraIsAttached(int32_t index) {
    if (index < 0 || index > 1) return SCE_CAMERA_ERROR_PARAM;
    return 0;
}

int APS5_VABI sceCameraGetDeviceIDWithoutOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraInitializeRegistryCalibData(void) {
    return SCE_CAMERA_ERROR_NOT_INIT;
}

int APS5_VABI sceCameraGetConnectedDeviceAndDriverMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetTrackerMode(void) {
    return SCE_CAMERA_ERROR_PARAM;
}

int APS5_VABI sceCameraGetCalibDataForEve(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraIsConnectedForSystem(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetRegister(void) {
    return SCE_CAMERA_ERROR_PARAM;
}

int APS5_VABI sceCameraSetVideoSyncInternal(void) {
    return SCE_CAMERA_ERROR_PARAM;
}

int APS5_VABI sceCameraSetAttributeInternal(void) {
    return 0;
}

int APS5_VABI sceCameraGetCalibDataFromDeviceForEve(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetVrCaptureInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetCalibDataForEve(void) {
    return SCE_CAMERA_ERROR_PARAM;
}

int APS5_VABI sceCameraSetCalibData(void) {
    return SCE_CAMERA_ERROR_NOT_INIT;
}

int APS5_VABI sceCameraSetProcessFocusByHandle(std::int32_t) {
    return 0;
}

int APS5_VABI sceCameraAccGetData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraAudioClose(void) {
    return 0;
}

int APS5_VABI sceCameraChangeAppModuleState(std::int32_t appModuleId) {
    if (appModuleId == 0) return SCE_CAMERA_ERROR_MBIS_ACCESS_DENIED;
    return SCE_CAMERA_ERROR_MBIS_FATAL;
}

int APS5_VABI sceCameraOpenByModuleId(std::int32_t) {
    return SCE_CAMERA_ERROR_PARAM;
}

int APS5_VABI sceCamera2GetCalibrationData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraAudioGetData(void* data) {
    return CheckAudioDataBuffer(data);
}

int APS5_VABI sceCameraGetCalibData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraWaitForEveReady(void) {
    return 0;
}

int APS5_VABI sceCameraAudioGetData2(void* data) {
    return CheckAudioDataBuffer(data);
}

int APS5_VABI sceCameraSetAppModuleFocus(std::int32_t, void*) {
    return SCE_CAMERA_ERROR_MBIS_ACCESS_DENIED;
}

int APS5_VABI sceCameraGetDeviceID(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetForceActivate(std::int32_t handle) {
    if (handle == 0 || handle < 0) return SCE_CAMERA_ERROR_PARAM;
    if (Camera::IsOpen(handle)) throw std::runtime_error("sceCameraSetForceActivate open handle");
    return SCE_CAMERA_ERROR_NOT_OPEN;
}

int APS5_VABI sceCameraSetProcessFocus(std::int32_t, void*) {
    return 0;
}

int APS5_VABI sceCameraDeviceOpen(void) {
    return 0;
}

int APS5_VABI sceCameraSetUacModeInternal(void) {
    return 0;
}

int APS5_VABI sceCameraGetRegister(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraRemoveAppModuleFocus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraWaitForLukeReady(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetDebugStop(void) {
    return SCE_CAMERA_ERROR_PARAM;
}

int APS5_VABI sceCameraIsConfigChangeDone(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetVideoSync(void) {
    return SCE_CAMERA_ERROR_PARAM;
}

int APS5_VABI sceCameraAudioReset(void) {
    return 0;
}
}
