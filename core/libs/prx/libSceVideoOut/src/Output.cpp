#include <cstring>
#include <mutex>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"

static int ValidateOutputConfig(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) {
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (reservedPtr != nullptr || reserved != 0) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (options != nullptr) {
        for (auto v : options->internalData) {
            if (v != 0) {
                return VIDEO_OUT_ERROR_INVALID_OPTION;
            }
        }
    }
    if (mode != VIDEO_OUT_OUTPUT_MODE_DEFAULT && mode != VIDEO_OUT_OUTPUT_MODE_119_88HZ) {
        return VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE;
    }
    return 0;
}

extern "C" {

int APS5_VABI sceVideoOutOpen(int user_id, int bus_type, int index, const void* param) {
    (void)param;
    if (user_id != 255 && user_id != 0) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (bus_type != VIDEO_OUT_BUS_TYPE_MAIN && bus_type != VIDEO_OUT_BUS_TYPE_OVERLAY && bus_type != VIDEO_OUT_BUS_TYPE_SUB) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (index != 0) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    const int handle = VideoOutDriver::Get().Open(bus_type);
    if (handle < 0) {
        return VIDEO_OUT_ERROR_RESOURCE_BUSY;
    }
    return handle;
}

int APS5_VABI sceVideoOutClose(int handle) {
    return VideoOutDriver::Get().Close(handle) ? 0 : VIDEO_OUT_ERROR_INVALID_HANDLE;
}

int APS5_VABI sceVideoOutSetFlipRate(int handle, int rate) {
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (rate < 0 || rate > 2) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    std::unique_lock lock(cfg->mutex);
    cfg->flipRate = rate;
    return 0;
}

int APS5_VABI sceVideoOutGetFlipStatus(int handle, VideoOutFlipStatus* status) {
    if (status == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    std::unique_lock lock(cfg->mutex);
    *status = cfg->flipStatus;
    return 0;
}

int APS5_VABI sceVideoOutGetVblankStatus(int handle, VideoOutVblankStatus* status) {
    if (status == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    std::unique_lock lock(cfg->mutex);
    *status = cfg->vblankStatus;
    return 0;
}

int APS5_VABI sceVideoOutGetOutputStatus(int handle, VideoOutOutputStatus* status) {
    if (status == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    std::unique_lock lock(cfg->mutex);
    status->resolution = (cfg->width >= 3840 || cfg->height >= 2160) ? 2u : 1u;
    status->dynamicRange = 1;
    status->refreshRate = VIDEO_OUT_REFRESH_RATE_59_94HZ;
    status->flags = 0;
    status->reserved[0] = 0;
    status->reserved[1] = 0;
    status->reserved[2] = 0;
    return 0;
}

int APS5_VABI sceVideoOutIsFlipPending(int handle) {
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    std::unique_lock lock(cfg->mutex);
    return cfg->flipStatus.flipPendingNum;
}

int APS5_VABI sceVideoOutWaitVblank(int handle) {
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    std::unique_lock lock(cfg->mutex);
    const uint64_t count = cfg->vblankStatus.count;
    while (cfg->opened && cfg->vblankStatus.count == count) {
        cfg->vblankCond.wait(lock);
    }
    return 0;
}

int APS5_VABI sceVideoOutInitializeOutputOptions(VideoOutOutputOptions* options) {
    if (options == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    std::memset(options, 0, sizeof(VideoOutOutputOptions));
    return 0;
}

int APS5_VABI sceVideoOutIsOutputSupported(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reserved_ptr, uint64_t reserved) {
    const int result = ValidateOutputConfig(handle, mode, options, reserved_ptr, reserved);
    if (result != 0) {
        return result;
    }
    return (mode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) ? 0 : 1;
}

int APS5_VABI sceVideoOutConfigureOutput(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reserved_ptr, uint64_t reserved) {
    const int supported = sceVideoOutIsOutputSupported(handle, mode, options, reserved_ptr, reserved);
    if (supported < 0) {
        return supported;
    }
    if (supported == 0 && mode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) {
        return VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE;
    }
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    std::unique_lock lock(cfg->mutex);
    cfg->outputMode = mode;
    return 0;
}

int APS5_VABI sceVideoOutSetWindowModeMargins(int handle, int top, int bottom) {
    (void)top;
    (void)bottom;
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    return 0;
}

int APS5_VABI sceVideoOutLatencyControlWaitBeforeInput(int handle) {
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    return 0;
}

int APS5_VABI sceVideoOutLatencyMeasureSetStartPoint(int handle, uint32_t point) {
    (void)point;
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    return 0;
}

int APS5_VABI sceVideoOutColorSettingsSetGamma(VideoOutColorSettings* settings, float gamma) {
    if (settings == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    if (gamma < 0.1f || gamma > 2.0f) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    settings->gamma = gamma;
    return 0;
}

int APS5_VABI sceVideoOutAdjustColor(int handle, const VideoOutColorSettings* settings) {
    if (settings == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    std::unique_lock lock(cfg->mutex);
    cfg->gamma = settings->gamma;
    return 0;
}

}
