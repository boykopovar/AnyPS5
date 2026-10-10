#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <cmath>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"

static constexpr std::uint32_t VIDEO_OUT_OUTPUT_OPTIONS_FIRST_WORD = 0x00FF0000;
static constexpr std::size_t VIDEO_OUT_OUTPUT_OPTIONS_FREE_WORD = 3;

static int validateOutputConfig(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) {
    if (!VideoOutDriver::Get().HasConfig(handle) || !VideoOutDriver::Get().IsOpen(handle)) return VIDEO_OUT_ERROR_INVALID_HANDLE;
    if (reservedPtr != nullptr || reserved != 0) return VIDEO_OUT_ERROR_INVALID_VALUE;
    switch (mode) {
    case VIDEO_OUT_OUTPUT_MODE_DEFAULT:
        break;
    case 0x4: case 0x7: case 0x8: case 0xC: case 0xD: case 0xE: case VIDEO_OUT_OUTPUT_MODE_119_88HZ: case 0x10: case 0x11: case 0x13:
        return VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE;
    case 0x12:
        throw std::runtime_error(std::string(__func__) + ": output mode 0x12 answers differently on two consoles");
    default:
        return VIDEO_OUT_ERROR_UNKNOWN_OUTPUT_MODE;
    }
    if (options != nullptr) {
        for (std::size_t index = 0; index < std::size(options->internalData); ++index) {
            const std::uint32_t expected = index == 0 ? VIDEO_OUT_OUTPUT_OPTIONS_FIRST_WORD : 0;
            if (index != VIDEO_OUT_OUTPUT_OPTIONS_FREE_WORD && options->internalData[index] != expected) return VIDEO_OUT_ERROR_INVALID_VALUE;
        }
    }
    return 0;
}

static void validateOpenParam(const void* param) {
    if (param == nullptr) return;
    VideoOutOpenParam openParam{};
    std::memcpy(&openParam, param, offsetof(VideoOutOpenParam, affinity));
    if (openParam.firstWord != VIDEO_OUT_OPEN_PARAM_FIRST_WORD) {
        throw std::runtime_error(std::string(__func__) + ": unsupported first word");
    }
    if (openParam.setPriority > 1 || openParam.setAffinity > 1) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    if (openParam.setPriority == 1 && (openParam.priority < VIDEO_OUT_SERVICE_THREAD_PRIORITY_HIGHEST || openParam.priority > VIDEO_OUT_SERVICE_THREAD_PRIORITY_LOWEST)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    if (openParam.setAffinity == 1) {
        std::memcpy(&openParam.affinity, static_cast<const std::byte*>(param) + offsetof(VideoOutOpenParam, affinity), sizeof(openParam.affinity));
        if (openParam.affinity == 0 || (openParam.affinity & ~VIDEO_OUT_SERVICE_THREAD_AFFINITY_ALL) != 0) {
            throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
        }
    }
}

extern "C" {

int APS5_VABI sceVideoOutOpen(int userId, int busType, int index, const void* param) try {
    validateOpenParam(param);
    if (userId != 255 && userId != 0) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    if (busType != VIDEO_OUT_BUS_TYPE_MAIN && busType != VIDEO_OUT_BUS_TYPE_OVERLAY && busType != VIDEO_OUT_BUS_TYPE_SUB) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    if (index != 0) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    const int handle = VideoOutDriver::Get().Open(busType);
    if (handle < 0) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_RESOURCE_BUSY");
    }
    return handle;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutClose(int handle) try {
    return VideoOutDriver::Get().Close(handle) ? 0 : VIDEO_OUT_ERROR_INVALID_HANDLE;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutAllowOutputResolutionWqhdDetection(int handle) try {
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutSetFlipRate(int handle, int rate) try {
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    if (rate < 0 || rate > 2) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    std::lock_guard lock(cfg->mutex);
    cfg->Check();
    cfg->flipRate = rate;
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutGetFlipStatus(int handle, VideoOutFlipStatus* status) try {
    if (status == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    *status = cfg->flipStatus;
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutGetVblankStatus(int handle, VideoOutVblankStatus* status) try {
    if (status == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    *status = cfg->vblankStatus;
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutGetOutputStatus(int handle, VideoOutOutputStatus* status) try {
    if (status == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    status->resolution = (cfg->width >= 3840 || cfg->height >= 2160) ? 2u : 1u;
    status->dynamicRange = 1;
    status->refreshRate = (cfg->outputMode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) ? VIDEO_OUT_REFRESH_RATE_119_88HZ : VIDEO_OUT_REFRESH_RATE_59_94HZ;
    status->flags = 0;
    status->reserved[0] = 0;
    status->reserved[1] = 0;
    status->reserved[2] = 0;
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutGetResolutionStatus(int handle, VideoOutResolutionStatus* status) try {
    if (status == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    *status = VideoOutResolutionStatus{};
    status->fullWidth = cfg->width;
    status->fullHeight = cfg->height;
    status->paneWidth = cfg->width;
    status->paneHeight = cfg->height;
    status->refreshRate = (cfg->outputMode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) ? VIDEO_OUT_REFRESH_RATE_119_88HZ : VIDEO_OUT_REFRESH_RATE_59_94HZ;
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutIsFlipPending(int handle) try {
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    return cfg->flipStatus.flipPendingNum;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutWaitVblank(int handle) try {
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    const uint64_t count = cfg->vblankStatus.count;
    cfg->vblankCond.wait(lock, cfg->shutdownToken, [&] { return !cfg->opened || cfg->closing || cfg->failure || cfg->vblankStatus.count != count; });
    cfg->Check();
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutInitializeOutputOptions(VideoOutOutputOptions* options) {
    if (options == nullptr) return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    std::memset(options, 0, sizeof(VideoOutOutputOptions));
    options->internalData[0] = VIDEO_OUT_OUTPUT_OPTIONS_FIRST_WORD;
    return 0;
}

int APS5_VABI sceVideoOutIsOutputSupported(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) try {
    const int result = validateOutputConfig(handle, mode, options, reservedPtr, reserved);
    if (result != 0) {
        return result;
    }
    return 1;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutConfigureOutput(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) try {
    const int supported = sceVideoOutIsOutputSupported(handle, mode, options, reservedPtr, reserved);
    if (supported < 0) {
        return supported;
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    cfg->outputMode = mode;
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutSetWindowModeMargins(int handle, int top, int bottom) try {
    (void)top;
    (void)bottom;
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutLatencyControlWaitBeforeInput(int handle) try {
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutLatencyMeasureSetStartPoint(int handle, uint32_t point) try {
    (void)point;
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutColorSettingsSetGamma(VideoOutColorSettings* settings, float gamma) {
    if (settings == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    if (!std::isfinite(gamma) || gamma < 0.1f || gamma > 2.0f) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    settings->gamma = gamma;
    return 0;
}

int APS5_VABI sceVideoOutAdjustColor(int handle, const VideoOutColorSettings* settings) try {
    if (settings == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    // Output gamma is accepted but not yet applied to presentation.
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutVrrUnpegFromFixedRate() try {
    NotImplemented_nid_no_patch(__func__);
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutVrrPegToFixedRate() try {
    NotImplemented_nid_no_patch(__func__);
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutAddVrrStatusFlagsPrivilege() {
    return 0;
}

}
