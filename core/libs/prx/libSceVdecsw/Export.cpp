#include "prx/libSceVideodec2/include/Api.hpp"
#include "prx/libSceVdecsw/include/Api.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <variant>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

using namespace SceVdecsw;

constexpr int VdecswErrorOutputPending = static_cast<int>(0x81510115u);
constexpr int VdecswErrorInputQueueEmpty = static_cast<int>(0x81510116u);
constexpr int VdecswErrorDecodePending = static_cast<int>(0x81510117u);
constexpr std::uint32_t CodecVp9 = 2382845;
constexpr std::size_t MaxQueuedPictures = 4;
constexpr std::uint32_t CodecAvc = 1;
constexpr std::uint32_t CodecHevc = 974921;

static_assert(sizeof(SceVideodec2::InputData) == 0x30 && sizeof(InputResult) == 0x18 && sizeof(FrameBuffer) == 0x18);
static_assert(sizeof(SceVideodec2::Vp9PictureInfo) == 0x58 && offsetof(SceVideodec2::Vp9PictureInfo, ptsData) == 0x10 && offsetof(SceVideodec2::Vp9PictureInfo, profile) == 0x28 && offsetof(SceVideodec2::Vp9PictureInfo, bitDepth) == 0x38 && offsetof(SceVideodec2::Vp9PictureInfo, colorSpace) == 0x3c && offsetof(SceVideodec2::Vp9PictureInfo, renderHeight) == 0x50);
static_assert(sizeof(OutputInfo) == 0x38 && offsetof(OutputInfo, isLastFrame) == 9 && offsetof(OutputInfo, pictureCount) == 0xb && offsetof(OutputInfo, isDiscardedFrame) == 0x1c);
static_assert(sizeof(AvcPictureInfo) == 0x78 && offsetof(AvcPictureInfo, picWidthInMbsMinus1) == 0x2c && offsetof(AvcPictureInfo, frameCropBottomOffset) == 0x44);
static_assert(sizeof(HevcPictureInfo) == 0xb8 && offsetof(HevcPictureInfo, picWidthInLumaSamples) == 0x28 && offsetof(HevcPictureInfo, sarWidth) == 0x42 && offsetof(HevcPictureInfo, picStruct) == 0x50 && offsetof(HevcPictureInfo, conformanceWindowFlag) == 0x5c && offsetof(HevcPictureInfo, idrPictureFlag) == 0x93 && offsetof(HevcPictureInfo, generalProfileIdc) == 0x97 && offsetof(HevcPictureInfo, generalLevelIdc) == 0x9b && offsetof(HevcPictureInfo, frameCroppingFlag) == 0xa5 && offsetof(HevcPictureInfo, frameCropBottomOffset) == 0xb4);

struct Videodec2Crop {
    std::uint32_t left;
    std::uint32_t right;
    std::uint32_t top;
    std::uint32_t bottom;
};

struct Videodec2PictureHeader {
    std::uint64_t pts;
    std::uint64_t dts;
    std::uint64_t attached;
    bool keyFrame;
    bool hevcIdr;
    std::uint32_t profile;
    std::uint32_t level;
    Videodec2Crop crop;
    std::uint8_t bitDepthMinus8;
};

}

extern "C" {

void Videodec2PictureHeader_nid_no_patch(std::uint64_t handle, const void* frameBuffer, Videodec2PictureHeader* header);
}

namespace {

struct PendingInput {
    SceVideodec2::InputData input;
    std::vector<std::uint8_t> data;
};

struct ConsumedInput {
    const void* au;
    std::uint32_t frames;
};

using PictureInfo = std::variant<AvcPictureInfo, HevcPictureInfo>;

struct Picture {
    std::vector<std::uint8_t> pixels;
    SceVideodec2::OutputInfo output;
    PictureInfo info;
};

struct Decoder {
    std::mutex mutex;
    std::uint64_t frameBytes = 0;
    std::deque<PendingInput> inputs;
    std::deque<ConsumedInput> consumed;
    std::deque<FrameBuffer> outputs;
    std::deque<Picture> pictures;
    std::map<const void*, PictureInfo> delivered;
    bool finalized = false;
    bool drained = false;
    std::map<const void*, SceVideodec2::Vp9PictureInfo> deliveredVp9;
};

std::mutex decodersMutex;
std::map<std::uint64_t, std::shared_ptr<Decoder>> decoders;

std::shared_ptr<Decoder> findDecoder(std::uint64_t handle) {
    std::lock_guard lock(decodersMutex);
    const auto found = decoders.find(handle);
    if (found == decoders.end()) throw std::runtime_error("Vdecsw: invalid decoder handle");
    return found->second;
}

PictureInfo pictureInfo(std::uint64_t handle, const SceVideodec2::OutputInfo& output) {
    Videodec2PictureHeader header{};
    Videodec2PictureHeader_nid_no_patch(handle, output.frameBuffer, &header);
    const auto& crop = header.crop;
    const bool cropped = crop.left != 0 || crop.right != 0 || crop.top != 0 || crop.bottom != 0;
    if (output.codecType == CodecHevc) {
        HevcPictureInfo info{};
        info.thisSize = sizeof(HevcPictureInfo);
        info.isValid = true;
        info.ptsData = header.pts;
        info.dtsData = header.dts;
        info.attachedData = header.attached;
        info.picWidthInLumaSamples = output.frameWidth;
        info.picHeightInLumaSamples = output.frameHeight;
        info.bitDepthLumaMinus8 = header.bitDepthMinus8;
        info.bitDepthChromaMinus8 = header.bitDepthMinus8;
        info.conformanceWindowFlag = cropped;
        info.confWinLeftOffset = crop.left / 2u;
        info.confWinRightOffset = crop.right / 2u;
        info.confWinTopOffset = crop.top / 2u;
        info.confWinBottomOffset = crop.bottom / 2u;
        info.idrPictureFlag = header.hevcIdr;
        info.irapPictureFlag = header.keyFrame;
        info.generalProfileIdc = static_cast<std::uint8_t>(header.profile);
        info.generalLevelIdc = static_cast<std::uint8_t>(header.level);
        info.frameCroppingFlag = cropped;
        info.frameCropLeftOffset = info.confWinLeftOffset;
        info.frameCropRightOffset = info.confWinRightOffset;
        info.frameCropTopOffset = info.confWinTopOffset;
        info.frameCropBottomOffset = info.confWinBottomOffset;
        return info;
    }
    if (output.codecType != CodecAvc) throw std::runtime_error("Vdecsw: picture info is only implemented for H.264 and HEVC");
    const auto widthInMbs = (output.frameWidth + 15u) / 16u;
    const auto heightInMbs = (output.frameHeight + 15u) / 16u;
    AvcPictureInfo info{};
    info.thisSize = sizeof(AvcPictureInfo);
    info.isValid = true;
    info.ptsData = header.pts;
    info.dtsData = header.dts;
    info.attachedData = header.attached;
    info.idrPictureFlag = header.keyFrame;
    info.profileIdc = static_cast<std::uint8_t>(header.profile);
    info.levelIdc = static_cast<std::uint8_t>(header.level);
    info.picWidthInMbsMinus1 = widthInMbs - 1u;
    info.picHeightInMapUnitsMinus1 = heightInMbs - 1u;
    info.frameMbsOnlyFlag = 1;
    info.frameCroppingFlag = cropped;
    info.frameCropLeftOffset = crop.left / 2u;
    info.frameCropRightOffset = crop.right / 2u;
    info.frameCropTopOffset = crop.top / 2u;
    info.frameCropBottomOffset = crop.bottom / 2u;
    return info;
}

SceVideodec2::Vp9PictureInfo vp9PictureInfo(const SceVideodec2::OutputInfo& output) {
    SceVideodec2::Vp9PictureInfo info{sizeof(SceVideodec2::Vp9PictureInfo)};
    sceVideodec2GetVp9PictureInfo_nid_postfix(&output, &info);

    if (!info.isValid) throw std::runtime_error("Vdecsw: decoded picture has no VP9 picture info");
    return info;
}

template <typename TInfo>
int deliveredPictureInfo(const OutputInfo* output, TInfo* info) {
    if (output == nullptr || info == nullptr || info->thisSize < sizeof(std::uint64_t)) throw std::runtime_error("Vdecsw: invalid argument pointer");
    if (output->pictureCount > 1) throw std::runtime_error("Vdecsw: second field picture info is not implemented");
    if (output->pictureCount == 0) return 0;
    std::vector<std::shared_ptr<Decoder>> all;
    {
        std::lock_guard lock(decodersMutex);
        for (const auto& [handle, decoder] : decoders) all.push_back(decoder);
    }
    for (const auto& decoder : all) {
        std::lock_guard lock(decoder->mutex);
        const auto found = decoder->delivered.find(output->frameBuffer);
        if (found == decoder->delivered.end()) continue;
        const auto* source = std::get_if<TInfo>(&found->second);
        if (source == nullptr) throw std::runtime_error("Vdecsw: picture info requested for another codec");
        const auto bytes = std::min<std::uint64_t>(info->thisSize, sizeof(TInfo)) - sizeof(std::uint64_t);
        std::memcpy(reinterpret_cast<std::uint8_t*>(info) + sizeof(std::uint64_t), reinterpret_cast<const std::uint8_t*>(source) + sizeof(std::uint64_t), bytes);
        return 0;
    }
    throw std::runtime_error("Vdecsw: picture info requested for a frame buffer no decoder returned");
}

void advance(std::uint64_t handle, Decoder& decoder) {
    while (decoder.pictures.size() < MaxQueuedPictures) {
        std::vector<std::uint8_t> pixels(decoder.frameBytes);
        SceVideodec2::FrameBuffer frame{sizeof(SceVideodec2::FrameBuffer), pixels.data(), pixels.size(), false};
        SceVideodec2::OutputInfo output{sizeof(SceVideodec2::OutputInfo)};
        if (!decoder.inputs.empty()) {
            const auto* au = static_cast<const void*>(decoder.inputs.front().input.auData);
            auto input = decoder.inputs.front().input;
            input.auData = decoder.inputs.front().data.data();
            sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output);

            decoder.inputs.pop_front();
            decoder.consumed.push_back({au, output.isValid ? 1u : 0u});
        } else if (decoder.finalized && !decoder.drained) {
            sceVideodec2Flush_nid_postfix(handle, &frame, &output);

            decoder.drained = !output.isValid;
        } else {
            return;
        }
        if (output.isValid) {
            const auto info = output.codecType == CodecVp9 ? PictureInfo{} : pictureInfo(handle, output);
            decoder.pictures.push_back({std::move(pixels), output, info});
        }
    }
}

}

extern "C" {

int APS5_VABI sceVdecswQueryComputeMemoryInfo(SceVideodec2::ComputeMemoryInfo* info) {
    return sceVideodec2QueryComputeMemoryInfo_nid_postfix(info);
}

int APS5_VABI sceVdecswAllocateComputeQueue(const SceVideodec2::ComputeConfigInfo* config, const SceVideodec2::ComputeMemoryInfo* memory, std::uint64_t* queue) {
    return sceVideodec2AllocateComputeQueue_nid_postfix(config, memory, queue);
}

int APS5_VABI sceVdecswReleaseComputeQueue(std::uint64_t queue) {
    return sceVideodec2ReleaseComputeQueue_nid_postfix(queue);
}

int APS5_VABI sceVdecswQueryDecoderMemoryInfo(const void* config, SceVideodec2::DecoderMemoryInfo* memory) {
    return sceVideodec2QueryDecoderMemoryInfo_nid_postfix(static_cast<const SceVideodec2::DecoderConfigInfo*>(config), memory);
}

int APS5_VABI sceVdecswCreateDecoder(const void* config, const SceVideodec2::DecoderMemoryInfo* memory, std::uint64_t* handle) {
    if (handle == nullptr || memory == nullptr || memory->thisSize < sizeof(SceVideodec2::DecoderMemoryInfo) || memory->maxFrameBufferSize == 0) throw std::runtime_error("Vdecsw: invalid decoder creation arguments");
    const auto result = sceVideodec2CreateDecoder_nid_postfix(static_cast<const SceVideodec2::DecoderConfigInfo*>(config), memory, handle);
    if (result != 0) return result;
    auto decoder = std::make_shared<Decoder>();
    decoder->frameBytes = memory->maxFrameBufferSize;
    std::lock_guard lock(decodersMutex);
    decoders.emplace(*handle, std::move(decoder));
    return 0;
}

int APS5_VABI sceVdecswDeleteDecoder(std::uint64_t handle) {
    {
        std::lock_guard lock(decodersMutex);
        if (decoders.erase(handle) == 0) throw std::runtime_error("Vdecsw: invalid decoder handle");
    }
    return sceVideodec2DeleteDecoder_nid_postfix(handle);
}

int APS5_VABI sceVdecswSetDecodeInput(std::uint64_t handle, const SceVideodec2::InputData* input) {
    if (input == nullptr || input->thisSize < sizeof(SceVideodec2::InputData)) throw std::runtime_error("Vdecsw: invalid decode input");
    const auto decoder = findDecoder(handle);
    std::lock_guard lock(decoder->mutex);
    if (decoder->finalized) throw std::runtime_error("Vdecsw: decode input after the sequence was finalized");
    if (input->auData == nullptr || input->auSize == 0) throw std::runtime_error("Vdecsw: empty access unit");
    decoder->inputs.push_back({*input, std::vector<std::uint8_t>(input->auData, input->auData + input->auSize)});
    advance(handle, *decoder);
    return 0;
}

int APS5_VABI sceVdecswTrySyncDecodeInput(std::uint64_t handle, InputResult* result) {
    if (result == nullptr || result->thisSize < sizeof(InputResult)) throw std::runtime_error("Vdecsw: invalid decode input result");
    const auto decoder = findDecoder(handle);
    std::lock_guard lock(decoder->mutex);
    advance(handle, *decoder);
    result->decodedAu = nullptr;
    result->outputFrameCount = 0;
    result->reserved = 0;
    if (!decoder->consumed.empty()) {
        result->decodedAu = decoder->consumed.front().au;
        result->outputFrameCount = decoder->consumed.front().frames;
        decoder->consumed.pop_front();
        return 0;
    }
    return decoder->inputs.empty() ? VdecswErrorInputQueueEmpty : VdecswErrorDecodePending;
}

int APS5_VABI sceVdecswSyncDecodeInput(std::uint64_t handle, InputResult* result) {
    for (;;) {
        const auto status = sceVdecswTrySyncDecodeInput(handle, result);
        if (status != VdecswErrorDecodePending) return status;
        std::this_thread::yield();
    }
}

int APS5_VABI sceVdecswSetDecodeOutput(std::uint64_t handle, const FrameBuffer* frame) {
    if (frame == nullptr || frame->thisSize < sizeof(FrameBuffer) || frame->frameBuffer == nullptr) throw std::runtime_error("Vdecsw: invalid decode output");
    const auto decoder = findDecoder(handle);
    std::lock_guard lock(decoder->mutex);
    decoder->outputs.push_back(*frame);
    return 0;
}

int APS5_VABI sceVdecswTrySyncDecodeOutput(std::uint64_t handle, OutputInfo* output) {
    if (output == nullptr || output->thisSize < sizeof(OutputInfo)) throw std::runtime_error("Vdecsw: invalid decode output info");
    const auto decoder = findDecoder(handle);
    std::lock_guard lock(decoder->mutex);
    advance(handle, *decoder);
    if (decoder->outputs.empty()) throw std::runtime_error("Vdecsw: output sync without an output buffer");
    const bool ended = decoder->finalized && decoder->drained;
    if (decoder->pictures.size() < (ended ? 1u : 2u)) {
        if (!ended) return VdecswErrorOutputPending;
        std::memset(reinterpret_cast<std::uint8_t*>(output) + sizeof(output->thisSize), 0, sizeof(OutputInfo) - sizeof(output->thisSize));
        output->isLastFrame = true;
        decoder->outputs.pop_front();
        return 0;
    }
    const auto target = decoder->outputs.front();
    auto picture = std::move(decoder->pictures.front());
    decoder->pictures.pop_front();
    if (target.frameBufferSize < picture.output.frameBufferSize) throw std::runtime_error("Vdecsw: output buffer is smaller than the decoded picture");
    std::memcpy(target.frameBuffer, picture.pixels.data(), picture.output.frameBufferSize);
    decoder->outputs.pop_front();
    output->isValid = true;
    output->isLastFrame = ended && decoder->pictures.empty();
    output->isErrorFrame = picture.output.isErrorFrame;
    output->pictureCount = picture.output.pictureCount;
    output->codecType = picture.output.codecType;
    output->frameWidth = picture.output.frameWidth;
    output->framePitch = picture.output.framePitch;
    output->frameHeight = picture.output.frameHeight;
    output->isDiscardedFrame = false;
    output->frameBuffer = target.frameBuffer;
    output->frameBufferSize = picture.output.frameBufferSize;
    output->frameFormat = picture.output.frameFormat;
    output->framePitchInBytes = picture.output.framePitchInBytes;
    if (picture.output.codecType == CodecVp9) decoder->deliveredVp9[target.frameBuffer] = vp9PictureInfo(picture.output);
    else decoder->delivered[target.frameBuffer] = picture.info;
    return 0;
}

int APS5_VABI sceVdecswSyncDecodeOutput(std::uint64_t handle, OutputInfo* output) {
    for (;;) {
        const auto status = sceVdecswTrySyncDecodeOutput(handle, output);
        if (status != VdecswErrorOutputPending) return status;
        std::this_thread::yield();
    }
}

int APS5_VABI sceVdecswGetPictureInfo(const OutputInfo* output, void* first, void* second) {
    static_cast<void>(second);
    if (output == nullptr || (output->thisSize | 8u) != sizeof(OutputInfo)) throw std::runtime_error("Vdecsw: invalid output info");
    if (output->pictureCount == 0) return 0;
    if (output->pictureCount > 1) NotImplemented_nid_no_patch(__func__);
    if (first == nullptr) return 0;
    std::uint64_t size;
    std::memcpy(&size, first, sizeof(size));
    if (size < sizeof(size)) throw std::runtime_error("Vdecsw: picture info is smaller than its size field");
    const auto copy = [first, size](const auto& source) {
        const auto bytes = std::min<std::uint64_t>(size, sizeof(source)) - sizeof(size);
        std::memcpy(static_cast<std::uint8_t*>(first) + sizeof(size), reinterpret_cast<const std::uint8_t*>(&source) + sizeof(size), bytes);
    };
    std::vector<std::shared_ptr<Decoder>> all;
    {
        std::lock_guard lock(decodersMutex);
        for (const auto& [handle, decoder] : decoders) all.push_back(decoder);
    }
    for (const auto& decoder : all) {
        std::lock_guard lock(decoder->mutex);
        if (const auto found = decoder->delivered.find(output->frameBuffer); found != decoder->delivered.end()) {
            std::visit(copy, found->second);
            return 0;
        }
        if (const auto found = decoder->deliveredVp9.find(output->frameBuffer); found != decoder->deliveredVp9.end()) {
            copy(found->second);
            return 0;
        }
    }
    throw std::runtime_error("Vdecsw: picture info requested for a frame buffer no decoder returned");
}

int APS5_VABI sceVdecswGetAvcPictureInfo(const OutputInfo* output, AvcPictureInfo* first, AvcPictureInfo* second) {
    static_cast<void>(second);
    return deliveredPictureInfo(output, first);
}

int APS5_VABI sceVdecswGetHevcPictureInfo(const OutputInfo* output, HevcPictureInfo* picture) {
    return deliveredPictureInfo(output, picture);
}

int APS5_VABI sceVdecswGetVp9PictureInfo(const OutputInfo* output, SceVideodec2::Vp9PictureInfo* picture) {
    if (output == nullptr || picture == nullptr || picture->thisSize < sizeof(std::uint64_t)) throw std::runtime_error("Vdecsw: invalid argument pointer");
    if (output->pictureCount > 1) throw std::runtime_error("Vdecsw: more than one VP9 picture info is not implemented");
    if (output->pictureCount == 0) return 0;
    std::vector<std::shared_ptr<Decoder>> all;
    {
        std::lock_guard lock(decodersMutex);
        for (const auto& [handle, decoder] : decoders) all.push_back(decoder);
    }
    for (const auto& decoder : all) {
        std::lock_guard lock(decoder->mutex);
        const auto found = decoder->deliveredVp9.find(output->frameBuffer);
        if (found == decoder->deliveredVp9.end()) continue;
        const auto bytes = std::min<std::uint64_t>(picture->thisSize, sizeof(SceVideodec2::Vp9PictureInfo)) - sizeof(std::uint64_t);
        std::memcpy(reinterpret_cast<std::uint8_t*>(picture) + sizeof(std::uint64_t), reinterpret_cast<const std::uint8_t*>(&found->second) + sizeof(std::uint64_t), bytes);
        return 0;
    }
    throw std::runtime_error("Vdecsw: VP9 picture info requested for a frame buffer no decoder returned");
}

int APS5_VABI sceVdecswFinalizeDecodeSequence(std::uint64_t handle) {
    const auto decoder = findDecoder(handle);
    std::lock_guard lock(decoder->mutex);
    decoder->finalized = true;
    advance(handle, *decoder);
    return 0;
}

int APS5_VABI sceVdecswResetDecoder(std::uint64_t handle) {
    const auto decoder = findDecoder(handle);
    std::lock_guard lock(decoder->mutex);
    decoder->inputs.clear();
    decoder->consumed.clear();
    decoder->outputs.clear();
    decoder->pictures.clear();
    decoder->delivered.clear();
    decoder->deliveredVp9.clear();
    decoder->finalized = false;
    decoder->drained = false;
    return sceVideodec2Reset_nid_postfix(handle);
}

}
