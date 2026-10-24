#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "prx/libc/include/General.hpp"
#include "H264Fixture.hpp"
#include "HevcFixture.hpp"
#include "Vp9Fixture.hpp"

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

struct Vp9PictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint32_t profile;
    std::uint32_t level;
    std::uint32_t keyFrameFlag;
    std::uint32_t intraOnly;
    std::uint8_t bitDepth;
    std::uint8_t colorRange;
    std::uint32_t colorSpace;
    std::uint32_t numFrames;
    std::uint32_t frameWidth;
    std::uint32_t frameHeight;
    std::uint32_t renderWidth;
    std::uint32_t renderHeight;
};

static_assert(sizeof(Vp9PictureInfo) == 88);
static_assert(offsetof(Vp9PictureInfo, colorSpace) == 60);

extern "C" {
int APS5_VABI sceVideodec2QueryDecoderMemoryInfo_nid_postfix(const DecoderConfigInfo*, DecoderMemoryInfo*);
int APS5_VABI sceVideodec2CreateDecoder_nid_postfix(const DecoderConfigInfo*, const DecoderMemoryInfo*, std::uint64_t*);
int APS5_VABI sceVideodec2DeleteDecoder_nid_postfix(std::uint64_t);
int APS5_VABI sceVideodec2Decode_nid_postfix(std::uint64_t, const InputData*, FrameBuffer*, OutputInfo*);
int APS5_VABI sceVideodec2Flush_nid_postfix(std::uint64_t, FrameBuffer*, OutputInfo*);
int APS5_VABI sceVideodec2Reset_nid_postfix(std::uint64_t);
int APS5_VABI sceVideodec2GetPictureInfo_nid_postfix(const OutputInfo*, AvcPictureInfo*, void*);
int APS5_VABI sceVideodec2GetVp9PictureInfo_nid_postfix(const OutputInfo*, Vp9PictureInfo*);
int APS5_VABI sceVideodec2GetAvcPictureInfo_nid_postfix(const OutputInfo*, AvcPictureInfo*, AvcPictureInfo*);
}

namespace {

using namespace H264Fixture;
using namespace HevcFixture;

constexpr std::uint32_t CodecHevc = 974921;
constexpr std::uint32_t CodecVp9 = 2382845;

constexpr std::array<std::uint8_t, 658> AvcIpppStream{
    0x00, 0x00, 0x00, 0x01, 0x09, 0x10, 0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xc0, 0x0a, 0xda, 0x11,
    0xec, 0x04, 0x40, 0x00, 0x00, 0x03, 0x00, 0x40, 0x00, 0x00, 0x0f, 0x03, 0xc4, 0x89, 0xa8, 0x00,
    0x00, 0x00, 0x01, 0x68, 0xce, 0x0f, 0x2c, 0x80, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x09, 0x7f,
    0xe4, 0x5e, 0xd2, 0xf1, 0x40, 0x00, 0x40, 0x07, 0xc5, 0x00, 0x02, 0x50, 0x06, 0x46, 0x28, 0xcd,
    0x0f, 0x80, 0x38, 0x07, 0xff, 0xc0, 0x07, 0xd8, 0x76, 0x33, 0x23, 0x81, 0xce, 0x7d, 0x73, 0x08,
    0x2f, 0xc0, 0xff, 0x1f, 0xc1, 0x61, 0x01, 0x2f, 0xc1, 0x00, 0x01, 0xf0, 0x19, 0x8d, 0x80, 0x38,
    0xd1, 0x0d, 0xcc, 0x7a, 0x99, 0xdf, 0xff, 0xda, 0x00, 0x3d, 0x82, 0x8e, 0xf2, 0x8f, 0x22, 0x18,
    0x94, 0x07, 0x24, 0x94, 0x45, 0x22, 0x44, 0x5c, 0xb8, 0x7f, 0xe1, 0xa2, 0x99, 0xa3, 0xef, 0xe2,
    0x80, 0x01, 0x9f, 0x03, 0x81, 0x0e, 0x01, 0x80, 0x70, 0x00, 0x10, 0x05, 0x00, 0x04, 0x00, 0x02,
    0x00, 0x6e, 0xbf, 0x6f, 0x66, 0xdb, 0x0c, 0x3f, 0x7c, 0x5b, 0xcc, 0xf6, 0x6c, 0xcc, 0x30, 0xe1,
    0x8c, 0x1c, 0x00, 0x04, 0x04, 0x80, 0x00, 0x42, 0x73, 0xfd, 0x1f, 0xfe, 0x20, 0x00, 0x13, 0x00,
    0x01, 0x08, 0xf8, 0x80, 0x00, 0x20, 0x2c, 0x1f, 0x04, 0xbb, 0xf3, 0xc0, 0x23, 0xdd, 0x63, 0x6f,
    0xf9, 0xe1, 0x04, 0x82, 0x53, 0x90, 0x01, 0x89, 0xe5, 0x89, 0x44, 0x5a, 0xf8, 0x39, 0xe0, 0x03,
    0x20, 0x10, 0x50, 0x00, 0x25, 0xf9, 0x8d, 0xe0, 0x04, 0xf3, 0x62, 0xfd, 0x35, 0x7d, 0x77, 0x7e,
    0x0b, 0xe1, 0x4d, 0xfe, 0x85, 0x2b, 0xef, 0xe8, 0xf0, 0x80, 0xa2, 0x80, 0x03, 0x04, 0x00, 0x06,
    0xc0, 0x72, 0x80, 0x02, 0x8c, 0x14, 0xe0, 0x9d, 0xcd, 0xe8, 0x3f, 0xe9, 0xb5, 0x9b, 0x06, 0x4d,
    0x5e, 0x80, 0xa2, 0x1f, 0x06, 0xda, 0xdf, 0x36, 0xfd, 0xa1, 0x0f, 0xfe, 0x08, 0xb0, 0x26, 0x48,
    0xaf, 0x17, 0xe9, 0xc5, 0x9f, 0xbf, 0xba, 0x2e, 0x69, 0xa9, 0xe6, 0x7c, 0xbf, 0x4f, 0xfe, 0xc1,
    0x64, 0x0a, 0xaa, 0x69, 0xda, 0x43, 0xff, 0xf9, 0xfd, 0xfc, 0xff, 0x77, 0xe9, 0x1e, 0x20, 0x18,
    0x30, 0x1f, 0x80, 0x12, 0xe4, 0x51, 0x4d, 0x9b, 0xff, 0x9b, 0x83, 0xa0, 0x01, 0x72, 0xd1, 0x7f,
    0x83, 0xa2, 0xe9, 0x82, 0x73, 0x13, 0xd3, 0x8d, 0x59, 0x95, 0x1f, 0xa6, 0x70, 0x4a, 0xeb, 0xb5,
    0xe0, 0x42, 0x10, 0xc0, 0x02, 0x00, 0x1c, 0x20, 0x00, 0x28, 0x14, 0x68, 0xe0, 0x1a, 0x82, 0xd4,
    0x87, 0x10, 0x00, 0x46, 0x54, 0x41, 0xc1, 0x81, 0x9a, 0x0c, 0x5d, 0xd0, 0x03, 0xd8, 0x0f, 0x52,
    0x07, 0x2c, 0x00, 0x7a, 0x92, 0x12, 0x02, 0xdc, 0x01, 0x67, 0x3a, 0xc1, 0xa4, 0x5a, 0x83, 0x95,
    0xc0, 0x30, 0x02, 0xf8, 0x49, 0xe2, 0x1e, 0x2b, 0xd5, 0x4a, 0x29, 0xa9, 0x6d, 0xf2, 0xc0, 0x7b,
    0x27, 0x2c, 0xb3, 0xde, 0x18, 0x00, 0x11, 0x71, 0xe0, 0x70, 0x02, 0x0c, 0x34, 0x07, 0x06, 0x01,
    0xa2, 0x90, 0x71, 0x40, 0x05, 0x12, 0x98, 0x01, 0x01, 0x63, 0xe2, 0xa8, 0xc0, 0xb8, 0xe0, 0xe0,
    0xf0, 0x06, 0x8a, 0x41, 0xd0, 0x01, 0x59, 0x12, 0x08, 0x30, 0x7d, 0x39, 0x02, 0x1d, 0x5e, 0x01,
    0x80, 0x26, 0x39, 0x0f, 0xc1, 0xef, 0x83, 0xc3, 0xf8, 0x3c, 0x7c, 0x4f, 0x86, 0xdd, 0x50, 0x6d,
    0x69, 0xd0, 0xef, 0xe9, 0x70, 0x35, 0xc4, 0xb3, 0x84, 0x40, 0x00, 0x40, 0x48, 0x00, 0x54, 0x84,
    0x00, 0x0d, 0x51, 0x81, 0xc3, 0x80, 0x3d, 0xd2, 0x1c, 0x20, 0x1c, 0xca, 0xa0, 0x10, 0x1a, 0x46,
    0x38, 0x04, 0x06, 0xd0, 0x75, 0x00, 0x2f, 0x48, 0x1e, 0x80, 0x3f, 0x49, 0x14, 0xc0, 0x3f, 0xc4,
    0x52, 0x01, 0x7e, 0x78, 0x36, 0x19, 0x08, 0x43, 0xf6, 0xc5, 0xc0, 0x01, 0xf8, 0x9f, 0x6e, 0x65,
    0xa4, 0xff, 0xff, 0x86, 0x0e, 0xf8, 0x00, 0x8b, 0xfb, 0xe4, 0xf4, 0x00, 0x57, 0x5c, 0x07, 0x4b,
    0x80, 0x66, 0xe3, 0xd2, 0xcd, 0xa7, 0x03, 0xa0, 0x41, 0x81, 0x20, 0x00, 0x20, 0x24, 0x80, 0x73,
    0x80, 0x0e, 0x52, 0x0e, 0xe5, 0xc0, 0x27, 0xd9, 0x08, 0x9c, 0x6c, 0xa6, 0x58, 0x86, 0xc5, 0xe9,
    0x0e, 0x40, 0x0c, 0xc8, 0x17, 0x5c, 0x50, 0x1a, 0xc1, 0x74, 0x47, 0x9f, 0xf0, 0x81, 0x0d, 0xb0,
    0x00, 0x00, 0x00, 0x01, 0x09, 0x30, 0x00, 0x00, 0x01, 0x41, 0x9a, 0x20, 0x25, 0xde, 0x49, 0xba,
    0xbd, 0x70, 0x47, 0xbd, 0x5a, 0xa0, 0x00, 0x00, 0x00, 0x01, 0x09, 0x30, 0x00, 0x00, 0x01, 0x41,
    0x9a, 0x40, 0x2d, 0xc4, 0xf0, 0x47, 0x55, 0xf7, 0xfa, 0xdf, 0xe0, 0x97, 0xbb, 0xee, 0xad, 0xf0,
    0x43, 0xd5, 0x7b, 0x00, 0x00, 0x00, 0x01, 0x09, 0x30, 0x00, 0x00, 0x01, 0x41, 0x9a, 0x60, 0x35,
    0xc6, 0xc0};

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}


template <typename TAction>
void checkThrows(TAction action) {
    bool threw = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "expected a decoding exception");
}

void testFailures() {
    DecoderConfigInfo config{sizeof(DecoderConfigInfo), 0, 2, 100, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    std::uint64_t handle = 0;
    checkThrows([&] { sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle); });
    check(handle == 0, "unsupported codec created a decoder");
    config.codecType = 1;
    config.maxFrameWidth = 0;
    checkThrows([&] { sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory); });
    config.maxFrameWidth = Width;
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "decoder creation failed");
    std::vector<std::uint8_t> buffer(memory.maxFrameBufferSize);
    FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
    OutputInfo output{sizeof(OutputInfo)};
    const std::array<std::uint8_t, 5> malformed{0, 0, 0, 8, 0x65};
    InputData input{sizeof(InputData), malformed.data(), malformed.size(), 0, 0, 0};
    checkThrows([&] { sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output); });
    input.auSize = 2;
    checkThrows([&] { sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output); });
    check(sceVideodec2Reset_nid_postfix(handle) == 0, "reset failed");
    AvcPictureInfo avc{sizeof(AvcPictureInfo)};
    checkThrows([&] { sceVideodec2GetAvcPictureInfo_nid_postfix(nullptr, &avc, nullptr); });
    checkThrows([&] { sceVideodec2GetAvcPictureInfo_nid_postfix(&output, nullptr, nullptr); });
    output.frameBuffer = buffer.data();
    check(sceVideodec2GetAvcPictureInfo_nid_postfix(&output, &avc, nullptr) == 0 && !avc.isValid, "unknown frame buffer reported a picture");
    frame.frameBufferSize = 1;
    checkThrows([&] {
        for (const auto& unit : accessUnits(false)) {
            input.auData = unit.data();
            input.auSize = unit.size();
            sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output);
        }
        sceVideodec2Flush_nid_postfix(handle, &frame, &output);
    });
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
}

using Vp9Fixture::Vp9Hashes;
using Vp9Fixture::ivfFrames;

void testClip(std::uint32_t codec, std::uint32_t profile, const std::vector<std::vector<std::uint8_t>>& units, const std::array<std::uint64_t, 6>& hashes) {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, codec, profile, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    std::uint64_t handle = 0;
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "decoder creation failed");
    std::vector<std::uint8_t> buffer(memory.maxFrameBufferSize);
    std::size_t pictures = 0;
    const auto take = [&](const OutputInfo& output, const FrameBuffer& frame) {
        check(output.isValid == frame.isAccepted, "frame buffer acceptance disagrees with the output");
        if (!output.isValid) return false;
        check(pictures < hashes.size(), "more pictures than access units");
        check(output.codecType == codec && !output.isErrorFrame && output.frameWidth == Width && output.frameHeight == Height && output.framePitch == 256 && output.frameBuffer == buffer.data(), "unexpected picture geometry");
        AvcPictureInfo info{sizeof(AvcPictureInfo)};
        checkThrows([&] { sceVideodec2GetPictureInfo_nid_postfix(&output, &info, nullptr); });
        Vp9PictureInfo vp9{sizeof(Vp9PictureInfo)};
        if (codec == CodecVp9) {
            check(sceVideodec2GetVp9PictureInfo_nid_postfix(&output, &vp9) == 0 && vp9.isValid, "VP9 picture metadata missing");
            check(vp9.ptsData == 1000 + pictures && vp9.dtsData == pictures && vp9.attachedData == 0, "VP9 picture timestamps differ");
            check(vp9.profile == 0 && vp9.bitDepth == 8 && vp9.colorSpace == 0 && vp9.colorRange == 0 && vp9.numFrames == 1 && vp9.intraOnly == 0, "VP9 coded header metadata differs");
            check(vp9.frameWidth == Width && vp9.frameHeight == Height && vp9.renderWidth == Width && vp9.renderHeight == Height, "VP9 picture dimensions differ");
            check(vp9.keyFrameFlag == (pictures == 0 ? 1u : 0u), "VP9 key frame flag differs");
            struct Guarded { Vp9PictureInfo info{sizeof(Vp9PictureInfo)}; std::uint64_t guard = 0x123456789abcdef0ull; } generic;
            check(sceVideodec2GetPictureInfo_nid_postfix(&output, reinterpret_cast<AvcPictureInfo*>(&generic.info), nullptr) == 0, "generic VP9 picture info failed");
            const auto& same = generic.info;
            check(same.thisSize == sizeof(Vp9PictureInfo) && same.isValid && same.ptsData == vp9.ptsData && same.dtsData == vp9.dtsData && same.attachedData == vp9.attachedData && same.profile == vp9.profile && same.level == vp9.level &&
                same.keyFrameFlag == vp9.keyFrameFlag && same.intraOnly == vp9.intraOnly && same.bitDepth == vp9.bitDepth && same.colorRange == vp9.colorRange && same.colorSpace == vp9.colorSpace && same.numFrames == vp9.numFrames &&
                same.frameWidth == vp9.frameWidth && same.frameHeight == vp9.frameHeight && same.renderWidth == vp9.renderWidth && same.renderHeight == vp9.renderHeight && generic.guard == 0x123456789abcdef0ull, "generic picture info differs from the VP9 layout");
        } else checkThrows([&] { sceVideodec2GetVp9PictureInfo_nid_postfix(&output, &vp9); });
        check(hashNv12(buffer.data(), output.framePitch, output.frameHeight) == hashes[pictures], "picture " + std::to_string(pictures) + " differs from the reference");
        ++pictures;
        return true;
    };
    check(units.size() == hashes.size(), "unexpected access unit count");
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
        take(output, frame);
    }
    for (;;) {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        if (!take(output, frame)) break;
    }
    check(pictures == hashes.size(), "missing pictures after flush");
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
}

void testVp9PictureMetadata() {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, CodecVp9, 0, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "VP9 memory query failed");
    std::uint64_t handle = 0;
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "VP9 create failed");
    const auto units = ivfFrames();
    std::vector<std::uint8_t> firstBuffer(memory.maxFrameBufferSize), secondBuffer(memory.maxFrameBufferSize);
    OutputInfo firstOutput{sizeof(OutputInfo)}, secondOutput{sizeof(OutputInfo)};
    const auto decode = [&](const std::vector<std::uint8_t>& bytes, std::vector<std::uint8_t>& buffer, OutputInfo& output, std::uint64_t tag) {
        const InputData input{sizeof(InputData), bytes.data(), bytes.size(), tag, tag + 1, tag + 2};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "VP9 metadata decode failed");
    };
    const auto query = [&](const OutputInfo& output) {
        struct Guarded { Vp9PictureInfo info{sizeof(Vp9PictureInfo)}; std::uint64_t guard = 0x123456789abcdef0ull; } guarded;
        check(sceVideodec2GetVp9PictureInfo_nid_postfix(&output, &guarded.info) == 0, "VP9 metadata query failed");
        check(guarded.guard == 0x123456789abcdef0ull && guarded.info.thisSize == sizeof(Vp9PictureInfo), "VP9 info overwrote caller storage");
        return guarded.info;
    };
    Vp9PictureInfo info{sizeof(Vp9PictureInfo)};
    checkThrows([&] { sceVideodec2GetVp9PictureInfo_nid_postfix(nullptr, &info); });
    checkThrows([&] { sceVideodec2GetVp9PictureInfo_nid_postfix(&firstOutput, nullptr); });
    info.thisSize = sizeof(Vp9PictureInfo) - 1;
    checkThrows([&] { sceVideodec2GetVp9PictureInfo_nid_postfix(&firstOutput, &info); });
    check(!query(firstOutput).isValid, "VP9 unknown buffer reported a picture");
    std::vector<std::uint8_t> superframe;
    for (std::size_t index = 0; index < 2; ++index) superframe.insert(superframe.end(), units[index].begin(), units[index].end());
    superframe.push_back(0xc9);
    for (std::size_t index = 0; index < 2; ++index) {
        superframe.push_back(static_cast<std::uint8_t>(units[index].size()));
        superframe.push_back(static_cast<std::uint8_t>(units[index].size() >> 8u));
    }
    superframe.push_back(0xc9);
    decode(superframe, firstBuffer, firstOutput, 9000);
    check(firstOutput.isValid && hashNv12(firstBuffer.data(), firstOutput.framePitch, firstOutput.frameHeight) == Vp9Hashes[0], "VP9 first superframe picture differs");
    const auto firstInfo = query(firstOutput);
    check(firstInfo.isValid && firstInfo.numFrames == 2 && firstInfo.keyFrameFlag == 1 && firstInfo.ptsData == 9000 && firstInfo.dtsData == 9001 && firstInfo.attachedData == 9002, "VP9 first superframe metadata differs");
    FrameBuffer frame{sizeof(FrameBuffer), secondBuffer.data(), secondBuffer.size(), false};
    check(sceVideodec2Flush_nid_postfix(handle, &frame, &secondOutput) == 0 && secondOutput.isValid, "VP9 second superframe picture missing");
    check(hashNv12(secondBuffer.data(), secondOutput.framePitch, secondOutput.frameHeight) == Vp9Hashes[1], "VP9 second superframe picture differs");
    const auto secondInfo = query(secondOutput);
    check(secondInfo.numFrames == 2 && secondInfo.keyFrameFlag == 0 && secondInfo.ptsData == 9000, "VP9 second superframe metadata differs");
    firstOutput.frameWidth = 1;
    check(query(firstOutput).frameWidth == Width && query(firstOutput).keyFrameFlag == 1, "VP9 retained picture metadata changed");
    check(sceVideodec2Reset_nid_postfix(handle) == 0 && !query(firstOutput).isValid, "VP9 reset retained old picture ownership");
    auto hidden = units[0];
    hidden[0] &= static_cast<std::uint8_t>(~2u);
    decode(hidden, firstBuffer, firstOutput, 9100);
    check(!firstOutput.isValid, "VP9 hidden keyframe was presented");
    decode(std::vector<std::uint8_t>{0x88}, secondBuffer, secondOutput, 9200);
    info = query(secondOutput);
    check(info.isValid && info.ptsData == 9200 && info.keyFrameFlag == 0 && info.frameWidth == Width, "VP9 existing reference metadata differs");
    check(hashNv12(secondBuffer.data(), secondOutput.framePitch, secondOutput.frameHeight) == Vp9Hashes[0], "VP9 existing reference pixels differ");
    check(sceVideodec2Reset_nid_postfix(handle) == 0, "VP9 reset failed");
    checkThrows([&] { decode(std::vector<std::uint8_t>{0x88}, firstBuffer, firstOutput, 9300); });
    checkThrows([&] { decode(std::vector<std::uint8_t>{0}, firstBuffer, firstOutput, 9400); });
    checkThrows([&] { decode(std::vector<std::uint8_t>{0x82}, firstBuffer, firstOutput, 9500); });
    checkThrows([&] { decode(std::vector<std::uint8_t>{0xc1, 0, 0, 0xc1}, firstBuffer, firstOutput, 9550); });
    std::vector<bool> bits;
    for (const auto byte : units[0]) for (int shift = 7; shift >= 0; --shift) bits.push_back(((byte >> shift) & 1u) != 0);
    check(bits.size() > 69, "VP9 test keyframe header is truncated");
    bits[35] = true;
    bits[68] = true;
    std::vector<bool> render;
    for (const auto value : {static_cast<unsigned>(Width * 2 - 1), static_cast<unsigned>(Height * 2 - 1)})
        for (int shift = 15; shift >= 0; --shift) render.push_back(((value >> shift) & 1u) != 0);
    bits.insert(bits.begin() + 69, render.begin(), render.end());
    std::vector<std::uint8_t> resized(bits.size() / 8);
    for (std::size_t index = 0; index < bits.size(); ++index) if (bits[index]) resized[index / 8] |= static_cast<std::uint8_t>(1u << (7u - index % 8));
    decode(resized, firstBuffer, firstOutput, 9600);
    info = query(firstOutput);
    check(info.isValid && info.bitDepth == 8 && info.colorRange == 1 && info.frameWidth == Width && info.frameHeight == Height && info.renderWidth == Width * 2 && info.renderHeight == Height * 2, "VP9 render size or color range metadata differs");
    check(hashNv12(firstBuffer.data(), firstOutput.framePitch, firstOutput.frameHeight) == Vp9Hashes[0], "VP9 render metadata changed coded pixels");
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0 && !query(firstOutput).isValid, "VP9 delete retained picture ownership");
}

void testTenBit() {
    for (const auto codec : {CodecHevc, CodecVp9}) {
        for (const auto [profile, expected] : {std::pair<std::uint32_t, std::uint64_t>{1, 2048ull * 1088 * 3 / 2}, {2, 3840ull * 1088 * 3 / 2}}) {
            const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, codec, profile, 0, 1920, 1088, 4, 1};
            DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
            check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0 && memory.maxFrameBufferSize == expected, "1920x1088 frame size differs for profile " + std::to_string(profile));
        }
        const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, codec, 2, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
        DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
        check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
        std::uint64_t handle = 0;
        check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "ten-bit decoder creation failed");
        if (codec == CodecHevc) {
            std::vector<std::uint8_t> buffer(memory.maxFrameBufferSize);
            const InputData input{sizeof(InputData), HevcMain10Stream.data(), HevcMain10Stream.size(), 0, 0, 0};
            FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
            OutputInfo output{sizeof(OutputInfo)};
            check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "ten-bit decode failed");
            if (!output.isValid) check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "ten-bit flush failed");
            check(output.isValid && frame.isAccepted && output.frameWidth == Width && output.frameHeight == Height && output.framePitch == 128 && output.framePitchInBytes == 256 && output.frameFormat == 0, "ten-bit picture geometry differs");
            std::uint64_t hash = 0xcbf29ce484222325ull;
            std::uint16_t largest = 0;
            const auto mix = [&](const std::uint8_t* row) {
                for (std::uint32_t column = 0; column < Width; ++column) {
                    std::uint16_t word = 0;
                    std::memcpy(&word, row + column * 2, 2);
                    largest = std::max(largest, word);
                    hash = (hash ^ word) * 0x100000001b3ull;
                }
            };
            for (std::uint32_t row = 0; row < Height; ++row) mix(buffer.data() + static_cast<std::size_t>(row) * output.framePitchInBytes);
            for (std::uint32_t row = 0; row < Height / 2; ++row) mix(buffer.data() + static_cast<std::size_t>(output.framePitchInBytes) * Height + static_cast<std::size_t>(row) * output.framePitchInBytes);
            check(largest == 1023, "ten-bit samples are not in the low bits of their words");
            check(hash == 0x453fd1cd69e47611ull, "ten-bit picture differs from the reference");
        }
        check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
    }
}

void testDecode(bool lengthPrefixed, bool separateParameters = false) {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 0, 1, 100, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    std::uint64_t handle = 0;
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "decoder creation failed");
    std::vector<std::uint8_t> buffer(memory.maxFrameBufferSize);
    std::size_t pictures = 0;
    const auto take = [&](const OutputInfo& output, const FrameBuffer& frame) {
        check(output.isValid == frame.isAccepted, "frame buffer acceptance disagrees with the output");
        if (!output.isValid) return false;
        check(pictures < PictureHashes.size(), "more pictures than access units");
        check(!output.isErrorFrame && output.frameWidth == Width && output.frameHeight == CodedHeight && output.framePitch == 256 && output.frameBuffer == buffer.data(), "unexpected picture geometry");
        AvcPictureInfo info{sizeof(AvcPictureInfo)};
        check(sceVideodec2GetPictureInfo_nid_postfix(&output, &info, nullptr) == 0 && info.isValid, "picture info missing");
        const auto unit = DisplayOrderUnits[pictures];
        check(info.ptsData == 1000 + unit && info.dtsData == unit && info.attachedData == 0xa0 + unit, "picture timestamps out of display order");
        check(info.idrPictureFlag == (pictures == 0 ? 1 : 0) && info.profileIdc == 100, "picture flags mismatch");
        AvcPictureInfo avc{sizeof(AvcPictureInfo)};
        check(sceVideodec2GetAvcPictureInfo_nid_postfix(&output, &avc, nullptr) == 0 && avc.isValid, "avc picture info missing");
        check(avc.ptsData == 1000 + unit && avc.dtsData == unit && avc.attachedData == 0xa0 + unit && avc.idrPictureFlag == (pictures == 0 ? 1 : 0) && avc.profileIdc == 100 && avc.picWidthInLumaSamples == Width && avc.picHeightInLumaSamples == CodedHeight, "avc picture info mismatch");
        check(hashNv12(buffer.data(), output.framePitch, output.frameHeight) == PictureHashes[pictures], "picture " + std::to_string(pictures) + " differs from the reference");
        ++pictures;
        return true;
    };
    auto units = accessUnits(lengthPrefixed);
    check(units.size() == PictureHashes.size(), "unexpected access unit count");
    if (separateParameters) {
        auto& first = units.front();
        std::size_t slice = 0;
        for (std::size_t index = 0; index + 3 < first.size(); ++index) {
            if (first[index] != 0 || first[index + 1] != 0 || first[index + 2] != 1) continue;
            const auto type = first[index + 3] & 0x1fu;
            if (type >= 1 && type <= 5) {
                slice = index > 0 && first[index - 1] == 0 ? index - 1 : index;
                break;
            }
            index += 2;
        }
        check(slice != 0, "the first access unit has no slice after its parameter sets");
        const std::vector<std::uint8_t> parameters(first.begin(), first.begin() + static_cast<std::ptrdiff_t>(slice));
        first.erase(first.begin(), first.begin() + static_cast<std::ptrdiff_t>(slice));
        const InputData input{sizeof(InputData), parameters.data(), parameters.size(), 999, 0, 0x9f};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "a unit of parameter sets alone was not accepted");
        check(!output.isValid && !frame.isAccepted, "a unit of parameter sets alone produced a picture");
    }
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
        take(output, frame);
    }
    for (;;) {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        if (!take(output, frame)) break;
    }
    check(pictures == PictureHashes.size(), "missing pictures after flush");
    check(sceVideodec2Reset_nid_postfix(handle) == 0, "reset failed");
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
    checkThrows([&] { sceVideodec2DeleteDecoder_nid_postfix(handle); });
}

void decodeInOrder(const std::vector<std::vector<std::uint8_t>>& units) {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 0, 1, 100, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    std::uint64_t handle = 0;
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "decoder creation failed");
    std::vector<std::uint8_t> buffer(memory.maxFrameBufferSize);
    std::size_t pictures = 0;
    const auto take = [&](const OutputInfo& output) {
        if (!output.isValid) return false;
        check(pictures < PictureHashes.size(), "more pictures than access units");
        check(!output.isErrorFrame && output.frameWidth == Width && output.frameHeight == CodedHeight, "unexpected picture geometry");
        check(hashNv12(buffer.data(), output.framePitch, output.frameHeight) == PictureHashes[pictures], "picture " + std::to_string(pictures) + " differs from the reference");
        ++pictures;
        return true;
    };
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
        take(output);
    }
    for (;;) {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        if (!take(output)) break;
    }
    check(pictures == PictureHashes.size(), "missing pictures after flush");
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
}

bool readsAsOneLengthPrefix(const std::vector<std::uint8_t>& unit) {
    const std::size_t length = (static_cast<std::size_t>(unit[0]) << 24u) | (static_cast<std::size_t>(unit[1]) << 16u) | (static_cast<std::size_t>(unit[2]) << 8u) | unit[3];
    return length + 4 == unit.size();
}

std::vector<std::uint8_t> lengthPrefixedFiller(std::size_t length) {
    std::vector<std::uint8_t> filler;
    for (int shift = 24; shift >= 0; shift -= 8) filler.push_back(static_cast<std::uint8_t>(length >> shift));
    filler.push_back(0x0c);
    filler.insert(filler.end(), length - 1, 0xff);
    return filler;
}

void testLengthPrefixLooksLikeStartCode() {
    auto units = accessUnits(true);
    const auto filler = lengthPrefixedFiller(0x101);
    check(!(units[0][0] == 0 && units[0][1] == 0 && (units[0][2] == 1 || units[0][3] == 1)), "the first length-prefixed unit already begins with start-code bytes");
    units[1].insert(units[1].begin(), filler.begin(), filler.end());
    check(units[1][0] == 0 && units[1][1] == 0 && units[1][2] == 1, "the crafted unit does not begin with start-code bytes");
    decodeInOrder(units);

    auto mixed = accessUnits(false);
    const auto mixedFiller = lengthPrefixedFiller(0x123);
    mixed[1] = accessUnits(true)[1];
    mixed[1].insert(mixed[1].begin(), mixedFiller.begin(), mixedFiller.end());
    check(mixed[0][2] == 0 && mixed[0][3] == 1 && mixed[1][0] == 0 && mixed[1][1] == 0 && mixed[1][2] == 1, "the length-prefixed unit does not follow an Annex-B unit with a four-byte start code");
    decodeInOrder(mixed);
}

void testStartCodeLooksLikeLengthPrefix() {
    auto units = accessUnits(false);
    for (auto& unit : units) {
        check(unit[0] == 0 && unit[1] == 0 && unit[2] == 0 && unit[3] == 1, "an access unit does not open with a four-byte start code");
        unit.erase(unit.begin());
    }
    auto& first = units.front();
    std::size_t sequence = 0;
    std::size_t afterPicture = 0;
    bool picture = false;
    for (std::size_t index = 0; index + 3 < first.size(); ++index) {
        if (first[index] != 0 || first[index + 1] != 0 || first[index + 2] != 1) continue;
        const auto type = first[index + 3] & 0x1fu;
        if (picture) {
            afterPicture = first[index - 1] == 0 ? index - 1 : index;
            break;
        }
        if (type == 7 && sequence == 0) sequence = index;
        if (type == 8 && sequence != 0) picture = true;
        index += 2;
    }
    check(sequence != 0 && afterPicture > sequence, "the first access unit has no SPS and PPS before another NAL");
    const std::vector<std::uint8_t> delimiter(first.begin(), first.begin() + static_cast<std::ptrdiff_t>(first[sequence - 1] == 0 ? sequence - 1 : sequence));
    check(delimiter.size() >= 4 && delimiter[2] == 1 && !readsAsOneLengthPrefix(delimiter), "the access unit delimiter is not an unambiguous unit with a three-byte start code");
    std::vector<std::uint8_t> parameters(first.begin() + static_cast<std::ptrdiff_t>(sequence), first.begin() + static_cast<std::ptrdiff_t>(afterPicture));
    first.erase(first.begin(), first.begin() + static_cast<std::ptrdiff_t>(afterPicture));
    if (first[2] == 0) first.erase(first.begin());
    const std::size_t padded = 4 + ((std::size_t{1} << 8u) | parameters[3]);
    check(parameters.size() < padded, "the parameter sets are too long to pad");
    parameters.resize(padded, 0);
    check(readsAsOneLengthPrefix(parameters), "the padded parameter sets do not read as one length-prefixed NAL");
    units.insert(units.begin(), {delimiter, parameters});
    decodeInOrder(units);
}

std::uint64_t createDecoder(std::uint32_t codec, std::uint32_t profile) {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, codec, profile, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    std::uint64_t handle = 0;
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "decoder creation failed");
    return handle;
}

AvcPictureInfo query(std::vector<std::uint8_t>& buffer) {
    OutputInfo output{sizeof(OutputInfo)};
    output.frameBuffer = buffer.data();
    AvcPictureInfo info{sizeof(AvcPictureInfo)};
    check(sceVideodec2GetAvcPictureInfo_nid_postfix(&output, &info, nullptr) == 0, "picture info query failed");
    return info;
}

bool owned(std::vector<std::uint8_t>& buffer, std::uint64_t pts) {
    const auto info = query(buffer);
    return info.isValid && info.ptsData == pts;
}

std::uint64_t publishAvc(std::uint64_t handle, std::uint64_t base, std::vector<std::uint8_t>& buffer) {
    const auto units = accessUnits(false);
    std::size_t pictures = 0;
    const auto take = [&](const OutputInfo& output) {
        if (!output.isValid) return false;
        check(owned(buffer, base + DisplayOrderUnits[pictures]), "picture info does not follow the latest publication");
        ++pictures;
        return true;
    };
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), base + unit, unit, base + unit};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
        take(output);
    }
    for (;;) {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        if (!take(output)) break;
    }
    check(pictures == PictureHashes.size(), "missing pictures after flush");
    return base + DisplayOrderUnits[pictures - 1];
}

void publishClip(std::uint64_t handle, const std::vector<std::vector<std::uint8_t>>& units, std::vector<std::uint8_t>& buffer) {
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), unit, unit, 0};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
    }
    for (;;) {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        if (!output.isValid) break;
    }
}

std::uint64_t frameBufferBytes() {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, 1, 100, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    return memory.maxFrameBufferSize;
}

void testEmptyAccessUnit() {
    constexpr int AccessUnitSizeError = static_cast<int>(0x811D010Du);
    constexpr int AccessUnitPointerError = static_cast<int>(0x811D010Eu);
    std::vector<std::uint8_t> buffer(frameBufferBytes());
    const auto handle = createDecoder(CodecHevc, 1);
    const auto units = hevcUnits(HevcStream.data(), HevcStream.size());
    std::size_t pictures = 0;
    const auto take = [&](const OutputInfo& output, const FrameBuffer& frame) {
        check(output.isValid == frame.isAccepted, "frame buffer acceptance disagrees with the output");
        if (!output.isValid) return false;
        check(pictures < HevcHashes.size(), "more pictures than access units");
        check(hashNv12(buffer.data(), output.framePitch, output.frameHeight) == HevcHashes[pictures], "picture " + std::to_string(pictures) + " differs from the reference after a refused access unit");
        ++pictures;
        return true;
    };
    const auto refuse = [&](const std::uint8_t* data, std::uint64_t size, int expected, const char* message) {
        const InputData input{sizeof(InputData), data, size, 2000, 0, 0};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), true};
        OutputInfo output{sizeof(OutputInfo)};
        output.isValid = true;
        output.pictureCount = 1;
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == expected, message);
        check(!output.isValid && output.pictureCount == 0 && !frame.isAccepted, "a refused access unit handed out a picture");
    };
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        if (unit == 2) {
            refuse(units[unit].data(), 0, AccessUnitSizeError, "an empty access unit did not return ACCESS_UNIT_SIZE");
            refuse(nullptr, units[unit].size(), AccessUnitPointerError, "a null access unit did not return ACCESS_UNIT_POINTER");
        }
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
        take(output, frame);
    }
    for (;;) {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        if (!take(output, frame)) break;
    }
    check(pictures == HevcHashes.size(), "refused access units cost pictures");
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
}

void testFlushAfterEveryUnit() {
    std::vector<std::vector<std::uint8_t>> units;
    for (std::size_t index = 0; index + 4 < AvcIpppStream.size(); ++index) {
        if (AvcIpppStream[index] == 0 && AvcIpppStream[index + 1] == 0 && AvcIpppStream[index + 2] == 0 && AvcIpppStream[index + 3] == 1 && (AvcIpppStream[index + 4] & 0x1fu) == 9) units.emplace_back();
        check(!units.empty(), "the IPPP stream does not start with an access unit delimiter");
        units.back().push_back(AvcIpppStream[index]);
    }
    for (std::size_t index = AvcIpppStream.size() - 4; index < AvcIpppStream.size(); ++index) units.back().push_back(AvcIpppStream[index]);
    check(units.size() == 4, "unexpected IPPP access unit count");
    std::vector<std::uint8_t> buffer(frameBufferBytes());
    const auto decode = [&](bool flushEachUnit) {
        const auto handle = createDecoder(1, 66);
        std::vector<std::uint64_t> hashes;
        const auto take = [&](const OutputInfo& output, const FrameBuffer& frame) {
            if (!output.isValid) return false;
            check(frame.isAccepted && output.frameWidth == Width && output.frameHeight == CodedHeight, "unexpected IPPP picture");
            hashes.push_back(hashNv12(buffer.data(), output.framePitch, output.frameHeight));
            return true;
        };
        const auto flush = [&] {
            FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
            OutputInfo output{sizeof(OutputInfo)};
            check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
            return take(output, frame);
        };
        for (std::size_t unit = 0; unit < units.size(); ++unit) {
            const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 3000 + unit, unit, 0};
            FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
            OutputInfo output{sizeof(OutputInfo)};
            check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "IPPP decode failed");
            take(output, frame);
            if (flushEachUnit) flush();
        }
        while (flush()) {}
        check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
        return hashes;
    };
    const auto reference = decode(false);
    check(reference.size() == units.size(), "an IPPP picture is missing without flushes");
    check(decode(true) == reference, "a flush after each access unit lost or changed the pictures that follow it");
}

void testDecodeAfterFlush() {
    std::vector<std::uint8_t> buffer(frameBufferBytes());
    const auto handle = createDecoder(1, 100);
    {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0 && !output.isValid && !frame.isAccepted, "flushing a decoder that decoded nothing gave a picture");
    }
    const auto units = accessUnits(false);
    for (const std::uint64_t base : {1000u, 5000u}) {
        std::size_t pictures = 0;
        const auto take = [&](const OutputInfo& output) {
            if (!output.isValid) return false;
            check(pictures < PictureHashes.size() && owned(buffer, base + DisplayOrderUnits[pictures]), "a picture decoded after a flush is out of display order");
            check(hashNv12(buffer.data(), output.framePitch, output.frameHeight) == PictureHashes[pictures], "picture " + std::to_string(pictures) + " decoded after a flush differs from the reference");
            ++pictures;
            return true;
        };
        for (std::size_t unit = 0; unit < units.size(); ++unit) {
            const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), base + unit, unit, base + unit};
            FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
            OutputInfo output{sizeof(OutputInfo)};
            check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode after a flush failed");
            take(output);
        }
        for (;;) {
            FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
            OutputInfo output{sizeof(OutputInfo)};
            check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
            if (!take(output)) break;
        }
        check(pictures == PictureHashes.size(), "missing pictures of a stream decoded after a flush");
    }
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
}

void testPictureOwnership() {
    std::vector<std::uint8_t> shared(frameBufferBytes()), other(frameBufferBytes());
    {
        const auto older = createDecoder(1, 100);
        const auto newer = createDecoder(1, 100);
        publishAvc(older, 1000, shared);
        const auto latest = publishAvc(newer, 5000, shared);
        check(owned(shared, latest), "an older decoder answered for a buffer a newer decoder wrote last");
        check(sceVideodec2Reset_nid_postfix(newer) == 0, "reset failed");
        check(!query(shared).isValid, "a stale picture survived the reset of the decoder that last wrote the buffer");
        check(sceVideodec2DeleteDecoder_nid_postfix(older) == 0 && sceVideodec2DeleteDecoder_nid_postfix(newer) == 0, "delete failed");
    }
    {
        const auto older = createDecoder(1, 100);
        const auto newer = createDecoder(1, 100);
        publishAvc(older, 1000, shared);
        publishAvc(newer, 5000, shared);
        check(sceVideodec2DeleteDecoder_nid_postfix(newer) == 0, "delete failed");
        check(!query(shared).isValid, "a stale picture survived the deletion of the decoder that last wrote the buffer");
        check(sceVideodec2DeleteDecoder_nid_postfix(older) == 0, "delete failed");
    }
    {
        const auto older = createDecoder(1, 100);
        const auto newer = createDecoder(1, 100);
        publishAvc(older, 1000, shared);
        const auto latest = publishAvc(newer, 5000, shared);
        check(sceVideodec2Reset_nid_postfix(older) == 0, "reset failed");
        check(owned(shared, latest), "resetting a decoder that no longer owns the buffer dropped its picture");
        check(sceVideodec2DeleteDecoder_nid_postfix(older) == 0, "delete failed");
        check(owned(shared, latest), "deleting a decoder that no longer owns the buffer dropped its picture");
        check(sceVideodec2DeleteDecoder_nid_postfix(newer) == 0, "delete failed");
        check(!query(shared).isValid, "a deleted decoder still owns its picture");
    }
    {
        const auto older = createDecoder(1, 100);
        const auto newer = createDecoder(1, 100);
        publishAvc(newer, 5000, shared);
        const auto latest = publishAvc(older, 1000, shared);
        const auto third = createDecoder(1, 100);
        const auto separate = publishAvc(third, 9000, other);
        check(owned(shared, latest) && owned(other, separate), "distinct buffers are not independent");
        check(sceVideodec2Reset_nid_postfix(older) == 0, "reset failed");
        check(!query(shared).isValid && owned(other, separate), "resetting one buffer's owner changed another buffer");
        check(sceVideodec2DeleteDecoder_nid_postfix(older) == 0 && sceVideodec2DeleteDecoder_nid_postfix(newer) == 0 && sceVideodec2DeleteDecoder_nid_postfix(third) == 0, "delete failed");
    }
    for (const auto codec : {CodecHevc, CodecVp9}) {
        const auto units = codec == CodecHevc ? hevcUnits(HevcStream.data(), HevcStream.size()) : ivfFrames();
        const auto avc = createDecoder(1, 100);
        const auto foreign = createDecoder(codec, codec == CodecHevc ? 1 : 0);
        publishAvc(avc, 1000, shared);
        publishClip(foreign, units, shared);
        checkThrows([&] { query(shared); });
        check(sceVideodec2DeleteDecoder_nid_postfix(avc) == 0, "delete failed");
        const auto newer = createDecoder(1, 100);
        const auto latest = publishAvc(newer, 5000, shared);
        check(owned(shared, latest), "a decoder of another codec answered for an AVC picture");
        check(sceVideodec2DeleteDecoder_nid_postfix(foreign) == 0 && sceVideodec2DeleteDecoder_nid_postfix(newer) == 0, "delete failed");
    }
}

void testConcurrentOwnership() {
    std::vector<std::uint8_t> shared(frameBufferBytes());
    const auto keeper = createDecoder(1, 100);
    const auto kept = publishAvc(keeper, 1000, shared);
    std::atomic<bool> done = false;
    std::string failure;
    std::thread reader([&] {
        try {
            while (!done.load()) {
                const auto info = query(shared);
                check(!info.isValid || info.ptsData == kept || (info.ptsData >= 5000 && info.ptsData < 5000 + PictureHashes.size()), "a query returned metadata no decoder published");
            }
        } catch (const std::exception& error) {
            failure = error.what();
        }
    });
    try {
        for (int round = 0; round < 8; ++round) {
            const auto writer = createDecoder(1, 100);
            publishAvc(writer, 5000, shared);
            if (round % 2 == 0) check(sceVideodec2Reset_nid_postfix(writer) == 0, "reset failed");
            check(sceVideodec2DeleteDecoder_nid_postfix(writer) == 0, "delete failed");
            check(!query(shared).isValid, "a removed writer left its picture behind");
        }
    } catch (...) {
        done = true;
        reader.join();
        throw;
    }
    done = true;
    reader.join();
    check(failure.empty(), failure);
    check(sceVideodec2DeleteDecoder_nid_postfix(keeper) == 0, "delete failed");
}

}

int main() {
    try {
        testFailures();
        testDecode(false);
        testDecode(true);
        testLengthPrefixLooksLikeStartCode();
        testStartCodeLooksLikeLengthPrefix();
        testDecode(false, true);
        const auto hevc = hevcUnits(HevcStream.data(), HevcStream.size());
        testClip(CodecHevc, 1, hevc, HevcHashes);
        std::vector<std::vector<std::uint8_t>> hevcLengthPrefixed;
        for (const auto& unit : hevc) {
            std::vector<std::uint8_t> converted;
            std::vector<std::size_t> nals;
            for (std::size_t index = 0; index + 3 < unit.size(); ++index) {
                if (unit[index] == 0 && unit[index + 1] == 0 && unit[index + 2] == 1) {
                    nals.push_back(index + 3);
                    index += 2;
                }
            }
            for (std::size_t nal = 0; nal < nals.size(); ++nal) {
                auto end = nal + 1 < nals.size() ? nals[nal + 1] - 3 : unit.size();
                while (end > nals[nal] && unit[end - 1] == 0) --end;
                const auto length = end - nals[nal];
                for (int shift = 24; shift >= 0; shift -= 8) converted.push_back(static_cast<std::uint8_t>(length >> shift));
                converted.insert(converted.end(), unit.begin() + static_cast<std::ptrdiff_t>(nals[nal]), unit.begin() + static_cast<std::ptrdiff_t>(end));
            }
            hevcLengthPrefixed.push_back(std::move(converted));
        }
        testClip(CodecHevc, 1, hevcLengthPrefixed, HevcHashes);
        testClip(CodecVp9, 0, ivfFrames(), Vp9Hashes);
        testVp9PictureMetadata();
        testTenBit();
        testDecodeAfterFlush();
        testEmptyAccessUnit();
        testFlushAfterEveryUnit();
        testPictureOwnership();
        testConcurrentOwnership();
        std::puts("Videodec2 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
