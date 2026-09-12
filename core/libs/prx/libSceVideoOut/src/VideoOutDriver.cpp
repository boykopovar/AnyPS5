#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"

#include <stdexcept>

VideoOutDriver& VideoOutDriver::Get() {
    static VideoOutDriver instance;
    return instance;
}

int VideoOutDriver::Open(int busType) {
    std::unique_lock lock(mutex);
    const int handle = busType + 1;
    if (handle <= 0 || handle >= VIDEO_OUT_NUM_MAX) {
        return -1;
    }
    auto& cfg = contexts[handle];
    std::unique_lock cfgLock(cfg.mutex);
    if (cfg.opened) {
        return -1;
    }
    cfg.closing = false;
    cfg.opened = true;
    cfg.flipRate = 0;
    cfg.outputMode = VIDEO_OUT_OUTPUT_MODE_DEFAULT;
    cfg.gamma = 1.0f;
    cfg.flipStatus = VideoOutFlipStatus{};
    cfg.flipStatus.flipArg = -1;
    cfg.flipStatus.currentBuffer = -1;
    cfg.vblankStatus = VideoOutVblankStatus{};
    cfg.preVblankStatus = VideoOutVblankStatus{};
    for (auto& buf : cfg.buffers) {
        buf = VideoOutBuffer{};
    }
    for (auto& grp : cfg.groups) {
        grp = BufferAttributeGroup{};
    }
    if (++cfg.generation == 0) {
        throw std::runtime_error("VideoOut port generation wrapped");
    }
    return handle;
}

bool VideoOutDriver::Close(int handle) {
    std::unique_lock lock(mutex);
    if (handle <= 0 || handle >= VIDEO_OUT_NUM_MAX) {
        return false;
    }
    auto& cfg = contexts[handle];
    std::unique_lock cfgLock(cfg.mutex);
    if (!cfg.opened || cfg.closing) {
        return false;
    }
    cfg.opened = false;
    cfg.closing = true;
    if (++cfg.generation == 0) {
        throw std::runtime_error("VideoOut port generation wrapped");
    }
    for (auto& buf : cfg.buffers) {
        buf = VideoOutBuffer{};
    }
    for (auto& grp : cfg.groups) {
        grp = BufferAttributeGroup{};
    }
    cfg.flipRate = 0;
    cfg.vblankCond.notify_all();
    return true;
}

VideoOutConfig* VideoOutDriver::GetConfig(int handle) {
    std::unique_lock lock(mutex);
    if (handle <= 0 || handle >= VIDEO_OUT_NUM_MAX || !contexts[handle].opened) {
        return nullptr;
    }
    return &contexts[handle];
}

bool VideoOutDriver::IsOpen(int handle) {
    std::unique_lock lock(mutex);
    return handle > 0 && handle < VIDEO_OUT_NUM_MAX && contexts[handle].opened;
}
