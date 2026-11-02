#include "prx/libSceVideodec2/include/Api.hpp"
#include "prx/libSceVideodec2/include/PictureMetadata.hpp"
#include <array>
#include <span>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>
#include <cerrno>
#include <limits>
#include <stdexcept>
#include "prx/libc/include/General.hpp"
#include "AvcMetadata.hpp"
#include "HevcMetadata.hpp"
#include "prx/libc/include/GuestArena.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace {

using namespace SceVideodec2;
using SceVideodec2::Internal::Crop;
using SceVideodec2::Internal::PictureHeader;

struct UnsupportedHevcMetadata {};

constexpr std::uint64_t WorkMemoryBytes = 1u << 20u;
constexpr std::uint32_t PitchAlignment = 256;
constexpr std::uint32_t HeightAlignment = 32;
constexpr std::uint32_t CodecAvc = 1;
constexpr std::uint32_t CodecHevc = 974921;
constexpr std::uint32_t CodecVp9 = 2382845;
constexpr int Videodec2ErrorAccessUnitSize = static_cast<int>(0x811D010Du);
constexpr int Videodec2ErrorAccessUnitPointer = static_cast<int>(0x811D010Eu);
constexpr int Videodec2ErrorStructureSize = static_cast<int>(0x811D0101u);
constexpr int Videodec2ErrorArgumentPointer = static_cast<int>(0x811D0102u);
constexpr int Videodec2ErrorFrameBufferSize = static_cast<int>(0x811D0106u);
constexpr int Videodec2ErrorFrameBufferPointer = static_cast<int>(0x811D0107u);
constexpr int Videodec2ErrorFrameBufferAlignment = static_cast<int>(0x811D0108u);
constexpr int Videodec2ErrorPictureCount = static_cast<int>(0x811D010Fu);

static_assert(sizeof(Vp9PictureInfo) == 88);
static_assert(offsetof(Vp9PictureInfo, colorSpace) == 60);
static_assert(sizeof(OutputInfo) == 0x38 && offsetof(OutputInfo, isDiscardedFrame) == 0xb && offsetof(OutputInfo, codecType) == 0xc && offsetof(OutputInfo, frameBuffer) == 0x20);

class Vp9Bits {
public:
    explicit Vp9Bits(std::span<const std::uint8_t> bytes) : bytes(bytes) {}
    std::uint32_t Read(unsigned count) {
        std::uint32_t result = 0;
        while (count-- != 0) {
            if (position / 8 >= bytes.size()) throw std::runtime_error("Videodec2: truncated VP9 header");
            result = (result << 1u) | ((bytes[position / 8] >> (7u - position % 8)) & 1u);
            ++position;
        }
        return result;
    }
private:
    std::span<const std::uint8_t> bytes;
    std::size_t position = 0;
};

struct Vp9Unit {
    std::span<const std::uint8_t> bytes;
    Vp9PictureInfo info{};
    bool shown = false;
};

Vp9Unit ParseVp9Unit(std::span<const std::uint8_t> bytes, std::array<Vp9PictureInfo, 8>& references, Vp9PictureInfo& colors, std::uint32_t count) {
    Vp9Bits bits(bytes);
    if (bits.Read(2) != 2) throw std::runtime_error("Videodec2: invalid VP9 frame marker");
    const auto low = bits.Read(1);
    const auto profile = low | (bits.Read(1) << 1u);
    if (profile == 3 && bits.Read(1) != 0) throw std::runtime_error("Videodec2: invalid VP9 reserved bit");
    Vp9Unit unit{bytes};
    auto& info = unit.info;
    if (bits.Read(1) != 0) {
        info = references[bits.Read(3)];
        if (!info.isValid) throw std::runtime_error("Videodec2: missing VP9 display reference");
        info.keyFrameFlag = 0;
        info.intraOnly = 0;
        info.profile = profile;
        info.numFrames = count;
        unit.shown = true;
        return unit;
    }
    const bool key = bits.Read(1) == 0;
    unit.shown = bits.Read(1) != 0;
    const bool resilient = bits.Read(1) != 0;
    const bool intra = !key && !unit.shown && bits.Read(1) != 0;
    if (!key && !resilient) bits.Read(2);
    const auto dimensions = [&] {
        info.frameWidth = bits.Read(16) + 1;
        info.frameHeight = bits.Read(16) + 1;
    };
    const auto render = [&] {
        const bool different = bits.Read(1) != 0;
        info.renderWidth = different ? bits.Read(16) + 1 : info.frameWidth;
        info.renderHeight = different ? bits.Read(16) + 1 : info.frameHeight;
    };
    std::uint32_t refresh = 0;
    if (key || intra) {
        if (bits.Read(24) != 0x498342) throw std::runtime_error("Videodec2: invalid VP9 sync code");
        info.bitDepth = 8;
        info.colorSpace = 1;
        if (key || profile != 0) {
            if (profile >= 2) info.bitDepth = bits.Read(1) != 0 ? 12 : 10;
            info.colorSpace = bits.Read(3);
            if (info.colorSpace != 7) {
                info.colorRange = static_cast<std::uint8_t>(bits.Read(1));
                if (profile == 1 || profile == 3) {
                    bits.Read(2);
                    if (bits.Read(1) != 0) throw std::runtime_error("Videodec2: invalid VP9 color reserved bit");
                }
            } else {
                info.colorRange = 1;
                if (profile == 1 || profile == 3) {
                    if (bits.Read(1) != 0) throw std::runtime_error("Videodec2: invalid VP9 RGB reserved bit");
                } else throw std::runtime_error("Videodec2: invalid VP9 RGB profile");
            }
        }
        refresh = key ? 255 : bits.Read(8);
        dimensions();
    } else {
        refresh = bits.Read(8);
        std::array<unsigned, 3> indexes{};
        for (auto& index : indexes) { index = bits.Read(3); bits.Read(1); }
        info = colors;
        if (!info.isValid) throw std::runtime_error("Videodec2: missing VP9 color configuration");
        bool inherited = false;
        for (const auto index : indexes) {
            if (bits.Read(1) != 0) {
                if (!references[index].isValid) throw std::runtime_error("Videodec2: missing VP9 size reference");
                info.frameWidth = references[index].frameWidth;
                info.frameHeight = references[index].frameHeight;
                inherited = true;
                break;
            }
        }
        if (!inherited) dimensions();
    }
    render();
    info.isValid = true;
    info.profile = profile;
    info.keyFrameFlag = key ? 1 : 0;
    info.intraOnly = intra ? 1 : 0;
    info.numFrames = count;
    if (key || intra) colors = info;
    for (unsigned index = 0; index < references.size(); ++index)
        if ((refresh & (1u << index)) != 0) references[index] = info;
    return unit;
}

std::vector<Vp9Unit> ParseVp9Packet(std::span<const std::uint8_t> bytes, std::array<Vp9PictureInfo, 8>& references, Vp9PictureInfo& colors) {
    std::vector<std::span<const std::uint8_t>> frames;
    const auto marker = bytes.back();
    const unsigned count = (marker & 7u) + 1;
    const unsigned magnitude = ((marker >> 3u) & 3u) + 1;
    const std::size_t indexSize = 2 + count * magnitude;
    if ((marker & 0xe0u) == 0xc0u && bytes.size() >= indexSize && bytes[bytes.size() - indexSize] == marker) {
        const auto end = bytes.size() - indexSize;
        std::size_t offset = 0;
        for (unsigned frame = 0; frame < count; ++frame) {
            std::size_t size = 0;
            for (unsigned byte = 0; byte < magnitude; ++byte)
                size |= static_cast<std::size_t>(bytes[end + 1 + frame * magnitude + byte]) << (8u * byte);
            if (size == 0 || size > end - offset) throw std::runtime_error("Videodec2: invalid VP9 superframe size");
            frames.push_back(bytes.subspan(offset, size));
            offset += size;
        }
        if (offset != end) throw std::runtime_error("Videodec2: VP9 superframe contains unindexed bytes");
    } else frames.push_back(bytes);
    std::vector<Vp9Unit> result;
    for (const auto frame : frames) result.push_back(ParseVp9Unit(frame, references, colors, static_cast<std::uint32_t>(frames.size())));
    return result;
}

struct Picture {
    std::uint64_t pts = 0;
    std::uint64_t dts = 0;
    std::uint64_t attached = 0;
    bool idr = false;
    Vp9PictureInfo vp9{};
    Crop crop{};
    bool hevcIdr = false;
    std::uint8_t bitDepthMinus8 = 0;
    Videodec2Hevc::Metadata hevc;
    Videodec2Avc::Metadata avc;
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
    std::uint32_t sampleBytes = 1;
    std::map<const void*, Picture> pictures;
    std::uint64_t decoded = 0;
    std::mutex mutex;
    AVCodecContext* context = nullptr;
    SwsContext* scaler = nullptr;
    std::map<std::int64_t, Picture> inputs;
    std::int64_t nextKey = 0;
    std::deque<FramePointer> ready;
    std::vector<std::uint8_t> annexB;
    std::vector<std::uint8_t> parameterSets;
    bool lengthPrefixed = false;
    bool shortStartCodes = false;
    bool flushing = false;
    std::array<Vp9PictureInfo, 8> vp9References{};
    Vp9PictureInfo vp9Colors{};
    Videodec2Hevc::State hevc;
    Videodec2Avc::State avc;

    ~Decoder() {
        sws_freeContext(scaler);
        avcodec_free_context(&context);
    }
};

std::mutex lock;
std::map<std::uint64_t, std::shared_ptr<Decoder>> decoders;
std::map<const void*, std::uint64_t> pictureOwners;
std::uint64_t nextDecoder = 1;
std::uint64_t nextQueue = 1;

bool Trace() {
    static const bool trace = std::getenv("APS5_TRACE_VIDEODEC") != nullptr;
    return trace;
}

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

std::uint32_t PitchBytes(std::uint32_t width, std::uint32_t sampleBytes) {
    return AlignUp(width * sampleBytes, PitchAlignment);
}

std::uint64_t ChromaOffset(std::uint32_t width, std::uint32_t height, std::uint32_t sampleBytes) {
    return static_cast<std::uint64_t>(PitchBytes(width, sampleBytes)) * height;
}

std::uint64_t LumaBytes(std::uint32_t width, std::uint32_t height, std::uint32_t sampleBytes) {
    return static_cast<std::uint64_t>(PitchBytes(width, sampleBytes)) * AlignUp(height, HeightAlignment);
}

std::uint32_t ConfiguredSampleBytes(std::uint32_t codec, std::uint32_t profile) {
    return (codec == CodecHevc || codec == CodecVp9) && profile == 2 ? 2 : 1;
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
    decoder.context->apply_cropping = 0;
    if (avcodec_open2(decoder.context, codec, nullptr) < 0) {
        avcodec_free_context(&decoder.context);
        throw std::runtime_error("Videodec2: cannot open decoder");
    }
}

bool TilesAsAvcc(const std::uint8_t* data, std::size_t size) {
    if (size < 4) return false;
    for (std::size_t index = 0; index < size;) {
        if (size - index < 4) return false;
        const std::size_t length = (static_cast<std::size_t>(data[index]) << 24u) | (static_cast<std::size_t>(data[index + 1]) << 16u) | (static_cast<std::size_t>(data[index + 2]) << 8u) | data[index + 3];
        if (length == 0 || length > size - index - 4) return false;
        index += 4 + length;
    }
    return true;
}

bool StartsWithStartCode(const std::uint8_t* data, std::size_t size) {
    return size >= 3 && data[0] == 0 && data[1] == 0 && (data[2] == 1 || (size >= 4 && data[2] == 0 && data[3] == 1));
}

const std::vector<std::uint8_t>* ToAnnexB(Decoder& decoder, const std::uint8_t* data, std::size_t size) {
    const bool tiles = TilesAsAvcc(data, size);
    const bool opensWithStartCode = StartsWithStartCode(data, size);
    if (!tiles && !opensWithStartCode) throw std::runtime_error("Videodec2: access unit is neither Annex-B nor length-prefixed");
    const bool shortStartCode = opensWithStartCode && data[2] == 1;
    bool lengthPrefixed = tiles;
    if (tiles && opensWithStartCode) {
        lengthPrefixed = shortStartCode ? !decoder.shortStartCodes : decoder.lengthPrefixed;
    } else {
        decoder.lengthPrefixed = tiles;
        if (shortStartCode) decoder.shortStartCodes = true;
    }
    if (lengthPrefixed) {
        decoder.annexB.clear();
        for (std::size_t index = 0; index < size;) {
            const std::size_t length = (static_cast<std::size_t>(data[index]) << 24u) | (static_cast<std::size_t>(data[index + 1]) << 16u) | (static_cast<std::size_t>(data[index + 2]) << 8u) | data[index + 3];
            static constexpr std::uint8_t startCode[4] = {0, 0, 0, 1};
            decoder.annexB.insert(decoder.annexB.end(), startCode, startCode + 4);
            decoder.annexB.insert(decoder.annexB.end(), data + index + 4, data + index + 4 + length);
            index += 4 + length;
        }
        return &decoder.annexB;
    }
    decoder.annexB.assign(data, data + size);
    return &decoder.annexB;
}

bool CarriesAvcSlice(const std::vector<std::uint8_t>& unit) {
    for (std::size_t index = 0; index + 3 < unit.size(); ++index) {
        if (unit[index] != 0 || unit[index + 1] != 0 || unit[index + 2] != 1) continue;
        const auto type = unit[index + 3] & 0x1fu;
        if (type >= 1 && type <= 5) return true;
        index += 2;
    }
    return false;
}

bool StartsHevcIdr(const std::vector<std::uint8_t>& unit) {
    for (std::size_t index = 0; index + 3 < unit.size(); ++index) {
        if (unit[index] != 0 || unit[index + 1] != 0 || unit[index + 2] != 1) continue;
        const auto type = (unit[index + 3] >> 1u) & 0x3fu;
        if (type < 32) return type == 19 || type == 20;
        index += 2;
    }
    return false;
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
    const auto format = static_cast<AVPixelFormat>(frame.format);
    const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get(format);
    if (descriptor == nullptr || descriptor->comp[0].depth > 10) throw std::runtime_error("Videodec2: output deeper than 10 bits is not implemented");
    const std::uint32_t sampleBytes = descriptor->comp[0].depth > 8 ? 2 : 1;
    if (sampleBytes == 2 && format != AV_PIX_FMT_YUV420P10LE) throw std::runtime_error("Videodec2: 10-bit output from a decoded format other than yuv420p10le is not implemented");
    decoder.sampleBytes = sampleBytes;
    const auto pitch = PitchBytes(width, sampleBytes);
    const auto luma = ChromaOffset(width, height, sampleBytes);
    if (luma + static_cast<std::uint64_t>(pitch) * (height / 2) > bytes) return false;
    std::uint8_t* planes[4] = {target, target + luma, nullptr, nullptr};
    const int strides[4] = {static_cast<int>(pitch), static_cast<int>(pitch), 0, 0};
    if (sampleBytes == 2) {
        for (std::uint32_t row = 0; row < height; ++row) std::memcpy(planes[0] + static_cast<std::size_t>(row) * pitch, frame.data[0] + static_cast<std::ptrdiff_t>(row) * frame.linesize[0], static_cast<std::size_t>(width) * 2);
        for (std::uint32_t row = 0; row < height / 2; ++row) {
            const auto* u = frame.data[1] + static_cast<std::ptrdiff_t>(row) * frame.linesize[1];
            const auto* v = frame.data[2] + static_cast<std::ptrdiff_t>(row) * frame.linesize[2];
            auto* uv = planes[1] + static_cast<std::size_t>(row) * pitch;
            for (std::uint32_t column = 0; column < width / 2; ++column) {
                std::memcpy(uv + column * 4, u + column * 2, 2);
                std::memcpy(uv + column * 4 + 2, v + column * 2, 2);
            }
        }
        return true;
    }
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

void ClearOutput(const Decoder& decoder, FrameBuffer* frame, OutputInfo* output) {
    output->isValid = false;
    output->isErrorFrame = false;
    output->pictureCount = 0;
    output->codecType = decoder.codec;
    if (frame) frame->isAccepted = false;
}

void EmitDecoded(std::uint64_t handle, Decoder& decoder, FrameBuffer* frame, OutputInfo* output) {
    ClearOutput(decoder, frame, output);
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
    timestamps.crop = {static_cast<std::uint32_t>(picture->crop_left), static_cast<std::uint32_t>(picture->crop_right), static_cast<std::uint32_t>(picture->crop_top), static_cast<std::uint32_t>(picture->crop_bottom)};
    decoder.width = static_cast<std::uint32_t>(picture->width);
    decoder.height = static_cast<std::uint32_t>(picture->height);
    if (decoder.context->profile > 0) decoder.profile = static_cast<std::uint32_t>(decoder.context->profile) & 0xffu;
    if (decoder.context->level > 0) decoder.level = static_cast<std::uint32_t>(decoder.context->level);
    if (!WriteNv12(decoder, *picture, static_cast<std::uint8_t*>(frame->frameBuffer), frame->frameBufferSize)) throw std::runtime_error("Videodec2: NV12 conversion failed");
    timestamps.bitDepthMinus8 = decoder.sampleBytes == 2 ? 2 : 0;
    const auto pitch = PitchBytes(decoder.width, decoder.sampleBytes);
    frame->isAccepted = true;
    output->isValid = true;
    output->isErrorFrame = false;
    output->pictureCount = 1;
    output->isDiscardedFrame = false;
    output->frameWidth = decoder.width;
    output->framePitch = pitch / decoder.sampleBytes;
    output->frameHeight = decoder.height;
    output->frameBuffer = frame->frameBuffer;
    output->frameBufferSize = frame->frameBufferSize;
    if (Fits(output, offsetof(OutputInfo, framePitchInBytes) + 4)) {
        output->frameFormat = 0;
        output->framePitchInBytes = pitch;
    }
    decoder.pictures[frame->frameBuffer] = timestamps;
    std::lock_guard guard(lock);
    if (decoders.contains(handle)) pictureOwners[frame->frameBuffer] = handle;
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
    memory->maxFrameBufferSize = LumaBytes(width, height, ConfiguredSampleBytes(config->codecType, config->profile)) * 3 / 2;
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
    std::erase_if(pictureOwners, [handle](const auto& owner) { return owner.second == handle; });
    return 0;
}

int APS5_VABI sceVideodec2Decode_nid_postfix(std::uint64_t handle, const InputData* input, FrameBuffer* frame, OutputInfo* output) {
    if (!input || !output) throw std::runtime_error("Videodec2: invalid argument pointer");
    const auto found = FindDecoder(handle);
    if (!found) throw std::runtime_error("Videodec2: invalid decoder handle");
    auto& decoder = *found;
    std::lock_guard guard(decoder.mutex);
    if (!input->auData || input->auSize == 0) {
        ClearOutput(decoder, frame, output);
        return input->auData ? Videodec2ErrorAccessUnitSize : Videodec2ErrorAccessUnitPointer;
    }
    if (Trace() && decoder.decoded < 3) std::fprintf(stderr, "[videodec2] Decode input=%llu frame=%llu output=%llu au=%llu pts=%llu size=%ux%u fb=%p/%llu\n", static_cast<unsigned long long>(input->thisSize), static_cast<unsigned long long>(frame ? frame->thisSize : 0), static_cast<unsigned long long>(output->thisSize), static_cast<unsigned long long>(input->auSize), static_cast<unsigned long long>(input->ptsData), decoder.width, decoder.height, frame ? frame->frameBuffer : nullptr, static_cast<unsigned long long>(frame ? frame->frameBufferSize : 0));
    ++decoder.decoded;
    const Picture picture{input->ptsData, input->dtsData, input->attachedData};
    if (decoder.flushing) {
        avcodec_flush_buffers(decoder.context);
        decoder.hevc.afterEnd = true;
        decoder.flushing = false;
    }
    if (input->auSize > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) throw std::runtime_error("Videodec2: invalid access unit");
    if (decoder.codec == CodecVp9) {
        auto references = decoder.vp9References;
        auto colors = decoder.vp9Colors;
        const auto units = ParseVp9Packet({input->auData, static_cast<std::size_t>(input->auSize)}, references, colors);
        for (const auto& unit : units) {
            AVPacket* packet = av_packet_alloc();
            if (!packet) throw std::runtime_error("Videodec2: cannot allocate VP9 packet");
            const auto freePacket = [](AVPacket* value) { av_packet_free(&value); };
            const std::unique_ptr<AVPacket, decltype(freePacket)> packetOwner(packet, freePacket);
            if (av_new_packet(packet, static_cast<int>(unit.bytes.size())) < 0) throw std::runtime_error("Videodec2: cannot allocate VP9 packet data");
            std::memcpy(packet->data, unit.bytes.data(), unit.bytes.size());
            packet->pts = decoder.nextKey;
            auto tagged = picture;
            tagged.vp9 = unit.info;
            tagged.vp9.ptsData = picture.pts;
            tagged.vp9.dtsData = picture.dts;
            tagged.vp9.attachedData = picture.attached;
            const auto key = decoder.nextKey++;
            decoder.inputs.emplace(key, tagged);
            Send(decoder, packet);
            if (!unit.shown) decoder.inputs.erase(key);
        }
        decoder.vp9References = references;
        decoder.vp9Colors = colors;
        EmitDecoded(handle, decoder, frame, output);
        return 0;
    }
    const auto* unit = ToAnnexB(decoder, input->auData, static_cast<std::size_t>(input->auSize));
    auto avc = decoder.codec == CodecAvc ? Videodec2Avc::Parse_nid_no_patch(decoder.avc, *unit) : Videodec2Avc::Metadata{};
    if (decoder.codec == CodecAvc && !CarriesAvcSlice(*unit)) {
        decoder.parameterSets.insert(decoder.parameterSets.end(), unit->begin(), unit->end());
        EmitDecoded(handle, decoder, frame, output);
        return 0;
    }
    AVPacket* packet = av_packet_alloc();
    if (!packet) throw std::runtime_error("Videodec2: cannot allocate packet");
    const auto freePacket = [](AVPacket* value) { av_packet_free(&value); };
    const std::unique_ptr<AVPacket, decltype(freePacket)> packetOwner(packet, freePacket);
    if (decoder.parameterSets.size() + unit->size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::runtime_error("Videodec2: invalid access unit");
    if (av_new_packet(packet, static_cast<int>(decoder.parameterSets.size() + unit->size())) < 0) throw std::runtime_error("Videodec2: cannot allocate packet data");
    if (!decoder.parameterSets.empty()) std::memcpy(packet->data, decoder.parameterSets.data(), decoder.parameterSets.size());
    std::memcpy(packet->data + decoder.parameterSets.size(), unit->data(), unit->size());
    decoder.parameterSets.clear();
    packet->pts = decoder.nextKey;
    auto tagged = picture;
    tagged.hevcIdr = decoder.codec == CodecHevc && StartsHevcIdr(*unit);
    if (decoder.codec == CodecHevc) {
        tagged.hevc = Videodec2Hevc::Parse_nid_no_patch(decoder.hevc, *unit);
        tagged.hevc.info.ptsData = picture.pts;
        tagged.hevc.info.dtsData = picture.dts;
        tagged.hevc.info.attachedData = picture.attached;
    }
    if (decoder.codec == CodecAvc) {
        tagged.avc = std::move(avc);
        tagged.avc.info.ptsData = picture.pts;
        tagged.avc.info.dtsData = picture.dts;
        tagged.avc.info.attachedData = picture.attached;
    }
    decoder.inputs.emplace(decoder.nextKey++, tagged);
    Send(decoder, packet);
    EmitDecoded(handle, decoder, frame, output);
    return 0;
}

int APS5_VABI sceVideodec2Flush_nid_postfix(std::uint64_t handle, FrameBuffer* frame, OutputInfo* output) {
    if (!output) throw std::runtime_error("Videodec2: invalid argument pointer");
    const auto found = FindDecoder(handle);
    if (!found) throw std::runtime_error("Videodec2: invalid decoder handle");
    auto& decoder = *found;
    std::lock_guard guard(decoder.mutex);
    if (!decoder.flushing && decoder.ready.empty() && !decoder.inputs.empty()) {
        Send(decoder, nullptr);
        decoder.flushing = true;
    }
    EmitDecoded(handle, decoder, frame, output);
    return 0;
}

int APS5_VABI sceVideodec2Reset_nid_postfix(std::uint64_t handle) {
    const auto found = FindDecoder(handle);
    if (!found) throw std::runtime_error("Videodec2: invalid decoder handle");
    auto& decoder = *found;
    std::lock_guard guard(decoder.mutex);
    decoder.pictures.clear();
    decoder.vp9References = {};
    decoder.vp9Colors = {};
    {
        std::lock_guard ownersGuard(lock);
        std::erase_if(pictureOwners, [handle](const auto& owner) { return owner.second == handle; });
    }
    avcodec_flush_buffers(decoder.context);
    decoder.hevc.afterEnd = true;
    decoder.ready.clear();
    decoder.inputs.clear();
    decoder.parameterSets.clear();
    decoder.flushing = false;
    return 0;
}

static std::uint64_t PictureInfoSize(const void* info) {
    std::uint64_t size = 0;
    std::memcpy(&size, info, sizeof(size));
    return size;
}

static bool PictureInfoSizeSupported(const void* info) {
    switch (PictureInfoSize(info)) {
    case 0x58:
    case 0x68:
    case 0x78:
    case 0x88:
    case 0xa8:
    case 0xb8: return true;
    default: return false;
    }
}

static int PictureArguments(const OutputInfo* output, void* first, void* second) {
    if (!first) return Videodec2ErrorArgumentPointer;
    auto* firstValidity = static_cast<std::uint8_t*>(first) + 8;
    GuestArena::HostWrite firstAccess(firstValidity, sizeof(std::uint32_t));
    if (!firstAccess.Open()) throw UnsupportedHevcMetadata{};
    std::optional<GuestArena::HostWrite> secondAccess;
    if (second) {
        secondAccess.emplace(static_cast<std::uint8_t*>(second) + 8, sizeof(std::uint32_t));
        if (!secondAccess->Open()) throw UnsupportedHevcMetadata{};
    }
    std::memset(firstValidity, 0, sizeof(std::uint32_t));
    if (second) std::memset(static_cast<std::uint8_t*>(second) + 8, 0, sizeof(std::uint32_t));
    if (!output) return Videodec2ErrorArgumentPointer;
    if (output->thisSize != 0x30 && output->thisSize != 0x38) return Videodec2ErrorStructureSize;
    if ((reinterpret_cast<std::uintptr_t>(output->frameBuffer) & 0xffu) != 0) return Videodec2ErrorFrameBufferAlignment;
    if (!output->frameBuffer) return Videodec2ErrorFrameBufferPointer;
    if (output->pictureCount != 1 && output->pictureCount != 2) return Videodec2ErrorPictureCount;
    if (output->frameBufferSize <= std::uint64_t{output->pictureCount} * 0x400u) return Videodec2ErrorFrameBufferSize;
    if (!PictureInfoSizeSupported(first) || (output->pictureCount == 2 && second && !PictureInfoSizeSupported(second))) return Videodec2ErrorStructureSize;
    return 0;
}

static int GetPictureInfo(const OutputInfo* output, void* first, void* second) {
    const int result = PictureArguments(output, first, second);
    if (result != 0) return result;
    if (output->pictureCount != 1) throw UnsupportedHevcMetadata{};
    const auto owner = [output]() -> std::uint64_t {
        std::lock_guard guard(lock);
        const auto found = pictureOwners.find(output->frameBuffer);
        return found != pictureOwners.end() ? found->second : 0;
    };
    for (auto handle = owner(); handle != 0; handle = owner()) {
        const auto decoder = FindDecoder(handle);
        if (!decoder) continue;
        std::lock_guard guard(decoder->mutex);
        if (owner() != handle) continue;
        const auto found = decoder->pictures.find(output->frameBuffer);
        if (found == decoder->pictures.end()) throw UnsupportedHevcMetadata{};
        const auto size = PictureInfoSize(first);
        if (decoder->codec == CodecVp9 && size == sizeof(Vp9PictureInfo)) {
            GuestArena::HostWrite access(first, sizeof(Vp9PictureInfo));
            if (!access.Open()) throw UnsupportedHevcMetadata{};
            std::memcpy(first, &found->second.vp9, sizeof(Vp9PictureInfo));
            std::memcpy(first, &size, sizeof(size));
            return 0;
        }
        if (decoder->codec == CodecHevc && size == sizeof(Videodec2Hevc::PictureInfo)) {
            if (!found->second.hevc.unsupported.empty()) throw UnsupportedHevcMetadata{};
            GuestArena::HostWrite access(first, sizeof(Videodec2Hevc::PictureInfo));
            if (!access.Open()) throw UnsupportedHevcMetadata{};
            std::memcpy(first, &found->second.hevc.info, sizeof(Videodec2Hevc::PictureInfo));
            std::memcpy(first, &size, sizeof(size));
            return 0;
        }
        if (decoder->codec == CodecAvc && size == sizeof(Videodec2Avc::PictureInfo)) {
            if (!found->second.avc.unsupported.empty()) throw UnsupportedHevcMetadata{};
            GuestArena::HostWrite access(first, sizeof(Videodec2Avc::PictureInfo));
            if (!access.Open()) throw UnsupportedHevcMetadata{};
            std::memcpy(first, &found->second.avc.info, sizeof(Videodec2Avc::PictureInfo));
            std::memcpy(first, &size, sizeof(size));
            return 0;
        }
        throw UnsupportedHevcMetadata{};
    }
    throw UnsupportedHevcMetadata{};
}

int APS5_VABI sceVideodec2GetPictureInfo_nid_postfix(const OutputInfo* output, AvcPictureInfo* first, void* second) {
    try { return GetPictureInfo(output, first, second); }
    catch (const UnsupportedHevcMetadata&) { NotImplemented_nid_no_patch(__func__); }
}

int APS5_VABI sceVideodec2GetAvcPictureInfo_nid_postfix(const OutputInfo* output, AvcPictureInfo* first, AvcPictureInfo* second) {
    try { return GetPictureInfo(output, first, second); }
    catch (const UnsupportedHevcMetadata&) { NotImplemented_nid_no_patch(__func__); }
}

int APS5_VABI sceVideodec2GetHevcPictureInfo_nid_postfix(const OutputInfo* output, Videodec2Hevc::PictureInfo* info) {
    try { return GetPictureInfo(output, info, nullptr); }
    catch (const UnsupportedHevcMetadata&) { NotImplemented_nid_no_patch(__func__); }
}

int APS5_VABI sceVideodec2GetVp9PictureInfo_nid_postfix(const OutputInfo* output, Vp9PictureInfo* info) {
    try { return GetPictureInfo(output, info, nullptr); }
    catch (const UnsupportedHevcMetadata&) { NotImplemented_nid_no_patch(__func__); }
}

void Videodec2PictureHeader_nid_no_patch(std::uint64_t handle, const void* frameBuffer, PictureHeader* header) {
    if (!header) throw std::runtime_error("Videodec2: invalid argument pointer");
    const auto found = FindDecoder(handle);
    if (!found) throw std::runtime_error("Videodec2: invalid decoder handle");
    auto& decoder = *found;
    std::lock_guard guard(decoder.mutex);
    const auto picture = decoder.pictures.find(frameBuffer);
    if (picture == decoder.pictures.end()) throw std::runtime_error("Videodec2: unknown frame buffer");
    const auto& stored = picture->second;
    *header = {stored.pts, stored.dts, stored.attached, stored.idr, stored.hevcIdr, decoder.profile, decoder.level, stored.crop, stored.bitDepthMinus8};
}

void Videodec2Vp9PictureInfo_nid_no_patch(std::uint64_t handle, const void* frameBuffer, Vp9PictureInfo* info) {
    if (!info) throw std::runtime_error("Videodec2: invalid argument pointer");
    const auto found = FindDecoder(handle);
    if (!found) throw std::runtime_error("Videodec2: invalid decoder handle");
    auto& decoder = *found;
    std::lock_guard guard(decoder.mutex);
    if (decoder.codec != CodecVp9) throw std::runtime_error("Videodec2: VP9 picture info requested for another codec");
    const auto picture = decoder.pictures.find(frameBuffer);
    if (picture == decoder.pictures.end()) throw std::runtime_error("Videodec2: unknown frame buffer");
    auto snapshot = picture->second.vp9;
    snapshot.thisSize = sizeof(*info);
    *info = snapshot;
}

}
