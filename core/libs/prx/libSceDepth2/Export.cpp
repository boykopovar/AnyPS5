#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceDepth2/include/InventoryExports.hpp"
#include "prx/libSceDepth2/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidValue = static_cast<int>(0x80BB0000u);
constexpr int ErrorInvalidOutputProperty = static_cast<int>(0x80BB0002u);
constexpr int ErrorInvalidInputProperty = static_cast<int>(0x80BB0003u);
constexpr int ErrorNonZeroArgument = static_cast<int>(0x80BB0004u);
constexpr int ErrorInvalidHandle = static_cast<int>(0x80BB0005u);
constexpr int ErrorInvalidSize = static_cast<int>(0x80BB0008u);
constexpr int ErrorReservedNotEmpty = static_cast<int>(0x80BB0009u);
constexpr int ErrorNullPointer = static_cast<int>(0x80BB000Au);
constexpr int ErrorInvalidMode = static_cast<int>(0x80BB000Bu);
constexpr int ErrorInvalidInputResolutionMode1 = static_cast<int>(0x80BB000Cu);
constexpr int ErrorInvalidType = static_cast<int>(0x80BB0012u);

bool IsOpenHandle(int32_t handle) {
    (void)handle;
    return false;
}

bool IsValidResolution(uint32_t width, uint32_t height, uint32_t mode, uint32_t type) {
    if (mode == 0) {
        if (width == 320 && height == 200) return true;
        if (width == 640 && height == 400) return true;
        if ((type == 3 || type == 4) && width == 1280 && height == 800) return true;
        return false;
    }
    if (mode == 1) {
        if (width == 480 && height == 270) return true;
        if (width == 960 && height == 540) return true;
        if ((type == 3 || type == 4) && width == 1920 && height == 1080) return true;
        return false;
    }
    return false;
}

}

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

int APS5_VABI sceDepth2QueryMemory(const SceDepth2Param* param, SceDepth2Memory* mem) {
    if (param == nullptr || mem == nullptr) {
        return ErrorNullPointer;
    }
    if (param->size != 0x40) {
        return ErrorInvalidSize;
    }
    if (param->type != 0 && param->type != 2 && param->type != 3 && param->type != 4) {
        return ErrorInvalidType;
    }
    if (param->mode != 0 && param->mode != 1) {
        return ErrorInvalidMode;
    }
    if (!IsValidResolution(param->inputWidth, param->inputHeight, param->mode, param->type)) {
        return param->mode == 1 ? ErrorInvalidInputResolutionMode1 : ErrorInvalidMode;
    }
    if (param->unknown0c > 1) {
        return ErrorInvalidInputProperty;
    }
    if (!IsValidResolution(param->outputWidth, param->outputHeight, param->mode, param->type)) {
        return ErrorInvalidMode;
    }
    if (param->unknown18 > 1) {
        return ErrorInvalidOutputProperty;
    }
    if (param->reserved20 != 0 || param->reserved24 != 0) {
        return ErrorReservedNotEmpty;
    }
    if (param->unknown30 >= 7 || param->unknown34 >= 8) {
        return ErrorInvalidValue;
    }
    uint64_t size = 0;
    if ((param->inputWidth == 320 && param->inputHeight == 200) || (param->inputWidth == 480 && param->inputHeight == 270)) {
        size = 0xc00000;
    } else if ((param->inputWidth == 640 && param->inputHeight == 400) || (param->inputWidth == 960 && param->inputHeight == 540)) {
        size = 0x2000000;
    } else if ((param->inputWidth == 1280 && param->inputHeight == 800) || (param->inputWidth == 1920 && param->inputHeight == 1080)) {
        size = 0x8000000;
    }
    mem->size = size;
    mem->alignment = 0x40000;
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
