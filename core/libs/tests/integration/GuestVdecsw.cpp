#include "prx/libc/include/General.hpp"
#include "H264Fixture.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace {

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

} // namespace

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
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

template<typename TValue>
TValue Sized() {
    TValue value{};
    value.thisSize = sizeof(TValue);
    return value;
}

constexpr int OutputPending = static_cast<int>(0x81510115u);
constexpr int InputQueueEmpty = static_cast<int>(0x81510116u);

class ComputeQueue {
public:
    ComputeQueue() {
        ComputeMemoryInfo compute = Sized<ComputeMemoryInfo>();
        RequireEqual(sceVdecswQueryComputeMemoryInfo(&compute), 0, "compute memory query status");
        Require(compute.cpuGpuMemorySize != 0, "compute memory query reports a non-zero size");
        const ComputeConfigInfo computeConfig{sizeof(ComputeConfigInfo), 3, 3, true};
        RequireEqual(sceVdecswAllocateComputeQueue(&computeConfig, &compute, &handle), 0, "compute queue allocation status");
    }

    ~ComputeQueue() {
        sceVdecswReleaseComputeQueue(handle);
    }

    ComputeQueue(const ComputeQueue&) = delete;
    ComputeQueue& operator=(const ComputeQueue&) = delete;

    std::uint64_t Handle() const noexcept {
        return handle;
    }

private:
    std::uint64_t handle = 0;
};

class Session {
public:
    Session() {
        DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, 1, 100, 42, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 3, queue.Handle(), 0x3f, 0, true, {}};
        RequireEqual(sceVdecswQueryDecoderMemoryInfo(&config, &memory), 0, "decoder memory query status");
        Require(memory.maxFrameBufferSize != 0, "decoder memory query reports a non-zero frame buffer size");
        RequireEqual(sceVdecswCreateDecoder(&config, &memory, &decoder), 0, "decoder creation status");
        created = true;
    }

    ~Session() {
        if (!created) return;
        try {
            sceVdecswDeleteDecoder(decoder);
        } catch (const std::exception&) {
        }
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    std::uint64_t Decoder() const noexcept {
        return decoder;
    }

    const DecoderMemoryInfo& Memory() const noexcept {
        return memory;
    }

private:
    ComputeQueue queue;
    DecoderMemoryInfo memory = Sized<DecoderMemoryInfo>();
    std::uint64_t decoder = 0;
    bool created = false;
};

struct DecodedPicture {
    OutputInfo output;
    bool expectedBuffer;
    int infoStatus;
    AvcPictureInfo info;
    std::uint64_t hash;
};

struct StreamRun {
    std::string failure;
    int idleStatus = 0;
    bool idleDecodedAuNull = false;
    std::vector<bool> consumed;
    std::uint32_t produced = 0;
    std::size_t picturesBeforeFinalize = 0;
    std::vector<DecodedPicture> pictures;
    OutputInfo end = Sized<OutputInfo>();
    int inputStatusAfterFinalize = 0;
};

void DecodeStream(const Session& session, StreamRun& run) {
    const auto units = AccessUnits(false);
    RequireEqual(units.size(), PictureHashes.size(), "access unit count");
    std::array<std::vector<std::uint8_t>, 2> buffers{std::vector<std::uint8_t>(session.Memory().maxFrameBufferSize), std::vector<std::uint8_t>(session.Memory().maxFrameBufferSize)};
    InputResult idle = Sized<InputResult>();
    run.idleStatus = sceVdecswTrySyncDecodeInput(session.Decoder(), &idle);
    run.idleDecodedAuNull = idle.decodedAu == nullptr;
    bool outputSet = false;
    bool ended = false;
    const auto drain = [&] {
        for (;;) {
            const auto slot = run.pictures.size() % 2;
            if (!outputSet) {
                const FrameBuffer frame{sizeof(FrameBuffer), buffers[slot].data(), buffers[slot].size()};
                RequireEqual(sceVdecswSetDecodeOutput(session.Decoder(), &frame), 0, "output buffer status");
                outputSet = true;
            }
            OutputInfo output = Sized<OutputInfo>();
            const auto status = sceVdecswTrySyncDecodeOutput(session.Decoder(), &output);
            if (status == OutputPending) return;
            RequireEqual(status, 0, "output sync status");
            outputSet = false;
            if (!output.isValid) {
                run.end = output;
                ended = true;
                return;
            }
            Require(run.pictures.size() < PictureHashes.size(), "more pictures than access units");
            DecodedPicture picture{output, output.frameBuffer == buffers[slot].data(), 0, Sized<AvcPictureInfo>(), 0};
            picture.infoStatus = sceVdecswGetAvcPictureInfo(&output, &picture.info, nullptr);
            picture.hash = HashNv12(static_cast<const std::uint8_t*>(output.frameBuffer), output.framePitch);
            run.pictures.push_back(picture);
        }
    };
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        RequireEqual(sceVdecswSetDecodeInput(session.Decoder(), &input), 0, "decode input status for unit " + std::to_string(unit));
        InputResult consumed = Sized<InputResult>();
        const auto status = sceVdecswTrySyncDecodeInput(session.Decoder(), &consumed);
        run.consumed.push_back(status == 0 && consumed.decodedAu == units[unit].data());
        run.produced += consumed.outputFrameCount;
        drain();
    }
    run.picturesBeforeFinalize = run.pictures.size();
    RequireEqual(sceVdecswFinalizeDecodeSequence(session.Decoder()), 0, "finalize status");
    while (!ended) drain();
    InputResult result = Sized<InputResult>();
    run.inputStatusAfterFinalize = sceVdecswTrySyncDecodeInput(session.Decoder(), &result);
}

StreamRun BuildStreamRun() {
    StreamRun run;
    try {
        const Session session;
        DecodeStream(session, run);
    } catch (const std::exception& error) {
        run.failure = error.what();
    }
    return run;
}

const StreamRun& DecodedStream() {
    static const StreamRun run = BuildStreamRun();
    Require(run.failure.empty(), "stream decode run failed: " + run.failure);
    return run;
}

struct SinglePictureRun {
    std::string failure;
    int inputStatus = -1;
    bool inputDecoded = false;
    int pendingStatus = -1;
    int finalizeStatus = -1;
    int finalStatus = -1;
    OutputInfo finalOutput = Sized<OutputInfo>();
    int resetStatus = -1;
    int inputStatusAfterReset = -1;
};

SinglePictureRun BuildSinglePictureRun() {
    SinglePictureRun run;
    try {
        const Session session;
        const auto units = AccessUnits(false);
        const InputData input{sizeof(InputData), units[0].data(), units[0].size(), 1000, 0, 0xa0};
        RequireEqual(sceVdecswSetDecodeInput(session.Decoder(), &input), 0, "decode input status");
        InputResult result = Sized<InputResult>();
        run.inputStatus = sceVdecswTrySyncDecodeInput(session.Decoder(), &result);
        run.inputDecoded = result.decodedAu == units[0].data();
        std::vector<std::uint8_t> buffer(session.Memory().maxFrameBufferSize);
        const FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size()};
        RequireEqual(sceVdecswSetDecodeOutput(session.Decoder(), &frame), 0, "output buffer status");
        OutputInfo pending = Sized<OutputInfo>();
        run.pendingStatus = sceVdecswTrySyncDecodeOutput(session.Decoder(), &pending);
        run.finalizeStatus = sceVdecswFinalizeDecodeSequence(session.Decoder());
        run.finalStatus = sceVdecswTrySyncDecodeOutput(session.Decoder(), &run.finalOutput);
        run.resetStatus = sceVdecswResetDecoder(session.Decoder());
        InputResult afterReset = Sized<InputResult>();
        run.inputStatusAfterReset = sceVdecswTrySyncDecodeInput(session.Decoder(), &afterReset);
    } catch (const std::exception& error) {
        run.failure = error.what();
    }
    return run;
}

const SinglePictureRun& DecodedSinglePicture() {
    static const SinglePictureRun run = BuildSinglePictureRun();
    Require(run.failure.empty(), "single-picture run failed: " + run.failure);
    return run;
}

std::string PictureLabel(std::size_t picture) {
    return "picture " + std::to_string(picture);
}

const Case idleDecoder{"Vdecsw_IdleDecoder_ReportsEmptyInputQueue", [] {
    const auto& run = DecodedStream();
    RequireEqual(run.idleStatus, InputQueueEmpty, "input sync status of an idle decoder");
    Require(run.idleDecodedAuNull, "an idle decoder reports no decoded access unit");
}};

const Case consumesUnits{"Vdecsw_DecodeStream_ConsumesEveryAccessUnit", [] {
    const auto& run = DecodedStream();
    for (std::size_t unit = 0; unit < run.consumed.size(); ++unit) {
        Require(run.consumed[unit], "access unit " + std::to_string(unit) + " is reported consumed by input sync");
    }
}};

const Case holdsLastPicture{"Vdecsw_DecodeStream_HoldsLastPictureUntilFinalize", [] {
    const auto& run = DecodedStream();
    Require(run.picturesBeforeFinalize < PictureHashes.size(), "the last picture was not returned before the sequence was finalized");
}};

const Case everyPicture{"Vdecsw_FinalizedStream_ReturnsEveryPicture", [] {
    const auto& run = DecodedStream();
    RequireEqual(run.pictures.size(), PictureHashes.size(), "pictures after finalizing the sequence");
    Require(run.produced <= PictureHashes.size(), "pictures counted by input sync do not exceed decoded pictures");
}};

const Case endOutput{"Vdecsw_FinalizedStream_EndsWithEmptyLastOutput", [] {
    const auto& run = DecodedStream();
    Require(run.end.isLastFrame, "the output without a picture is marked last");
    RequireEqual(run.end.pictureCount, 0, "picture count of the end-of-sequence output");
}};

const Case lastFlag{"Vdecsw_FinalizedStream_MarksOnlyLastPictureLast", [] {
    const auto& run = DecodedStream();
    for (std::size_t index = 0; index < run.pictures.size(); ++index) {
        RequireEqual(run.pictures[index].output.isLastFrame, index + 1 == PictureHashes.size(), PictureLabel(index) + " last flag");
    }
}};

const Case outputGeometry{"Vdecsw_DecodeStream_ReportsPictureGeometry", [] {
    const auto& run = DecodedStream();
    for (std::size_t index = 0; index < run.pictures.size(); ++index) {
        const auto& picture = run.pictures[index];
        const auto label = PictureLabel(index);
        Require(!picture.output.isErrorFrame, label + " is not an error frame");
        Require(!picture.output.isDiscardedFrame, label + " is not discarded");
        RequireEqual(picture.output.pictureCount, 1, label + " picture count");
        RequireEqual(picture.output.frameWidth, Width, label + " width");
        RequireEqual(picture.output.frameHeight, Height, label + " height");
        Require(picture.expectedBuffer, label + " is written to the output buffer set for it");
    }
}};

const Case displayOrder{"Vdecsw_PictureInfo_FollowsDisplayOrder", [] {
    const auto& run = DecodedStream();
    for (std::size_t index = 0; index < run.pictures.size(); ++index) {
        const auto& picture = run.pictures[index];
        const auto label = PictureLabel(index);
        const auto unit = DisplayOrderUnits[index];
        RequireEqual(picture.infoStatus, 0, label + " picture info status");
        Require(picture.info.isValid, label + " picture info is valid");
        RequireEqual(picture.info.ptsData, 1000 + unit, label + " pts");
        RequireEqual(picture.info.dtsData, unit, label + " dts");
        RequireEqual(picture.info.attachedData, 0xa0 + unit, label + " attached data");
        RequireEqual(picture.info.idrPictureFlag, index == 0 ? 1 : 0, label + " idr flag");
        RequireEqual(picture.info.profileIdc, 100, label + " profile");
    }
}};

const Case infoGeometry{"Vdecsw_PictureInfo_ReportsCroppedGeometry", [] {
    const auto& run = DecodedStream();
    for (std::size_t index = 0; index < run.pictures.size(); ++index) {
        const auto& picture = run.pictures[index];
        const auto label = PictureLabel(index);
        RequireEqual(picture.infoStatus, 0, label + " picture info status");
        RequireEqual(picture.info.picWidthInMbsMinus1, Width / 16 - 1, label + " width in macroblocks");
        RequireEqual(picture.info.picHeightInMapUnitsMinus1, (Height + 15) / 16 - 1, label + " height in map units");
        RequireEqual(picture.info.frameMbsOnlyFlag, 1, label + " frame macroblocks only flag");
        RequireEqual(picture.info.frameCroppingFlag, 1, label + " cropping flag");
        RequireEqual(picture.info.frameCropRightOffset, 0u, label + " right crop");
        RequireEqual(picture.info.frameCropBottomOffset, ((Height + 15) / 16 * 16 - Height) / 2, label + " bottom crop");
    }
}};

const Case referenceHashes{"Vdecsw_DecodeStream_PicturesMatchReference", [] {
    const auto& run = DecodedStream();
    for (std::size_t index = 0; index < run.pictures.size(); ++index) {
        RequireEqual(run.pictures[index].hash, PictureHashes[index], PictureLabel(index) + " hash");
    }
}};

const Case finalizedQueue{"Vdecsw_FinalizedDecoder_ReportsEmptyInputQueue", [] {
    const auto& run = DecodedStream();
    RequireEqual(run.inputStatusAfterFinalize, InputQueueEmpty, "input sync status of a finalized decoder");
}};

const Case inputWithoutOutput{"Vdecsw_InputWithoutOutputBuffer_IsDecoded", [] {
    const auto& run = DecodedSinglePicture();
    RequireEqual(run.inputStatus, 0, "input sync status before an output buffer is set");
    Require(run.inputDecoded, "the input is reported decoded before an output buffer is set");
}};

const Case openSequence{"Vdecsw_OpenSequence_HoldsOnlyPicture", [] {
    const auto& run = DecodedSinglePicture();
    RequireEqual(run.pendingStatus, OutputPending, "output sync status of an open single-picture sequence");
}};

const Case finalizedSequence{"Vdecsw_FinalizedSequence_MarksOnlyPictureLast", [] {
    const auto& run = DecodedSinglePicture();
    RequireEqual(run.finalizeStatus, 0, "finalize status");
    RequireEqual(run.finalStatus, 0, "output sync status after finalizing");
    Require(run.finalOutput.isValid, "the only picture is returned after finalizing");
    Require(run.finalOutput.isLastFrame, "the only picture of a finalized sequence is marked last");
}};

const Case resetDecoder{"Vdecsw_Reset_DropsQueuedInputs", [] {
    const auto& run = DecodedSinglePicture();
    RequireEqual(run.resetStatus, 0, "reset status");
    RequireEqual(run.inputStatusAfterReset, InputQueueEmpty, "input sync status of a reset decoder");
}};

} // namespace
