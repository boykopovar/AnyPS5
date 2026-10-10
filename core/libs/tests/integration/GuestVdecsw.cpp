#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include "prx/libc/include/General.hpp"
#include "H264Fixture.hpp"

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
int APS5_VABI sceVdecswSetDecodeOutput(std::uint64_t, const FrameBuffer*);
int APS5_VABI sceVdecswTrySyncDecodeOutput(std::uint64_t, OutputInfo*);
int APS5_VABI sceVdecswGetAvcPictureInfo(const OutputInfo*, AvcPictureInfo*, AvcPictureInfo*);
int APS5_VABI sceVdecswFinalizeDecodeSequence(std::uint64_t);
int APS5_VABI sceVdecswResetDecoder(std::uint64_t);
}

namespace {

using namespace H264Fixture;

constexpr int OutputPending = static_cast<int>(0x81510115u);
constexpr int InputQueueEmpty = static_cast<int>(0x81510116u);

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Session {
    std::uint64_t queue = 0;
    std::uint64_t decoder = 0;
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};

    Session() {
        ComputeMemoryInfo compute{sizeof(ComputeMemoryInfo)};
        check(sceVdecswQueryComputeMemoryInfo(&compute) == 0 && compute.cpuGpuMemorySize != 0, "compute memory query failed");
        const ComputeConfigInfo computeConfig{sizeof(ComputeConfigInfo), 3, 3, true};
        check(sceVdecswAllocateComputeQueue(&computeConfig, &compute, &queue) == 0, "compute queue allocation failed");
        DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, 1, 100, 42, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 3, queue, 0x3f, 0, true, {}};
        check(sceVdecswQueryDecoderMemoryInfo(&config, &memory) == 0 && memory.maxFrameBufferSize != 0, "decoder memory query failed");
        check(sceVdecswCreateDecoder(&config, &memory, &decoder) == 0, "decoder creation failed");
    }

    ~Session() {
        sceVdecswDeleteDecoder(decoder);
        sceVdecswReleaseComputeQueue(queue);
    }
};

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
            check(!output.isErrorFrame && !output.isDiscardedFrame && output.pictureCount == 1 && output.frameWidth == Width && output.frameHeight == Height && output.frameBuffer == buffers[pictures % 2].data(), "unexpected picture geometry");
            AvcPictureInfo info{sizeof(AvcPictureInfo)};
            check(sceVdecswGetAvcPictureInfo(&output, &info, nullptr) == 0 && info.isValid, "picture info missing");
            const auto unit = DisplayOrderUnits[pictures];
            check(info.ptsData == 1000 + unit && info.dtsData == unit && info.attachedData == 0xa0 + unit && info.idrPictureFlag == (pictures == 0 ? 1 : 0) && info.profileIdc == 100, "pictures out of display order");
            check(info.picWidthInMbsMinus1 == Width / 16 - 1 && info.picHeightInMapUnitsMinus1 == (Height + 15) / 16 - 1 && info.frameMbsOnlyFlag == 1 && info.frameCroppingFlag == 1 && info.frameCropRightOffset == 0 && info.frameCropBottomOffset == ((Height + 15) / 16 * 16 - Height) / 2, "picture geometry in the picture info is wrong");
            check(hashNv12(static_cast<const std::uint8_t*>(output.frameBuffer), output.framePitch) == PictureHashes[pictures], "picture " + std::to_string(pictures) + " differs from the reference");
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

}

int main() {
    try {
        testDecode();
        testWithoutOutput();
        std::puts("Vdecsw tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
