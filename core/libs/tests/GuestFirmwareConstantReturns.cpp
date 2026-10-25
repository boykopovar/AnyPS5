#include "SceTypes.hpp"
#include "prx/libSceComposite/libSceComposite.hpp"
#include "prx/libSceFsInternalForVsh/libSceFsInternalForVsh.hpp"
#include "prx/libSceHttpCache/libSceHttpCache.hpp"
#include "prx/libScePlayReady/libScePlayReady.hpp"
#include "prx/libSceSdma/libSceSdma.hpp"
#include "prx/libSceSsl/libSceSsl.hpp"
#include "prx/libSceVisionManager/libSceVisionManager.hpp"
#include "prx/libmdbg_syscore/libmdbg_syscore.hpp"
#include <cstdlib>

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
