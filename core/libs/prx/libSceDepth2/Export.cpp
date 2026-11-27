#include "prx/libc/include/GuestPtr.hpp"
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
    SceDepth2Param query{};
    if (!GuestPtr<const SceDepth2Param>(param).CopyIn(query)) {
        return ErrorNullPointer;
    }
    if (query.size != 0x40) {
        return ErrorInvalidSize;
    }
    if (query.type != 0 && query.type != 2 && query.type != 3 && query.type != 4) {
        return ErrorInvalidType;
    }
    if (query.mode != 0 && query.mode != 1) {
        return ErrorInvalidMode;
    }
    if (!IsValidResolution(query.inputWidth, query.inputHeight, query.mode, query.type)) {
        return query.mode == 1 ? ErrorInvalidInputResolutionMode1 : ErrorInvalidMode;
    }
    if (query.unknown0c > 1) {
        return ErrorInvalidInputProperty;
    }
    if (!IsValidResolution(query.outputWidth, query.outputHeight, query.mode, query.type)) {
        return ErrorInvalidMode;
    }
    if (query.unknown18 > 1) {
        return ErrorInvalidOutputProperty;
    }
    if (query.reserved20 != 0 || query.reserved24 != 0) {
        return ErrorReservedNotEmpty;
    }
    if (query.unknown30 >= 7 || query.unknown34 >= 8) {
        return ErrorInvalidValue;
    }
    uint64_t size = 0;
    if ((query.inputWidth == 320 && query.inputHeight == 200) || (query.inputWidth == 480 && query.inputHeight == 270)) {
        size = 0xc00000;
    } else if ((query.inputWidth == 640 && query.inputHeight == 400) || (query.inputWidth == 960 && query.inputHeight == 540)) {
        size = 0x2000000;
    } else if ((query.inputWidth == 1280 && query.inputHeight == 800) || (query.inputWidth == 1920 && query.inputHeight == 1080)) {
        size = 0x8000000;
    }
    const SceDepth2Memory memory{size, 0x40000};
    if (!GuestPtr<SceDepth2Memory>(mem).CopyOut(memory)) {
        return ErrorNullPointer;
    }
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
