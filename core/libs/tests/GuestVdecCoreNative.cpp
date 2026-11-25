#include "prx/libSceVdecCore.native/include/Api.hpp"
#include "H264Fixture.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80C00001);
constexpr int InvalidCodecType = static_cast<int>(0x80C00003);
constexpr int InvalidAddress = static_cast<int>(0x80C00007);
constexpr int MaxMapCount = static_cast<int>(0x80C00015);
constexpr std::uint32_t CodecAvc = 0;
constexpr std::uint32_t DecoderVdec2 = 2;
constexpr std::uint64_t SlotSize = 0x10000;
constexpr std::uint32_t OutputPitch = 64;
constexpr std::array<std::uint64_t, 6> VisiblePictureHashes{
    0x5871fdc85dfa274eull, 0x26ce4afb974cffc4ull, 0xd2e739633733d5baull,
    0x54feb3d290e028fdull, 0xfa1342a4ba1819c8ull, 0x870125dd7f079a4cull,
};

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

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "VdecCore.native: %s\n", message);
        std::abort();
    }
}

VdecCoreConfigInfo MakeConfig() {
    VdecCoreConfigInfo config{};
    config.decoderType = DecoderVdec2;
    config.codecType = CodecAvc;
    config.maxFrameWidth = static_cast<std::int32_t>(H264Fixture::Width);
    config.maxFrameHeight = static_cast<std::int32_t>(H264Fixture::Height);
    config.horizontalAlign = 256;
    config.maxDpbFrameCount = 0;
    return config;
}

void CheckCreateVariants() {
    VdecCoreConfigInfo config = MakeConfig();
    VdecCoreMemoryInfo memory{};
    void* decoder = nullptr;

    Require(sceVdecCoreCreateDecoderBid(0, nullptr, &memory, &decoder) == InvalidArgument, "bid null config");
    Require(sceVdecCoreCreateDecoderBid(0, &config, &memory, nullptr) == InvalidArgument, "bid null decoder");
    Require(sceVdecCoreCreateDecoderSvp(0, nullptr, &memory, &decoder) == InvalidArgument, "svp null config");
    Require(sceVdecCoreCreateDecoderSvp(0, &config, &memory, nullptr) == InvalidArgument, "svp null decoder");

    config.decoderType = 9;
    Require(sceVdecCoreCreateDecoderBid(1, &config, &memory, &decoder) == InvalidArgument, "bid invalid decoderType");
    Require(sceVdecCoreCreateDecoderSvp(1, &config, &memory, &decoder) == InvalidArgument, "svp invalid decoderType");
    config.decoderType = DecoderVdec2;

    config.codecType = 5;
    Require(sceVdecCoreCreateDecoderBid(1, &config, &memory, &decoder) == InvalidCodecType, "bid invalid codecType");
    Require(sceVdecCoreCreateDecoderSvp(1, &config, &memory, &decoder) == InvalidCodecType, "svp invalid codecType");
    config.codecType = CodecAvc;

    errno = 71;
    Require(sceVdecCoreCreateDecoderBid(0, reinterpret_cast<const VdecCoreConfigInfo*>(static_cast<std::uintptr_t>(0x10)),
                                        &memory, &decoder) == InvalidArgument && errno == 71,
            "bid unreadable config preserves errno");
    Require(sceVdecCoreCreateDecoderSvp(0, &config, &memory,
                                        reinterpret_cast<void**>(static_cast<std::uintptr_t>(0x10))) == InvalidArgument && errno == 71,
            "svp unwritable decoder preserves errno");

    void* bidDecoder = nullptr;
    Require(sceVdecCoreCreateDecoderBid(2, &config, &memory, &bidDecoder) == 0 && bidDecoder != nullptr, "bid create succeeds");
    Require(sceVdecCoreDeleteDecoder(bidDecoder) == 0, "bid delete succeeds");

    void* svpDecoder = nullptr;
    Require(sceVdecCoreCreateDecoderSvp(3, &config, &memory, &svpDecoder) == 0 && svpDecoder != nullptr, "svp create succeeds");
    Require(sceVdecCoreDeleteDecoder(svpDecoder) == 0, "svp delete succeeds");
}

void CheckMapMemory() {
    VdecCoreConfigInfo config = MakeConfig();
    void* decoder = nullptr;
    Require(sceVdecCoreCreateDecoderBid(0, &config, nullptr, &decoder) == 0, "create decoder for map");

    std::vector<std::uint8_t> dummy(0x1000);
    Require(sceVdecCoreMapMemory(nullptr, dummy.data(), dummy.size()) == InvalidArgument, "map null decoder");
    Require(sceVdecCoreMapMemory(decoder, nullptr, dummy.size()) == InvalidArgument, "map null address");
    Require(sceVdecCoreMapMemory(decoder, dummy.data(), 0) == InvalidArgument, "map zero size");

    for (std::uint32_t i = 0; i < 16; ++i) {
        Require(sceVdecCoreMapMemory(decoder, dummy.data(), dummy.size()) == 0, "map within 16-block limit");
    }
    Require(sceVdecCoreMapMemory(decoder, dummy.data(), dummy.size()) == MaxMapCount, "17th map call returns 0x80C00015");
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete mapped decoder");
}

void CheckSetDecodeInputSvp() {
    VdecCoreConfigInfo config = MakeConfig();
    void* decoder = nullptr;
    Require(sceVdecCoreCreateDecoderSvp(4, &config, nullptr, &decoder) == 0, "create svp decoder");

    std::vector<std::uint8_t> slotPool(SlotSize * 6);
    Require(sceVdecCoreMapMemory(decoder, slotPool.data(), slotPool.size()) == 0, "map output slots via MapMemory");

    std::vector<std::uint8_t> rawAuStorage(256, 0);
    const std::uintptr_t baseAddr = reinterpret_cast<std::uintptr_t>(rawAuStorage.data());
    const std::uintptr_t alignedAddr = (baseAddr + 63u) & ~std::uintptr_t{63u};
    auto* alignedPtr = reinterpret_cast<std::uint8_t*>(alignedAddr);

    VdecCoreSvpInputMeta meta{};
    VdecCoreSvpInputBuffer buf{};
    buf.auData = alignedPtr + 1;
    buf.auSize = 16;
    std::array<std::uint32_t, 2> status{};

    Require(sceVdecCoreSetDecodeInputSvp(nullptr, &meta, &buf, status.data()) == InvalidArgument, "svp input null decoder");
    Require(sceVdecCoreSetDecodeInputSvp(decoder, nullptr, &buf, status.data()) == InvalidArgument, "svp input null meta");
    Require(sceVdecCoreSetDecodeInputSvp(decoder, &meta, nullptr, status.data()) == InvalidArgument, "svp input null buffer");

    errno = 71;
    Require(sceVdecCoreSetDecodeInputSvp(decoder, reinterpret_cast<const VdecCoreSvpInputMeta*>(static_cast<std::uintptr_t>(0x10)),
                                         &buf, status.data()) == InvalidArgument && errno == 71,
            "svp input unreadable meta preserves errno");
    Require(sceVdecCoreSetDecodeInputSvp(decoder, &meta,
                                         reinterpret_cast<const VdecCoreSvpInputBuffer*>(static_cast<std::uintptr_t>(0x10)),
                                         status.data()) == InvalidArgument && errno == 71,
            "svp input unreadable buffer preserves errno");

    Require(sceVdecCoreSetDecodeInputSvp(decoder, &meta, &buf, status.data()) == InvalidAddress, "unaligned svp auData rejected");

    const std::vector<std::vector<std::uint8_t>> units = H264Fixture::accessUnits(H264Fixture::Stream.data(), H264Fixture::Stream.size(), false);
    Require(units.size() == H264Fixture::PictureHashes.size(), "fixture unit count");
    for (std::size_t i = 0; i < units.size(); ++i) {
        std::vector<std::uint8_t> staging(units[i].size() + 64);
        const std::uintptr_t addr = (reinterpret_cast<std::uintptr_t>(staging.data()) + 63u) & ~std::uintptr_t{63u};
        auto* dst = reinterpret_cast<std::uint8_t*>(addr);
        std::memcpy(dst, units[i].data(), units[i].size());

        VdecCoreSvpInputMeta auMeta{};
        auMeta.pts = 1000 + static_cast<std::int64_t>(i);
        auMeta.dts = 2000 + static_cast<std::int64_t>(i);
        auMeta.userData = 3000 + static_cast<std::uint64_t>(i);
        VdecCoreSvpInputBuffer auBuf{};
        auBuf.auData = dst;
        auBuf.auSize = units[i].size();
        status = {};
        Require(sceVdecCoreSetDecodeInputSvp(decoder, &auMeta, &auBuf, status.data()) == 0, "svp decode input accepted");
        Require(status[0] == 1 && status[1] == 1, "svp input ready status");
    }

    std::uint32_t count = 0;
    Require(sceVdecCoreFlushDecodeOutput(decoder, &count, nullptr) == 0 && count == units.size(), "flush decoded svp frames");
    for (std::size_t picture = 0; picture < units.size(); ++picture) {
        VdecCoreOutputInfo output{};
        Require(sceVdecCoreGetDecodeOutput(decoder, nullptr, 0, &output, nullptr) == 0, "get decoded svp frame");
        Require(output.frameBuffer == slotPool.data() + picture * SlotSize, "decoded frame landed in mapped slot");
        Require(output.framePitch == OutputPitch && output.frameHeight == H264Fixture::CodedHeight, "decoded frame geometry");
        const auto unit = H264Fixture::DisplayOrderUnits[picture];
        Require(output.pictures[0].pts == 1000 + unit && output.pictures[0].userData == 3000 + unit, "decoded frame metadata");
        const auto* frameBytes = static_cast<const std::uint8_t*>(output.frameBuffer);
        Require(VisibleHash(frameBytes, output.framePitch, output.frameHeight) == VisiblePictureHashes[picture],
                "decoded frame pixel hash");
        Require(ExtendsLastRows(frameBytes, output.framePitch, output.frameHeight), "decoded frame padding rows");
    }
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete svp decoder");
}

}

int main() {
    CheckCreateVariants();
    CheckMapMemory();
    CheckSetDecodeInputSvp();
    std::puts("ok");
    return 0;
}
