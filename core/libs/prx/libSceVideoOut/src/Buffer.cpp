#include <cstring>
#include <mutex>
#include <stdexcept>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"

extern "C" {

void APS5_VABI sceVideoOutSetBufferAttribute2(VideoOutBufferAttribute2* attribute, uint64_t pixelFormat, uint32_t tilingMode, uint32_t width, uint32_t height, uint64_t option, uint32_t dccControl, uint64_t dccCbRegisterClearColor) {
    if (attribute == nullptr) {
        throw std::runtime_error("sceVideoOutSetBufferAttribute2: null attribute");
    }
    std::memset(attribute, 0, sizeof(VideoOutBufferAttribute2));
    attribute->tiling_mode = tilingMode;
    attribute->aspect_ratio = 0;
    attribute->width = width;
    attribute->height = height;
    attribute->pitch_in_pixel = 0;
    attribute->option = option;
    attribute->pixel_format = pixelFormat;
    attribute->dcc_cb_register_clear_color = dccCbRegisterClearColor;
    attribute->dcc_control = dccControl;
}

int APS5_VABI sceVideoOutRegisterBuffers2(int handle, int setIndex, int bufferIndexStart, const VideoOutBuffers* buffers, int bufferNum, const VideoOutBufferAttribute2* attribute, int category, void* option) {
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (buffers == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_ADDRESS;
    }
    if (attribute == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_OPTION;
    }
    if (option != nullptr) {
        return VIDEO_OUT_ERROR_INVALID_OPTION;
    }
    if (setIndex < 0 || setIndex >= VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX || bufferIndexStart < 0 || bufferIndexStart >= VIDEO_OUT_BUFFER_NUM_MAX || bufferNum < 1 || bufferNum > VIDEO_OUT_BUFFER_NUM_MAX || bufferIndexStart + bufferNum > VIDEO_OUT_BUFFER_NUM_MAX) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (category != VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_UNCOMPRESSED && category != VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_COMPRESSED) {
        return VIDEO_OUT_ERROR_INVALID_CATEGORY;
    }
    std::unique_lock lock(cfg->mutex);
    if (cfg->closing) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (cfg->groups[setIndex].occupied) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    for (int i = 0; i < bufferNum; i++) {
        if (cfg->buffers[bufferIndexStart + i].Occupied()) {
            return VIDEO_OUT_ERROR_SLOT_OCCUPIED;
        }
    }
    BufferAttributeGroup group{};
    group.attribute = *attribute;
    group.category = category;
    group.occupied = true;
    cfg->groups[setIndex] = group;
    cfg->width = attribute->width;
    cfg->height = attribute->height;
    for (int i = 0; i < bufferNum; i++) {
        VideoOutBuffer buf{};
        buf.groupIndex = setIndex;
        buf.dataAddress = reinterpret_cast<uint64_t>(buffers[i].data);
        buf.metadataAddress = reinterpret_cast<uint64_t>(buffers[i].metadata);
        cfg->buffers[bufferIndexStart + i] = buf;
    }
    return 0;
}

int APS5_VABI sceVideoOutSubmitChangeBufferAttribute2(int handle, int setIndex, const VideoOutBufferAttribute2* attribute, void* option) {
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (attribute == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_OPTION;
    }
    if (setIndex < 0 || setIndex >= VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    if (option != nullptr) {
        return VIDEO_OUT_ERROR_INVALID_OPTION;
    }
    std::unique_lock lock(cfg->mutex);
    if (cfg->closing || !cfg->groups[setIndex].occupied) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    cfg->groups[setIndex].attribute = *attribute;
    return 0;
}

int APS5_VABI sceVideoOutUnregisterBuffers(int handle, int setIndex) {
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (setIndex < 0 || setIndex >= VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    std::unique_lock lock(cfg->mutex);
    if (cfg->closing) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (!cfg->groups[setIndex].occupied) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    cfg->groups[setIndex] = BufferAttributeGroup{};
    for (auto& buf : cfg->buffers) {
        if (buf.groupIndex == setIndex) {
            buf = VideoOutBuffer{};
        }
    }
    return 0;
}

int APS5_VABI sceVideoOutSubmitFlip(int handle, int index, int flipMode, int64_t flipArg) {
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (flipMode < VIDEO_OUT_FLIP_MODE_VSYNC || flipMode > VIDEO_OUT_FLIP_MODE_VSYNC_MULTI) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (index < VIDEO_OUT_BUFFER_INDEX_BLACK || index >= VIDEO_OUT_BUFFER_NUM_MAX) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    std::unique_lock lock(cfg->mutex);
    if (cfg->closing) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    const bool special = (index == VIDEO_OUT_BUFFER_INDEX_BLANK || index == VIDEO_OUT_BUFFER_INDEX_BLACK);
    if (!special && !cfg->buffers[index].Occupied()) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    lock.unlock();
    VideoOutDriver::Get().SubmitFlip(cfg, index, flipMode, flipArg);
    return 0;
}

}
