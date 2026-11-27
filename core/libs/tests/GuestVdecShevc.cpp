#include "prx/libSceVdecShevc/libSceVdecShevc.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

using HandleCall = decltype(&sceSdecExecuteSwHevc);

void ClobberErrno(std::uintptr_t, std::size_t) { errno = 99; }

void Require(bool value) {
    if (!value) std::abort();
}

std::array<unsigned char, 0x40> Block() {
    std::array<unsigned char, 0x40> block{};
    block.fill(0x5A);
    return block;
}

void PutQuad(std::array<unsigned char, 0x40>& block, std::size_t offset, std::uint64_t value) {
    std::memcpy(block.data() + offset, &value, sizeof(value));
}

std::uint64_t GetQuad(const std::array<unsigned char, 0x40>& block, std::size_t offset) {
    std::uint64_t value = 0;
    std::memcpy(&value, block.data() + offset, sizeof(value));
    return value;
}

void NoDecoderAnswers() {
    const std::array<HandleCall, 9> calls{
        sceSdecExecuteSwHevc,
        sceSdecGetFrameSwHevc,
        sceSdecResetSwHevc,
        libSceVdecShevcUnknown_79qqGAVnWfA_nid_no_patch,
        libSceVdecShevcUnknown_PTO17yTeoSk_nid_no_patch,
        libSceVdecShevcUnknown_hZXstu74B4I_nid_no_patch,
        libSceVdecShevcUnknown_lLaAHswK1zQ_nid_no_patch,
        libSceVdecShevcUnknown_wvq_plus_GPpswaw_nid_no_patch,
        libSceVdecShevcUnknown_xbRElcEVmdM_nid_no_patch,
    };
    auto handle = Block();
    PutQuad(handle, 0, 0);
    const auto before = handle;
    for (const HandleCall call : calls) {
        for (int round = 0; round < 2; ++round) {
            Require(call(nullptr) == 1);
            errno = 71;
            Require(call(handle.data()) == 1);
            Require(errno == 71);
        }
    }
    Require(handle == before);

    std::array<unsigned char, 0x40> destroyed{};
    destroyed.fill(0xA5);
    PutQuad(destroyed, 0x10, 0);
    errno = 71;
    Require(sceSdecDestroySwHevc(destroyed.data()) == 0);
    Require(errno == 71);
    Require(GetQuad(destroyed, 0x8) == 0);
    const auto destroyedBefore = destroyed;
    for (const HandleCall call : calls) {
        errno = 71;
        Require(call(destroyed.data()) == 1);
        Require(errno == 71);
    }
    Require(destroyed == destroyedBefore);
}

void CreateRejects() {
    for (const std::uint32_t first : {0u, 2u, 0xFFFFFFFFu, 0x5A5A5A5Au}) {
        auto param = Block();
        std::memcpy(param.data(), &first, sizeof(first));
        const auto before = param;
        for (int round = 0; round < 2; ++round) {
            errno = 71;
            Require(sceSdecCreateSwHevc(param.data()) == 3);
            Require(errno == 71);
        }
        Require(param == before);
    }
}

void DestroyWithoutDecoder() {
    for (int round = 0; round < 2; ++round) Require(sceSdecDestroySwHevc(nullptr) == 0);

    auto handle = Block();
    PutQuad(handle, 0x10, 0);
    auto expected = handle;
    PutQuad(expected, 0x8, 0);
    errno = 71;
    Require(sceSdecDestroySwHevc(handle.data()) == 0);
    Require(errno == 71);
    Require(handle == expected);
    Require(GetQuad(handle, 0x8) == 0);
    errno = 71;
    Require(sceSdecDestroySwHevc(handle.data()) == 0);
    Require(errno == 71);
    Require(handle == expected);

    auto cleared = Block();
    PutQuad(cleared, 0x8, 0);
    PutQuad(cleared, 0x10, 0);
    const auto clearedBefore = cleared;
    Require(sceSdecDestroySwHevc(cleared.data()) == 0);
    Require(cleared == clearedBefore);
}

SdecHevcConfig MakeConfig(
    std::uint32_t defaultDepthMode,
    std::uint8_t levelIdc,
    std::int32_t frameCount,
    std::int32_t widthInMbs,
    std::int32_t heightInMbs,
    std::uint32_t extraWidthPixels,
    std::uint32_t extraHeightPixels,
    std::uint8_t depthLuma,
    std::uint8_t depthChroma
) {
    SdecHevcConfig config{};
    config.magic = 1;
    config.defaultDepthMode = defaultDepthMode;
    config.levelIdc = levelIdc;
    config.frameCount = frameCount;
    config.widthInMbs = widthInMbs;
    config.heightInMbs = heightInMbs;
    config.extraWidthPixels = extraWidthPixels;
    config.extraHeightPixels = extraHeightPixels;
    config.depthLuma = depthLuma;
    config.depthChroma = depthChroma;
    return config;
}

SdecRecord MakeRecord(std::uint32_t instanceMode, std::uint32_t unknown8, std::int32_t extraFrames) {
    SdecRecord record{};
    record.instanceMode = instanceMode;
    record.unknown8 = unknown8;
    record.extraFrames = extraFrames;
    return record;
}

void ExpectSizes(
    const SdecHevcConfig& config,
    const SdecRecord& record,
    std::uint32_t expectedDpb,
    std::uint64_t expectedSize1,
    std::uint64_t expectedSize2,
    std::uint64_t expectedSize3
) {
    SdecMemory memory{};
    std::uint32_t dpbFrames = 0xDEADBEEFu;
    std::uint8_t tooBig = 0x5Au;
    errno = 71;
    Require(sceSdecQueryMemorySwHevc(&config, &record, 1, &memory, &dpbFrames, &tooBig) == 0);
    Require(errno == 71);
    Require(tooBig == 0x5Au);
    Require(dpbFrames == expectedDpb);
    Require(memory.blocks[0].base == 0);
    Require(memory.blocks[0].size == expectedSize1);
    Require(memory.blocks[0].alignment == 0);
    Require(memory.blocks[0].reserved == 0);
    Require(memory.blocks[1].base == 0);
    Require(memory.blocks[1].size == expectedSize2);
    Require(memory.blocks[1].alignment == 256);
    Require(memory.blocks[1].reserved == 0);
    Require(memory.blocks[2].base == 0);
    Require(memory.blocks[2].size == expectedSize3);
    Require(memory.blocks[2].alignment == 256);
    Require(memory.blocks[2].reserved == 0);

    SdecMemory noTooBig{};
    std::uint32_t dpbNoTooBig = 0;
    errno = 71;
    Require(sceSdecQueryMemorySwHevc(&config, &record, 1, &noTooBig, &dpbNoTooBig, nullptr) == 0);
    Require(errno == 71);
    Require(dpbNoTooBig == expectedDpb);
    Require(std::memcmp(&noTooBig, &memory, sizeof(memory)) == 0);
}

void QueryMemoryRejectsAndOrder() {
    const auto validConfig = MakeConfig(1, 120, -1, 40, 30, 0, 0, 8, 8);
    const auto validRecord = MakeRecord(2, 2, 0);
    SdecMemory memory{};
    std::uint32_t dpbFrames = 0xDEADBEEFu;
    std::uint8_t tooBig = 0x5Au;

    errno = 71;
    Require(sceSdecQueryMemorySwHevc(&validConfig, &validRecord, 0, &memory, &dpbFrames, &tooBig) == 3);
    Require(errno == 71);
    Require(dpbFrames == 0xDEADBEEFu);
    Require(tooBig == 0x5Au);

    Require(sceSdecQueryMemorySwHevc(nullptr, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);
    Require(sceSdecQueryMemorySwHevc(&validConfig, nullptr, 1, &memory, &dpbFrames, &tooBig) == 3);
    Require(sceSdecQueryMemorySwHevc(&validConfig, &validRecord, 1, nullptr, &dpbFrames, &tooBig) == 3);
    Require(sceSdecQueryMemorySwHevc(&validConfig, &validRecord, 1, &memory, nullptr, &tooBig) == 3);

    const auto* badConfig = reinterpret_cast<const SdecHevcConfig*>(std::uintptr_t{0x10});
    const auto* badRecord = reinterpret_cast<const SdecRecord*>(std::uintptr_t{0x10});
    auto* badMemory = reinterpret_cast<SdecMemory*>(std::uintptr_t{0x10});
    auto* badDpb = reinterpret_cast<std::uint32_t*>(std::uintptr_t{0x10});
    Require(sceSdecQueryMemorySwHevc(badConfig, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);
    Require(sceSdecQueryMemorySwHevc(&validConfig, badRecord, 1, &memory, &dpbFrames, &tooBig) == 3);
    Require(sceSdecQueryMemorySwHevc(&validConfig, &validRecord, 1, badMemory, &dpbFrames, &tooBig) == 3);
    Require(sceSdecQueryMemorySwHevc(&validConfig, &validRecord, 1, &memory, badDpb, &tooBig) == 3);
    Require(errno == 71);

    const std::array<std::array<std::uint8_t, 2>, 3> badDepths{{{8, 10}, {8, 0}, {0, 8}}};
    for (const auto& pair : badDepths) {
        const auto badDepthConfig = MakeConfig(1, 120, -1, 40, 30, 0, 0, pair[0], pair[1]);
        dpbFrames = 0xDEADBEEFu;
        Require(sceSdecQueryMemorySwHevc(&badDepthConfig, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);
        Require(dpbFrames == 0xDEADBEEFu);
    }

    const auto badInstances = MakeRecord(2, 17, 0);
    dpbFrames = 0xDEADBEEFu;
    Require(sceSdecQueryMemorySwHevc(&validConfig, &badInstances, 1, &memory, &dpbFrames, &tooBig) == 3);
    Require(dpbFrames == 16);

    auto badMagic = validConfig;
    badMagic.magic = 0;
    dpbFrames = 0;
    Require(sceSdecQueryMemorySwHevc(&badMagic, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);
    Require(dpbFrames == 16);

    auto zeroWidth = validConfig;
    zeroWidth.widthInMbs = 0;
    dpbFrames = 0;
    Require(sceSdecQueryMemorySwHevc(&zeroWidth, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);
    Require(dpbFrames == 16);

    auto zeroHeight = validConfig;
    zeroHeight.heightInMbs = 0;
    dpbFrames = 0;
    Require(sceSdecQueryMemorySwHevc(&zeroHeight, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);
    Require(dpbFrames == 16);

    auto badExtraW = validConfig;
    badExtraW.extraWidthPixels = 4;
    Require(sceSdecQueryMemorySwHevc(&badExtraW, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);

    auto badExtraH = validConfig;
    badExtraH.extraHeightPixels = 16;
    Require(sceSdecQueryMemorySwHevc(&badExtraH, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);

    const auto tooLarge = MakeConfig(1, 30, -1, 16, 10, 0, 0, 8, 8);
    dpbFrames = 0;
    tooBig = 0;
    Require(sceSdecQueryMemorySwHevc(&tooLarge, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);
    Require(dpbFrames == 6);
    Require(tooBig == 1);
    Require(sceSdecQueryMemorySwHevc(&tooLarge, &validRecord, 1, &memory, &dpbFrames, nullptr) == 3);

    const auto wideRefused = MakeConfig(1, 180, -1, 1025, 1, 0, 0, 8, 8);
    tooBig = 0;
    Require(sceSdecQueryMemorySwHevc(&wideRefused, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);
    Require(tooBig == 0);

    const auto tallRefused = MakeConfig(1, 180, -1, 1, 684, 0, 0, 8, 8);
    Require(sceSdecQueryMemorySwHevc(&tallRefused, &validRecord, 1, &memory, &dpbFrames, &tooBig) == 3);

    const auto tooManyFrames = MakeRecord(2, 2, 0x2000);
    Require(sceSdecQueryMemorySwHevc(&validConfig, &tooManyFrames, 1, &memory, &dpbFrames, &tooBig) == 3);
}

void QueryMemoryDpbTable() {
    struct DpbCase {
        std::uint8_t levelIdc;
        std::int32_t frameCount;
        std::int32_t widthInMbs;
        std::int32_t heightInMbs;
        std::uint32_t expectedDpb;
    };
    const std::array<DpbCase, 16> cases{{
        {30, -1, 6, 6, 16},
        {30, -1, 12, 6, 12},
        {30, -1, 18, 6, 8},
        {30, -1, 12, 12, 6},
        {60, -1, 20, 20, 6},
        {63, -1, 30, 20, 8},
        {90, -1, 40, 20, 12},
        {93, -1, 60, 40, 8},
        {120, -1, 80, 45, 12},
        {123, -1, 120, 68, 6},
        {180, -1, 512, 256, 6},
        {183, -1, 300, 256, 8},
        {186, -1, 128, 128, 16},
        {0, -1, 240, 135, 6},
        {255, 99, 240, 135, 16},
        {150, 0, 240, 135, 0},
    }};
    const auto record = MakeRecord(1, 1, 0);
    for (const auto& entry : cases) {
        const auto config = MakeConfig(1, entry.levelIdc, entry.frameCount, entry.widthInMbs, entry.heightInMbs, 0, 0, 8, 8);
        SdecMemory memory{};
        std::uint32_t dpbFrames = 99;
        Require(sceSdecQueryMemorySwHevc(&config, &record, 1, &memory, &dpbFrames, nullptr) == 0);
        Require(dpbFrames == entry.expectedDpb);
    }
}

void QueryMemoryExactSizes() {
    ExpectSizes(MakeConfig(1, 120, -1, 120, 67, 0, 8, 0, 0), MakeRecord(0, 16, -2), 6, 11571032ull, 26881024ull, 25673472ull);
    ExpectSizes(MakeConfig(1, 120, -1, 120, 67, 0, 8, 0, 0), MakeRecord(5, 16, -2), 6, 11571032ull, 26881024ull, 25673472ull);
    ExpectSizes(MakeConfig(0, 93, -1, 80, 45, 0, 0, 0, 0), MakeRecord(1, 7, 2), 6, 5899160ull, 9931520ull, 26915328ull);
    ExpectSizes(MakeConfig(1, 90, 4, 20, 15, 0, 0, 8, 8), MakeRecord(2, 0, 1), 4, 3542136ull, 939264ull, 1192448ull);
    ExpectSizes(MakeConfig(2, 150, -1, 240, 135, 8, 0, 10, 10), MakeRecord(2, 4, 3), 6, 62585976ull, 187889664ull, 283173376ull);
    ExpectSizes(MakeConfig(1, 60, -1, 4, 4, 0, 0, 8, 8), MakeRecord(3, 2, 0), 16, 3687000ull, 603904ull, 476928ull);
    ExpectSizes(MakeConfig(0, 93, -1, 40, 30, 0, 0, 10, 10), MakeRecord(3, 3, 1), 12, 5685048ull, 6445312ull, 16982272ull);
    ExpectSizes(MakeConfig(1, 93, -1, 40, 30, 0, 0, 0, 10), MakeRecord(3, 3, 1), 12, 5685048ull, 6052096ull, 9240832ull);
    ExpectSizes(MakeConfig(1, 93, -1, 40, 30, 0, 0, 12, 10), MakeRecord(3, 3, 1), 12, 5685048ull, 6445312ull, 16982272ull);
    ExpectSizes(MakeConfig(1, 93, -1, 31, 20, 8, 0, 8, 8), MakeRecord(4, 2, 0), 16, 4496536ull, 2586368ull, 4916992ull);
    ExpectSizes(MakeConfig(1, 120, -1, 32, 20, 0, 0, 10, 10), MakeRecord(4, 5, 0), 16, 5539352ull, 4730112ull, 9569280ull);
    ExpectSizes(MakeConfig(1, 120, -1, 32, 20, 0, 0, 10, 10), MakeRecord(4, 6, 0), 16, 5886648ull, 5490944ull, 9588480ull);
    ExpectSizes(MakeConfig(1, 180, -1, 1024, 136, 0, 0, 8, 8), MakeRecord(2, 2, 0), 6, 136041496ull, 447491328ull, 412458752ull);
    ExpectSizes(MakeConfig(0, 30, -1, -1, 10, 8, 8, 0, 0), MakeRecord(3, 2, 0), 16, 4033976ull, 1883136ull, 7617792ull);
    ExpectSizes(MakeConfig(1, 120, -1, 50, -1, 0, 0, 8, 8), MakeRecord(4, 3, 1), 16, 19559352ull, 43927296ull, 99153920ull);

    struct ModeDeltaCase {
        std::int32_t w;
        std::int32_t h;
        std::uint64_t expectedDelta;
    };
    const std::array<ModeDeltaCase, 5> mode3Deltas{{{1, 1, 2560}, {2, 2, 1536}, {5, 4, 6912}, {4, 4, 0}, {16, 16, 0}}};
    for (const auto& entry : mode3Deltas) {
        const auto cfg = MakeConfig(1, 93, -1, entry.w, entry.h, entry.w == 5 ? 8u : 0u, entry.h == 4 && entry.w == 5 ? 8u : 0u, 8, 8);
        const auto r2 = MakeRecord(2, 1, 0);
        const auto r3 = MakeRecord(3, 1, 0);
        SdecMemory m2{};
        SdecMemory m3{};
        std::uint32_t d2 = 0;
        std::uint32_t d3 = 0;
        Require(sceSdecQueryMemorySwHevc(&cfg, &r2, 1, &m2, &d2, nullptr) == 0);
        Require(sceSdecQueryMemorySwHevc(&cfg, &r3, 1, &m3, &d3, nullptr) == 0);
        Require(m3.blocks[1].size - m2.blocks[1].size == entry.expectedDelta);
    }

    const auto cfg640 = MakeConfig(1, 93, -1, 40, 23, 0, 0, 8, 8);
    const auto r3 = MakeRecord(3, 1, 0);
    const auto r4 = MakeRecord(4, 1, 0);
    SdecMemory m3{};
    SdecMemory m4{};
    std::uint32_t dpb = 0;
    Require(sceSdecQueryMemorySwHevc(&cfg640, &r3, 1, &m3, &dpb, nullptr) == 0);
    Require(sceSdecQueryMemorySwHevc(&cfg640, &r4, 1, &m4, &dpb, nullptr) == 0);
    Require(m4.blocks[2].size - m3.blocks[2].size == 43776);
}

void QueryMemoryThrowsOnUnknownMode() {
    const auto config = MakeConfig(1, 90, -1, 20, 15, 0, 0, 8, 8);
    const auto record = MakeRecord(6, 2, 0);
    SdecMemory memory{};
    std::uint32_t dpbFrames = 0;
    bool threw = false;
    try {
        sceSdecQueryMemorySwHevc(&config, &record, 1, &memory, &dpbFrames, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

} // namespace

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    NoDecoderAnswers();
    CreateRejects();
    DestroyWithoutDecoder();
    QueryMemoryRejectsAndOrder();
    QueryMemoryDpbTable();
    QueryMemoryExactSizes();
    QueryMemoryThrowsOnUnknownMode();
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
