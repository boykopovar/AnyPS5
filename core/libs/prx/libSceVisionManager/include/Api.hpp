#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVisionManagerApplyCalibrateHmdResult(void* argument1);
int APS5_VABI sceVisionManagerCancelRequest();
int APS5_VABI sceVisionManagerClose();
int APS5_VABI sceVisionManagerFinalize();
int APS5_VABI sceVisionManagerGenerateRegisterUserData(void* argument1, void* argument2);
int APS5_VABI sceVisionManagerGenerateRegisterUserDataFinalize();
int APS5_VABI sceVisionManagerGenerateRegisterUserDataInitialize();
int APS5_VABI sceVisionManagerGetCalibrateTrackingLedInfo();
int APS5_VABI sceVisionManagerGetCalibrateTrackingLedNecessity(void* argument1, void* argument2);
int APS5_VABI sceVisionManagerGetHitCountOfRegisterUserData(void* argument1, void* argument2, std::uint64_t argument3, void* argument4);
int APS5_VABI sceVisionManagerGetLibraryVersion(std::uint64_t* version);
int APS5_VABI sceVisionManagerGetNumOfRegisterUserDataSlot(std::uint64_t version, std::uint32_t* count);
int APS5_VABI sceVisionManagerGetRegisterUserDataSize();
int APS5_VABI sceVisionManagerGetRegisterUserDataVersion(void* argument1, void* argument2, void* argument3);
int APS5_VABI sceVisionManagerGetResultOfCalibrateHmd();
int APS5_VABI sceVisionManagerGetResultOfCalibrateStereoCamera(void* argument1, void* argument2);
int APS5_VABI sceVisionManagerGetResultOfCalibrateTrackingLed();
int APS5_VABI sceVisionManagerGetResultOfFindFace(void* argument1, void* argument2, void* argument3, void* argument4);
int APS5_VABI sceVisionManagerGetResultOfFindPad(void* context, void* request, void* result);
int APS5_VABI sceVisionManagerGetResultOfFindPadCorrespondRegion(void* context, void* request, void* result);
int APS5_VABI sceVisionManagerGetResultOfFindUser();
int APS5_VABI sceVisionManagerGetResultOfFindUserInRegion();
int APS5_VABI sceVisionManagerGetResultOfFindUserWithoutIdentify();
int APS5_VABI sceVisionManagerGetResultOfRegisterUserInRegion();
int APS5_VABI sceVisionManagerGetStoreCameraImgYPtr();
int APS5_VABI sceVisionManagerGetWorkingMemorySize(std::uint32_t* first, std::uint32_t* second, std::uint32_t* third);
int APS5_VABI sceVisionManagerInitialize();
int APS5_VABI sceVisionManagerLoadSettings();
int APS5_VABI sceVisionManagerOpen();
int APS5_VABI sceVisionManagerRegistryClear();
int APS5_VABI sceVisionManagerRequestCalibrateHmd();
int APS5_VABI sceVisionManagerRequestCalibrateStereoCamera();
int APS5_VABI sceVisionManagerRequestCalibrateTrackingLed();
int APS5_VABI sceVisionManagerRequestClose();
int APS5_VABI sceVisionManagerRequestFindFace();
int APS5_VABI sceVisionManagerRequestFindPad(void* context, const std::int32_t* parameter);
int APS5_VABI sceVisionManagerRequestFindPadCorrespondRegion(void* context, const std::int32_t* parameter, std::int32_t region, void* result);
int APS5_VABI sceVisionManagerRequestFindUser();
int APS5_VABI sceVisionManagerRequestFindUserInRegion();
int APS5_VABI sceVisionManagerRequestFindUserWithoutIdentify();
int APS5_VABI sceVisionManagerRequestHeadDetection();
int APS5_VABI sceVisionManagerRequestInitializeRegisterUserData(void* argument1);
int APS5_VABI sceVisionManagerRequestOpen(void* argument1);
int APS5_VABI sceVisionManagerRequestPadTracking(void* context, const std::int32_t* parameter);
int APS5_VABI sceVisionManagerRequestRecogUtility();
int APS5_VABI sceVisionManagerRequestRegisterUserInRegion();
int APS5_VABI sceVisionManagerRequestStoreCameraImgY();
int APS5_VABI sceVisionManagerSetCallbackForUpdateCalibrateHmdResult(void* argument1);
int APS5_VABI sceVisionManagerSetCallbackForUpdateCalibrateStereoCameraResult(void* argument1);
int APS5_VABI sceVisionManagerSetCallbackForUpdateCalibrateTrackingLedResult(void* argument1);
int APS5_VABI sceVisionManagerSetCallbackForUpdateCameraFrame();
int APS5_VABI sceVisionManagerSetCallbackForUpdateCtrlCore();
int APS5_VABI sceVisionManagerSetCallbackForUpdateRecog();
int APS5_VABI sceVisionManagerSetCallbackForUpdateRunningStatus(void* argument1, void* argument2);
int APS5_VABI sceVisionManagerSetCallbackForUpdateStoreFrame();
int APS5_VABI sceVisionManagerWaitResponse();

}
