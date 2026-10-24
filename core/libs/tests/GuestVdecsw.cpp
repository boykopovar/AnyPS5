#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include "prx/libc/include/General.hpp"
#include "Vp9Fixture.hpp"
#include "H264Fixture.hpp"
#include "HevcFixture.hpp"

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
    std::uint64_t computeQueue;
    std::uint64_t cpuAffinityMask;
    std::int32_t cpuThreadPriority;
    bool optimizeProgressiveVideo;
    std::uint8_t reserved[19];
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

struct InputResult {
    std::uint64_t thisSize;
    const void* decodedAu;
    std::uint32_t outputFrameCount;
    std::uint32_t reserved;
};

struct FrameBuffer {
    std::uint64_t thisSize;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
};

struct OutputInfo {
    std::uint64_t thisSize;
    bool isValid;
    bool isLastFrame;
    bool isErrorFrame;
    std::uint8_t pictureCount;
    std::uint32_t codecType;
    std::uint32_t frameWidth;
    std::uint32_t framePitch;
    std::uint32_t frameHeight;
    bool isDiscardedFrame;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
    std::uint32_t frameFormat;
    std::uint32_t framePitchInBytes;
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

static_assert(sizeof(Vp9PictureInfo) == 0x58 && offsetof(Vp9PictureInfo, ptsData) == 0x10 && offsetof(Vp9PictureInfo, profile) == 0x28 && offsetof(Vp9PictureInfo, bitDepth) == 0x38 && offsetof(Vp9PictureInfo, colorSpace) == 0x3c && offsetof(Vp9PictureInfo, renderHeight) == 0x50);

struct AvcPictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint8_t idrPictureFlag;
    std::uint8_t profileIdc;
    std::uint8_t levelIdc;
    std::uint32_t picWidthInMbsMinus1;
    std::uint32_t picHeightInMapUnitsMinus1;
    std::uint8_t frameMbsOnlyFlag;
    std::uint8_t frameCroppingFlag;
    std::uint32_t frameCropLeftOffset;
    std::uint32_t frameCropRightOffset;
    std::uint32_t frameCropTopOffset;
    std::uint32_t frameCropBottomOffset;
    std::uint8_t vui[44];
};

struct HevcPictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint32_t picWidthInLumaSamples;
    std::uint32_t picHeightInLumaSamples;
    std::uint8_t bitDepthLumaMinus8;
    std::uint8_t bitDepthChromaMinus8;
    std::uint8_t timingInfoPresentFlag;
    std::uint32_t numUnitsInTick;
    std::uint32_t timeScale;
    std::uint32_t aspectRatioInfoPresentFlag;
    std::uint8_t aspectRatioIdc;
    std::uint16_t sarWidth;
    std::uint16_t sarHeight;
    std::uint8_t videoSignal[8];
    std::uint32_t picStruct;
    std::uint32_t sourceScanType;
    std::uint32_t duplicateFlag;
    std::uint32_t conformanceWindowFlag;
    std::uint32_t confWinLeftOffset;
    std::uint32_t confWinRightOffset;
    std::uint32_t confWinTopOffset;
    std::uint32_t confWinBottomOffset;
    std::uint32_t defaultDisplayWindow[5];
    std::uint8_t chromaLocation[4];
    std::uint8_t presentFlags[11];
    std::uint8_t idrPictureFlag;
    std::uint8_t irapPictureFlag;
    std::uint8_t generalProfileSpace;
    std::uint8_t generalTierFlag;
    std::uint8_t generalProfileIdc;
    std::uint8_t generalSourceFlags[3];
    std::uint8_t generalLevelIdc;
    std::uint8_t subLayer[9];
    std::uint8_t frameCroppingFlag;
    std::uint32_t frameCropLeftOffset;
    std::uint32_t frameCropRightOffset;
    std::uint32_t frameCropTopOffset;
    std::uint32_t frameCropBottomOffset;
};

static_assert(sizeof(HevcPictureInfo) == 0xb8 && offsetof(HevcPictureInfo, idrPictureFlag) == 0x93 && offsetof(HevcPictureInfo, frameCroppingFlag) == 0xa5);
static_assert(sizeof(DecoderConfigInfo) == 0x50 && sizeof(OutputInfo) == 0x38 && offsetof(OutputInfo, isLastFrame) == 9 && offsetof(OutputInfo, pictureCount) == 0xb && sizeof(AvcPictureInfo) == 0x78);

extern "C" {
int APS5_VABI sceVdecswQueryComputeMemoryInfo(ComputeMemoryInfo*);
int APS5_VABI sceVdecswAllocateComputeQueue(const ComputeConfigInfo*, const ComputeMemoryInfo*, std::uint64_t*);
int APS5_VABI sceVdecswReleaseComputeQueue(std::uint64_t);
int APS5_VABI sceVdecswQueryDecoderMemoryInfo(const DecoderConfigInfo*, DecoderMemoryInfo*);
int APS5_VABI sceVdecswCreateDecoder(const DecoderConfigInfo*, const DecoderMemoryInfo*, std::uint64_t*);
int APS5_VABI sceVdecswDeleteDecoder(std::uint64_t);
int APS5_VABI sceVdecswSetDecodeInput(std::uint64_t, const InputData*);
int APS5_VABI sceVdecswTrySyncDecodeInput(std::uint64_t, InputResult*);
int APS5_VABI sceVdecswSyncDecodeInput(std::uint64_t, InputResult*);
int APS5_VABI sceVdecswSetDecodeOutput(std::uint64_t, const FrameBuffer*);
int APS5_VABI sceVdecswTrySyncDecodeOutput(std::uint64_t, OutputInfo*);
int APS5_VABI sceVdecswSyncDecodeOutput(std::uint64_t, OutputInfo*);
int APS5_VABI sceVdecswGetPictureInfo(const OutputInfo*, void*, void*);
int APS5_VABI sceVdecswGetVp9PictureInfo(const OutputInfo*, Vp9PictureInfo*);
int APS5_VABI sceVdecswGetAvcPictureInfo(const OutputInfo*, AvcPictureInfo*, AvcPictureInfo*);
int APS5_VABI sceVdecswGetHevcPictureInfo(const OutputInfo*, HevcPictureInfo*);
int APS5_VABI sceVdecswFinalizeDecodeSequence(std::uint64_t);
int APS5_VABI sceVdecswResetDecoder(std::uint64_t);
}

namespace {

using namespace H264Fixture;
using namespace HevcFixture;
using namespace Vp9Fixture;

constexpr int OutputPending = static_cast<int>(0x81510115u);
constexpr int InputQueueEmpty = static_cast<int>(0x81510116u);
constexpr std::uint32_t CodecAvc = 1;
constexpr std::uint32_t CodecHevc = 974921;
constexpr std::uint32_t CodecVp9 = 2382845;
constexpr std::array<std::size_t, 6> HevcDisplayOrderUnits{0, 3, 2, 1, 5, 4};

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Session {
    std::uint64_t queue = 0;
    std::uint64_t decoder = 0;
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};

    explicit Session(std::uint32_t codec = CodecAvc, std::uint32_t profile = 100, std::uint32_t level = 42) {
        ComputeMemoryInfo compute{sizeof(ComputeMemoryInfo)};
        check(sceVdecswQueryComputeMemoryInfo(&compute) == 0 && compute.cpuGpuMemorySize != 0, "compute memory query failed");
        const ComputeConfigInfo computeConfig{sizeof(ComputeConfigInfo), 3, 3, true};
        check(sceVdecswAllocateComputeQueue(&computeConfig, &compute, &queue) == 0, "compute queue allocation failed");
        DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, codec, profile, level, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 3, queue, 0x3f, 0, true, {}};
        check(sceVdecswQueryDecoderMemoryInfo(&config, &memory) == 0 && memory.maxFrameBufferSize != 0, "decoder memory query failed");
        check(sceVdecswCreateDecoder(&config, &memory, &decoder) == 0, "decoder creation failed");
    }

    ~Session() {
        sceVdecswDeleteDecoder(decoder);
        sceVdecswReleaseComputeQueue(queue);
    }
};

template <typename TAction>
bool throws(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

template <typename TInfo>
void checkGenericPictureInfo(const OutputInfo& output, const TInfo& typed) {
    const std::array<std::uint64_t, 7> sizes{8, 9, 0x28, sizeof(TInfo) - 1, sizeof(TInfo), sizeof(TInfo) + 16, std::numeric_limits<std::uint64_t>::max()};
    for (const auto size : sizes) {
        std::array<std::uint8_t, sizeof(TInfo) + 32> storage;
        storage.fill(0xcc);
        std::memcpy(storage.data() + 1, &size, sizeof(size));
        auto expected = storage;
        const auto copied = std::min<std::uint64_t>(size, sizeof(TInfo)) - sizeof(size);
        std::memcpy(expected.data() + 1 + sizeof(size), reinterpret_cast<const std::uint8_t*>(&typed) + sizeof(size), copied);
        auto second = storage;
        const auto secondBefore = second;
        check(sceVdecswGetPictureInfo(&output, storage.data() + 1, second.data()) == 0, "generic picture info query failed");
        check(storage == expected && second == secondBefore, "generic picture info bytes, size or guards differ");
    }
    for (std::uint64_t size = 0; size < sizeof(size); ++size) {
        auto tooSmall = typed;
        tooSmall.thisSize = size;
        const auto before = tooSmall;
        check(throws([&] { sceVdecswGetPictureInfo(&output, &tooSmall, nullptr); }), "generic picture info accepted an undersized size field");
        check(std::memcmp(&tooSmall, &before, sizeof(tooSmall)) == 0, "an invalid generic query changed its output");
    }
    auto second = typed;
    const auto secondBefore = second;
    check(sceVdecswGetPictureInfo(&output, nullptr, &second) == 0, "generic picture info rejected a null first output");
    check(std::memcmp(&second, &secondBefore, sizeof(second)) == 0, "generic picture info wrote a nonexistent second picture");
    auto shorter = output;
    shorter.thisSize = sizeof(OutputInfo) - 8;
    auto actual = typed;
    check(sceVdecswGetPictureInfo(&shorter, &actual, nullptr) == 0 && std::memcmp(&actual, &typed, sizeof(actual)) == 0, "generic picture info rejected the shorter output descriptor");
}

void testGenericPictureInfoValidation() {
    OutputInfo output{sizeof(OutputInfo)};
    std::array<std::uint64_t, 8> first;
    first.fill(0xccccccccccccccccull);
    const auto before = first;
    auto second = first;
    check(throws([&] { sceVdecswGetPictureInfo(nullptr, first.data(), second.data()); }), "generic picture info accepted a null output descriptor");
    check(first == before && second == before, "a null descriptor changed picture info");
    check(sceVdecswGetPictureInfo(&output, first.data(), second.data()) == 0 && first == before && second == before, "an empty output changed picture info");
    check(sceVdecswGetPictureInfo(&output, nullptr, nullptr) == 0, "an empty output required picture-info pointers");
    for (const auto size : {0ull, 0x28ull, 0x31ull, 0x40ull, 0x100000038ull}) {
        output.thisSize = size;
        check(throws([&] { sceVdecswGetPictureInfo(&output, first.data(), second.data()); }) && first == before && second == before, "generic picture info accepted an invalid descriptor size");
    }
    output.thisSize = sizeof(OutputInfo);
    first[0] = sizeof(first);
    const auto firstBefore = first;
    output.pictureCount = 1;
    output.frameBuffer = first.data();
    check(throws([&] { sceVdecswGetPictureInfo(&output, first.data(), nullptr); }) && first == firstBefore, "generic picture info accepted a foreign frame buffer");
    output.frameBuffer = nullptr;
    check(throws([&] { sceVdecswGetPictureInfo(&output, first.data(), nullptr); }) && first == firstBefore, "generic picture info accepted a null frame buffer");
}

void testGenericPictureInfoLifetime() {
    OutputInfo output{sizeof(OutputInfo)};
    std::vector<std::uint8_t> buffer;
    {
        Session session;
        buffer.resize(session.memory.maxFrameBufferSize);
        const auto units = accessUnits(false);
        for (std::uint64_t iteration = 0; iteration < 2; ++iteration) {
            const InputData input{sizeof(InputData), units[0].data(), units[0].size(), 1000 + iteration, iteration, 0xa0 + iteration};
            check(sceVdecswSetDecodeInput(session.decoder, &input) == 0, "generic lifetime input failed");
            InputResult consumed{sizeof(InputResult)};
            check(sceVdecswSyncDecodeInput(session.decoder, &consumed) == 0, "generic lifetime input was not consumed");
            const FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size()};
            check(sceVdecswSetDecodeOutput(session.decoder, &frame) == 0, "generic lifetime buffer failed");
            check(sceVdecswFinalizeDecodeSequence(session.decoder) == 0, "generic lifetime finalize failed");
            check(sceVdecswSyncDecodeOutput(session.decoder, &output) == 0 && output.isValid, "generic lifetime picture missing");
            AvcPictureInfo info{sizeof(AvcPictureInfo)};
            check(sceVdecswGetPictureInfo(&output, &info, nullptr) == 0 && info.ptsData == input.ptsData && info.dtsData == iteration && info.attachedData == input.attachedData, "generic picture info retained stale timestamps");
            if (iteration == 0) {
                check(sceVdecswResetDecoder(session.decoder) == 0, "generic lifetime reset failed");
                const auto before = info;
                check(throws([&] { sceVdecswGetPictureInfo(&output, &info, nullptr); }) && std::memcmp(&info, &before, sizeof(info)) == 0, "generic AVC picture info survived a decoder reset");
            }
        }
    }
    AvcPictureInfo info{sizeof(AvcPictureInfo)};
    const auto before = info;
    check(throws([&] { sceVdecswGetPictureInfo(&output, &info, nullptr); }) && std::memcmp(&info, &before, sizeof(info)) == 0, "generic picture info survived decoder destruction");
}

void testDecode() {
    Session session;
    const auto units = accessUnits(false);
    check(units.size() == PictureHashes.size(), "unexpected access unit count");
    std::array<std::vector<std::uint8_t>, 2> buffers{std::vector<std::uint8_t>(session.memory.maxFrameBufferSize), std::vector<std::uint8_t>(session.memory.maxFrameBufferSize)};
    InputResult result{sizeof(InputResult)};
    check(sceVdecswTrySyncDecodeInput(session.decoder, &result) == InputQueueEmpty && result.decodedAu == nullptr, "an idle decoder did not report an empty input queue");
    std::size_t pictures = 0;
    bool outputSet = false;
    bool ended = false;
    const auto drain = [&] {
        for (;;) {
            if (!outputSet) {
                const FrameBuffer frame{sizeof(FrameBuffer), buffers[pictures % 2].data(), buffers[pictures % 2].size()};
                check(sceVdecswSetDecodeOutput(session.decoder, &frame) == 0, "output buffer was rejected");
                outputSet = true;
            }
            OutputInfo output{sizeof(OutputInfo)};
            const auto status = sceVdecswTrySyncDecodeOutput(session.decoder, &output);
            if (status == OutputPending) return;
            check(status == 0, "output sync failed");
            outputSet = false;
            if (!output.isValid) {
                check(output.isLastFrame && output.pictureCount == 0, "an output without a picture is not the end of the sequence");
                ended = true;
                return;
            }
            check(pictures < PictureHashes.size(), "more pictures than access units");
            check(output.isLastFrame == (pictures + 1 == PictureHashes.size()), "only the last picture of the finalized sequence is marked last");
            check(!output.isErrorFrame && !output.isDiscardedFrame && output.pictureCount == 1 && output.frameWidth == Width && output.frameHeight == CodedHeight && output.frameBuffer == buffers[pictures % 2].data(), "unexpected picture geometry");
            AvcPictureInfo info{sizeof(AvcPictureInfo)};
            check(sceVdecswGetAvcPictureInfo(&output, &info, nullptr) == 0 && info.isValid, "picture info missing");
            checkGenericPictureInfo(output, info);
            const auto unit = DisplayOrderUnits[pictures];
            check(info.ptsData == 1000 + unit && info.dtsData == unit && info.attachedData == 0xa0 + unit && info.idrPictureFlag == (pictures == 0 ? 1 : 0) && info.profileIdc == 100, "pictures out of display order");
            check(info.picWidthInMbsMinus1 == Width / 16 - 1 && info.picHeightInMapUnitsMinus1 == CodedHeight / 16 - 1 && info.frameMbsOnlyFlag == 1 && info.frameCroppingFlag == 1 && info.frameCropLeftOffset == 0 && info.frameCropRightOffset == 0 && info.frameCropTopOffset == 0 && info.frameCropBottomOffset == (CodedHeight - Height) / 2, "picture geometry in the picture info is wrong");
            check(hashNv12(static_cast<const std::uint8_t*>(output.frameBuffer), output.framePitch, output.frameHeight) == PictureHashes[pictures], "picture " + std::to_string(pictures) + " differs from the reference");
            ++pictures;
        }
    };
    std::uint32_t produced = 0;
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        check(sceVdecswSetDecodeInput(session.decoder, &input) == 0, "decode input was rejected");
        InputResult consumed{sizeof(InputResult)};
        check(sceVdecswTrySyncDecodeInput(session.decoder, &consumed) == 0 && consumed.decodedAu == units[unit].data(), "an access unit was not consumed");
        produced += consumed.outputFrameCount;
        drain();
    }
    check(pictures < PictureHashes.size(), "the last picture was returned before the sequence was finalized");
    check(sceVdecswFinalizeDecodeSequence(session.decoder) == 0, "finalize failed");
    while (!ended) drain();
    check(pictures == PictureHashes.size(), "missing pictures after finalizing the sequence");
    check(produced <= PictureHashes.size(), "more pictures counted than decoded");
    check(sceVdecswTrySyncDecodeInput(session.decoder, &result) == InputQueueEmpty, "a finalized decoder did not report an empty input queue");
}

void testHevc() {
    Session session(CodecHevc, 1, 30);
    const auto units = hevcUnits(HevcStream.data(), HevcStream.size());
    check(units.size() == HevcHashes.size(), "unexpected HEVC access unit count");
    std::vector<std::uint8_t> buffer(session.memory.maxFrameBufferSize);
    std::size_t pictures = 0;
    bool ended = false;
    const auto drain = [&] {
        for (;;) {
            const FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size()};
            check(sceVdecswSetDecodeOutput(session.decoder, &frame) == 0, "HEVC output buffer was rejected");
            OutputInfo output{sizeof(OutputInfo)};
            const auto status = sceVdecswTrySyncDecodeOutput(session.decoder, &output);
            if (status == OutputPending) return;
            check(status == 0, "HEVC output sync failed");
            if (!output.isValid) {
                ended = true;
                return;
            }
            check(pictures < HevcHashes.size() && output.codecType == CodecHevc && output.pictureCount == 1 && output.frameWidth == Width && output.frameHeight == Height, "unexpected HEVC picture geometry");
            HevcPictureInfo info{sizeof(HevcPictureInfo)};
            check(sceVdecswGetHevcPictureInfo(&output, &info) == 0 && info.isValid, "HEVC picture info missing");
            checkGenericPictureInfo(output, info);
            const auto unit = HevcDisplayOrderUnits[pictures];
            check(info.ptsData == 1000 + unit && info.dtsData == unit && info.attachedData == 0xa0 + unit, "HEVC pictures out of display order");
            check(info.idrPictureFlag == (pictures == 0 ? 1 : 0) && info.irapPictureFlag == (pictures == 0 ? 1 : 0), "HEVC random access flags differ");
            check(info.generalProfileIdc == 1 && info.generalLevelIdc == 30 && info.bitDepthLumaMinus8 == 0 && info.bitDepthChromaMinus8 == 0, "HEVC profile, level or bit depth differs");
            check(info.picWidthInLumaSamples == Width && info.picHeightInLumaSamples == Height && info.conformanceWindowFlag == 0 && info.frameCroppingFlag == 0, "HEVC picture geometry in the picture info is wrong");
            AvcPictureInfo avc{sizeof(AvcPictureInfo)};
            bool avcThrows = false;
            try {
                sceVdecswGetAvcPictureInfo(&output, &avc, nullptr);
            } catch (const std::exception&) {
                avcThrows = true;
            }
            check(avcThrows, "AVC picture info was returned for an HEVC picture");
            check(hashNv12(buffer.data(), output.framePitch, output.frameHeight) == HevcHashes[pictures], "HEVC picture " + std::to_string(pictures) + " differs from the reference");
            ++pictures;
        }
    };
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        check(sceVdecswSetDecodeInput(session.decoder, &input) == 0, "HEVC decode input was rejected");
        InputResult consumed{sizeof(InputResult)};
        check(sceVdecswTrySyncDecodeInput(session.decoder, &consumed) == 0 && consumed.decodedAu == units[unit].data(), "an HEVC access unit was not consumed");
        drain();
    }
    check(sceVdecswFinalizeDecodeSequence(session.decoder) == 0, "HEVC finalize failed");
    while (!ended) drain();
    check(pictures == HevcHashes.size(), "missing HEVC pictures after finalizing the sequence");
}

void testHevcCrop() {
    Session session(CodecHevc, 1, 30);
    std::vector<std::uint8_t> buffer(session.memory.maxFrameBufferSize);
    const FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size()};
    check(sceVdecswSetDecodeOutput(session.decoder, &frame) == 0, "cropped HEVC output buffer was rejected");
    const InputData input{sizeof(InputData), HevcCroppedStream.data(), HevcCroppedStream.size(), 1000, 0, 0xa0};
    check(sceVdecswSetDecodeInput(session.decoder, &input) == 0, "cropped HEVC decode input was rejected");
    InputResult consumed{sizeof(InputResult)};
    check(sceVdecswSyncDecodeInput(session.decoder, &consumed) == 0, "the cropped HEVC access unit was not consumed");
    check(sceVdecswFinalizeDecodeSequence(session.decoder) == 0, "cropped HEVC finalize failed");
    OutputInfo output{sizeof(OutputInfo)};
    check(sceVdecswSyncDecodeOutput(session.decoder, &output) == 0 && output.isValid && output.frameWidth == 64 && output.frameHeight == 40, "unexpected cropped HEVC picture geometry");
    HevcPictureInfo info{sizeof(HevcPictureInfo)};
    check(sceVdecswGetHevcPictureInfo(&output, &info) == 0 && info.isValid, "cropped HEVC picture info missing");
    checkGenericPictureInfo(output, info);
    check(info.picWidthInLumaSamples == 64 && info.picHeightInLumaSamples == 40, "cropped HEVC coded size differs");
    check(info.conformanceWindowFlag == 1 && info.confWinLeftOffset == 0 && info.confWinRightOffset == 0 && info.confWinTopOffset == 0 && info.confWinBottomOffset == 2, "HEVC conformance window differs");
    check(info.frameCroppingFlag == 1 && info.frameCropLeftOffset == 0 && info.frameCropRightOffset == 0 && info.frameCropTopOffset == 0 && info.frameCropBottomOffset == 2, "HEVC frame cropping differs");
}

void testWithoutOutput() {
    Session session;
    const auto units = accessUnits(false);
    const InputData input{sizeof(InputData), units[0].data(), units[0].size(), 1000, 0, 0xa0};
    check(sceVdecswSetDecodeInput(session.decoder, &input) == 0, "decode input was rejected");
    InputResult result{sizeof(InputResult)};
    check(sceVdecswTrySyncDecodeInput(session.decoder, &result) == 0 && result.decodedAu == units[0].data(), "an input was not decoded before an output buffer was set");
    std::vector<std::uint8_t> buffer(session.memory.maxFrameBufferSize);
    const FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size()};
    check(sceVdecswSetDecodeOutput(session.decoder, &frame) == 0, "output buffer was rejected");
    OutputInfo output{sizeof(OutputInfo)};
    check(sceVdecswTrySyncDecodeOutput(session.decoder, &output) == OutputPending, "the only picture of an open sequence was returned before it was known to be the last");
    check(sceVdecswFinalizeDecodeSequence(session.decoder) == 0, "finalize failed");
    check(sceVdecswTrySyncDecodeOutput(session.decoder, &output) == 0 && output.isValid && output.isLastFrame, "the only picture of a finalized sequence is not marked last");
    check(sceVdecswResetDecoder(session.decoder) == 0, "reset failed");
    check(sceVdecswTrySyncDecodeInput(session.decoder, &result) == InputQueueEmpty, "a reset decoder kept its inputs");
}

void testSyncWrappers() {
    Session session;
    InputResult idle{sizeof(InputResult)};
    check(sceVdecswSyncDecodeInput(session.decoder, &idle) == InputQueueEmpty, "sync input on an idle decoder did not report an empty queue");
    const auto units = accessUnits(false);
    const InputData input{sizeof(InputData), units[0].data(), units[0].size(), 1000, 0, 0xa0};
    check(sceVdecswSetDecodeInput(session.decoder, &input) == 0, "decode input was rejected");
    InputResult result{sizeof(InputResult)};
    check(sceVdecswSyncDecodeInput(session.decoder, &result) == 0 && result.decodedAu == units[0].data(), "sync input did not consume the access unit");
    std::vector<std::uint8_t> buffer(session.memory.maxFrameBufferSize);
    const FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size()};
    check(sceVdecswSetDecodeOutput(session.decoder, &frame) == 0, "output buffer was rejected");
    check(sceVdecswFinalizeDecodeSequence(session.decoder) == 0, "finalize failed");
    OutputInfo output{sizeof(OutputInfo)};
    check(sceVdecswSyncDecodeOutput(session.decoder, &output) == 0 && output.isValid && output.isLastFrame, "sync output did not return the finalized picture");
    HevcPictureInfo hevc{sizeof(HevcPictureInfo)};
    bool hevcThrows = false;
    try {
        sceVdecswGetHevcPictureInfo(&output, &hevc);
    } catch (const std::exception&) {
        hevcThrows = true;
    }
    check(hevcThrows, "HEVC picture info was returned for an H.264 picture");
}

struct Vp9Session {
    std::uint64_t queue = 0;
    std::uint64_t decoder = 0;
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};

    Vp9Session() {
        ComputeMemoryInfo compute{sizeof(ComputeMemoryInfo)};
        check(sceVdecswQueryComputeMemoryInfo(&compute) == 0 && compute.cpuGpuMemorySize != 0, "compute memory query failed");
        const ComputeConfigInfo computeConfig{sizeof(ComputeConfigInfo), 3, 3, true};
        check(sceVdecswAllocateComputeQueue(&computeConfig, &compute, &queue) == 0, "compute queue allocation failed");
        DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, CodecVp9, 0, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 3, queue, 0x3f, 0, true, {}};
        check(sceVdecswQueryDecoderMemoryInfo(&config, &memory) == 0 && memory.maxFrameBufferSize != 0, "VP9 decoder memory query failed");
        check(sceVdecswCreateDecoder(&config, &memory, &decoder) == 0, "VP9 decoder creation failed");
    }

    ~Vp9Session() {
        sceVdecswDeleteDecoder(decoder);
        sceVdecswReleaseComputeQueue(queue);
    }
};

Vp9PictureInfo sentinelVp9Info(std::uint64_t size = sizeof(Vp9PictureInfo)) {
    Vp9PictureInfo info;
    std::memset(&info, 0xcc, sizeof(info));
    info.thisSize = size;
    return info;
}

bool sameBytes(const Vp9PictureInfo& left, const Vp9PictureInfo& right) {
    return std::memcmp(&left, &right, sizeof(Vp9PictureInfo)) == 0;
}

void checkVp9Info(const Vp9PictureInfo& info, std::size_t picture) {
    const std::string name = "VP9 picture " + std::to_string(picture);
    check(info.thisSize == sizeof(Vp9PictureInfo) && info.isValid, name + " has no picture info");
    check(info.ptsData == 1000 + picture && info.dtsData == picture && info.attachedData == 0xa0 + picture, name + " timestamps differ");
    check(info.profile == 0 && info.bitDepth == 8 && info.colorRange == 0 && info.colorSpace == 0 && info.numFrames == 1 && info.intraOnly == 0, name + " coded header fields differ");
    check(info.keyFrameFlag == (picture == 0 ? 1u : 0u), name + " key frame flag differs");
    check(info.frameWidth == Width && info.frameHeight == Height && info.renderWidth == Width && info.renderHeight == Height, name + " dimensions differ");
}

void testVp9PictureInfo() {
    Vp9Session session;
    const auto units = ivfFrames();
    check(units.size() == Vp9Hashes.size(), "unexpected VP9 access unit count");
    std::array<std::vector<std::uint8_t>, Vp9Hashes.size()> buffers;
    for (auto& buffer : buffers) buffer.resize(session.memory.maxFrameBufferSize);
    std::array<OutputInfo, Vp9Hashes.size()> outputs{};
    std::size_t pictures = 0;
    bool outputSet = false;
    bool ended = false;
    const auto drain = [&] {
        for (;;) {
            auto& target = buffers[pictures % buffers.size()];
            if (!outputSet) {
                const FrameBuffer frame{sizeof(FrameBuffer), target.data(), target.size()};
                check(sceVdecswSetDecodeOutput(session.decoder, &frame) == 0, "VP9 output buffer was rejected");
                outputSet = true;
            }
            OutputInfo output{sizeof(OutputInfo)};
            const auto status = sceVdecswTrySyncDecodeOutput(session.decoder, &output);
            if (status == OutputPending) return;
            check(status == 0, "VP9 output sync failed");
            outputSet = false;
            if (!output.isValid) {
                ended = true;
                return;
            }
            check(pictures < Vp9Hashes.size(), "more VP9 pictures than access units");
            check(output.codecType == CodecVp9 && output.pictureCount == 1 && output.frameWidth == Width && output.frameHeight == Height && output.frameBuffer == target.data(), "unexpected VP9 picture geometry");
            auto info = sentinelVp9Info();
            check(sceVdecswGetVp9PictureInfo(&output, &info) == 0, "VP9 picture info query failed");
            checkGenericPictureInfo(output, info);
            checkVp9Info(info, pictures);
            check(hashNv12(target.data(), output.framePitch, output.frameHeight) == Vp9Hashes[pictures], "VP9 picture " + std::to_string(pictures) + " differs from the reference");
            outputs[pictures] = output;
            ++pictures;
        }
    };
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        check(sceVdecswSetDecodeInput(session.decoder, &input) == 0, "VP9 decode input was rejected");
        InputResult consumed{sizeof(InputResult)};
        check(sceVdecswTrySyncDecodeInput(session.decoder, &consumed) == 0 && consumed.decodedAu == units[unit].data(), "a VP9 access unit was not consumed");
        drain();
    }
    check(sceVdecswFinalizeDecodeSequence(session.decoder) == 0, "VP9 finalize failed");
    while (!ended) drain();
    check(pictures == Vp9Hashes.size(), "missing VP9 pictures after finalizing the sequence");

    for (std::size_t picture = 0; picture < outputs.size(); ++picture) {
        auto info = sentinelVp9Info();
        check(sceVdecswGetVp9PictureInfo(&outputs[picture], &info) == 0, "VP9 picture info of an earlier picture was lost");
        checkVp9Info(info, picture);
        checkGenericPictureInfo(outputs[picture], info);
    }

    const auto unchanged = sentinelVp9Info();
    auto info = unchanged;
    check(throws([&] { sceVdecswGetVp9PictureInfo(nullptr, &info); }) && sameBytes(info, unchanged), "a null output info was accepted");
    check(throws([&] { sceVdecswGetVp9PictureInfo(&outputs[0], nullptr); }), "a null VP9 picture info was accepted");
    auto tooSmall = sentinelVp9Info(7);
    const auto tooSmallBefore = tooSmall;
    check(throws([&] { sceVdecswGetVp9PictureInfo(&outputs[0], &tooSmall); }) && sameBytes(tooSmall, tooSmallBefore), "a VP9 picture info smaller than its size field was accepted");
    auto doubled = outputs[0];
    doubled.pictureCount = 2;
    check(throws([&] { sceVdecswGetVp9PictureInfo(&doubled, &info); }) && sameBytes(info, unchanged), "an output with two pictures was accepted");
    auto empty = outputs[0];
    empty.pictureCount = 0;
    check(sceVdecswGetVp9PictureInfo(&empty, &info) == 0 && sameBytes(info, unchanged), "an output without a picture wrote a VP9 picture info");
    std::uint8_t foreign = 0;
    auto unknown = outputs[0];
    unknown.frameBuffer = &foreign;
    check(throws([&] { sceVdecswGetVp9PictureInfo(&unknown, &info); }) && sameBytes(info, unchanged), "a frame buffer no decoder returned was accepted");
    unknown.frameBuffer = nullptr;
    check(throws([&] { sceVdecswGetVp9PictureInfo(&unknown, &info); }) && sameBytes(info, unchanged), "a null frame buffer was accepted");

    const auto head = offsetof(Vp9PictureInfo, profile);
    auto partial = sentinelVp9Info(head);
    check(sceVdecswGetVp9PictureInfo(&outputs[2], &partial) == 0, "a VP9 picture info with a short size field was rejected");
    check(partial.thisSize == head && partial.isValid && partial.ptsData == 1002 && partial.dtsData == 2 && partial.attachedData == 0xa2, "the leading VP9 picture info fields were not written");
    check(partial.profile == unchanged.profile && partial.frameWidth == unchanged.frameWidth && partial.renderHeight == unchanged.renderHeight, "a VP9 picture info with a short size field was written beyond it");

    struct Oversized {
        Vp9PictureInfo info;
        std::uint8_t tail[64];
    } oversized;
    std::memset(&oversized, 0xcc, sizeof(oversized));
    oversized.info.thisSize = sizeof(Oversized);
    check(sceVdecswGetVp9PictureInfo(&outputs[3], &oversized.info) == 0, "a VP9 picture info with a long size field was rejected");
    check(oversized.info.thisSize == sizeof(Oversized), "a long VP9 picture info size field was changed");
    oversized.info.thisSize = sizeof(Vp9PictureInfo);
    checkVp9Info(oversized.info, 3);
    for (const auto byte : oversized.tail) check(byte == 0xcc, "a VP9 picture info with a long size field was written beyond the structure");

    AvcPictureInfo avc{sizeof(AvcPictureInfo)};
    check(throws([&] { sceVdecswGetAvcPictureInfo(&outputs[0], &avc, nullptr); }), "AVC picture info was returned for a VP9 picture");

    check(sceVdecswResetDecoder(session.decoder) == 0, "VP9 reset failed");
    check(throws([&] { sceVdecswGetVp9PictureInfo(&outputs[0], &info); }) && sameBytes(info, unchanged), "VP9 picture info survived a decoder reset");
    check(throws([&] { sceVdecswGetPictureInfo(&outputs[0], &info, nullptr); }) && sameBytes(info, unchanged), "generic picture info survived a decoder reset");
}

void testVp9PictureInfoOfAvcPicture() {
    Session session;
    const auto units = accessUnits(false);
    const InputData input{sizeof(InputData), units[0].data(), units[0].size(), 1000, 0, 0xa0};
    check(sceVdecswSetDecodeInput(session.decoder, &input) == 0, "decode input was rejected");
    InputResult result{sizeof(InputResult)};
    check(sceVdecswSyncDecodeInput(session.decoder, &result) == 0, "the access unit was not consumed");
    std::vector<std::uint8_t> buffer(session.memory.maxFrameBufferSize);
    const FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size()};
    check(sceVdecswSetDecodeOutput(session.decoder, &frame) == 0, "output buffer was rejected");
    check(sceVdecswFinalizeDecodeSequence(session.decoder) == 0, "finalize failed");
    OutputInfo output{sizeof(OutputInfo)};
    check(sceVdecswSyncDecodeOutput(session.decoder, &output) == 0 && output.isValid && output.pictureCount == 1, "sync output did not return the finalized picture");
    const auto unchanged = sentinelVp9Info();
    auto info = unchanged;
    check(throws([&] { sceVdecswGetVp9PictureInfo(&output, &info); }) && sameBytes(info, unchanged), "VP9 picture info was returned for an H.264 picture");
}

}

void testHevcMain10() {
    Session session(CodecHevc, 2, 30);
    std::vector<std::uint8_t> buffer(session.memory.maxFrameBufferSize);
    const FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size()};
    check(sceVdecswSetDecodeOutput(session.decoder, &frame) == 0, "Main 10 HEVC output buffer was rejected");
    const InputData input{sizeof(InputData), HevcMain10Stream.data(), HevcMain10Stream.size(), 1000, 0, 0xa0};
    check(sceVdecswSetDecodeInput(session.decoder, &input) == 0, "Main 10 HEVC decode input was rejected");
    InputResult consumed{sizeof(InputResult)};
    check(sceVdecswSyncDecodeInput(session.decoder, &consumed) == 0, "the Main 10 HEVC access unit was not consumed");
    check(sceVdecswFinalizeDecodeSequence(session.decoder) == 0, "Main 10 HEVC finalize failed");
    OutputInfo output{sizeof(OutputInfo)};
    check(sceVdecswSyncDecodeOutput(session.decoder, &output) == 0 && output.isValid, "the Main 10 HEVC picture was not output");
    HevcPictureInfo info{sizeof(HevcPictureInfo)};
    check(sceVdecswGetHevcPictureInfo(&output, &info) == 0 && info.isValid, "Main 10 HEVC picture info missing");
    checkGenericPictureInfo(output, info);
    check(info.generalProfileIdc == 2 && info.bitDepthLumaMinus8 == 2 && info.bitDepthChromaMinus8 == 2, "Main 10 HEVC profile or bit depth differs");
}

int main() {
    try {
        testGenericPictureInfoValidation();
        testGenericPictureInfoLifetime();
        testDecode();
        testHevc();
        testHevcCrop();
        testHevcMain10();
        testWithoutOutput();
        testSyncWrappers();
        testVp9PictureInfo();
        testVp9PictureInfoOfAvcPicture();
        std::puts("Vdecsw tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
