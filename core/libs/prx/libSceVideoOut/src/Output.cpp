#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <cmath>

#include "SceTypes.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"

static constexpr std::uint32_t VIDEO_OUT_OUTPUT_OPTIONS_FIRST_WORD = 0x00FF0000;
static constexpr std::size_t VIDEO_OUT_OUTPUT_OPTIONS_FREE_WORD = 3;

static VideoOutOutputStatus outputStatus(const auto& cfg) {
    VideoOutOutputStatus status{};
    status.resolution = VideoOutDriver::Get().GetDisplayResolution();
    const bool hdr = AgcDriver::CurrentHostHdr().available;
    status.dynamicRange = hdr ? VIDEO_OUT_DYNAMIC_RANGE_HDR : VIDEO_OUT_DYNAMIC_RANGE_SDR;
    status.refreshRate = (cfg.outputMode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) ? VIDEO_OUT_REFRESH_RATE_119_88HZ : VIDEO_OUT_REFRESH_RATE_59_94HZ;
    status.flags = hdr ? VIDEO_OUT_OUTPUT_STATUS_FLAG_HDR : 0;
    return status;
}

int ValidateVideoOutOutputMode_nid_no_patch(uint64_t mode, const VideoOutOutputOptions* options, int displayRefreshRate) {
    switch (mode) {
    case VIDEO_OUT_OUTPUT_MODE_DEFAULT:
        break;
    case VIDEO_OUT_OUTPUT_MODE_119_88HZ:
        if (displayRefreshRate < 120) return VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE;
        break;
    case 0x4: case 0x7: case 0x8: case 0xC: case 0xD: case 0xE: case 0x10: case 0x11: case 0x13:
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

int VideoOutOutputSupport_nid_no_patch(uint64_t mode, const VideoOutOutputOptions* options, int displayRefreshRate) {
    const int result = ValidateVideoOutOutputMode_nid_no_patch(mode, options, displayRefreshRate);
    if (mode == VIDEO_OUT_OUTPUT_MODE_119_88HZ && result == VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE) return 0;
    return result != 0 ? result : 1;
}

static int validateOutputConfig(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) {
    if (!VideoOutDriver::Get().HasConfig(handle) || !VideoOutDriver::Get().IsOpen(handle)) return VIDEO_OUT_ERROR_INVALID_HANDLE;
    if (reservedPtr != nullptr || reserved != 0) return VIDEO_OUT_ERROR_INVALID_VALUE;
    return VideoOutOutputSupport_nid_no_patch(mode, options, VideoOutDriver::Get().GetDisplayRefreshRate());
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
    if (busType == VIDEO_OUT_BUS_TYPE_HMD2_MAIN) return VIDEO_OUT_ERROR_INVALID_VALUE;
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
    if (handle < 0) return VIDEO_OUT_ERROR_RESOURCE_BUSY;
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
    if (rate < 0 || rate > 2) return VIDEO_OUT_ERROR_INVALID_VALUE;
    std::lock_guard lock(cfg->mutex);
    cfg->Check();
    cfg->flipRate = rate;
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutGetFlipStatus(int handle, VideoOutFlipStatus* status) try {
    const bool open = VideoOutDriver::Get().HasConfig(handle);
    if (status == nullptr) {
        if (!open) {
            throw std::runtime_error(std::string(__func__) + ": a null output with an invalid handle is not measured");
        }
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    if (!open) return VIDEO_OUT_ERROR_INVALID_HANDLE;
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    if (cfg->ps4) {
        const auto& current = cfg->flipStatus;
        VideoOutFlipStatusPs4 legacy{current.count, current.processTime, 0, current.flipArg, 0, 0, current.gcQueueNum, current.flipPendingNum, current.currentBuffer, 0};
        *reinterpret_cast<VideoOutFlipStatusPs4*>(status) = legacy;
    } else {
        *status = cfg->flipStatus;
    }
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
    if (cfg->ps4) {
        const auto& current = cfg->vblankStatus;
        VideoOutVblankStatusPs4 legacy{current.count, current.processTime, 0, current.flags};
        *reinterpret_cast<VideoOutVblankStatusPs4*>(status) = legacy;
    } else {
        *status = cfg->vblankStatus;
    }
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
    *status = outputStatus(*cfg);
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutGetOutputStatusWithVrr(int handle, void* status) try {
    if (status == nullptr) return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    if (!VideoOutDriver::Get().HasConfig(handle) || !VideoOutDriver::Get().IsOpen(handle)) return VIDEO_OUT_ERROR_INVALID_HANDLE;
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    std::array<std::byte, 80> bytes{};
    const auto output = outputStatus(*cfg);
    std::memcpy(bytes.data(), &output, sizeof(output));
    std::memcpy(status, bytes.data(), bytes.size());
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
    if (!VideoOutDriver::Get().HasConfig(handle)) return VIDEO_OUT_ERROR_INVALID_HANDLE;
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    std::unique_lock lock(cfg->mutex);
    cfg->Check();
    return cfg->flipStatus.flipPendingNum;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

static int cursorHandleError(const char* function, int handle, int index) {
    if (index != 0) NotImplemented_nid_no_patch(function);
    if (!VideoOutDriver::Get().HasConfig(handle)) return VIDEO_OUT_ERROR_INVALID_HANDLE;
    return 0;
}

int APS5_VABI sceVideoOutCursorEnable(int32_t handle, int32_t index, const void* address) try {
    if (const int error = cursorHandleError(__func__, handle, index)) return error;
    if (address == nullptr) return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    NotImplemented_nid_no_patch(__func__);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutCursorDisable(int32_t handle, int32_t index) try {
    if (const int error = cursorHandleError(__func__, handle, index)) return error;
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutCursorSetImageAddress(int32_t handle, int32_t index, const void* address) try {
    if (const int error = cursorHandleError(__func__, handle, index)) return error;
    if (address == nullptr) return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    NotImplemented_nid_no_patch(__func__);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutCursorSetPosition(int32_t handle, int32_t index, uint32_t, uint32_t) try {
    if (const int error = cursorHandleError(__func__, handle, index)) return error;
    return 0;
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
    return validateOutputConfig(handle, mode, options, reservedPtr, reserved);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutConfigureOutput(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reservedPtr, uint64_t reserved) try {
    const int supported = sceVideoOutIsOutputSupported(handle, mode, options, reservedPtr, reserved);
    if (supported < 0) {
        return supported;
    }
    if (mode == VIDEO_OUT_OUTPUT_MODE_119_88HZ && (GetAppAttribute3_nid_no_patch() & 0x40u) == 0) return VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE;
    if (supported == 0) return VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE;
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

int APS5_VABI sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(VideoOutColorSpaceConversionSettings* settings) {
    if (settings == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    std::memset(settings, 0, sizeof(VideoOutColorSpaceConversionSettings));
    return 0;
}

int APS5_VABI sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(VideoOutColorSpaceConversionSettings* settings, float lumaScale, uint32_t option) {
    if (settings == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    (void)lumaScale;
    (void)option;
    return 0;
}

int APS5_VABI sceVideoOutAdjustColorSpaceConversion(int handle, const VideoOutColorSpaceConversionSettings* settings) try {
    if (settings == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_ADDRESS");
    }
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutAdjustColor_(int handle, const VideoOutColorSettings* settings, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSettings)) throw std::runtime_error("sceVideoOutAdjustColor_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutAdjustColor(handle, settings);
}

int APS5_VABI sceVideoOutColorSettingsSetGamma_(VideoOutColorSettings* settings, float gamma, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSettings)) throw std::runtime_error("sceVideoOutColorSettingsSetGamma_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutColorSettingsSetGamma(settings, gamma);
}

int APS5_VABI sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(VideoOutColorSpaceConversionSettings* settings, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSpaceConversionSettings)) throw std::runtime_error("sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(settings);
}

int APS5_VABI sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(VideoOutColorSpaceConversionSettings* settings, float lumaScale, uint32_t option, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSpaceConversionSettings)) throw std::runtime_error("sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(settings, lumaScale, option);
}

int APS5_VABI sceVideoOutAdjustColorSpaceConversion_(int handle, const VideoOutColorSpaceConversionSettings* settings, uint32_t settings_size) {
    if (settings_size < sizeof(VideoOutColorSpaceConversionSettings)) throw std::runtime_error("sceVideoOutAdjustColorSpaceConversion_: VIDEO_OUT_ERROR_INVALID_VALUE");
    return sceVideoOutAdjustColorSpaceConversion(handle, settings);
}

int APS5_VABI sceVideoOutSetGlobalBlendSpace(int handle, uint32_t space, uint32_t option) try {
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    (void)space;
    (void)option;
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutVrrUnpegFromFixedRate(int handle) try {
    if (VideoOutDriver::Get().GetConfig(handle) == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutVrrPegToFixedRate(int handle, uint64_t arg1, uint64_t arg2) try {
    if (VideoOutDriver::Get().GetConfig(handle) == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": VIDEO_OUT_ERROR_INVALID_HANDLE");
    }
    if (arg1 != 0 || arg2 != 0) {
        throw std::runtime_error(std::string(__func__) + ": unknown arguments " + std::to_string(arg1) + ", " + std::to_string(arg2));
    }
    return 0;
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceVideoOutAddVrrStatusFlagsPrivilege() {
    return 0;
}

}
