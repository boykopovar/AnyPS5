#include "prx/libSceVdecCore/include/Api.hpp"
#include "prx/libSceVdecCore/include/Decoder.hpp"
#include "H264Fixture.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80C00001);
constexpr int InvalidCodecType = static_cast<int>(0x80C00003);
constexpr int NoPicture = static_cast<int>(0x80C00010);
constexpr int DecodeRunning = static_cast<int>(0x80C00018);
constexpr std::int64_t PtsInvalid = -1;
constexpr std::uint64_t SlotSize = 0x10000;
constexpr std::uint32_t OutputPitch = 64;
constexpr std::array<std::uint64_t, 6> VisiblePictureHashes{
    0x5871fdc85dfa274eull, 0x26ce4afb974cffc4ull, 0xd2e739633733d5baull,
    0x54feb3d290e028fdull, 0xfa1342a4ba1819c8ull, 0x870125dd7f079a4cull,
};
constexpr std::uint32_t CodecAvc = 0;
constexpr std::uint32_t CodecHevc = 4;
constexpr std::uint32_t DecoderVdec2 = 2;
constexpr std::uint32_t DecoderVdec2Special2 = 8;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "VdecCore: %s\n", message);
        std::abort();
    }
}

void* Resolve(const char* name) {
#ifdef _WIN32
    const HMODULE module = GetModuleHandleA("libSceVdecCore.prx");
    Require(module != nullptr, "library module");
    return reinterpret_cast<void*>(GetProcAddress(module, name));
#else
    return dlsym(RTLD_DEFAULT, name);
#endif
}

VdecCoreConfigInfo Config(std::int32_t width, std::int32_t height, std::int32_t horizontalAlign, std::int32_t dpb) {
    VdecCoreConfigInfo config{};
    config.decoderType = DecoderVdec2;
    config.codecType = CodecAvc;
    config.maxFrameWidth = width;
    config.maxFrameHeight = height;
    config.horizontalAlign = horizontalAlign;
    config.maxDpbFrameCount = dpb;
    return config;
}

void CheckExports() {
    for (const char* name : {"sceVdecCoreQueryInstanceSize", "sceVdecCoreQueryFrameBufferInfo", "sceVdecCoreQueryComputeResourceInfo",
                             "sceVdecCoreCreateDecoder", "sceVdecCoreDeleteDecoder", "sceVdecCoreMapMemoryBlock"}) {
        Require(Resolve(name) != nullptr, name);
    }
}

void CheckInstanceSize() {
    VdecCoreConfigInfo config = Config(0, 0, 0, 0);
    std::uint64_t instance = 1, compute = 1, reserved = 1;
    Require(sceVdecCoreQueryInstanceSize(nullptr, &instance, &compute, &reserved) == InvalidArgument, "instance size without a config");
    config.codecType = 5;
    Require(sceVdecCoreQueryInstanceSize(&config, &instance, &compute, &reserved) == InvalidCodecType, "instance size for codec type 5");
    Require(instance == 1 && compute == 1 && reserved == 1, "a refused query leaves its outputs alone");
    config.codecType = CodecHevc;
    Require(sceVdecCoreQueryInstanceSize(&config, &instance, &compute, &reserved) == 0, "instance size");
    Require(instance == 0x40000 && compute == 0x10000 && reserved == 0, "instance, compute and reserved sizes");
    Require(sceVdecCoreQueryInstanceSize(&config, nullptr, nullptr, nullptr) == 0, "instance size with no outputs");
}

void RequireFrameBuffer(const VdecCoreConfigInfo& config, std::uint32_t size, std::uint32_t minCount, std::uint32_t align, std::uint32_t width, std::uint32_t height, const char* message) {
    VdecCoreFrameBufferInfo info{};
    Require(sceVdecCoreQueryFrameBufferInfo(&config, &info) == 0, message);
    Require(info.frameBufferSize == size, message);
    Require(info.frameBufferAlignment == 256, message);
    Require(info.minFrameBufferCount == minCount, message);
    Require(info.horizontalAlignment == align, message);
    Require(info.frameWidth == width && info.frameHeight == height, message);
}

void CheckFrameBufferInfo() {
    VdecCoreFrameBufferInfo info{};
    VdecCoreConfigInfo config = Config(0, 0, 0, 0);
    Require(sceVdecCoreQueryFrameBufferInfo(nullptr, &info) == InvalidArgument, "frame buffer info without a config");
    Require(sceVdecCoreQueryFrameBufferInfo(&config, nullptr) == InvalidArgument, "frame buffer info without an output");
    config.codecType = 5;
    config.maxDpbFrameCount = 17;
    Require(sceVdecCoreQueryFrameBufferInfo(&config, &info) == InvalidCodecType, "the codec type is checked before the DPB count");
    config.codecType = CodecAvc;
    Require(sceVdecCoreQueryFrameBufferInfo(&config, &info) == InvalidArgument, "DPB count 17");
    config.maxDpbFrameCount = -2;
    Require(sceVdecCoreQueryFrameBufferInfo(&config, &info) == InvalidArgument, "DPB count -2");

    RequireFrameBuffer(Config(0, 0, 0, 0), 0x340000, 17, 256, 1920, 1080, "1920x1080 defaults");
    RequireFrameBuffer(Config(64, 40, 256, 0), 0x10000, 17, 256, 64, 40, "64x40, DPB 0");
    RequireFrameBuffer(Config(64, 40, 256, -1), 0x10000, 17, 256, 64, 40, "64x40, DPB -1");
    RequireFrameBuffer(Config(64, 40, 256, 16), 0x10000, 17, 256, 64, 40, "64x40, DPB 16");
    RequireFrameBuffer(Config(64, 40, 256, 4), 0x10000, 5, 256, 64, 40, "64x40, DPB 4");
    RequireFrameBuffer(Config(3840, 2160, 0, 0), 0xbe0000, 17, 256, 3840, 2160, "3840x2160");
    RequireFrameBuffer(Config(1280, 720, 100, 0), 0x160000, 17, 100, 1280, 720, "1280x720 with a horizontal alignment of 100");
}

void CheckComputeResourceInfo() {
    Require(sceVdecCoreQueryComputeResourceInfo(nullptr) == InvalidArgument, "compute resource info without an output");
    VdecCoreComputeResourceInfo info{7, 7, 7, 7};
    Require(sceVdecCoreQueryComputeResourceInfo(&info) == 0, "compute resource info");
    Require(info.memoryType == 0 && info.size == 0x10000 && info.alignment == 0x10000 && info.reserved == 0, "compute resource sizes");
}

void CheckCreateAndDelete() {
    VdecCoreConfigInfo config = Config(0, 0, 0, 0);
    VdecCoreMemoryInfo memory{};
    void* decoder = &memory;
    Require(sceVdecCoreCreateDecoder(nullptr, &memory, nullptr, &decoder) == InvalidArgument, "create without a config");
    Require(sceVdecCoreCreateDecoder(&config, &memory, nullptr, nullptr) == InvalidArgument, "create without an output");
    config.decoderType = DecoderVdec2Special2 + 1;
    Require(sceVdecCoreCreateDecoder(&config, &memory, nullptr, &decoder) == InvalidArgument, "create with decoder type 9");
    config.decoderType = DecoderVdec2;
    config.codecType = 5;
    Require(sceVdecCoreCreateDecoder(&config, &memory, nullptr, &decoder) == InvalidCodecType, "create with codec type 5");
    Require(decoder == &memory, "a refused create leaves the output alone");

    config.codecType = CodecAvc;
    void* avc = nullptr;
    Require(sceVdecCoreCreateDecoder(&config, &memory, nullptr, &avc) == 0 && avc != nullptr, "create an AVC decoder");
    config.codecType = CodecHevc;
    config.decoderType = DecoderVdec2Special2;
    void* hevc = nullptr;
    Require(sceVdecCoreCreateDecoder(&config, nullptr, nullptr, &hevc) == 0 && hevc != nullptr && hevc != avc, "create an HEVC decoder of type 8");

    Require(sceVdecCoreDeleteDecoder(nullptr) == InvalidArgument, "delete without a decoder");
    Require(sceVdecCoreDeleteDecoder(avc) == 0, "delete the AVC decoder");
    Require(sceVdecCoreDeleteDecoder(hevc) == 0, "delete the HEVC decoder");
}

std::vector<void*> Slots(void* decoder) {
    std::array<void*, 16> slots{};
    const std::uint32_t count = VdecCoreFrameBufferSlots_nid_no_patch(decoder, slots.data(), static_cast<std::uint32_t>(slots.size()));
    Require(count <= slots.size(), "slot count");
    return std::vector<void*>(slots.begin(), slots.begin() + count);
}

void CheckMapMemoryBlock() {
    Require(sceVdecCoreMapMemoryBlock(nullptr, nullptr, 0) == InvalidArgument, "map without a decoder");
    const VdecCoreConfigInfo config = Config(64, 40, 256, 0);
    VdecCoreFrameBufferInfo info{};
    Require(sceVdecCoreQueryFrameBufferInfo(&config, &info) == 0 && info.frameBufferSize == 0x10000, "64x40 frame buffer size");
    void* decoder = nullptr;
    Require(sceVdecCoreCreateDecoder(&config, nullptr, nullptr, &decoder) == 0, "create the decoder to map into");
    Require(Slots(decoder).empty(), "a new decoder has no frame buffer slots");

    std::vector<std::uint8_t> memory(0x50000);
    std::uint8_t* base = memory.data();
    const std::array<VdecCoreMemoryBlock, 3> blocks{{
        {base, 0x28000, 256, 0},
        {base + 0x30000, 0x8000, 256, 0},
        {nullptr, 0x20000, 256, 0},
    }};
    Require(sceVdecCoreMapMemoryBlock(decoder, blocks.data(), static_cast<std::uint32_t>(blocks.size())) == 0, "map three blocks");
    const std::vector<void*> cut = Slots(decoder);
    Require(cut.size() == 2, "a 2.5-slot block gives two slots; a short block and a null block give none");
    Require(cut[0] == base && cut[1] == base + info.frameBufferSize, "slots start at the block and are one frame buffer apart");

    Require(sceVdecCoreMapMemoryBlock(decoder, nullptr, 3) == 0, "map with no blocks");
    Require(sceVdecCoreMapMemoryBlock(decoder, blocks.data(), 0) == 0, "map zero blocks");
    Require(Slots(decoder).size() == 2, "mapping nothing adds no slots");

    const VdecCoreMemoryBlock exact{base + 0x40000, 0x10000, 256, 0};
    Require(sceVdecCoreMapMemoryBlock(decoder, &exact, 1) == 0, "map a block of exactly one frame buffer");
    const std::vector<void*> grown = Slots(decoder);
    Require(grown.size() == 3 && grown[2] == base + 0x40000, "an exact block adds one slot after the earlier ones");
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete the mapped decoder");
}

void* CreateAvc(std::int32_t width) {
    const VdecCoreConfigInfo config = Config(width, static_cast<std::int32_t>(H264Fixture::Height), 256, 0);
    void* decoder = nullptr;
    Require(sceVdecCoreCreateDecoder(&config, nullptr, nullptr, &decoder) == 0 && decoder != nullptr, "create an AVC decoder");
    return decoder;
}

const std::vector<std::vector<std::uint8_t>>& Units() {
    static const std::vector<std::vector<std::uint8_t>> units = H264Fixture::accessUnits(H264Fixture::Stream.data(), H264Fixture::Stream.size(), false);
    Require(units.size() == H264Fixture::PictureHashes.size(), "the fixture's access units");
    return units;
}

void Feed(void* decoder, std::size_t unit, std::int64_t pts, std::int64_t dts, std::uint64_t userData) {
    const auto& data = Units()[unit];
    const VdecCoreInputData input{data.data(), data.size(), pts, dts, userData, 0, 0};
    std::array<std::uint32_t, 2> status{};
    Require(sceVdecCoreSetDecodeInput(decoder, &input, nullptr, status.data()) == 0, "set an access unit as decode input");
    Require(status[0] == 1 && status[1] == 1, "the input status of an accepted access unit");
}

void FeedAll(void* decoder, std::int64_t ptsBase, std::int64_t dtsBase, std::uint64_t userBase) {
    for (std::size_t unit = 0; unit < Units().size(); ++unit) {
        const auto offset = static_cast<std::int64_t>(unit);
        Feed(decoder, unit, ptsBase == PtsInvalid ? PtsInvalid : ptsBase + offset, dtsBase == PtsInvalid ? PtsInvalid : dtsBase + offset, userBase + unit);
    }
}

std::uint32_t Flush(void* decoder) {
    std::uint32_t count = 0xffffffffu;
    Require(sceVdecCoreFlushDecodeOutput(decoder, &count, nullptr) == 0, "flush the decode output");
    return count;
}

VdecCoreOutputInfo Output(void* decoder, VdecCoreFrameBuffer* frameBuffer) {
    VdecCoreOutputInfo output{};
    Require(sceVdecCoreGetDecodeOutput(decoder, frameBuffer, 0, &output, nullptr) == 0, "get a decoded picture");
    return output;
}

std::uint64_t VisibleHash(const std::uint8_t* buffer, std::uint32_t pitch, std::uint32_t frameHeight) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    const auto mix = [&](const std::uint8_t* row) {
        for (std::uint32_t column = 0; column < H264Fixture::Width; ++column) hash = (hash ^ row[column]) * 0x100000001b3ull;
    };
    for (std::uint32_t row = 0; row < H264Fixture::Height; ++row) mix(buffer + static_cast<std::size_t>(row) * pitch);
    const auto* chroma = buffer + static_cast<std::size_t>(pitch) * frameHeight;
    for (std::uint32_t row = 0; row < H264Fixture::Height / 2; ++row) mix(chroma + static_cast<std::size_t>(row) * pitch);
    return hash;
}

bool ExtendsLastRows(const std::uint8_t* buffer, std::uint32_t pitch, std::uint32_t frameHeight) {
    const auto* chroma = buffer + static_cast<std::size_t>(pitch) * frameHeight;
    const auto* lastLuma = buffer + static_cast<std::size_t>(pitch) * (H264Fixture::Height - 1);
    const auto* lastChroma = chroma + static_cast<std::size_t>(pitch) * (H264Fixture::Height / 2 - 1);
    for (std::uint32_t row = H264Fixture::Height; row < frameHeight; ++row) {
        if (std::memcmp(buffer + static_cast<std::size_t>(row) * pitch, lastLuma, H264Fixture::Width) != 0) return false;
    }
    for (std::uint32_t row = H264Fixture::Height / 2; row < frameHeight / 2; ++row) {
        if (std::memcmp(chroma + static_cast<std::size_t>(row) * pitch, lastChroma, H264Fixture::Width) != 0) return false;
    }
    return true;
}

const VdecCoreHwPictureInfo& HwInfo(const VdecCoreOutputInfo& output, std::uint64_t allocatedSize) {
    return *reinterpret_cast<const VdecCoreHwPictureInfo*>(static_cast<const std::uint8_t*>(output.frameBuffer) + allocatedSize - sizeof(VdecCoreHwPictureInfo));
}

void RequirePicture(const VdecCoreOutputInfo& output, std::size_t picture, std::uint64_t allocatedSize, std::uint64_t pts, std::uint64_t userData, const char* message) {
    const std::uint32_t height = H264Fixture::CodedHeight;
    auto* buffer = static_cast<const std::uint8_t*>(output.frameBuffer);
    Require(output.pictureCount == 1 && buffer != nullptr && output.frameFormat == 0 && output.reserved0 == 0, message);
    Require(output.frameWidth == H264Fixture::Width && output.framePitch == OutputPitch && output.frameHeight == height && output.framePitchInBytes == OutputPitch, message);
    Require(output.lumaBuffer == buffer && output.chromaBuffer == buffer + static_cast<std::size_t>(OutputPitch) * height, message);
    const VdecCorePicture& first = output.pictures[0];
    Require(first.flags == 0 && first.reserved0 == 0 && first.pictureInfo == nullptr && first.frameBuffer == buffer && first.frameBufferSize == OutputPitch * height * 3 / 2 && first.framePitch == OutputPitch && first.reserved1 == 0, message);
    Require(output.pictures[1].frameBuffer == nullptr, message);
    Require(first.pts == pts && first.userData == userData, message);
    Require(output.aspectRatio == 1 && output.sarWidth == 1 && output.videoFormat == 5 && output.colourPrimaries == 1 && output.transferCharacteristics == 1 && output.videoFullRangeFlag == 0 && output.colourDescriptionPresentFlag == 1, message);
    Require(VisibleHash(buffer, output.framePitch, output.frameHeight) == VisiblePictureHashes[picture], message);
    Require(ExtendsLastRows(buffer, output.framePitch, output.frameHeight), message);
    const VdecCoreHwPictureInfo& info = HwInfo(output, allocatedSize);
    Require(info.codecType == 0 && info.userData == userData && info.pts == pts && info.dts == first.dts && info.valid == 1, message);
    Require(info.width == H264Fixture::Width && info.height == height && info.cropLeft == 0 && info.cropRight == 0 && info.cropTop == 0 && info.cropBottom == (height - H264Fixture::Height) / 2, message);
    Require(info.sarWidth == 1 && info.sarHeight == 1 && info.flags == 0x03, message);
}

void CheckDecodeArguments() {
    void* decoder = CreateAvc(static_cast<std::int32_t>(H264Fixture::Width));
    const auto& unit = Units()[0];
    VdecCoreInputData input{unit.data(), unit.size(), 0, 0, 0, 0, 0};
    std::array<std::uint32_t, 2> status{7, 7};
    Require(sceVdecCoreSetDecodeInput(nullptr, &input, nullptr, status.data()) == InvalidArgument, "set input without a decoder");
    Require(sceVdecCoreSetDecodeInput(decoder, nullptr, nullptr, status.data()) == InvalidArgument, "set input without input data");
    Require(status[0] == 7 && status[1] == 7, "an input refused for a missing decoder or input leaves the status alone");
    input.auData = nullptr;
    Require(sceVdecCoreSetDecodeInput(decoder, &input, nullptr, status.data()) == InvalidArgument, "set input without access unit data");
    Require(status[0] == 1 && status[1] == 1, "the status is written before the access unit is checked");
    status = {7, 7};
    input.auData = unit.data();
    input.auSize = 0;
    Require(sceVdecCoreSetDecodeInput(decoder, &input, nullptr, status.data()) == InvalidArgument, "set input with an empty access unit");
    Require(status[0] == 1 && status[1] == 1, "the status is written before the access unit size is checked");
    input.auSize = unit.size();
    Require(sceVdecCoreSetDecodeInput(decoder, &input, nullptr, nullptr) == 0, "set input without a status");

    std::uint32_t count = 7;
    std::uint64_t pointers[2] = {7, 7};
    VdecCoreOutputInfo output{};
    VdecCoreFrameBuffer frameBuffer{};
    Require(sceVdecCoreResetDecoder(nullptr, nullptr) == InvalidArgument, "reset without a decoder");
    Require(sceVdecCoreSyncDecode(nullptr, &count) == InvalidArgument, "sync without a decoder");
    Require(sceVdecCoreSyncDecodeWptr(nullptr, &count, &pointers[0], &pointers[1]) == InvalidArgument, "write-pointer sync without a decoder");
    Require(sceVdecCoreTrySyncDecode(nullptr, &count) == InvalidArgument, "try-sync without a decoder");
    Require(sceVdecCoreTrySyncDecodeWptr(nullptr, &count, &pointers[0], &pointers[1]) == InvalidArgument, "write-pointer try-sync without a decoder");
    Require(sceVdecCoreGetDecodeOutput(nullptr, &frameBuffer, 0, &output, nullptr) == InvalidArgument, "get output without a decoder");
    Require(sceVdecCoreGetDecodeOutput(decoder, &frameBuffer, 0, nullptr, nullptr) == InvalidArgument, "get output without output info");
    Require(sceVdecCoreFlushDecodeOutput(nullptr, &count, nullptr) == InvalidArgument, "flush without a decoder");
    Require(count == 7 && pointers[0] == 7 && pointers[1] == 7, "refused calls leave their outputs alone");

    Require(sceVdecCoreSyncDecode(decoder, nullptr) == 0, "sync without a count");
    Require(sceVdecCoreFlushDecodeOutput(decoder, nullptr, nullptr) == 0, "flush without a count");
    std::uint32_t held = 7;
    Require(sceVdecCoreTrySyncDecode(decoder, nullptr) == 0 && sceVdecCoreTrySyncDecode(decoder, &held) == 0 && held == 1, "try-sync after a flush counts the held picture");
    Require(sceVdecCoreResetDecoder(decoder, nullptr) == 0, "reset");
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete the argument decoder");
}

void CheckEmptyQueue() {
    void* decoder = CreateAvc(100);
    std::uint32_t count = 7;
    Require(sceVdecCoreTrySyncDecode(decoder, &count) == 0 && count == 0, "try-sync on an idle decoder");
    count = 7;
    Require(sceVdecCoreSyncDecode(decoder, &count) == 0 && count == 0, "sync on an idle decoder");
    VdecCoreOutputInfo output;
    std::memset(&output, 0xab, sizeof(output));
    Require(sceVdecCoreGetDecodeOutput(decoder, nullptr, 0, &output, nullptr) == NoPicture, "get output from an empty queue");
    Require(output.frameWidth == 100 && output.framePitch == 128 && output.frameHeight == H264Fixture::Height, "an empty queue reports the maximum width, its 64-byte pitch and the maximum height");
    Require(output.pictureCount == 0 && output.frameBuffer == nullptr && output.frameFormat == 0 && output.reserved0 == 0, "an empty queue reports no picture");
    Require(output.pictures[0].frameBuffer == nullptr && output.pictures[1].frameBuffer == nullptr && output.lumaBuffer == nullptr && output.chromaBuffer == nullptr, "an empty queue reports no buffers");
    Require(output.pictures[0].pts == 0xababababababababull && output.aspectRatio == 0xababababu && output.colourDescriptionPresentFlag == 0xababababu, "an empty queue leaves the other fields alone");
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete the idle decoder");
}

void CheckDecodeIntoSlots() {
    void* decoder = CreateAvc(static_cast<std::int32_t>(H264Fixture::Width));
    std::vector<std::uint8_t> memory(SlotSize * 4);
    const VdecCoreMemoryBlock block{memory.data(), memory.size(), 256, 0};
    Require(sceVdecCoreMapMemoryBlock(decoder, &block, 1) == 0, "map four frame buffer slots");
    const std::vector<void*> slots = Slots(decoder);
    Require(slots.size() == 4, "four slots");

    std::size_t pictures = 0;
    std::vector<std::uint64_t> dts;
    const auto take = [&](std::uint32_t available) {
        for (std::uint32_t index = 0; index < available; ++index) {
            const VdecCoreOutputInfo output = Output(decoder, nullptr);
            Require(pictures < H264Fixture::PictureHashes.size(), "more pictures than access units");
            Require(output.frameBuffer == slots[pictures % slots.size()], "pictures go to the mapped slots in turn");
            const auto unit = H264Fixture::DisplayOrderUnits[pictures];
            RequirePicture(output, pictures, SlotSize, 1000 + unit, 3000 + unit, "a picture decoded into a slot");
            dts.push_back(output.pictures[0].dts);
            ++pictures;
        }
    };
    for (std::size_t unit = 0; unit < Units().size(); ++unit) {
        Feed(decoder, unit, 1000 + static_cast<std::int64_t>(unit), 2000 + static_cast<std::int64_t>(unit), 3000 + unit);
        std::uint32_t tried = 0xffffffffu;
        const int result = sceVdecCoreTrySyncDecode(decoder, &tried);
        Require(result == 0 || (result == DecodeRunning && tried == 0), "try-sync during a decode");
        std::uint32_t count = 0xffffffffu;
        Require(sceVdecCoreSyncDecode(decoder, &count) == 0, "sync after an access unit");
        take(count);
    }
    take(Flush(decoder));
    Require(pictures == H264Fixture::PictureHashes.size(), "every access unit gives a picture");
    for (const auto value : dts) Require(value >= 2000 && value < 2000 + Units().size(), "a picture's dts is one of the inputs'");
    VdecCoreOutputInfo output{};
    Require(sceVdecCoreGetDecodeOutput(decoder, nullptr, 0, &output, nullptr) == NoPicture, "no picture after the flushed ones");
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete the slot decoder");
}

void CheckInvalidPts() {
    void* decoder = CreateAvc(static_cast<std::int32_t>(H264Fixture::Width));
    FeedAll(decoder, PtsInvalid, PtsInvalid, 5000);
    Require(Flush(decoder) == H264Fixture::PictureHashes.size(), "a flush holds every picture");
    for (std::size_t picture = 0; picture < H264Fixture::PictureHashes.size(); ++picture) {
        const VdecCoreOutputInfo output = Output(decoder, nullptr);
        RequirePicture(output, picture, SlotSize, ~std::uint64_t{0}, 5000 + H264Fixture::DisplayOrderUnits[picture], "a picture without a pts");
        Require(output.pictures[0].dts == ~std::uint64_t{0}, "a picture without a dts takes the last input's invalid dts");
    }
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete the invalid pts decoder");
}

void CheckDestinations() {
    void* decoder = CreateAvc(static_cast<std::int32_t>(H264Fixture::Width));
    FeedAll(decoder, 1000, 2000, 3000);
    Require(Flush(decoder) == H264Fixture::PictureHashes.size(), "a flush holds every picture for the fallback");
    const VdecCoreOutputInfo fallback = Output(decoder, nullptr);
    RequirePicture(fallback, 0, SlotSize, 1000, 3000, "a picture in the fallback buffer");
    const VdecCoreOutputInfo again = Output(decoder, nullptr);
    Require(again.frameBuffer == fallback.frameBuffer, "the fallback buffer is reused");
    RequirePicture(again, 1, SlotSize, 1000 + H264Fixture::DisplayOrderUnits[1], 3000 + H264Fixture::DisplayOrderUnits[1], "a second picture in the fallback buffer");

    std::vector<std::uint8_t> slotMemory(SlotSize);
    const VdecCoreMemoryBlock block{slotMemory.data(), slotMemory.size(), 256, 0};
    Require(sceVdecCoreMapMemoryBlock(decoder, &block, 1) == 0, "map one slot");
    std::vector<std::uint8_t> caller(0x2000);
    VdecCoreFrameBuffer frameBuffer{caller.data(), caller.size()};
    const VdecCoreOutputInfo supplied = Output(decoder, &frameBuffer);
    Require(supplied.frameBuffer == caller.data(), "a caller frame buffer comes before the slots");
    RequirePicture(supplied, 2, caller.size(), 1000 + H264Fixture::DisplayOrderUnits[2], 3000 + H264Fixture::DisplayOrderUnits[2], "a picture in the caller's frame buffer");
    VdecCoreFrameBuffer unaddressed{nullptr, caller.size()};
    const VdecCoreOutputInfo slot = Output(decoder, &unaddressed);
    Require(slot.frameBuffer == slotMemory.data(), "a frame buffer without an address leaves the picture to the slots");
    RequirePicture(slot, 3, SlotSize, 1000 + H264Fixture::DisplayOrderUnits[3], 3000 + H264Fixture::DisplayOrderUnits[3], "a picture in a slot keeps the slot's size, not the caller's");
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete the destination decoder");
}

void CheckOutputStaysInItsDestination() {
    void* decoder = CreateAvc(static_cast<std::int32_t>(H264Fixture::Width));
    FeedAll(decoder, 1000, 2000, 3000);
    Require(Flush(decoder) == H264Fixture::PictureHashes.size(), "a flush holds every picture for the destination bounds");
    constexpr std::uint8_t Guard = 0x5a;
    std::vector<std::uint8_t> memory(SlotSize * 2, Guard);
    const VdecCoreMemoryBlock block{memory.data(), SlotSize, 256, 0};
    Require(sceVdecCoreMapMemoryBlock(decoder, &block, 1) == 0, "map one slot followed by guard bytes");
    VdecCoreFrameBuffer larger{nullptr, SlotSize * 2};
    const VdecCoreOutputInfo slot = Output(decoder, &larger);
    Require(slot.frameBuffer == memory.data(), "a frame buffer with no address and a larger size goes to the slot");
    RequirePicture(slot, 0, SlotSize, 1000, 3000, "a picture in a slot under a larger caller size");
    Require(std::all_of(memory.begin() + static_cast<std::ptrdiff_t>(SlotSize), memory.end(), [](std::uint8_t byte) { return byte == Guard; }), "a picture in a slot under a larger caller size wrote past the slot");
    const std::uint32_t pictureSize = OutputPitch * H264Fixture::CodedHeight * 3 / 2;
    std::vector<std::uint8_t> tight(pictureSize + 0x200, Guard);
    VdecCoreFrameBuffer small{tight.data(), tight.size()};
    const VdecCoreOutputInfo caller = Output(decoder, &small);
    Require(caller.frameBuffer == tight.data(), "a caller frame buffer with room for the picture only");
    const auto unit = H264Fixture::DisplayOrderUnits[1];
    Require(caller.pictures[0].pts == 1000 + unit && caller.pictures[0].userData == 3000 + unit, "the picture in a caller frame buffer with room for the picture only");
    Require(VisibleHash(tight.data(), caller.framePitch, caller.frameHeight) == VisiblePictureHashes[1] && ExtendsLastRows(tight.data(), caller.framePitch, caller.frameHeight), "picture information too large for the room after a picture overwrote the picture");
    Require(std::all_of(tight.begin() + static_cast<std::ptrdiff_t>(pictureSize), tight.end(), [](std::uint8_t byte) { return byte == Guard; }), "picture information was written into a caller frame buffer with no room for it after the picture");
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete the bounds decoder");
}

void CheckFlushResetAndSync() {
    void* decoder = CreateAvc(static_cast<std::int32_t>(H264Fixture::Width));
    std::vector<std::uint8_t> memory(SlotSize * 3);
    const VdecCoreMemoryBlock block{memory.data(), memory.size(), 256, 0};
    Require(sceVdecCoreMapMemoryBlock(decoder, &block, 1) == 0, "map three slots");
    const std::vector<void*> slots = Slots(decoder);

    FeedAll(decoder, 1000, 2000, 3000);
    const std::uint32_t held = Flush(decoder);
    Require(held == H264Fixture::PictureHashes.size(), "a flush returns every held picture");
    std::uint32_t count = 0;
    Require(sceVdecCoreSyncDecode(decoder, &count) == 0 && count == held, "sync after a flush counts the held pictures");
    std::uint32_t tried = 0;
    Require(sceVdecCoreTrySyncDecode(decoder, &tried) == 0 && tried == held, "try-sync after a sync counts the held pictures");
    std::uint64_t pointers[2] = {7, 7};
    std::uint32_t forwarded = 0;
    Require(sceVdecCoreSyncDecodeWptr(decoder, &forwarded, &pointers[0], &pointers[1]) == 0 && forwarded == held, "the write-pointer sync forwards to the sync");
    forwarded = 0;
    Require(sceVdecCoreTrySyncDecodeWptr(decoder, &forwarded, &pointers[0], &pointers[1]) == 0 && forwarded == held, "the write-pointer try-sync forwards to the try-sync");
    Require(pointers[0] == 7 && pointers[1] == 7, "the write-pointer variants leave the pointers alone");

    Require(Output(decoder, nullptr).frameBuffer == slots[0], "the first picture goes to the first slot");
    Require(Output(decoder, nullptr).frameBuffer == slots[1], "the second picture goes to the second slot");

    FeedAll(decoder, 1000, 2000, 3000);
    Require(sceVdecCoreResetDecoder(decoder, nullptr) == 0, "reset with queued input");
    count = 7;
    Require(sceVdecCoreSyncDecode(decoder, &count) == 0 && count == 0, "a reset drops the queued input and its pictures");
    tried = 7;
    Require(sceVdecCoreTrySyncDecode(decoder, &tried) == 0 && tried == 0, "try-sync after a reset");
    VdecCoreOutputInfo output{};
    Require(sceVdecCoreGetDecodeOutput(decoder, nullptr, 0, &output, nullptr) == NoPicture, "no picture after a reset");
    Require(Flush(decoder) == 0, "a flush after a reset holds nothing");

    FeedAll(decoder, 1000, 2000, 3000);
    Require(Flush(decoder) == H264Fixture::PictureHashes.size(), "the decoder decodes again after a reset");
    const VdecCoreOutputInfo restarted = Output(decoder, nullptr);
    Require(restarted.frameBuffer == slots[0], "a reset starts the slots again");
    RequirePicture(restarted, 0, SlotSize, 1000, 3000, "the first picture after a reset");
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete the flushed decoder");
}

void CheckDeleteWithQueuedInput() {
    void* decoder = CreateAvc(static_cast<std::int32_t>(H264Fixture::Width));
    for (int round = 0; round < 8; ++round) FeedAll(decoder, 1000, 2000, 3000);
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete a decoder with queued input");
    void* held = CreateAvc(static_cast<std::int32_t>(H264Fixture::Width));
    FeedAll(held, 1000, 2000, 3000);
    Require(Flush(held) == H264Fixture::PictureHashes.size(), "a flush before the delete holds every picture");
    Require(sceVdecCoreDeleteDecoder(held) == 0, "delete a decoder with held pictures");
}

}

int main() {
    CheckExports();
    CheckInstanceSize();
    CheckFrameBufferInfo();
    CheckComputeResourceInfo();
    CheckCreateAndDelete();
    CheckMapMemoryBlock();
    CheckDecodeArguments();
    CheckEmptyQueue();
    CheckDecodeIntoSlots();
    CheckInvalidPts();
    CheckDestinations();
    CheckOutputStaysInItsDestination();
    CheckFlushResetAndSync();
    CheckDeleteWithQueuedInput();
    return 0;
}
