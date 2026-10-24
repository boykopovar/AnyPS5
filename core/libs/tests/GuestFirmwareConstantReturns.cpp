#include "SceTypes.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceCompositorCreateIndirectRenderTarget(void);
int APS5_VABI sceCompositorDeleteIndirectRenderTarget(void);
int APS5_VABI sceCompositorSetIndirectCanvasCommand(void);
int APS5_VABI sceCompositorSetIndirectCompositionCommand(void);
int APS5_VABI sceCompositorSetIndirectRenderTargetConfigCommand(void);
int APS5_VABI sceFsErriCtl(void);
int APS5_VABI sceFsErriGetState(void);
int APS5_VABI sceHttpCacheDebugSetUrl(void);
int APS5_VABI scePlayReadyMemGetStats(void);
int APS5_VABI sceSdmaInitialize(void);
int APS5_VABI sceSdmaFinalize(void);
int APS5_VABI sceSdmaQueryWorkingBufferSize(void);
int APS5_VABI sceSdmaFlushAllWithEvent(void);
int APS5_VABI sceSdmaWaitForEvent(void);
int APS5_VABI CRYPTO_uninitAsymmetricKey(void);
int APS5_VABI VLONG_freeVlongQueue(void);
int APS5_VABI sceVisionManagerOpen(void);
int APS5_VABI sceVisionManagerClose(void);
int APS5_VABI sceVisionManagerRequestFindUser(void);
int APS5_VABI sceVisionManagerRequestFindUserInRegion(void);
int APS5_VABI sceVisionManagerRequestFindUserWithoutIdentify(void);
int APS5_VABI sceVisionManagerRequestRegisterUserInRegion(void);
int APS5_VABI sceVisionManagerGetResultOfFindUser(void);
int APS5_VABI sceVisionManagerGetResultOfFindUserInRegion(void);
int APS5_VABI sceVisionManagerGetResultOfFindUserWithoutIdentify(void);
int APS5_VABI sceVisionManagerGetResultOfRegisterUserInRegion(void);
int APS5_VABI sceVisionManagerLoadSettings(void);
int APS5_VABI sceDebugGetProcessEventCntlFlag(void);
int APS5_VABI sceDebugSetProcessEventCntlFlag(void);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

void CheckComposite() {
    constexpr int Unsupported = static_cast<int>(0x80d40006);
    Require(sceCompositorCreateIndirectRenderTarget() == Unsupported);
    Require(sceCompositorDeleteIndirectRenderTarget() == Unsupported);
    Require(sceCompositorSetIndirectCanvasCommand() == 0);
    Require(sceCompositorSetIndirectCompositionCommand() == 0);
    Require(sceCompositorSetIndirectRenderTargetConfigCommand() == 0);
}

void CheckFsInternal() {
    constexpr int KernelNotSupported = static_cast<int>(0x8002002d);
    Require(sceFsErriCtl() == KernelNotSupported);
    Require(sceFsErriGetState() == KernelNotSupported);
}

void CheckHttpCacheAndPlayReady() {
    Require(sceHttpCacheDebugSetUrl() == 0);
    Require(scePlayReadyMemGetStats() == static_cast<int>(0x80004001));
}

void CheckSdma() {
    constexpr int SdmaError = static_cast<int>(0x809900c9);
    Require(sceSdmaInitialize() == 0);
    Require(sceSdmaFinalize() == 0);
    Require(sceSdmaQueryWorkingBufferSize() == 0x8000);
    Require(sceSdmaFlushAllWithEvent() == SdmaError);
    Require(sceSdmaWaitForEvent() == SdmaError);
}

void CheckSsl() {
    Require(CRYPTO_uninitAsymmetricKey() == 0);
    Require(VLONG_freeVlongQueue() == 0);
}

void CheckVisionManager() {
    constexpr int VisionError = static_cast<int>(0x80e70004);
    Require(sceVisionManagerOpen() == VisionError);
    Require(sceVisionManagerClose() == VisionError);
    Require(sceVisionManagerRequestFindUser() == VisionError);
    Require(sceVisionManagerRequestFindUserInRegion() == VisionError);
    Require(sceVisionManagerRequestFindUserWithoutIdentify() == VisionError);
    Require(sceVisionManagerRequestRegisterUserInRegion() == VisionError);
    Require(sceVisionManagerGetResultOfFindUser() == VisionError);
    Require(sceVisionManagerGetResultOfFindUserInRegion() == VisionError);
    Require(sceVisionManagerGetResultOfFindUserWithoutIdentify() == VisionError);
    Require(sceVisionManagerGetResultOfRegisterUserInRegion() == VisionError);
    Require(sceVisionManagerLoadSettings() == 0);
}

void CheckDebugSyscore() {
    Require(sceDebugGetProcessEventCntlFlag() == 1);
    Require(sceDebugSetProcessEventCntlFlag() == 1);
}

}

int main() {
    CheckComposite();
    CheckFsInternal();
    CheckHttpCacheAndPlayReady();
    CheckSdma();
    CheckSsl();
    CheckVisionManager();
    CheckDebugSyscore();
    return 0;
}
