// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libSceVdecCore/include/Api.hpp"
#include "prx/libSceVdecCore/include/Decoder.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace {

constexpr int VdecCoreErrorInvalidArgument = static_cast<int>(0x80C00001);
constexpr int VdecCoreErrorInvalidCodecType = static_cast<int>(0x80C00003);
constexpr int VdecCoreErrorNoPicture = static_cast<int>(0x80C00010);
constexpr int VdecCoreErrorFatal = static_cast<int>(0x80C00012);
constexpr int VdecCoreErrorDecodeRunning = static_cast<int>(0x80C00018);
constexpr std::int64_t VdecCorePtsInvalid = -1;
constexpr std::uint32_t CodecHevc = 4;
constexpr std::uint32_t MaxCodecType = 4;
constexpr std::uint32_t MaxDecoderType = 8;
constexpr std::int32_t MaxDpbFrameCount = 16;
constexpr std::uint32_t DefaultFrameWidth = 1920;
constexpr std::uint32_t DefaultFrameHeight = 1080;
constexpr std::uint32_t DefaultHorizontalAlign = 256;
constexpr std::uint32_t FrameHeightAlign = 16;
constexpr std::uint32_t FrameWidthAlign = 16;
constexpr std::uint32_t OutputPitchAlign = 64;
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

struct Command {
    enum class Type {
        Packet,
        Reset,
        Flush,
    };
    Type type = Type::Packet;
    AVPacket* packet = nullptr;
    std::shared_ptr<std::atomic<bool>> done;
};

struct Decoder {
    std::uint32_t codecType = 0;
    Geometry geometry{};
    AVCodecContext* context = nullptr;
    SwsContext* scaler = nullptr;
    AVFrame* decodedFrame = nullptr;
    std::vector<VdecCoreMemoryBlock> mappedBlocks;
    std::vector<void*> frameBufferSlots;
    std::uint32_t currentSlot = 0;
    std::vector<std::uint8_t> fallbackFrameBuffer;
    std::int64_t lastPts = VdecCorePtsInvalid;
    std::int64_t lastDts = VdecCorePtsInvalid;
    std::uint64_t lastUserData = 0;
    std::mutex mutex;
    std::condition_variable inputReady;
    std::condition_variable outputReady;
    std::deque<Command> commands;
    std::deque<AVFrame*> frames;
    bool busy = false;
    std::jthread worker;

    ~Decoder() {
        {
            std::lock_guard lock(mutex);
            worker.request_stop();
        }
        inputReady.notify_all();
        if (worker.joinable()) worker.join();
        for (auto& command : commands) av_packet_free(&command.packet);
        commands.clear();
        for (auto* frame : frames) av_frame_free(&frame);
        frames.clear();
        av_frame_free(&decodedFrame);
        avcodec_free_context(&context);
        sws_freeContext(scaler);
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

Decoder& Lookup(const void* decoder, const char* caller) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return *Find(state, decoder, caller)->second;
}

void DrainFrames(Decoder& decoder) {
    while (true) {
        AVFrame* frame = av_frame_alloc();
        if (frame == nullptr) break;
        if (avcodec_receive_frame(decoder.context, frame) != 0) {
            av_frame_free(&frame);
            break;
        }
        std::lock_guard lock(decoder.mutex);
        decoder.frames.push_back(frame);
    }
}

void RunWorker(std::stop_token stop, Decoder* decoder) {
    while (!stop.stop_requested()) {
        Command command;
        {
            std::unique_lock lock(decoder->mutex);
            decoder->inputReady.wait(lock, [&] { return stop.stop_requested() || !decoder->commands.empty(); });
            if (stop.stop_requested()) break;
            command = std::move(decoder->commands.front());
            decoder->commands.pop_front();
            decoder->busy = true;
        }
        switch (command.type) {
        case Command::Type::Packet: {
            int result = avcodec_send_packet(decoder->context, command.packet);
            if (result == AVERROR_EOF) {
                avcodec_flush_buffers(decoder->context);
                result = avcodec_send_packet(decoder->context, command.packet);
            }
            if (result == AVERROR(EAGAIN)) {
                DrainFrames(*decoder);
                avcodec_send_packet(decoder->context, command.packet);
            }
            av_packet_free(&command.packet);
            DrainFrames(*decoder);
            break;
        }
        case Command::Type::Flush:
            avcodec_send_packet(decoder->context, nullptr);
            DrainFrames(*decoder);
            avcodec_flush_buffers(decoder->context);
            command.done->store(true, std::memory_order_release);
            break;
        case Command::Type::Reset:
            avcodec_flush_buffers(decoder->context);
            {
                std::lock_guard lock(decoder->mutex);
                for (auto* frame : decoder->frames) av_frame_free(&frame);
                decoder->frames.clear();
                decoder->currentSlot = 0;
            }
            command.done->store(true, std::memory_order_release);
            break;
        }
        std::lock_guard lock(decoder->mutex);
        decoder->busy = !decoder->commands.empty();
        decoder->outputReady.notify_all();
    }
}

void RunCommand(Decoder& decoder, Command::Type type, std::uint32_t* count) {
    auto done = std::make_shared<std::atomic<bool>>(false);
    std::unique_lock lock(decoder.mutex);
    decoder.commands.push_back({type, nullptr, done});
    decoder.inputReady.notify_one();
    decoder.outputReady.wait(lock, [&] { return done->load(std::memory_order_acquire); });
    if (count != nullptr) *count = static_cast<std::uint32_t>(decoder.frames.size());
}

AVFrame* ConvertNv12(Decoder& decoder, const AVFrame& frame) {
    AVFrame* converted = av_frame_alloc();
    if (converted == nullptr) throw std::runtime_error("VdecCore: cannot allocate an NV12 frame");
    converted->format = AV_PIX_FMT_NV12;
    converted->width = frame.width;
    converted->height = frame.height;
    converted->pts = frame.pts;
    converted->pkt_dts = frame.pkt_dts;
    converted->opaque = frame.opaque;
    if (av_frame_get_buffer(converted, 0) < 0) {
        av_frame_free(&converted);
        throw std::runtime_error("VdecCore: cannot allocate an NV12 frame");
    }
    decoder.scaler = sws_getCachedContext(decoder.scaler, frame.width, frame.height, static_cast<AVPixelFormat>(frame.format), frame.width, frame.height, AV_PIX_FMT_NV12, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (decoder.scaler == nullptr || sws_scale(decoder.scaler, frame.data, frame.linesize, 0, frame.height, converted->data, converted->linesize) != frame.height) {
        av_frame_free(&converted);
        throw std::runtime_error("VdecCore: cannot convert a decoded picture to NV12");
    }
    return converted;
}

void CopyNv12(std::uint8_t* destination, const AVFrame& frame) {
    const auto width = static_cast<std::uint32_t>(frame.width);
    const auto height = static_cast<std::uint32_t>(frame.height);
    const std::size_t pitch = AlignUp(width, OutputPitchAlign);
    const std::size_t alignedHeight = AlignUp(height, FrameHeightAlign);
    std::uint8_t* luma = destination;
    std::uint8_t* chroma = destination + pitch * alignedHeight;
    const auto lumaRow = [&](std::size_t row) { return frame.data[0] + row * static_cast<std::size_t>(frame.linesize[0]); };
    const auto chromaRow = [&](std::size_t row) { return frame.data[1] + row * static_cast<std::size_t>(frame.linesize[1]); };
    for (std::size_t row = 0; row < height; ++row) std::memcpy(luma + row * pitch, lumaRow(row), width);
    for (std::size_t row = 0; row < height / 2; ++row) std::memcpy(chroma + row * pitch, chromaRow(row), width);
    for (std::size_t row = height; row < alignedHeight && height > 0; ++row) std::memcpy(luma + row * pitch, lumaRow(height - 1), width);
    for (std::size_t row = height / 2; row < alignedHeight / 2 && height / 2 > 0; ++row) std::memcpy(chroma + row * pitch, chromaRow(height / 2 - 1), width);
}

std::uint64_t PictureTime(std::int64_t decoded, std::int64_t last) {
    return static_cast<std::uint64_t>(decoded != AV_NOPTS_VALUE ? decoded : last);
}

}

extern "C" {

std::uint32_t VdecCoreFrameBufferSlots_nid_no_patch(const void* decoder, void** slots, std::uint32_t capacity) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto& target = *Find(state, decoder, "VdecCoreFrameBufferSlots")->second;
    std::lock_guard decoderLock(target.mutex);
    const auto& found = target.frameBufferSlots;
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
    created->decodedFrame = av_frame_alloc();
    created->worker = std::jthread(RunWorker, created.get());
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

int APS5_VABI sceVdecCoreFlushDecodeOutput(void* decoder, std::uint32_t* count, void*) {
    if (decoder == nullptr) return VdecCoreErrorInvalidArgument;
    RunCommand(Lookup(decoder, __func__), Command::Type::Flush, count);
    return 0;
}

int APS5_VABI sceVdecCoreGetDecodeOutput(void* decoder, VdecCoreFrameBuffer* frameBuffer, std::uint32_t, VdecCoreOutputInfo* output, void*) {
    if (decoder == nullptr || output == nullptr) return VdecCoreErrorInvalidArgument;
    auto& target = Lookup(decoder, __func__);
    AVFrame* frame = nullptr;
    {
        std::lock_guard lock(target.mutex);
        if (target.frames.empty()) {
            output->frameWidth = target.geometry.width;
            output->framePitch = AlignUp(target.geometry.width, OutputPitchAlign);
            output->frameHeight = target.geometry.height;
            output->pictureCount = 0;
            output->frameBuffer = nullptr;
            output->frameFormat = 0;
            output->reserved0 = 0;
            output->pictures[0].frameBuffer = nullptr;
            output->pictures[1].frameBuffer = nullptr;
            output->lumaBuffer = nullptr;
            output->chromaBuffer = nullptr;
            return VdecCoreErrorNoPicture;
        }
        frame = target.frames.front();
        target.frames.pop_front();
    }
    struct Release {
        AVFrame*& frame;
        ~Release() { av_frame_free(&frame); }
    } release{frame};
    if (frame->format != AV_PIX_FMT_NV12) {
        AVFrame* converted = ConvertNv12(target, *frame);
        av_frame_free(&frame);
        frame = converted;
    }
    const auto width = static_cast<std::uint32_t>(frame->width);
    const auto height = static_cast<std::uint32_t>(frame->height);
    const std::uint32_t alignedWidth = AlignUp(width, FrameWidthAlign);
    const std::uint32_t pitch = AlignUp(width, OutputPitchAlign);
    const std::uint32_t alignedHeight = AlignUp(height, FrameHeightAlign);
    const std::uint32_t pictureSize = pitch * alignedHeight * 3 / 2;
    const std::uint32_t sarWidth = frame->sample_aspect_ratio.num > 0 ? static_cast<std::uint32_t>(frame->sample_aspect_ratio.num) : 1;
    const std::uint32_t sarHeight = frame->sample_aspect_ratio.den > 0 ? static_cast<std::uint32_t>(frame->sample_aspect_ratio.den) : 1;
    std::uint8_t* destination = nullptr;
    std::uint64_t capacity = 0;
    std::int64_t lastPts = 0;
    std::int64_t lastDts = 0;
    {
        std::lock_guard lock(target.mutex);
        lastPts = target.lastPts;
        lastDts = target.lastDts;
        if (frameBuffer != nullptr && frameBuffer->frameBuffer != nullptr) {
            destination = static_cast<std::uint8_t*>(frameBuffer->frameBuffer);
            capacity = frameBuffer->frameBufferSize > 0 ? frameBuffer->frameBufferSize : target.geometry.frameBufferSize;
        } else if (!target.frameBufferSlots.empty()) {
            destination = static_cast<std::uint8_t*>(target.frameBufferSlots[target.currentSlot % target.frameBufferSlots.size()]);
            ++target.currentSlot;
            capacity = target.geometry.frameBufferSize;
        } else {
            const std::uint64_t requested = frameBuffer != nullptr && frameBuffer->frameBufferSize > 0 ? frameBuffer->frameBufferSize : target.geometry.frameBufferSize;
            capacity = std::max<std::uint64_t>(std::uint64_t{pictureSize} + sizeof(VdecCoreHwPictureInfo), requested);
            if (target.fallbackFrameBuffer.size() < capacity) target.fallbackFrameBuffer.resize(static_cast<std::size_t>(capacity));
            destination = target.fallbackFrameBuffer.data();
        }
    }
    if (pictureSize > capacity) throw std::runtime_error("VdecCore: a decoded picture is larger than its frame buffer");
    CopyNv12(destination, *frame);
    const std::uint64_t pts = PictureTime(frame->pts, lastPts);
    const std::uint64_t dts = PictureTime(frame->pkt_dts, lastDts);
    const auto userData = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(frame->opaque));
    if (capacity - pictureSize >= sizeof(VdecCoreHwPictureInfo)) {
        auto* info = reinterpret_cast<VdecCoreHwPictureInfo*>(destination + capacity - sizeof(VdecCoreHwPictureInfo));
        std::memset(info, 0, sizeof(*info));
        info->codecType = target.codecType == CodecHevc ? 1 : 0;
        info->userData = userData;
        info->pts = pts;
        info->dts = dts;
        info->valid = 1;
        info->width = alignedWidth;
        info->height = alignedHeight;
        info->cropLeft = 0;
        info->cropRight = pitch > width ? (pitch - width) / 2 : 0;
        info->cropTop = 0;
        info->cropBottom = alignedHeight > height ? (alignedHeight - height) / 2 : 0;
        info->sarWidth = sarWidth;
        info->sarHeight = sarHeight;
        info->flags = 0x01;
        if (info->cropRight > 0 || info->cropBottom > 0) info->flags |= 0x02;
    }
    output->frameWidth = alignedWidth;
    output->framePitch = pitch;
    output->frameHeight = alignedHeight;
    output->pictureCount = 1;
    output->frameBuffer = destination;
    output->frameFormat = 0;
    output->reserved0 = 0;
    output->pictures[0].flags = target.codecType;
    output->pictures[0].reserved0 = 0;
    output->pictures[0].pts = pts;
    output->pictures[0].dts = dts;
    output->pictures[0].userData = userData;
    output->pictures[0].pictureInfo = nullptr;
    output->pictures[0].frameBuffer = destination;
    output->pictures[0].frameBufferSize = pictureSize;
    output->pictures[0].framePitch = pitch;
    output->pictures[0].reserved1 = 0;
    output->pictures[1].frameBuffer = nullptr;
    output->aspectRatio = 1;
    output->sarWidth = sarWidth;
    output->videoFormat = 5;
    output->colourPrimaries = 1;
    output->transferCharacteristics = 1;
    output->framePitchInBytes = pitch;
    output->lumaBuffer = destination;
    output->chromaBuffer = destination + static_cast<std::size_t>(pitch) * alignedHeight;
    output->videoFullRangeFlag = frame->color_range == AVCOL_RANGE_JPEG ? 1 : 0;
    output->colourDescriptionPresentFlag = 1;
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
    std::lock_guard decoderLock(target.mutex);
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

int APS5_VABI sceVdecCoreResetDecoder(void* decoder, void*) {
    if (decoder == nullptr) return VdecCoreErrorInvalidArgument;
    RunCommand(Lookup(decoder, __func__), Command::Type::Reset, nullptr);
    return 0;
}

int APS5_VABI sceVdecCoreSetDecodeInput(void* decoder, const VdecCoreInputData* input, void*, std::uint32_t* status) {
    if (decoder == nullptr || input == nullptr) return VdecCoreErrorInvalidArgument;
    auto& target = Lookup(decoder, __func__);
    if (status != nullptr) {
        status[0] = 1;
        status[1] = 1;
    }
    if (input->auData == nullptr || input->auSize == 0) return VdecCoreErrorInvalidArgument;
    const std::int64_t pts = input->pts == VdecCorePtsInvalid ? AV_NOPTS_VALUE : input->pts;
    const std::int64_t dts = input->dts == VdecCorePtsInvalid ? AV_NOPTS_VALUE : input->dts;
    AVPacket* packet = av_packet_alloc();
    if (packet == nullptr) return VdecCoreErrorFatal;
    if (av_new_packet(packet, static_cast<int>(input->auSize)) < 0) {
        av_packet_free(&packet);
        return VdecCoreErrorFatal;
    }
    std::memcpy(packet->data, input->auData, static_cast<std::size_t>(input->auSize));
    packet->pts = pts;
    packet->dts = dts;
    packet->opaque = reinterpret_cast<void*>(static_cast<std::uintptr_t>(input->userData));
    {
        std::lock_guard lock(target.mutex);
        target.lastPts = input->pts;
        target.lastDts = input->dts;
        target.lastUserData = input->userData;
        target.commands.push_back({Command::Type::Packet, packet, nullptr});
        target.busy = true;
    }
    target.inputReady.notify_one();
    return 0;
}

int APS5_VABI sceVdecCoreSetDecodeOutputSw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreSyncDecode(void* decoder, std::uint32_t* count) {
    if (decoder == nullptr) return VdecCoreErrorInvalidArgument;
    auto& target = Lookup(decoder, __func__);
    std::unique_lock lock(target.mutex);
    target.outputReady.wait(lock, [&] { return !target.frames.empty() || (!target.busy && target.commands.empty()); });
    if (count != nullptr) *count = static_cast<std::uint32_t>(target.frames.size());
    return 0;
}

int APS5_VABI sceVdecCoreSyncDecodeOutputSw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreSyncDecodeWptr(void* decoder, std::uint32_t* count, void*, void*) {
    return sceVdecCoreSyncDecode(decoder, count);
}

int APS5_VABI sceVdecCoreTrySyncDecode(void* decoder, std::uint32_t* count) {
    if (decoder == nullptr) return VdecCoreErrorInvalidArgument;
    auto& target = Lookup(decoder, __func__);
    std::lock_guard lock(target.mutex);
    if (!target.frames.empty()) {
        if (count != nullptr) *count = static_cast<std::uint32_t>(target.frames.size());
        return 0;
    }
    if (count != nullptr) *count = 0;
    if (target.busy || !target.commands.empty()) return VdecCoreErrorDecodeRunning;
    return 0;
}

int APS5_VABI sceVdecCoreTrySyncDecodeOutputSw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVdecCoreTrySyncDecodeWptr(void* decoder, std::uint32_t* count, void*, void*) {
    return sceVdecCoreTrySyncDecode(decoder, count);
}

}
