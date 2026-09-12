#include <cstring>
#include <mutex>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"

extern "C" {

void APS5_VABI sceVideoOutSetBufferAttribute2(VideoOutBufferAttribute2* attribute, uint64_t pixel_format, uint32_t tiling_mode, uint32_t width, uint32_t height, uint64_t option, uint32_t dcc_control, uint64_t dcc_cb_register_clear_color) {
    if (attribute == nullptr) {
        return;
    }
    std::memset(attribute, 0, sizeof(VideoOutBufferAttribute2));
    attribute->tiling_mode = tiling_mode;
    attribute->aspect_ratio = 0;
    attribute->width = width;
    attribute->height = height;
    attribute->pitch_in_pixel = 0;
    attribute->option = option;
    attribute->pixel_format = pixel_format;
    attribute->dcc_cb_register_clear_color = dcc_cb_register_clear_color;
    attribute->dcc_control = dcc_control;
}

int APS5_VABI sceVideoOutRegisterBuffers2(int handle, int set_index, int buffer_index_start, const VideoOutBuffers* buffers, int buffer_num, const VideoOutBufferAttribute2* attribute, int category, void* option) {
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
    if (set_index < 0 || set_index >= VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX || buffer_index_start < 0 || buffer_index_start >= VIDEO_OUT_BUFFER_NUM_MAX || buffer_num < 1 || buffer_num > VIDEO_OUT_BUFFER_NUM_MAX || buffer_index_start + buffer_num > VIDEO_OUT_BUFFER_NUM_MAX) {
        return VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (category != VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_UNCOMPRESSED && category != VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_COMPRESSED) {
        return VIDEO_OUT_ERROR_INVALID_CATEGORY;
    }
    std::unique_lock lock(cfg->mutex);
    if (cfg->closing) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (cfg->groups[set_index].occupied) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    for (int i = 0; i < buffer_num; i++) {
        if (cfg->buffers[buffer_index_start + i].Occupied()) {
            return VIDEO_OUT_ERROR_SLOT_OCCUPIED;
        }
    }
    BufferAttributeGroup group{};
    group.attribute = *attribute;
    group.category = category;
    group.occupied = true;
    cfg->groups[set_index] = group;
    for (int i = 0; i < buffer_num; i++) {
        VideoOutBuffer buf{};
        buf.groupIndex = set_index;
        buf.dataAddress = reinterpret_cast<uint64_t>(buffers[i].data);
        buf.metadataAddress = reinterpret_cast<uint64_t>(buffers[i].metadata);
        cfg->buffers[buffer_index_start + i] = buf;
    }
    return 0;
}

int APS5_VABI sceVideoOutSubmitChangeBufferAttribute2(int handle, int set_index, const VideoOutBufferAttribute2* attribute, void* option) {
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (attribute == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_OPTION;
    }
    if (set_index < 0 || set_index >= VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    if (option != nullptr) {
        return VIDEO_OUT_ERROR_INVALID_OPTION;
    }
    std::unique_lock lock(cfg->mutex);
    if (cfg->closing || !cfg->groups[set_index].occupied) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    cfg->groups[set_index].attribute = *attribute;
    return 0;
}

int APS5_VABI sceVideoOutUnregisterBuffers(int handle, int set_index) {
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (set_index < 0 || set_index >= VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    std::unique_lock lock(cfg->mutex);
    if (cfg->closing) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (!cfg->groups[set_index].occupied) {
        return VIDEO_OUT_ERROR_INVALID_INDEX;
    }
    cfg->groups[set_index] = BufferAttributeGroup{};
    for (auto& buf : cfg->buffers) {
        if (buf.groupIndex == set_index) {
            buf = VideoOutBuffer{};
        }
    }
    return 0;
}

int APS5_VABI sceVideoOutSubmitFlip(int handle, int index, int flip_mode, int64_t flip_arg) {
    (void)flip_arg;
    auto* cfg = VideoOutDriver::Get().GetConfig(handle);
    if (cfg == nullptr) {
        return VIDEO_OUT_ERROR_INVALID_HANDLE;
    }
    if (flip_mode < VIDEO_OUT_FLIP_MODE_VSYNC || flip_mode > VIDEO_OUT_FLIP_MODE_VSYNC_MULTI) {
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
    cfg->flipStatus.currentBuffer = index;
    cfg->flipStatus.flipArg = flip_arg;
    cfg->flipStatus.count++;
    return 0;
}

}
