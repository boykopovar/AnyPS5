#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <cmath>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"

static int validateOutputConfig(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) {
    if (!VideoOutDriver::Get().IsOpen(handle)) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    if (reservedPtr != nullptr || reserved != 0) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_VALUE");
    }
    if (options != nullptr) {
        for (auto v : options->internalData) {
            if (v != 0) {
                throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_OPTION");
            }
        }
    }
    if (mode != VIDEO_OUT_OUTPUT_MODE_DEFAULT && mode != VIDEO_OUT_OUTPUT_MODE_119_88HZ) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE");
    }
    return 0;
}

extern "C" {

int APS5_VABI sceVideoOutOpen(int userId, int busType, int index, const void* param) try {
    if (param != nullptr) {
        std::array<std::uint32_t, VIDEO_OUT_OPEN_PARAM_SIZE / sizeof(std::uint32_t)> words{};
        std::memcpy(words.data(), param, sizeof(words));
        if (words[0] != VIDEO_OUT_OPEN_PARAM_SIZE) {
            throw std::runtime_error(std::string(__func__) + ": unsupported param size");
        }
        if (std::any_of(words.begin() + 1, words.end(), [](std::uint32_t word) { return word != 0; })) {
            throw std::runtime_error(std::string(__func__) + ": param options not implemented");
        }
    }
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
    if (options == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    std::memset(options, 0, sizeof(VideoOutOutputOptions));
    return 0;
}

int APS5_VABI sceVideoOutIsOutputSupported(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) try {
    const int result = validateOutputConfig(handle, mode, options, reservedPtr, reserved);
    if (result != 0) {
        return result;
    }
    return (mode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) ? 0 : 1;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutConfigureOutput(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) try {
    const int supported = sceVideoOutIsOutputSupported(handle, mode, options, reservedPtr, reserved);
    if (supported < 0) {
        return supported;
    }
    if (supported == 0 && mode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE");
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

APS5_EXPORT("T4ucGB8CsnM", sceVideoOutVrrPeg);
APS5_EXPORT("5tRaBjtdTzY", sceVideoOutVrrPeg);
int APS5_VABI sceVideoOutVrrPeg(std::uint64_t a0, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3) try {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

APS5_EXPORT("kP2L8t3j-aM", sceVideoOutVrrStatusUnknown00);
int APS5_VABI sceVideoOutVrrStatusUnknown00() try {
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

APS5_EXPORT("LibwuIonIBw", sceVideoOutVrrStatusUnknown01);
int APS5_VABI sceVideoOutVrrStatusUnknown01() try {
    NotImplemented_nid_no_patch(__func__);
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

}
