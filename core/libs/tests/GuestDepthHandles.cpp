#include "prx/libSceDepth/libSceDepth.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "DEPTH HANDLES: %s\n", message);
    std::abort();
}

constexpr int ErrorInvalidHandle = static_cast<int>(0x80BB0005u);
constexpr int ResultCameraFactoryCalibrationFailure = static_cast<int>(0x80BBFFFFu);

}

int main() {
    constexpr std::array<int32_t, 7> handles{-2147483647 - 1, -1, 0, 1, 3, 4, 2147483647};
    for (const int32_t handle : handles) {
        Require(sceDepthDisableHeteroMode(handle) == ErrorInvalidHandle, "sceDepthDisableHeteroMode");
        Require(sceDepthEnableHeteroMode(handle) == ErrorInvalidHandle, "sceDepthEnableHeteroMode");
        Require(sceDepthEnableHeteroModeAndGenerateInformation(handle) == ErrorInvalidHandle, "sceDepthEnableHeteroModeAndGenerateInformation");
        Require(sceDepthGetSrParameter(handle) == ErrorInvalidHandle, "sceDepthGetSrParameter");
        Require(sceDepthHasCalibrationData(handle) == ErrorInvalidHandle, "sceDepthHasCalibrationData");
        Require(sceDepthHeadCandidateDetectorGetResults(handle) == ErrorInvalidHandle, "sceDepthHeadCandidateDetectorGetResults");
        Require(sceDepthLoadCalibrationData(handle) == ErrorInvalidHandle, "sceDepthLoadCalibrationData");
        Require(sceDepthSaveInternalBuffers(handle) == ErrorInvalidHandle, "sceDepthSaveInternalBuffers");
        Require(sceDepthSetPostFilterParameter(handle) == ErrorInvalidHandle, "sceDepthSetPostFilterParameter");
        Require(sceDepthSetPreFilterParameter(handle) == ErrorInvalidHandle, "sceDepthSetPreFilterParameter");
        Require(sceDepthSetSrParameter(handle) == ErrorInvalidHandle, "sceDepthSetSrParameter");
        Require(sceDepthSetSrTnrParameter(handle) == ErrorInvalidHandle, "sceDepthSetSrTnrParameter");
        Require(sceDepthSubmit(handle) == ErrorInvalidHandle, "sceDepthSubmit");
        Require(sceDepthTerminate(handle) == ErrorInvalidHandle, "sceDepthTerminate");
        Require(sceDepthWaitAndExecutePostProcess(handle) == ErrorInvalidHandle, "sceDepthWaitAndExecutePostProcess");
        Require(libSceDepthUnknown_unL9sWu4IIw_nid_no_patch(handle) == ErrorInvalidHandle, "unL9sWu4IIw");
        Require(_sceDepthSetCameraFactoryCalibrationData(handle) == ResultCameraFactoryCalibrationFailure, "_sceDepthSetCameraFactoryCalibrationData");
        Require(_sceDepthSetHeteroAdjustParam(handle) == -1, "_sceDepthSetHeteroAdjustParam");
        Require(_sceDepthSetHeteroMode(handle) == -1, "_sceDepthSetHeteroMode");
    }
    std::puts("Depth handle tests passed");
    return 0;
}
