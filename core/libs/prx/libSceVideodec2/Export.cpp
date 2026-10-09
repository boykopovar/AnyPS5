#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <cerrno>
#include <limits>
#include <stdexcept>
#include "prx/libc/include/General.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace {

constexpr std::uint64_t WorkMemoryBytes = 1u << 20u;
constexpr std::uint32_t PitchAlignment = 256;
constexpr std::uint32_t HeightAlignment = 32;
constexpr std::uint32_t CodecAvc = 1;
constexpr std::uint32_t CodecHevc = 974921;
constexpr std::uint32_t CodecVp9 = 2382845;

struct ComputeMemoryInfo {
    std::uint64_t thisSize;
    std::uint64_t cpuGpuMemorySize;
    void* cpuGpuMemory;
};

struct ComputeConfigInfo {
    std::uint64_t thisSize;
    std::uint16_t computePipeId;
    std::uint16_t computeQueueId;
    bool checkMemoryType;
};

struct DecoderConfigInfo {
    std::uint64_t thisSize;
    std::uint32_t resourceType;
    std::uint32_t codecType;
    std::uint32_t profile;
    std::uint32_t maxLevel;
    std::int32_t maxFrameWidth;
    std::int32_t maxFrameHeight;
    std::int32_t maxDpbFrameCount;
    std::uint32_t decodePipelineDepth;
};

struct DecoderMemoryInfo {
    std::uint64_t thisSize;
    std::uint64_t cpuMemorySize;
    void* cpuMemory;
    std::uint64_t gpuMemorySize;
    void* gpuMemory;
    std::uint64_t cpuGpuMemorySize;
    void* cpuGpuMemory;
    std::uint64_t maxFrameBufferSize;
    std::uint32_t frameBufferAlignment;
};

struct InputData {
    std::uint64_t thisSize;
    const std::uint8_t* auData;
    std::uint64_t auSize;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
};

struct FrameBuffer {
    std::uint64_t thisSize;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
    bool isAccepted;
};

struct OutputInfo {
    std::uint64_t thisSize;
    bool isValid;
    bool isErrorFrame;
    std::uint8_t pictureCount;
    std::uint32_t codecType;
    std::uint32_t frameWidth;
    std::uint32_t framePitch;
    std::uint32_t frameHeight;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
    std::uint32_t frameFormat;
    std::uint32_t framePitchInBytes;
};

struct AvcPictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint8_t idrPictureFlag;
    std::uint8_t profileIdc;
    std::uint8_t levelIdc;
    std::uint32_t picWidthInLumaSamples;
    std::uint32_t picHeightInLumaSamples;
};

struct Picture {
    std::uint64_t pts = 0;
    std::uint64_t dts = 0;
    std::uint64_t attached = 0;
    bool idr = false;
};

struct FrameDeleter {
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};
using FramePointer = std::unique_ptr<AVFrame, FrameDeleter>;

struct Decoder {
    std::uint32_t codec = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t profile = 0;
    std::uint32_t level = 0;
    std::map<const void*, Picture> pictures;
    std::uint64_t decoded = 0;
    std::mutex mutex;
    AVCodecContext* context = nullptr;
    SwsContext* scaler = nullptr;
    std::map<std::int64_t, Picture> inputs;
    std::int64_t nextKey = 0;
    std::deque<FramePointer> ready;
    std::vector<std::uint8_t> annexB;
    bool flushing = false;

    ~Decoder() {
        sws_freeContext(scaler);
        avcodec_free_context(&context);
    }
};

std::mutex lock;
std::map<std::uint64_t, std::shared_ptr<Decoder>> decoders;
std::uint64_t nextDecoder = 1;
std::uint64_t nextQueue = 1;

bool Trace() {
    static const bool trace = std::getenv("APS5_TRACE_VIDEODEC") != nullptr;
    return trace;
}

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

std::uint64_t ChromaOffset(std::uint32_t width, std::uint32_t height) {
    return static_cast<std::uint64_t>(AlignUp(width, PitchAlignment)) * height;
}

std::uint64_t LumaBytes(std::uint32_t width, std::uint32_t height) {
    return static_cast<std::uint64_t>(AlignUp(width, PitchAlignment)) * AlignUp(height, HeightAlignment);
}

template <typename TObject>
bool Fits(const TObject* object, std::size_t end) {
    return object->thisSize >= end;
}

std::shared_ptr<Decoder> FindDecoder(std::uint64_t handle) {
    std::lock_guard guard(lock);
    const auto found = decoders.find(handle);
    return found != decoders.end() ? found->second : nullptr;
}

AVCodecID CodecId(std::uint32_t codec) {
    switch (codec) {
        case CodecAvc: return AV_CODEC_ID_H264;
        case CodecHevc: return AV_CODEC_ID_HEVC;
        case CodecVp9: return AV_CODEC_ID_VP9;
        default: throw std::runtime_error("Videodec2: unsupported codec");
    }
}

void OpenCodec(Decoder& decoder) {
    const AVCodec* codec = avcodec_find_decoder(CodecId(decoder.codec));
    if (codec == nullptr) throw std::runtime_error("Videodec2: decoder is unavailable");
    decoder.context = avcodec_alloc_context3(codec);
    if (decoder.context == nullptr) throw std::runtime_error("Videodec2: cannot allocate codec context");
    decoder.context->err_recognition = AV_EF_EXPLODE;
    decoder.context->thread_count = 0;
    decoder.context->thread_type = FF_THREAD_SLICE;
    if (avcodec_open2(decoder.context, codec, nullptr) < 0) {
        avcodec_free_context(&decoder.context);
        throw std::runtime_error("Videodec2: cannot open decoder");
    }
}

const std::vector<std::uint8_t>* ToAnnexB(Decoder& decoder, const std::uint8_t* data, std::size_t size) {
    if (size >= 4 && data[0] == 0 && data[1] == 0 && (data[2] == 1 || (data[2] == 0 && data[3] == 1))) {
        decoder.annexB.assign(data, data + size);
        return &decoder.annexB;
    }
    decoder.annexB.clear();
    for (std::size_t index = 0; index < size;) {
        if (size - index < 4) throw std::runtime_error("Videodec2: truncated NAL length");
        const std::size_t length = (static_cast<std::size_t>(data[index]) << 24u) | (static_cast<std::size_t>(data[index + 1]) << 16u) | (static_cast<std::size_t>(data[index + 2]) << 8u) | data[index + 3];
        if (length == 0 || length > size - index - 4) throw std::runtime_error("Videodec2: invalid NAL length");
        static constexpr std::uint8_t startCode[4] = {0, 0, 0, 1};
        decoder.annexB.insert(decoder.annexB.end(), startCode, startCode + 4);
        decoder.annexB.insert(decoder.annexB.end(), data + index + 4, data + index + 4 + length);
        index += 4 + length;
    }
    return &decoder.annexB;
}

void Receive(Decoder& decoder) {
    for (;;) {
        FramePointer frame(av_frame_alloc());
        if (!frame) throw std::runtime_error("Videodec2: cannot allocate frame");
        const int result = avcodec_receive_frame(decoder.context, frame.get());
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) return;
        if (result < 0 || frame->decode_error_flags != 0) throw std::runtime_error("Videodec2: frame decoding failed");
        decoder.ready.push_back(std::move(frame));
    }
}

void Send(Decoder& decoder, const AVPacket* packet) {
    const int result = avcodec_send_packet(decoder.context, packet);
    if (result < 0) throw std::runtime_error("Videodec2: packet submission failed");
    Receive(decoder);
}

bool WriteNv12(Decoder& decoder, const AVFrame& frame, std::uint8_t* target, std::uint64_t bytes) {
    if (frame.width <= 0 || frame.height <= 0 || frame.width % 2 != 0 || frame.height % 2 != 0) throw std::runtime_error("Videodec2: unsupported frame dimensions");
    const auto width = static_cast<std::uint32_t>(frame.width);
    const auto height = static_cast<std::uint32_t>(frame.height);
    const auto pitch = AlignUp(width, PitchAlignment);
    const auto luma = ChromaOffset(width, height);
    if (luma + static_cast<std::uint64_t>(pitch) * (height / 2) > bytes) return false;
    std::uint8_t* planes[4] = {target, target + luma, nullptr, nullptr};
    const int strides[4] = {static_cast<int>(pitch), static_cast<int>(pitch), 0, 0};
    const auto format = static_cast<AVPixelFormat>(frame.format);
    const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get(format);
    if (descriptor == nullptr || descriptor->comp[0].depth > 8) throw std::runtime_error("Videodec2: output deeper than 8 bits is not implemented");
    if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUVJ420P) {
        for (std::uint32_t row = 0; row < height; ++row) std::memcpy(planes[0] + static_cast<std::size_t>(row) * pitch, frame.data[0] + static_cast<std::ptrdiff_t>(row) * frame.linesize[0], width);
        for (std::uint32_t row = 0; row < height / 2; ++row) {
            const auto* u = frame.data[1] + static_cast<std::ptrdiff_t>(row) * frame.linesize[1];
            const auto* v = frame.data[2] + static_cast<std::ptrdiff_t>(row) * frame.linesize[2];
            auto* uv = planes[1] + static_cast<std::size_t>(row) * pitch;
            for (std::uint32_t column = 0; column < width / 2; ++column) {
                uv[column * 2] = u[column];
                uv[column * 2 + 1] = v[column];
            }
        }
        return true;
    }
    decoder.scaler = sws_getCachedContext(decoder.scaler, frame.width, frame.height, format, frame.width, frame.height, AV_PIX_FMT_NV12, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (decoder.scaler == nullptr) return false;
    return sws_scale(decoder.scaler, frame.data, frame.linesize, 0, frame.height, planes, strides) == frame.height;
}

void EmitDecoded(Decoder& decoder, FrameBuffer* frame, OutputInfo* output) {
    output->isValid = false;
    output->isErrorFrame = false;
    output->pictureCount = 0;
    output->codecType = decoder.codec;
    if (frame) frame->isAccepted = false;
    if (decoder.ready.empty()) return;
    if (!frame || !frame->frameBuffer) throw std::runtime_error("Videodec2: missing output buffer");
    const auto picture = std::move(decoder.ready.front());
    decoder.ready.pop_front();
    const auto key = picture->pts;
    const auto found = decoder.inputs.find(key);
    if (found == decoder.inputs.end()) throw std::runtime_error("Videodec2: missing picture timestamps");
    auto timestamps = found->second;
    decoder.inputs.erase(found);
    timestamps.idr = (picture->flags & AV_FRAME_FLAG_KEY) != 0;
    decoder.width = static_cast<std::uint32_t>(picture->width);
    decoder.height = static_cast<std::uint32_t>(picture->height);
    if (decoder.context->profile > 0) decoder.profile = static_cast<std::uint32_t>(decoder.context->profile) & 0xffu;
    if (decoder.context->level > 0) decoder.level = static_cast<std::uint32_t>(decoder.context->level);
    if (!WriteNv12(decoder, *picture, static_cast<std::uint8_t*>(frame->frameBuffer), frame->frameBufferSize)) throw std::runtime_error("Videodec2: NV12 conversion failed");
    const auto pitch = AlignUp(decoder.width, PitchAlignment);
    frame->isAccepted = true;
    output->isValid = true;
    output->isErrorFrame = false;
    output->pictureCount = 1;
    output->frameWidth = decoder.width;
    output->framePitch = pitch;
    output->frameHeight = decoder.height;
    output->frameBuffer = frame->frameBuffer;
    output->frameBufferSize = frame->frameBufferSize;
    if (Fits(output, offsetof(OutputInfo, framePitchInBytes) + 4)) {
        output->frameFormat = 0;
        output->framePitchInBytes = pitch;
    }
    decoder.pictures[frame->frameBuffer] = timestamps;
}

}

extern "C" {

int APS5_VABI sceVideodec2QueryComputeMemoryInfo_nid_postfix(ComputeMemoryInfo* info) {
    if (!info) throw std::runtime_error("Videodec2: invalid argument pointer");
    if (Trace()) std::fprintf(stderr, "[videodec2] QueryComputeMemoryInfo size=%llu\n", static_cast<unsigned long long>(info->thisSize));
    info->cpuGpuMemorySize = WorkMemoryBytes;
    return 0;
}

int APS5_VABI sceVideodec2AllocateComputeQueue_nid_postfix(const ComputeConfigInfo* config, const ComputeMemoryInfo* memory, std::uint64_t* queue) {
    if (!config || !memory || !queue) throw std::runtime_error("Videodec2: invalid argument pointer");
    if (Trace()) std::fprintf(stderr, "[videodec2] AllocateComputeQueue config=%llu memory=%llu\n", static_cast<unsigned long long>(config->thisSize), static_cast<unsigned long long>(memory->thisSize));
    std::lock_guard guard(lock);
    *queue = nextQueue++;
    return 0;
}

int APS5_VABI sceVideodec2ReleaseComputeQueue_nid_postfix(std::uint64_t queue) {
    (void)queue;
    return 0;
}

int APS5_VABI sceVideodec2QueryDecoderMemoryInfo_nid_postfix(const DecoderConfigInfo* config, DecoderMemoryInfo* memory) {
    if (!config || !memory) throw std::runtime_error("Videodec2: invalid argument pointer");
    if (Trace()) std::fprintf(stderr, "[videodec2] QueryDecoderMemoryInfo config=%llu memory=%llu codec=%u profile=%u %dx%d dpb=%d\n", static_cast<unsigned long long>(config->thisSize), static_cast<unsigned long long>(memory->thisSize), config->codecType, config->profile, config->maxFrameWidth, config->maxFrameHeight, config->maxDpbFrameCount);
    memory->cpuMemorySize = WorkMemoryBytes;
    memory->gpuMemorySize = WorkMemoryBytes;
    memory->cpuGpuMemorySize = WorkMemoryBytes;
    if (config->maxFrameWidth <= 0 || config->maxFrameHeight <= 0) throw std::runtime_error("Videodec2: invalid frame dimensions");
    const auto width = static_cast<std::uint32_t>(config->maxFrameWidth);
    const auto height = static_cast<std::uint32_t>(config->maxFrameHeight);
    memory->maxFrameBufferSize = LumaBytes(width, height) * 3 / 2;
    memory->frameBufferAlignment = PitchAlignment;
    return 0;
}

int APS5_VABI sceVideodec2CreateDecoder_nid_postfix(const DecoderConfigInfo* config, const DecoderMemoryInfo* memory, std::uint64_t* handle) {
    if (!config || !memory || !handle) throw std::runtime_error("Videodec2: invalid argument pointer");
    CodecId(config->codecType);
    auto decoder = std::make_shared<Decoder>();
    decoder->codec = config->codecType;
    OpenCodec(*decoder);
    std::lock_guard guard(lock);
    *handle = nextDecoder++;
    decoders.emplace(*handle, decoder);
    std::fprintf(stderr, "[videodec2] decoder %llu: codec=%u max %dx%d (%s)\n", static_cast<unsigned long long>(*handle), config->codecType, config->maxFrameWidth, config->maxFrameHeight, decoder->context->codec->long_name);
    return 0;
}

int APS5_VABI sceVideodec2DeleteDecoder_nid_postfix(std::uint64_t handle) {
    std::lock_guard guard(lock);
    if (decoders.erase(handle) == 0) throw std::runtime_error("Videodec2: invalid decoder handle");
    return 0;
}

int APS5_VABI sceVideodec2Decode_nid_postfix(std::uint64_t handle, const InputData* input, FrameBuffer* frame, OutputInfo* output) {
    if (!input || !output) throw std::runtime_error("Videodec2: invalid argument pointer");
    const auto found = FindDecoder(handle);
    if (!found) throw std::runtime_error("Videodec2: invalid decoder handle");
    auto& decoder = *found;
    std::lock_guard guard(decoder.mutex);
    if (Trace() && decoder.decoded < 3) std::fprintf(stderr, "[videodec2] Decode input=%llu frame=%llu output=%llu au=%llu pts=%llu size=%ux%u fb=%p/%llu\n", static_cast<unsigned long long>(input->thisSize), static_cast<unsigned long long>(frame ? frame->thisSize : 0), static_cast<unsigned long long>(output->thisSize), static_cast<unsigned long long>(input->auSize), static_cast<unsigned long long>(input->ptsData), decoder.width, decoder.height, frame ? frame->frameBuffer : nullptr, static_cast<unsigned long long>(frame ? frame->frameBufferSize : 0));
    ++decoder.decoded;
    const Picture picture{input->ptsData, input->dtsData, input->attachedData};
    if (decoder.flushing) throw std::runtime_error("Videodec2: reset required after flushing");
    if (!input->auData || input->auSize == 0 || input->auSize > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) throw std::runtime_error("Videodec2: invalid access unit");
    if (decoder.codec == CodecVp9) decoder.annexB.assign(input->auData, input->auData + input->auSize);
    const auto* unit = decoder.codec == CodecVp9 ? &decoder.annexB : ToAnnexB(decoder, input->auData, static_cast<std::size_t>(input->auSize));
    AVPacket* packet = av_packet_alloc();
    if (!packet) throw std::runtime_error("Videodec2: cannot allocate packet");
    const auto freePacket = [](AVPacket* value) { av_packet_free(&value); };
    const std::unique_ptr<AVPacket, decltype(freePacket)> packetOwner(packet, freePacket);
    if (av_new_packet(packet, static_cast<int>(unit->size())) < 0) throw std::runtime_error("Videodec2: cannot allocate packet data");
    std::memcpy(packet->data, unit->data(), unit->size());
    packet->pts = decoder.nextKey;
    decoder.inputs.emplace(decoder.nextKey++, picture);
    Send(decoder, packet);
    EmitDecoded(decoder, frame, output);
    return 0;
}

int APS5_VABI sceVideodec2Flush_nid_postfix(std::uint64_t handle, FrameBuffer* frame, OutputInfo* output) {
    if (!output) throw std::runtime_error("Videodec2: invalid argument pointer");
    const auto found = FindDecoder(handle);
    if (!found) throw std::runtime_error("Videodec2: invalid decoder handle");
    auto& decoder = *found;
    std::lock_guard guard(decoder.mutex);
    if (!decoder.flushing) {
        Send(decoder, nullptr);
        decoder.flushing = true;
    }
    EmitDecoded(decoder, frame, output);
    return 0;
}

int APS5_VABI sceVideodec2Reset_nid_postfix(std::uint64_t handle) {
    const auto found = FindDecoder(handle);
    if (!found) throw std::runtime_error("Videodec2: invalid decoder handle");
    auto& decoder = *found;
    std::lock_guard guard(decoder.mutex);
    decoder.pictures.clear();
    avcodec_flush_buffers(decoder.context);
    decoder.ready.clear();
    decoder.inputs.clear();
    decoder.flushing = false;
    return 0;
}

int APS5_VABI sceVideodec2GetPictureInfo_nid_postfix(const OutputInfo* output, AvcPictureInfo* first, void* second) {
    (void)second;
    if (!output || !first) throw std::runtime_error("Videodec2: invalid argument pointer");
    std::vector<std::shared_ptr<Decoder>> all;
    {
        std::lock_guard guard(lock);
        for (const auto& [handle, decoder] : decoders) all.push_back(decoder);
    }
    for (const auto& decoder : all) {
        std::lock_guard guard(decoder->mutex);
        const auto found = decoder->pictures.find(output->frameBuffer);
        if (found == decoder->pictures.end()) continue;
        if (decoder->codec != CodecAvc) throw std::runtime_error("Videodec2: picture info is only implemented for H.264");
        const auto& picture = found->second;
        first->isValid = true;
        first->ptsData = picture.pts;
        first->dtsData = picture.dts;
        first->attachedData = picture.attached;
        if (Fits(first, offsetof(AvcPictureInfo, picHeightInLumaSamples) + 4)) {
            first->idrPictureFlag = picture.idr ? 1 : 0;
            first->profileIdc = static_cast<std::uint8_t>(decoder->profile);
            first->levelIdc = static_cast<std::uint8_t>(decoder->level);
            first->picWidthInLumaSamples = output->frameWidth;
            first->picHeightInLumaSamples = output->frameHeight;
        }
        return 0;
    }
    first->isValid = false;
    return 0;
}

int APS5_VABI sceVideodec2GetAvcPictureInfo_nid_postfix(const OutputInfo* output, AvcPictureInfo* first, AvcPictureInfo* second) {
    return sceVideodec2GetPictureInfo_nid_postfix(output, first, second);
}

}
