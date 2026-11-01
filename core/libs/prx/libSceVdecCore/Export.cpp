// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libSceVdecCore/include/Api.hpp"
#include "prx/libSceVdecCore/include/Decoder.hpp"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace {

constexpr int VdecCoreErrorInvalidArgument = static_cast<int>(0x80C00001);
constexpr int VdecCoreErrorInvalidCodecType = static_cast<int>(0x80C00003);
constexpr std::uint32_t CodecHevc = 4;
constexpr std::uint32_t MaxCodecType = 4;
constexpr std::uint32_t MaxDecoderType = 8;
constexpr std::int32_t MaxDpbFrameCount = 16;
constexpr std::uint32_t DefaultFrameWidth = 1920;
constexpr std::uint32_t DefaultFrameHeight = 1080;
constexpr std::uint32_t DefaultHorizontalAlign = 256;
constexpr std::uint32_t FrameHeightAlign = 16;
constexpr std::uint32_t FrameBufferPadding = 0x1000;
constexpr std::uint32_t FrameBufferGranularity = 0x10000;
constexpr std::uint32_t FrameBufferAlignment = 256;
constexpr std::uint64_t InstanceSize = 0x40000;
constexpr std::uint64_t ComputeSize = 0x10000;
constexpr std::uint32_t ComputeAlignment = 0x10000;

struct Geometry {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t dpb;
    std::uint32_t horizontalAlign;
    std::uint32_t frameBufferSize;
};

struct Decoder {
    std::uint32_t codecType = 0;
    Geometry geometry{};
    AVCodecContext* context = nullptr;
    std::vector<VdecCoreMemoryBlock> mappedBlocks;
    std::vector<void*> frameBufferSlots;

    ~Decoder() {
        avcodec_free_context(&context);
    }
};

struct Decoders {
    std::mutex mutex;
    std::unordered_map<const void*, std::unique_ptr<Decoder>> live;
};

Decoders& State() {
    static Decoders decoders;
    return decoders;
}

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment) {
    const std::uint32_t remainder = value % alignment;
    return remainder == 0 ? value : value - remainder + alignment;
}

Geometry FrameGeometry(const VdecCoreConfigInfo& config) {
    Geometry geometry{};
    geometry.width = config.maxFrameWidth != 0 ? static_cast<std::uint32_t>(config.maxFrameWidth) : DefaultFrameWidth;
    geometry.height = config.maxFrameHeight != 0 ? static_cast<std::uint32_t>(config.maxFrameHeight) : DefaultFrameHeight;
    geometry.dpb = config.maxDpbFrameCount > 0 && config.maxDpbFrameCount <= MaxDpbFrameCount ? static_cast<std::uint32_t>(config.maxDpbFrameCount) : MaxDpbFrameCount;
    geometry.horizontalAlign = config.horizontalAlign > 0 ? static_cast<std::uint32_t>(config.horizontalAlign) : DefaultHorizontalAlign;
    const std::uint32_t pitch = AlignUp(geometry.width, geometry.horizontalAlign);
    const std::uint32_t height = AlignUp(geometry.height, FrameHeightAlign);
    geometry.frameBufferSize = AlignUp(pitch * height * 3 / 2 + FrameBufferPadding, FrameBufferGranularity);
    return geometry;
}

void OpenCodec(Decoder& decoder) {
    const AVCodec* codec = avcodec_find_decoder(decoder.codecType == CodecHevc ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264);
    if (codec == nullptr) throw std::runtime_error("VdecCore: decoder is unavailable");
    decoder.context = avcodec_alloc_context3(codec);
    if (decoder.context == nullptr) throw std::runtime_error("VdecCore: cannot allocate codec context");
    decoder.context->width = static_cast<int>(decoder.geometry.width);
    decoder.context->height = static_cast<int>(decoder.geometry.height);
    decoder.context->flags |= AV_CODEC_FLAG_COPY_OPAQUE;
    if (avcodec_open2(decoder.context, codec, nullptr) < 0) throw std::runtime_error("VdecCore: cannot open decoder");
}

std::unordered_map<const void*, std::unique_ptr<Decoder>>::iterator Find(Decoders& state, const void* decoder, const char* caller) {
    const auto found = state.live.find(decoder);
    if (found == state.live.end()) throw std::invalid_argument(std::string(caller) + ": not a decoder sceVdecCoreCreateDecoder returned");
    return found;
}

}

extern "C" {

std::uint32_t VdecCoreFrameBufferSlots_nid_no_patch(const void* decoder, void** slots, std::uint32_t capacity) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    const auto& found = Find(state, decoder, "VdecCoreFrameBufferSlots")->second->frameBufferSlots;
    const auto copied = std::min<std::size_t>(capacity, found.size());
    std::copy_n(found.begin(), copied, slots);
    return static_cast<std::uint32_t>(found.size());
}

int APS5_VABI sceVdecCoreQueryMemorySizeSvp() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVdecCoreAnalyzeStream() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreCreateDecoder(const VdecCoreConfigInfo* config, const VdecCoreMemoryInfo*, void*, void** decoder) {
    if (config == nullptr || decoder == nullptr) return VdecCoreErrorInvalidArgument;
    if (config->decoderType > MaxDecoderType) return VdecCoreErrorInvalidArgument;
    if (config->codecType > MaxCodecType) return VdecCoreErrorInvalidCodecType;
    auto created = std::make_unique<Decoder>();
    created->codecType = config->codecType;
    created->geometry = FrameGeometry(*config);
    OpenCodec(*created);
    void* handle = created.get();
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.live.emplace(handle, std::move(created));
    *decoder = handle;
    return 0;
}

int APS5_VABI sceVdecCoreDeleteDecoder(void* decoder) {
    if (decoder == nullptr) return VdecCoreErrorInvalidArgument;
    std::unique_ptr<Decoder> removed;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        const auto found = Find(state, decoder, __func__);
        removed = std::move(found->second);
        state.live.erase(found);
    }
    return 0;
}

int APS5_VABI sceVdecCoreFinalizeComputeResource() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreFlushDecodeOutput() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreGetDecodeOutput() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreInitializeComputeResource() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreMapMemoryBlock(void* decoder, const VdecCoreMemoryBlock* blocks, std::uint32_t count) {
    if (decoder == nullptr) return VdecCoreErrorInvalidArgument;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto& target = *Find(state, decoder, __func__)->second;
    if (blocks == nullptr || count == 0) return 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        const VdecCoreMemoryBlock block = blocks[index];
        target.mappedBlocks.push_back(block);
        auto* base = static_cast<std::uint8_t*>(block.address);
        const std::uint32_t slotSize = target.geometry.frameBufferSize;
        if (base == nullptr || slotSize == 0 || block.size < slotSize) continue;
        const std::uint64_t slots = block.size / slotSize;
        for (std::uint64_t slot = 0; slot < slots; ++slot) target.frameBufferSlots.push_back(base + slot * slotSize);
    }
    return 0;
}

int APS5_VABI sceVdecCoreQueryComputeResourceInfo(VdecCoreComputeResourceInfo* info) {
    if (info == nullptr) return VdecCoreErrorInvalidArgument;
    info->memoryType = 0;
    info->size = ComputeSize;
    info->alignment = ComputeAlignment;
    info->reserved = 0;
    return 0;
}

int APS5_VABI sceVdecCoreQueryFrameBufferInfo(const VdecCoreConfigInfo* config, VdecCoreFrameBufferInfo* info) {
    if (info == nullptr || config == nullptr) return VdecCoreErrorInvalidArgument;
    if (config->codecType > MaxCodecType) return VdecCoreErrorInvalidCodecType;
    if (config->maxDpbFrameCount < -1 || config->maxDpbFrameCount > MaxDpbFrameCount) return VdecCoreErrorInvalidArgument;
    const Geometry geometry = FrameGeometry(*config);
    info->frameBufferSize = geometry.frameBufferSize;
    info->frameBufferAlignment = FrameBufferAlignment;
    info->minFrameBufferCount = geometry.dpb + 1;
    info->horizontalAlignment = geometry.horizontalAlign;
    info->frameWidth = geometry.width;
    info->frameHeight = geometry.height;
    return 0;
}

int APS5_VABI sceVdecCoreQueryInstanceSize(const VdecCoreConfigInfo* config, std::uint64_t* instanceSize, std::uint64_t* computeSize, std::uint64_t* reservedSize) {
    if (config == nullptr) return VdecCoreErrorInvalidArgument;
    if (config->codecType > MaxCodecType) return VdecCoreErrorInvalidCodecType;
    if (instanceSize != nullptr) *instanceSize = InstanceSize;
    if (computeSize != nullptr) *computeSize = ComputeSize;
    if (reservedSize != nullptr) *reservedSize = 0;
    return 0;
}

int APS5_VABI sceVdecCoreResetDecoder() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreSetDecodeInput() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreSetDecodeOutputSw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreSyncDecode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreSyncDecodeOutputSw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreSyncDecodeWptr() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreTrySyncDecode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreTrySyncDecodeOutputSw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreTrySyncDecodeWptr() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
