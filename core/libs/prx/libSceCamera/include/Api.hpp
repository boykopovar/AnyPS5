#pragma once

#include "SceTypes.hpp"

struct CameraFramePosition {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t xSize;
    std::uint32_t ySize;
};

struct CameraExposureGain {
    std::uint32_t exposureControl;
    std::uint32_t exposure;
    std::uint32_t gain;
    std::uint32_t mode;
};

struct CameraWhiteBalance {
    std::uint32_t whiteBalanceControl;
    std::uint32_t gainRed;
    std::uint32_t gainBlue;
    std::uint32_t gainGreen;
};

struct CameraGamma {
    std::uint32_t gammaControl;
    std::uint32_t value;
    std::uint8_t reserved[16];
};

struct CameraAttribute {
    std::uint32_t sizeThis;
    std::uint32_t channel;
    CameraFramePosition framePosition;
    CameraExposureGain exposureGain;
    CameraWhiteBalance whiteBalance;
    CameraGamma gamma;
    std::uint32_t saturation;
    std::uint32_t contrast;
    std::uint32_t sharpness;
    std::int32_t hue;
    std::uint32_t reserved[4];
};
static_assert(sizeof(CameraAttribute) == 112);

struct CameraAutoExposureGainTarget {
    std::uint32_t sizeThis;
    std::uint32_t target;
};

struct CameraConfigExtension {
    std::uint32_t formatLevel[4];
    std::uint32_t resolution;
    std::uint32_t framerate;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t reserved;
    void* baseOption;
};

struct CameraConfig {
    std::uint32_t sizeThis;
    std::uint32_t configType;
    CameraConfigExtension extension[2];
};
static_assert(sizeof(CameraConfig) == 104);

struct CameraDeviceInfo {
    std::uint32_t sizeThis;
    std::uint32_t infoRevision;
    std::uint32_t deviceRevision;
    std::uint32_t padding;
};

struct CameraGetCalibrationDataParameter {
    std::uint32_t size;
    std::uint32_t formatType;
    std::uint32_t functionType;
};

struct CameraStartParameter {
    std::uint32_t sizeThis;
    std::uint32_t formatLevel[2];
    void* startOption;
};

struct CameraFrameData {
    std::uint32_t sizeThis;
    std::uint32_t readMode;
    std::uint32_t framePosition[2][4][4];
    void* framePointerList[2][4];
    std::uint32_t frameSize[2][4];
    std::uint32_t status[2];
    std::uint8_t reserved[344];
};
static_assert(sizeof(CameraFrameData) == 584);

extern "C" {

int APS5_VABI sceCameraIsAttached(int32_t index);
int APS5_VABI sceCameraOpen(std::int32_t userId, std::int32_t type, std::int32_t index, void*);
int APS5_VABI sceCameraClose(std::int32_t handle);
int APS5_VABI sceCameraStart(std::int32_t handle, const CameraStartParameter* param);
int APS5_VABI sceCameraStop(std::int32_t handle);
int APS5_VABI sceCameraGetConfig(std::int32_t handle, CameraConfig* config);
int APS5_VABI sceCameraSetConfig(std::int32_t handle, const CameraConfig* config);
int APS5_VABI sceCameraSetAttribute(std::int32_t handle, const CameraAttribute* attribute);
int APS5_VABI sceCameraGetDeviceInfo(std::int32_t reserved, const CameraDeviceInfo* deviceInfo);
int APS5_VABI sceCameraGetFrameData(std::int32_t handle, CameraFrameData* frameData);
int APS5_VABI sceCameraIsValidFrameData(std::int32_t handle, const CameraFrameData* frameData);
int APS5_VABI sceCameraStopByHandle(std::int32_t handle);
int APS5_VABI sceCameraGetDeviceIDWithoutOpen(void);
int APS5_VABI sceCameraGetAttribute(std::int32_t handle, CameraAttribute* attribute);
int APS5_VABI sceCameraInitializeRegistryCalibData(void);
int APS5_VABI sceCamera2GetContrast(void);
int APS5_VABI sceCamera2Finalize(void);
int APS5_VABI sceCameraGetConnectedDeviceAndDriverMode(void);
int APS5_VABI sceCamera2IsAttached(void);
int APS5_VABI sceCamera2SetSharpness(void);
int APS5_VABI sceCameraSetTrackerMode(void);
int APS5_VABI sceCameraSetContrast(std::int32_t handle, std::uint32_t channel, std::uint32_t, const void* option);
int APS5_VABI sceCameraGetCalibDataForEve(void);
int APS5_VABI sceCameraGetCalibDataFromDevice(void);
int APS5_VABI sceCamera2SetContrast(void);
int APS5_VABI sceCameraIsConnectedForSystem(void);
int APS5_VABI sceCamera2SetExposureGain(void);
int APS5_VABI sceCameraSetRegister(void);
int APS5_VABI sceCameraSetVideoSyncInternal(void);
int APS5_VABI sceCameraSetConfigInternal(std::int32_t handle, const CameraConfig* config);
int APS5_VABI sceCameraSetAttributeInternal(void);
int APS5_VABI sceCamera2SetWhiteBalance(void);
int APS5_VABI sceCameraGetMmapConnectedCount(std::uint32_t* count);
int APS5_VABI sceCamera2GetSaturation(void);
int APS5_VABI sceCameraGetDeviceConfig(std::int32_t handle, CameraConfig* config);
int APS5_VABI sceCameraAudioOpen(void);
int APS5_VABI sceCamera2GetAutoExposureGain(void);
int APS5_VABI sceCameraGetCalibDataFromDeviceForEve(void);
int APS5_VABI sceCameraGetWhiteBalance(std::int32_t handle, std::uint32_t channel, CameraWhiteBalance* whiteBalance, void* option);
int APS5_VABI sceCameraGetVrCaptureInfo(void);
int APS5_VABI sceCameraSetCalibDataForEve(void);
int APS5_VABI sceCamera2GetAttribute(void);
int APS5_VABI sceCameraSetLensCorrection(std::int32_t handle, std::uint32_t channel, std::uint32_t enable, const void* option);
int APS5_VABI sceCameraSetCalibData(void);
int APS5_VABI sceCameraSetProcessFocusByHandle(void);
int APS5_VABI sceCamera2SetConfig(void);
int APS5_VABI sceCameraGetExposureGain(std::int32_t handle, std::uint32_t channel, CameraExposureGain* exposureGain, void* option);
int APS5_VABI sceCameraSetSharpness(std::int32_t handle, std::uint32_t channel, std::uint32_t sharpness, const void* option);
int APS5_VABI sceCamera2Initialize(void);
int APS5_VABI sceCameraSetHue(std::int32_t handle, std::uint32_t channel, std::int32_t, const void* option);
int APS5_VABI sceCameraAccGetData(void);
int APS5_VABI sceCamera2SetAttribute(void);
int APS5_VABI sceCameraGetCalibrationData(const CameraGetCalibrationDataParameter* param, void* calibrationData);
int APS5_VABI sceCameraGetSaturation(std::int32_t handle, std::uint32_t channel, std::uint32_t* saturation, void* option);
int APS5_VABI sceCamera2GetHue(void);
int APS5_VABI sceCamera2Stop(void);
int APS5_VABI sceCamera2SetSaturation(void);
int APS5_VABI sceCamera2GetAutoWhiteBalance(void);
int APS5_VABI sceCameraAudioClose(void);
int APS5_VABI sceCameraGetProductInfo(void* productInfo);
int APS5_VABI sceCameraGetGamma(std::int32_t handle, std::uint32_t channel, CameraGamma* gamma, void* option);
int APS5_VABI sceCameraGetLensCorrection(std::int32_t handle, std::uint32_t channel, std::uint32_t* enable, void* option);
int APS5_VABI sceCameraChangeAppModuleState(void);
int APS5_VABI sceCamera2GetFrameData(void);
int APS5_VABI sceCameraGetContrast(std::int32_t handle, std::uint32_t channel, std::uint32_t* contrast, void* option);
int APS5_VABI sceCameraSetSaturation(std::int32_t handle, std::uint32_t channel, std::uint32_t, const void* option);
int APS5_VABI sceCameraGetSharpness(std::int32_t handle, std::uint32_t channel, std::uint32_t* sharpness, void* option);
int APS5_VABI sceCamera2GetExposureGain(void);
int APS5_VABI sceCameraStartByHandle(std::int32_t handle, const CameraStartParameter* param);
int APS5_VABI sceCamera2Open(void);
int APS5_VABI sceCamera2Start(void);
int APS5_VABI sceCameraSetLensCorrectionInternal(std::int32_t handle, std::uint32_t channel, std::uint32_t enable, const void* option);
int APS5_VABI sceCameraOpenByModuleId(void);
int APS5_VABI sceCamera2GetCalibrationData(void);
int APS5_VABI sceCameraAudioGetData(void* data);
int APS5_VABI sceCamera2SetHue(void);
int APS5_VABI sceCamera2GetGamma(void);
int APS5_VABI sceCameraGetCalibData(void);
int APS5_VABI sceCameraWaitForEveReady(void);
int APS5_VABI sceCameraGetRegistryInfo(void* registryInfo);
int APS5_VABI sceCameraAudioGetData2(void* data);
int APS5_VABI sceCameraSetAppModuleFocus(void);
int APS5_VABI sceCameraSetDefectivePixelCancellation(std::int32_t handle, std::uint32_t channel, std::uint32_t enable, const void* option);
int APS5_VABI sceCameraGetDeviceID(void);
int APS5_VABI sceCameraSetForceActivate(void);
int APS5_VABI sceCameraSetWhiteBalance(std::int32_t handle, std::uint32_t channel, const CameraWhiteBalance* whiteBalance, const void* option);
int APS5_VABI sceCamera2GetFieldOfView(void);
int APS5_VABI sceCamera2IsValidFrameData(void);
int APS5_VABI sceCameraSetProcessFocus(void);
int APS5_VABI sceCameraSetGamma(std::int32_t handle, std::uint32_t channel, const CameraGamma* gamma, const void* option);
int APS5_VABI sceCamera2GetConfig(void);
int APS5_VABI sceCamera2GetWhiteBalance(void);
int APS5_VABI sceCameraGetDeviceConfigWithoutHandle(CameraConfig* config);
int APS5_VABI sceCameraDeviceOpen(void);
int APS5_VABI sceCameraSetUacModeInternal(void);
int APS5_VABI sceCameraGetAutoExposureGain(std::int32_t handle, std::uint32_t channel, std::uint32_t* enable, CameraAutoExposureGainTarget* option);
int APS5_VABI sceCameraGetRegister(void);
int APS5_VABI sceCamera2SetGamma(void);
int APS5_VABI sceCameraRemoveAppModuleFocus(void);
int APS5_VABI sceCameraGetAutoWhiteBalance(std::int32_t handle, std::uint32_t channel, std::uint32_t* enable, void* option);
int APS5_VABI sceCameraGetDefectivePixelCancellation(std::int32_t handle, std::uint32_t channel, std::uint32_t* enable, void* option);
int APS5_VABI sceCamera2Close(void);
int APS5_VABI sceCamera2SetVideoSync(void);
int APS5_VABI sceCameraWaitForLukeReady(void);
int APS5_VABI sceCameraSetDefectivePixelCancellationInternal(std::int32_t handle, std::uint32_t channel, std::uint32_t enable, const void* option);
int APS5_VABI sceCamera2GetSharpness(void);
int APS5_VABI sceCamera2SetAutoWhiteBalance(void);
int APS5_VABI sceCameraSetDebugStop(void);
int APS5_VABI sceCamera2SetAutoExposureGain(void);
int APS5_VABI sceCameraIsConfigChangeDone(void);
int APS5_VABI sceCameraSetExposureGain(std::int32_t handle, std::uint32_t channel, const CameraExposureGain* exposureGain, const void* option);
int APS5_VABI sceCameraSetVideoSync(void);
int APS5_VABI sceCameraAudioReset(void);
int APS5_VABI sceCameraSetAutoExposureGain(std::int32_t handle, std::uint32_t channel, std::uint32_t, const CameraAutoExposureGainTarget* option);
int APS5_VABI sceCameraSetAutoWhiteBalance(std::int32_t handle, std::uint32_t channel, std::uint32_t, const void* option);
int APS5_VABI sceCameraGetHue(std::int32_t handle, std::uint32_t channel, std::int32_t* hue, void* option);
int APS5_VABI sceCameraCloseByHandle(std::int32_t handle);

}
