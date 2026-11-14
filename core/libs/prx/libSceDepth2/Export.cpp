#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceDepth2/include/InventoryExports.hpp"
#include "prx/libSceDepth2/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidValue = static_cast<int>(0x80BB0000u);
constexpr int ErrorNonZeroArgument = static_cast<int>(0x80BB0004u);
constexpr int ErrorInvalidHandle = static_cast<int>(0x80BB0005u);
constexpr int ErrorNullPointer = static_cast<int>(0x80BB000Au);

bool IsOpenHandle(int32_t handle) {
    (void)handle;
    return false;
}

} // namespace

extern "C" {

int APS5_VABI sceDepth2GetImage(int32_t handle, uint32_t argument1, void* image, uint32_t argument3) {
    (void)argument3;
    if (argument1 != 0) return ErrorNonZeroArgument;
    if (image == nullptr) return ErrorNullPointer;
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepth2Initialize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepth2QueryMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepth2SaveInternalBuffers(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepth2SetCameraFactoryCalibrationData(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepth2SetCommand(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepth2SetRoi(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepth2Submit(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepth2Terminate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepth2WaitAndExecutePostProcess(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceDepth2Unknown_tLM3AwtzaHs_nid_no_patch(int32_t handle, void* data, int32_t count) {
    if (data == nullptr) return ErrorNullPointer;
    if (count < 0) return ErrorInvalidValue;
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tLM3AwtzaHs", libSceDepth2Unknown_tLM3AwtzaHs_nid_no_patch);

}
