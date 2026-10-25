#include "prx/libSceCamera/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_CAMERA_ERROR_PARAM = static_cast<int>(0x802E0000);

extern "C" {

int APS5_VABI sceCameraIsAttached(int32_t index) {
    if (index != 0) return SCE_CAMERA_ERROR_PARAM;
    return 0;
}

int APS5_VABI sceCameraOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraStop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetAttribute(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetDeviceInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetFrameData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraIsValidFrameData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraStopByHandle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetDeviceIDWithoutOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetAttribute(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraInitializeRegistryCalibData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetContrast(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2Finalize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetConnectedDeviceAndDriverMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2IsAttached(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetSharpness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetTrackerMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetContrast(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetCalibDataForEve(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetCalibDataFromDevice(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetContrast(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraIsConnectedForSystem(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetExposureGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetRegister(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetVideoSyncInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetConfigInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetAttributeInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetWhiteBalance(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetMmapConnectedCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetSaturation(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetDeviceConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraAudioOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetAutoExposureGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetCalibDataFromDeviceForEve(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetWhiteBalance(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetVrCaptureInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetCalibDataForEve(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetAttribute(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetLensCorrection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetCalibData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetProcessFocusByHandle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetExposureGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetSharpness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2Initialize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetHue(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraAccGetData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetAttribute(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetCalibrationData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetSaturation(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetHue(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2Stop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetSaturation(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetAutoWhiteBalance(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraAudioClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetProductInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetGamma(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetLensCorrection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraChangeAppModuleState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetFrameData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetContrast(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetSaturation(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetSharpness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetExposureGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraStartByHandle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2Open(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2Start(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetLensCorrectionInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraOpenByModuleId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetCalibrationData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraAudioGetData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetHue(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetGamma(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetCalibData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraWaitForEveReady(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetRegistryInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraAudioGetData2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetAppModuleFocus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetDefectivePixelCancellation(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetDeviceID(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetForceActivate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetWhiteBalance(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetFieldOfView(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2IsValidFrameData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetProcessFocus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetGamma(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetWhiteBalance(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetDeviceConfigWithoutHandle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraDeviceOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetUacModeInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetAutoExposureGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetRegister(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetGamma(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraRemoveAppModuleFocus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetAutoWhiteBalance(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetDefectivePixelCancellation(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2Close(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetVideoSync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraWaitForLukeReady(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetDefectivePixelCancellationInternal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2GetSharpness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetAutoWhiteBalance(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetDebugStop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCamera2SetAutoExposureGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraIsConfigChangeDone(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetExposureGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetVideoSync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraAudioReset(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetAutoExposureGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraSetAutoWhiteBalance(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraGetHue(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCameraCloseByHandle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
