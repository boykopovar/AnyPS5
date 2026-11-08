#include "prx/libSceVideodec2/HevcMetadata.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "HevcFixture.hpp"
#include "HevcMetadataFixture.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace SceVideodec2;


namespace {

constexpr std::uint32_t codecHevc = 974921;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "HEVC metadata: %s\n", message);
    std::abort();
}

template<typename TCall>
void Rejects(TCall call, const char* message) {
    try { call(); }
    catch (const std::runtime_error&) { return; }
    Require(false, message);
}

template<typename TCall>
bool TryProjection(TCall call) {
    try { call(); return true; }
    catch (const std::runtime_error&) { return false; }
}

int QueryPicture(unsigned getter, const OutputInfo* output, void* first, void* second = nullptr) {
    switch (getter) {
    case 0: return sceVideodec2GetPictureInfo_nid_postfix(output, static_cast<AvcPictureInfo*>(first), second);
    case 1: return sceVideodec2GetAvcPictureInfo_nid_postfix(output, static_cast<AvcPictureInfo*>(first), static_cast<AvcPictureInfo*>(second));
    case 2: return sceVideodec2GetHevcPictureInfo_nid_postfix(output, static_cast<Videodec2Hevc::PictureInfo*>(first));
    default: return sceVideodec2GetVp9PictureInfo_nid_postfix(output, static_cast<Vp9PictureInfo*>(first));
    }
}

void ArgumentErrors() {
    constexpr int pointerError = static_cast<int>(0x811D0102u);
    constexpr int sizeError = static_cast<int>(0x811D0101u);
    constexpr int alignmentError = static_cast<int>(0x811D0108u);
    constexpr int bufferError = static_cast<int>(0x811D0107u);
    constexpr int countError = static_cast<int>(0x811D010Fu);
    using Bytes = std::array<std::uint8_t, sizeof(Videodec2Hevc::PictureInfo) + 32>;
    const auto initialize = [](Bytes& bytes) {
        bytes.fill(0xa5);
        const std::uint64_t size = sizeof(Videodec2Hevc::PictureInfo);
        std::memcpy(bytes.data() + 16, &size, sizeof(size));
    };
    for (unsigned getter = 0; getter < 4; ++getter) {
        const bool generic = getter < 2;
        const auto query = [getter](const OutputInfo* output, void* first, void* second) {
            return QueryPicture(getter, output, first, second);
        };
        for (const std::uint32_t codec : {codecHevc, 1u, 2382845u, 0u}) {
            OutputInfo valid{sizeof(OutputInfo)};
            valid.codecType = codec;
            valid.frameBuffer = reinterpret_cast<void*>(0x100000);
            valid.frameBufferSize = 0x10000;
            valid.pictureCount = 1;
            Bytes first{}, second{};
            initialize(first);
            initialize(second);
            const auto secondBefore = second;
            Require(query(nullptr, nullptr, reinterpret_cast<void*>(1)) == pointerError, "null first must return before touching second");
            Require(query(&valid, nullptr, second.data() + 16) == pointerError && second == secondBefore, "null first mutated second");
            const auto check = [&](const OutputInfo* output, int expected) {
                initialize(first);
                initialize(second);
                auto expectedFirst = first;
                auto expectedSecond = second;
                std::memset(expectedFirst.data() + 24, 0, 4);
                if (generic) std::memset(expectedSecond.data() + 24, 0, 4);
                Require(query(output, first.data() + 16, second.data() + 16) == expected, "native picture argument result/order");
                Require(first == expectedFirst && second == expectedSecond, "native argument rejection must only clear validity words");
            };
            check(nullptr, pointerError);
            for (const std::uint64_t size : {0u, 0x28u, 0x40u}) {
                auto invalid = valid;
                invalid.thisSize = size;
                invalid.frameBuffer = reinterpret_cast<void*>(0x100001);
                invalid.pictureCount = 0;
                check(&invalid, sizeError);
                invalid.frameBuffer = nullptr;
                check(&invalid, sizeError);
            }
            for (const std::uintptr_t offset : {1u, 0x80u}) {
                auto invalid = valid;
                invalid.frameBuffer = reinterpret_cast<void*>(0x100000 + offset);
                invalid.pictureCount = 0;
                invalid.frameBufferSize = 0x400;
                check(&invalid, alignmentError);
            }
            auto invalid = valid;
            invalid.frameBuffer = nullptr;
            invalid.pictureCount = 0;
            check(&invalid, bufferError);
            for (const std::uint8_t count : {0, 3, 255}) {
                invalid = valid;
                invalid.pictureCount = count;
                invalid.frameBufferSize = 0x400;
                check(&invalid, countError);
                invalid.thisSize = 0x30;
                invalid.frameBuffer = reinterpret_cast<void*>(0x100100);
                check(&invalid, countError);
            }
            initialize(first);
            auto both = first;
            std::memset(both.data() + 24, 0, 4);
            Require(query(nullptr, first.data() + 16, first.data() + 16) == pointerError && first == both, "identical metadata destinations");
            alignas(OutputInfo) std::array<std::uint8_t, sizeof(OutputInfo) + 16> aliased{};
            std::memcpy(aliased.data() + 16, &valid, sizeof(valid));
            auto* output = reinterpret_cast<OutputInfo*>(aliased.data() + 16);
            Require(query(output, aliased.data() + 8, nullptr) == sizeError && output->thisSize == 0, "argument fields must be read after aliased validity clear");
            if (generic) {
                initialize(first);
                auto outputCopy = valid;
                auto* secondAlias = reinterpret_cast<std::uint8_t*>(&outputCopy.frameBuffer) - 8;
                Require(query(&outputCopy, first.data() + 16, secondAlias) == bufferError && outputCopy.frameBuffer == nullptr, "second early clear must precede frame-buffer validation");
            }
        }
    }
}

void ArgumentSizes() {
    constexpr int sizeError = static_cast<int>(0x811D0101u);
    constexpr int bufferError = static_cast<int>(0x811D0106u);
    using Bytes = std::array<std::uint8_t, sizeof(Videodec2Hevc::PictureInfo) + 32>;
    alignas(256) std::array<std::uint8_t, 0x1000> backing{};
    for (unsigned getter = 0; getter < 4; ++getter) {
        for (const std::uint32_t codec : {codecHevc, 1u, 2382845u, 0u}) {
            for (const std::uint64_t outputSize : {0x30u, 0x38u}) {
                OutputInfo output{outputSize};
                output.codecType = codec;
                output.frameBuffer = backing.data();
                Bytes first{}, second{};
                const auto initialize = [&](std::uint64_t firstSize, std::uint64_t secondSize) {
                    first.fill(0xa5);
                    second.fill(0x5a);
                    std::memcpy(first.data() + 16, &firstSize, sizeof(firstSize));
                    std::memcpy(second.data() + 16, &secondSize, sizeof(secondSize));
                };
                const auto check = [&](int expected, bool withSecond = true) {
                    auto expectedFirst = first;
                    auto expectedSecond = second;
                    std::memset(expectedFirst.data() + 24, 0, 4);
                    if (getter < 2 && withSecond) std::memset(expectedSecond.data() + 24, 0, 4);
                    Require(QueryPicture(getter, &output, first.data() + 16, withSecond ? second.data() + 16 : nullptr) == expected, "native buffer/version error order");
                    Require(first == expectedFirst && second == expectedSecond, "buffer/version error changed more than validity words");
                };
                for (const std::uint8_t count : {1, 2}) {
                    output.pictureCount = count;
                    const auto boundary = std::uint64_t{count} * 0x400;
                    for (const auto bytes : {std::uint64_t{0}, boundary - 1, boundary}) {
                        for (const bool withSecond : {false, true}) {
                            output.frameBufferSize = bytes;
                            initialize(0, 0x60);
                            check(bufferError, withSecond);
                        }
                    }
                    for (const auto bytes : {boundary + 1, (std::uint64_t{1} << 63) + 0x400, UINT64_MAX}) {
                        output.frameBufferSize = bytes;
                        initialize(0, 0xb8);
                        check(sizeError);
                        for (const std::uint64_t rejected : {0u, 0x38u, 0x50u, 0x60u, 0x98u, 0xb0u, 0xc8u}) {
                            initialize(rejected, 0xb8);
                            check(sizeError);
                        }
                        for (const std::uint64_t accepted : {0x58u, 0x68u, 0x78u, 0x88u, 0xa8u, 0xb8u}) {
                            initialize(accepted, 0x60);
                            if (count == 2 && getter < 2) {
                                check(sizeError);
                            } else if (bytes == boundary + 1) {
                                auto expectedFirst = first;
                                auto expectedSecond = second;
                                std::memset(expectedFirst.data() + 24, 0, 4);
                                if (getter < 2) std::memset(expectedSecond.data() + 24, 0, 4);
                                const bool completed = TryProjection([&] { QueryPicture(getter, &output, first.data() + 16, second.data() + 16); });
                                Require(std::memcmp(first.data(), expectedFirst.data(), 16) == 0 && std::memcmp(first.data() + 16 + accepted, expectedFirst.data() + 16 + accepted, first.size() - 16 - accepted) == 0, "accepted-version output crossed its declared boundary");
                                if (!completed) Require(first == expectedFirst && second == expectedSecond, "unsupported projection changed more than validity words");
                            }
                        }
                    }
                }
                if (getter < 2) {
                    output.pictureCount = 1;
                    output.frameBufferSize = 0x401;
                    initialize(0xb8, 0xb8);
                    auto expected = first;
                    std::memset(expected.data() + 16, 0, 4);
                    std::memset(expected.data() + 24, 0, 4);
                    Require(QueryPicture(getter, &output, first.data() + 16, first.data() + 8) == sizeError && first == expected, "second alias must be cleared before destination version read");
                    initialize(0xb8, 0xb8);
                    output.frameBufferSize = 0x401;
                    auto* sizeAlias = reinterpret_cast<std::uint8_t*>(&output.frameBufferSize) - 8;
                    Require(QueryPicture(getter, &output, first.data() + 16, sizeAlias) == bufferError && output.frameBufferSize == 0, "buffer-size gate must read after secondary validity clear");
                    output.pictureCount = 2;
                    output.frameBufferSize = 0x801;
                    for (const std::uint64_t secondSize : {0x58u, 0x68u, 0x78u, 0x88u, 0xa8u, 0xb8u}) {
                        initialize(0xb8, secondSize);
                        auto beforeFirst = first;
                        auto beforeSecond = second;
                        const bool completed = TryProjection([&] { QueryPicture(getter, &output, first.data() + 16, second.data() + 16); });
                        Require(std::memcmp(first.data(), beforeFirst.data(), 16) == 0 && std::memcmp(first.data() + 16 + 0xb8, beforeFirst.data() + 16 + 0xb8, 16) == 0, "two-picture primary output canary");
                        Require(std::memcmp(second.data(), beforeSecond.data(), 16) == 0 && std::memcmp(second.data() + 16 + secondSize, beforeSecond.data() + 16 + secondSize, second.size() - 16 - secondSize) == 0, "two-picture secondary output canary");
                        if (!completed) {
                            std::memset(beforeFirst.data() + 24, 0, 4);
                            std::memset(beforeSecond.data() + 24, 0, 4);
                            Require(first == beforeFirst && second == beforeSecond, "unsupported two-picture projection changed more than validity words");
                        }
                    }
                    initialize(0xb8, 0);
                    TryProjection([&] { QueryPicture(getter, &output, first.data() + 16); });
                }
            }
        }
    }
}

struct Decoder {
    std::uint64_t handle = 0;
    DecoderMemoryInfo memory{sizeof(memory)};
    std::vector<std::uint8_t> storage;
    std::span<std::uint8_t> buffer;
    FrameBuffer frame{sizeof(frame)};
    OutputInfo output{sizeof(output)};

    explicit Decoder(unsigned profile) {
        const DecoderConfigInfo config{sizeof(config), 1, codecHevc, profile, 0, 64, 48, 4, 1};
        Require(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "query memory");
        Require(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "create decoder");
        buffer = HevcFixture::AlignedFrameStorage(storage, memory.maxFrameBufferSize, memory.frameBufferAlignment);
        frame.frameBuffer = buffer.data();
        frame.frameBufferSize = buffer.size();
    }

    ~Decoder() { if (handle) sceVideodec2DeleteDecoder_nid_postfix(handle); }

    void Decode(const std::vector<std::uint8_t>& bytes, std::uint64_t tag) {
        const InputData input{sizeof(input), bytes.data(), bytes.size(), 1000 + tag, 2000 + tag, 3000 + tag};
        Require(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode unit");
    }

    Videodec2Hevc::PictureInfo Query() {
        struct Guarded {
            Videodec2Hevc::PictureInfo info{sizeof(info)};
            std::uint64_t guard = 0x123456789abcdef0;
        } result;
        Require(sceVideodec2GetHevcPictureInfo_nid_postfix(&output, &result.info) == 0, "query HEVC info");
        Require(result.info.thisSize == sizeof(result.info) && result.info.isValid, "valid HEVC picture");
        Require(result.guard == 0x123456789abcdef0, "HEVC info overwrote adjacent memory");
        Videodec2Hevc::PictureInfo generic{sizeof(generic)};
        Require(sceVideodec2GetPictureInfo_nid_postfix(&output, reinterpret_cast<AvcPictureInfo*>(&generic), nullptr) == 0, "generic HEVC query");
        Require(generic.ptsData == result.info.ptsData && generic.dtsData == result.info.dtsData && generic.attachedData == result.info.attachedData && generic.width == result.info.width && generic.timeScale == result.info.timeScale && generic.spsPresent == result.info.spsPresent, "generic and dedicated info disagree");
        return result.info;
    }
};

struct GuestMetadataMapping {
    static constexpr std::size_t block = 0x10000;
    unsigned char* memory = nullptr;
    bool watched = false;
#ifdef _WIN32
    HANDLE section = nullptr;
#endif

    GuestMetadataMapping() {
        Require(GuestArena::GuestArenaAvailable_nid_postfix(), "guest arena unavailable");
        memory = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(2 * block, block));
        Require(memory && GuestArena::GuestArenaContains_nid_postfix(memory, 2 * block), "allocate guest metadata mapping");
#ifdef _WIN32
        section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, 2 * block, nullptr);
        Require(section != nullptr, "create guest metadata section");
        GuestArena::GuestArenaMap_nid_postfix(memory, 2 * block, section, 0, PAGE_READWRITE);
#else
        GuestArena::GuestArenaCommit_nid_postfix(memory, 2 * block, 0x04, block);
#endif
        watched = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
        if (watched) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory, 2 * block);
    }

    ~GuestMetadataMapping() {
        if (watched) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(memory, 2 * block);
        Protect(memory, 2 * block, 0x04);
        GuestArena::GuestArenaReset_nid_postfix(memory, 2 * block);
        GuestArena::GuestArenaRelease_nid_postfix(memory, 2 * block);
#ifdef _WIN32
        GuestArena::GuestArenaForgetSection_nid_postfix(section);
        Require(CloseHandle(section), "close guest metadata section");
#endif
    }

    Videodec2Hevc::PictureInfo* Info() const {
        return reinterpret_cast<Videodec2Hevc::PictureInfo*>(memory + block - 96);
    }

    void Protect(void* pointer, std::size_t bytes, std::uint32_t protection) {
#ifdef _WIN32
        auto cursor = reinterpret_cast<std::uintptr_t>(pointer);
        const auto end = cursor + bytes;
        while (cursor < end) {
            MEMORY_BASIC_INFORMATION region{};
            Require(VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region)) == sizeof(region), "query guest metadata section");
            const auto regionEnd = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
            Require(region.State == MEM_COMMIT && regionEnd > cursor, "guest metadata section is not committed");
            const auto span = std::min(end - cursor, regionEnd - cursor);
            DWORD previous = 0;
            Require(VirtualProtect(reinterpret_cast<void*>(cursor), span, protection, &previous), "protect guest metadata section");
            cursor += span;
        }
#endif
        GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(pointer), bytes, protection);
    }

    void Initialize() {
        Protect(memory, 2 * block, 0x04);
        GuestArena::HostWrite access(memory, 2 * block);
        Require(access.Open(), "initialize guest metadata access");
        std::memset(memory, 0xa7, 2 * block);
        Info()->thisSize = sizeof(*Info());
        Info()->isValid = true;
    }

    std::size_t Collect() const {
        std::size_t writes = 0;
        if (watched) Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(memory), 2 * block,
            [](void* context, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::size_t*>(context) += end - begin; }, &writes), "collect guest metadata writes");
        return writes;
    }
};

void GuestArgumentWrites() {
    GuestMetadataMapping mapping;
    auto* second = reinterpret_cast<AvcPictureInfo*>(mapping.memory + GuestMetadataMapping::block + 32);
    constexpr int pointerError = static_cast<int>(0x811D0102u);
    for (unsigned getter = 0; getter < 4; ++getter) {
        const bool generic = getter < 2;
        const auto query = [&] {
            return QueryPicture(getter, nullptr, mapping.Info(), second);
        };
        mapping.Initialize();
        auto expected = std::vector<std::uint8_t>(mapping.memory, mapping.memory + 2 * GuestMetadataMapping::block);
        std::memset(expected.data() + GuestMetadataMapping::block - 96 + 8, 0, 4);
        if (generic) std::memset(expected.data() + GuestMetadataMapping::block + 32 + 8, 0, 4);
        mapping.Collect();
        Require(query() == pointerError, "guest-mapped argument error result");
        Require(std::memcmp(mapping.memory, expected.data(), expected.size()) == 0, "guest-mapped argument prefix changed more than validity words");
        if (mapping.watched) Require(mapping.Collect() != 0, "guest argument prefix write not reported");
#ifdef _WIN32
        for (const std::size_t block : {0u, 1u}) {
            if (!generic && block == 1) continue;
            mapping.Initialize();
            expected.assign(mapping.memory, mapping.memory + 2 * GuestMetadataMapping::block);
            mapping.Protect(mapping.memory + block * GuestMetadataMapping::block, GuestMetadataMapping::block, 0x02);
            Rejects(query, "read-only early argument destination did not refuse");
            Require(std::memcmp(mapping.memory, expected.data(), expected.size()) == 0, "rejected early scope partially cleared either destination");
        }
#endif
    }
}

void SecondaryVersionNoRead() {
    GuestMetadataMapping mapping;
    auto* second = mapping.memory + GuestMetadataMapping::block - 8;
    OutputInfo output{sizeof(output)};
    alignas(256) std::array<std::uint8_t, 0x1000> backing{};
    output.frameBuffer = backing.data();
    output.frameBufferSize = 0x401;
    output.pictureCount = 1;
    for (unsigned getter = 0; getter < 2; ++getter) {
        mapping.Initialize();
        Videodec2Hevc::PictureInfo info{sizeof(info)};
        auto expected = std::vector<std::uint8_t>(mapping.memory, mapping.memory + 2 * GuestMetadataMapping::block);
        std::memset(expected.data() + GuestMetadataMapping::block, 0, 4);
        mapping.Protect(mapping.memory, GuestMetadataMapping::block, 0x01);
        TryProjection([&] { QueryPicture(getter, &output, &info, second); });
        mapping.Protect(mapping.memory, GuestMetadataMapping::block, 0x04);
        Require(std::memcmp(mapping.memory, expected.data(), expected.size()) == 0, "count-one secondary version was accessed beyond validity word");
    }
}

void GuestMetadataWrites() {
    Decoder decoder(1);
    decoder.Decode(HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size()).front(), 91);
    if (!decoder.output.isValid) Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0, "flush mapped metadata fixture");
    Require(decoder.output.isValid, "mapped metadata fixture produced no picture");
    const auto expected = decoder.Query();
    GuestMetadataMapping mapping;
    for (unsigned getter = 0; getter < 4; ++getter) {
        for (const bool owned : {false, true}) {
            auto output = decoder.output;
            if (!owned) output.frameBuffer = nullptr;
            const auto query = [&] {
                return QueryPicture(getter, &output, mapping.Info());
            };
            mapping.Initialize();
            auto* first = reinterpret_cast<unsigned char*>(mapping.Info()) - 16;
            std::array<unsigned char, sizeof(Videodec2Hevc::PictureInfo) + 32> initial{};
            std::memcpy(initial.data(), first, initial.size());
            mapping.Collect();
            Require(query() == (owned ? 0 : static_cast<int>(0x811D0107u)), "guest-mapped metadata query");
            if (owned) {
                Require(mapping.Info()->isValid && mapping.Info()->thisSize == sizeof(expected) &&
                    mapping.Info()->ptsData == expected.ptsData && mapping.Info()->attachedData == expected.attachedData &&
                    mapping.Info()->width == expected.width && mapping.Info()->profile == expected.profile, "guest-mapped metadata value");
                Require(std::memcmp(first, initial.data(), 16) == 0 &&
                    std::memcmp(first + 16 + sizeof(expected), initial.data() + 16 + sizeof(expected), 16) == 0, "guest metadata canaries");
            } else {
                std::memset(initial.data() + 16 + offsetof(Videodec2Hevc::PictureInfo, isValid), 0, 4);
                Require(std::memcmp(first, initial.data(), initial.size()) == 0, "invalid-picture mapping changes only validity word");
            }
            if (mapping.watched) Require(mapping.Collect() != 0, "guest metadata write not reported");
#ifdef _WIN32
            mapping.Initialize();
            std::memcpy(initial.data(), first, initial.size());
            const auto refused = mapping.memory + (owned ? GuestMetadataMapping::block : 0);
            mapping.Protect(refused, GuestMetadataMapping::block, 0x02);
            {
                GuestArena::HostWrite access(owned ? static_cast<void*>(mapping.Info()) : static_cast<void*>(&mapping.Info()->isValid),
                    owned ? sizeof(*mapping.Info()) : sizeof(std::uint32_t));
                Require(!access.Open(), "read-only metadata mapping unexpectedly writable");
            }
            Rejects(query, "read-only metadata query did not reject access");
            if (owned) std::memset(initial.data() + 16 + offsetof(Videodec2Hevc::PictureInfo, isValid), 0, 4);
            Require(std::memcmp(first, initial.data(), initial.size()) == 0, "rejected payload write changed more than its early validity word");
#endif
        }
    }
}

void CheckProjectionWrites(Decoder& decoder);

void CheckCommon(const Videodec2Hevc::PictureInfo& info, unsigned bitDepth) {
    Require(info.width == 64 && info.height == 40, "coded luma dimensions");
    Require(info.bitDepthLumaMinus8 == bitDepth && info.bitDepthChromaMinus8 == bitDepth, "source bit depths");
    Require(info.timingInfoPresent == 1 && info.numUnitsInTick == 1 && info.timeScale == 30, "VUI timing syntax");
    Require(info.aspectRatioInfoPresent == 1 && info.aspectRatioIdc == 1 && info.sarWidth == 1 && info.sarHeight == 1, "VUI sample aspect ratio");
    Require(info.videoSignalTypePresent == 1 && info.videoFormat == 5 && info.fullRange == 0 && info.colourDescriptionPresent == 0 && info.colourPrimaries == 2 && info.transferCharacteristics == 2 && info.matrixCoefficients == 2, "signalled video format and inferred colours");
    Require(info.frameFieldInfoPresent == 0 && info.picStruct == 0 && info.sourceScanType == 1 && info.duplicate == 0, "PTL progressive scan inference");
    Require(info.defaultDisplayWindow == 0 && info.displayLeft == 0 && info.displayRight == 0 && info.displayTop == 0 && info.displayBottom == 0, "default display window inference");
    Require(info.chromaLocInfoPresent == 0 && info.chromaLocTop == 0 && info.chromaLocBottom == 0 && info.fieldSequence == 0, "inferred chroma location and field sequence");
    Require(info.profileSpace == 0 && info.tier == 0 && info.profile == (bitDepth == 0 ? 1 : 2) && info.level == 30 && info.progressiveSource == 1 && info.interlacedSource == 0 && info.frameOnlyConstraint == 1, "general profile-tier-level syntax");
    Require(info.subLayerProfilePresent == 0 && info.subLayerLevelPresent == 0 && info.subLayerProfileSpace == 0 && info.subLayerTier == 0 && info.subLayerProfile == 0 && info.subLayerLevel == 0 && info.subLayerOrderingPresent == 1 && info.maxDecPicBufferingMinus1 == 4, "single-layer PTL and DPB syntax");
    Require(info.cropping == 0 && info.cropLeft == 0 && info.cropRight == 0 && info.cropTop == 0 && info.cropBottom == 0, "absent tail cropping fields");
}

void Clips(bool alternativeTransfer = false) {
    Decoder decoder(1);
    std::size_t pictures = 0;
    const auto take = [&] {
        if (!decoder.output.isValid) return;
        const auto info = decoder.Query();
        CheckCommon(info, 0);
        constexpr std::array<unsigned, 6> order{0, 3, 2, 1, 5, 4};
        Require(pictures < order.size(), "too many pictures");
        const auto tag = order[pictures];
        Require(info.ptsData == 1000 + tag && info.dtsData == 2000 + tag && info.attachedData == 3000 + tag, "per-picture access-unit association through B-frame reordering");
        Require(info.vpsPresent == (pictures == 0) && info.spsPresent == (pictures == 0) && info.ppsPresent == (pictures == 0) && info.delimiterPresent == 1, "presence is from the emitted picture's unit");
        Require(info.conformanceWindow == 0 && info.confLeft == 0 && info.confRight == 0 && info.confTop == 0 && info.confBottom == 0, "no conformance window");
        Require(info.idr == (pictures == 0) && info.irap == (pictures == 0) && info.fillerPresent == 0, "per-picture IDR/IRAP and filler flags");
        Require(info.alternativeTransferPresent == (alternativeTransfer && pictures == 0) && info.preferredTransfer == (alternativeTransfer ? 16 : 0), "alternative transfer persists through B-frame reordering");
        ++pictures;
    };
    auto units = HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size());
    if (alternativeTransfer) {
        const std::array<std::uint8_t, 9> sei{0, 0, 1, 0x4e, 1, 147, 1, 16, 0x80};
        units[0].insert(units[0].begin() + 7, sei.begin(), sei.end());
    }
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        decoder.Decode(units[unit], unit);
        take();
    }
    for (std::size_t drain = 0; drain <= units.size(); ++drain) {
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0, "drain clip");
        if (!decoder.output.isValid) break;
        take();
    }
    Require(pictures == 6, "decoded all six picture metadata records");
    Videodec2Hevc::PictureInfo invalid{sizeof(invalid)};
    invalid.isValid = true;
    Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "reset");
    Require(sceVideodec2GetHevcPictureInfo_nid_postfix(&decoder.output, &invalid) == static_cast<int>(0x811D010Fu) && !invalid.isValid, "reset output has no picture count");
    invalid.thisSize = 8;
    invalid.isValid = true;
    Require(sceVideodec2GetPictureInfo_nid_postfix(&decoder.output, reinterpret_cast<AvcPictureInfo*>(&invalid), nullptr) == static_cast<int>(0x811D010Fu) && !invalid.isValid, "picture-count error must precede destination size");
    decoder.Decode(units.front(), 99);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "decode after reset");
    Require(decoder.Query().attachedData == 3099, "new metadata after reset");
}

void Single(std::span<const std::uint8_t> stream, unsigned bitDepth, unsigned bottomCrop, bool delimiter) {
    Decoder decoder(bitDepth == 0 ? 1 : 2);
    decoder.Decode({stream.begin(), stream.end()}, 7);
    if (!decoder.output.isValid) Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0, "single-picture drain");
    if (bottomCrop != 0) {
        Require(decoder.output.isValid, "cropped picture remains decodable");
        CheckProjectionWrites(decoder);
        return;
    }
    const auto info = decoder.Query();
    CheckCommon(info, bitDepth);
    Require(info.attachedData == 3007 && info.vpsPresent == 1 && info.spsPresent == 1 && info.ppsPresent == 1 && info.delimiterPresent == delimiter, "single-picture tags and presence");
    Require(info.idr == 1 && info.irap == 1 && info.preferredTransfer == 0 && info.pictureTimingPresent == 0, "single-picture tail metadata");
    Require(info.conformanceWindow == (bottomCrop != 0) && info.confBottom == bottomCrop && info.confLeft == 0 && info.confRight == 0 && info.confTop == 0, "raw chroma-unit conformance offsets");
    for (unsigned getter = 0; getter < 4; ++getter) {
        alignas(8) std::array<std::uint8_t, sizeof(info) + 32> unaligned;
        unaligned.fill(0xa5);
        const std::uint64_t nativeSize = sizeof(info);
        std::memcpy(unaligned.data() + 17, &nativeSize, sizeof(nativeSize));
        const auto guarded = unaligned;
        Require(QueryPicture(getter, &decoder.output, unaligned.data() + 17) == 0, "unaligned metadata destination");
        Videodec2Hevc::PictureInfo aligned{};
        std::memcpy(&aligned, unaligned.data() + 17, sizeof(aligned));
        Require(aligned.isValid && aligned.ptsData == info.ptsData && aligned.width == info.width, "unaligned metadata values");
        Require(std::memcmp(unaligned.data(), guarded.data(), 17) == 0 && std::memcmp(unaligned.data() + 17 + sizeof(info), guarded.data() + 17 + sizeof(info), 15) == 0, "unaligned metadata canaries");
        for (const std::uint32_t codec : {codecHevc, 1u, 2382845u, 0u}) {
            for (const std::uint64_t size : {0x30u, 0x38u}) {
                auto output = decoder.output;
                output.codecType = codec;
                output.thisSize = size;
                Videodec2Hevc::PictureInfo projected{sizeof(projected)};
                std::array<std::uint8_t, sizeof(projected)> second;
                second.fill(0xa5);
                const std::uint64_t secondSize = 0x60;
                std::memcpy(second.data(), &secondSize, sizeof(secondSize));
                auto expected = second;
                if (getter < 2) std::memset(expected.data() + 8, 0, 4);
                Require(QueryPicture(getter, &output, &projected, second.data()) == 0, "all named wrappers must project owned HEVC metadata after native validation");
                Require(projected.thisSize == sizeof(projected) && projected.isValid && projected.ptsData == info.ptsData && projected.width == info.width, "mutable codec tag changed tracked metadata projection");
                Require(second == expected, "count-one secondary output must only clear its validity word");
            }
        }
    }
}

void UnsupportedPicture() {
    Decoder decoder(1);
    auto units = HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size());
    const std::array<std::uint8_t, 9> unknownSei{0, 0, 1, 0x4e, 1, 200, 1, 0x42, 0x80};
    units[0].insert(units[0].begin() + 7, unknownSei.begin(), unknownSei.end());
    decoder.Decode(units[0], 0);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "unsupported metadata retains decoded output");
    Videodec2Hevc::PictureInfo untouched;
    std::memset(&untouched, 0x5a, sizeof(untouched));
    untouched.thisSize = sizeof(untouched);
    std::array<std::uint8_t, sizeof(untouched)> before;
    std::memcpy(before.data(), &untouched, before.size());
    std::memset(before.data() + 8, 0, 4);
    const bool typedCompleted = TryProjection([&] { sceVideodec2GetHevcPictureInfo_nid_postfix(&decoder.output, &untouched); });
    if (!typedCompleted) Require(std::memcmp(before.data(), &untouched, before.size()) == 0, "unsupported query changed more than validity word");
    std::memset(&untouched, 0x5a, sizeof(untouched));
    untouched.thisSize = sizeof(untouched);
    const bool genericCompleted = TryProjection([&] { sceVideodec2GetPictureInfo_nid_postfix(&decoder.output, reinterpret_cast<AvcPictureInfo*>(&untouched), nullptr); });
    if (!genericCompleted) Require(std::memcmp(before.data(), &untouched, before.size()) == 0, "generic unsupported query changed more than validity word");
    Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "clear unsupported metadata on reset");
    const auto clean = HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size());
    decoder.Decode(clean[0], 80);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "clean replacement picture");
    Require(decoder.Query().ptsData == 1080, "replacement metadata does not retain unsupported reason");
    const auto previous = decoder.output;
    Require(sceVideodec2DeleteDecoder_nid_postfix(decoder.handle) == 0, "delete owner");
    decoder.handle = 0;
    untouched = {sizeof(untouched)};
    untouched.isValid = true;
    const bool completed = TryProjection([&] { sceVideodec2GetHevcPictureInfo_nid_postfix(&previous, &untouched); });
    if (!completed) Require(!untouched.isValid, "unsupported untracked metadata retained validity");
}

std::vector<std::uint8_t> Replacement(unsigned wanted) {
    const auto& original = HevcFixture::HevcMain10Stream;
    std::vector<std::size_t> starts;
    for (std::size_t index = 0; index + 2 < original.size(); ++index)
        if (original[index] == 0 && original[index + 1] == 0 && original[index + 2] == 1) starts.push_back(index + 3);
    for (std::size_t index = 0; index < starts.size(); ++index) {
        const auto header = starts[index];
        if (((original[header] >> 1) & 63) != wanted) continue;
        const auto end = index + 1 == starts.size() ? original.size() : starts[index + 1] - 3;
        std::vector<bool> bits;
        unsigned zeros = 0;
        for (std::size_t byte = header + 2; byte < end; ++byte) {
            const auto value = original[byte];
            if (zeros == 2 && value == 3) { zeros = 0; continue; }
            for (int bit = 7; bit >= 0; --bit) bits.push_back(((value >> bit) & 1) != 0);
            zeros = value == 0 ? zeros + 1 : 0;
        }
        while (!bits.empty() && !bits.back()) bits.pop_back();
        Require(bits.size() >= 3, "parameter-set fixture trailing bits");
        bits.pop_back();
        Require(!bits.back(), "fixture extension flag is zero");
        if (wanted == 32) {
            bits.pop_back();
            Require(!bits.back(), "fixture VPS timing flag is zero");
            bits.pop_back();
            bits.push_back(true);
            for (const unsigned value : std::array{1u, 30u})
                for (int bit = 31; bit >= 0; --bit) bits.push_back(((value >> bit) & 1) != 0);
            bits.insert(bits.end(), {false, true, false});
        } else {
            bits.back() = true;
            bits.insert(bits.end(), 8, false);
        }
        bits.push_back(true);
        while (bits.size() % 8 != 0) bits.push_back(false);
        std::vector<std::uint8_t> encoded;
        zeros = 0;
        for (std::size_t byte = 0; byte < bits.size(); byte += 8) {
            std::uint8_t value = 0;
            for (unsigned bit = 0; bit < 8; ++bit) value = static_cast<std::uint8_t>((value << 1) | bits[byte + bit]);
            if (zeros == 2 && value <= 3) { encoded.push_back(3); zeros = 0; }
            encoded.push_back(value);
            zeros = value == 0 ? zeros + 1 : 0;
        }
        std::vector<std::uint8_t> result(original.begin(), original.begin() + header + 2);
        result.insert(result.end(), encoded.begin(), encoded.end());
        result.insert(result.end(), original.begin() + end, original.end());
        return result;
    }
    Require(false, "fixture parameter set not found");
    return {};
}

void CheckProjectionWrites(Decoder& decoder) {
    Videodec2Hevc::PictureInfo info;
    std::memset(&info, 0x6b, sizeof(info));
    info.thisSize = sizeof(info);
    std::array<std::uint8_t, sizeof(info)> before;
    std::memcpy(before.data(), &info, sizeof(info));
    std::memset(before.data() + 8, 0, 4);
    const bool completed = TryProjection([&] { sceVideodec2GetHevcPictureInfo_nid_postfix(&decoder.output, &info); });
    if (!completed) Require(std::memcmp(before.data(), &info, sizeof(info)) == 0, "ambiguous replacement query changed more than validity word");
}

void ParameterReplacements() {
    const std::vector<std::uint8_t> original(HevcFixture::HevcMain10Stream.begin(), HevcFixture::HevcMain10Stream.end());
    for (const unsigned type : std::array{0u, 32u, 33u, 34u}) {
        Decoder decoder(2);
        decoder.Decode(original, 10);
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "initial replacement-test picture");
        Require(decoder.Query().attachedData == 3010, "initial parameter metadata");
        const auto previous = decoder.output;
        std::vector<std::uint8_t> nextStorage;
        const auto nextBuffer = HevcFixture::AlignedFrameStorage(nextStorage, decoder.buffer.size(), decoder.memory.frameBufferAlignment);
        decoder.frame.frameBuffer = nextBuffer.data();
        decoder.Decode(type == 0 ? original : Replacement(type), 20);
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "decode repeated or replaced parameter sets");
        if (type == 0) Require(decoder.Query().attachedData == 3020, "identical repeated headers remain supported");
        else CheckProjectionWrites(decoder);
        Videodec2Hevc::PictureInfo old{sizeof(old)};
        Require(sceVideodec2GetHevcPictureInfo_nid_postfix(&previous, &old) == 0 && old.isValid && old.attachedData == 3010, "previous picture metadata remains immutable after parameter updates");
        if (type != 0) {
            Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "reset after replacement");
            decoder.Decode(original, 30);
            Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "decode after replacement reset");
            CheckProjectionWrites(decoder);
        }
    }
    Decoder recreated(2);
    recreated.Decode(original, 40);
    Require(sceVideodec2Flush_nid_postfix(recreated.handle, &recreated.frame, &recreated.output) == 0 && recreated.output.isValid, "recreated backend picture");
    Require(recreated.Query().attachedData == 3040, "new backend context recovers supported metadata");
}

void ExtendedMetadata() {
    for (const bool interlaced : {false, true}) {
        const auto bytes = interlaced ? std::span<const std::uint8_t>(HevcMetadataFixture::Interlaced) : std::span<const std::uint8_t>(HevcMetadataFixture::Rich);
        Decoder decoder(1);
        decoder.Decode({bytes.begin(), bytes.end()}, 50);
        if (!decoder.output.isValid) Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0, "extended metadata flush");
        const auto info = decoder.Query();
        Require(info.width == 64 && info.height == 48 && info.numUnitsInTick == 1 && info.timeScale == 24, "extended dimensions and timing");
        if (interlaced) {
            Require(info.fieldSequence == 1 && info.frameFieldInfoPresent == 1 && info.picStruct == 1 && info.sourceScanType == 0 && info.duplicate == 0, "picture-timing SEI and field metadata");
            Require(info.pictureTimingPresent == 1 && info.progressiveSource == 0 && info.interlacedSource == 1 && info.frameOnlyConstraint == 0, "interlaced PTL and SEI presence tail");
        } else {
            Require(info.aspectRatioIdc == 2 && info.sarWidth == 12 && info.sarHeight == 11, "table-derived non-square sample aspect");
            Require(info.videoSignalTypePresent == 1 && info.videoFormat == 1 && info.fullRange == 1 && info.colourDescriptionPresent == 1 && info.colourPrimaries == 9 && info.transferCharacteristics == 16 && info.matrixCoefficients == 9, "explicit HDR color syntax");
            Require(info.chromaLocInfoPresent == 1 && info.chromaLocTop == 3 && info.chromaLocBottom == 3, "explicit chroma sample locations");
            Require(info.defaultDisplayWindow == 0 && info.cropping == 0, "unwindowed rich picture");
        }
        Require(info.profile == 4 && info.level == 30 && info.subLayerOrderingPresent == 1 && info.maxDecPicBufferingMinus1 == 2 && info.idr == 1 && info.irap == 1, "extended PTL and DPB tail");
    }
    Decoder windowed(1);
    windowed.Decode({HevcMetadataFixture::Windowed.begin(), HevcMetadataFixture::Windowed.end()}, 51);
    if (!windowed.output.isValid) Require(sceVideodec2Flush_nid_postfix(windowed.handle, &windowed.frame, &windowed.output) == 0, "windowed picture flush");
    Require(windowed.output.isValid, "windowed picture remains decodable");
    CheckProjectionWrites(windowed);
}

void CabacPadding() {
    Decoder decoder(2);
    std::vector<std::uint8_t> padded(HevcFixture::HevcMain10Stream.begin(), HevcFixture::HevcMain10Stream.end());
    padded.insert(padded.end(), {0, 0, 3});
    decoder.Decode(padded, 60);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "terminal CABAC prevention byte decode");
    Require(decoder.Query().attachedData == 3060, "terminal CABAC prevention byte metadata");
    Videodec2Hevc::State state;
    padded.back() = 2;
    const auto invalid = Videodec2Hevc::Parse_nid_no_patch(state, padded);
    Require(!invalid.unsupported.empty(), "missing emulation prevention was accepted");
}

void MalformedParameterHistory() {
    const std::vector<std::uint8_t> original(HevcFixture::HevcMain10Stream.begin(), HevcFixture::HevcMain10Stream.end());
    Decoder decoder(2);
    decoder.Decode(original, 70);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "initial malformed-history picture");
    Require(decoder.Query().attachedData == 3070, "initial malformed-history metadata");
    std::vector<std::uint8_t> malformed{0, 0, 1, 0x44, 1, 0x80};
    malformed.insert(malformed.end(), original.begin(), original.end());
    bool failed = false;
    try { decoder.Decode(malformed, 71); }
    catch (const std::runtime_error&) { failed = true; }
    if (!failed) {
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "malformed parameter output");
        CheckProjectionWrites(decoder);
    }
    Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "reset malformed parameter state");
    decoder.Decode(original, 72);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "clean decode after malformed parameter reset");
    CheckProjectionWrites(decoder);
}

void TransferAssociation() {
    const auto original = HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size());
    const std::array<std::uint8_t, 9> transfer{0, 0, 1, 0x4e, 1, 147, 1, 16, 0x80};
    {
        Decoder decoder(1);
        auto units = original;
        units[0].insert(units[0].begin() + 7, transfer.begin(), transfer.end());
        auto conflict = transfer;
        conflict[7] = 18;
        units[0].insert(units[0].begin() + 7, conflict.begin(), conflict.end());
        unsigned pictures = 0;
        for (std::size_t index = 0; index < units.size(); ++index) {
            decoder.Decode(units[index], index);
            if (decoder.output.isValid) { CheckProjectionWrites(decoder); ++pictures; }
        }
        for (;;) {
            Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0, "conflicting transfer drain");
            if (!decoder.output.isValid) break;
            CheckProjectionWrites(decoder);
            ++pictures;
        }
        Require(pictures == 6, "conflicting transfers retain all decoded pictures");
        Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "reset conflicting transfer sequence");
        decoder.Decode(original[0], 90);
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "new IRAP transfer sequence");
        Require(decoder.Query().preferredTransfer == 0, "new CLVS clears prior transfer ambiguity");
    }
    {
        Decoder decoder(1);
        try { decoder.Decode({transfer.begin(), transfer.end()}, 91); }
        catch (const std::runtime_error&) {}
        Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "reset pictureless transfer packet");
        decoder.Decode(original[0], 92);
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "decode after pictureless transfer");
        CheckProjectionWrites(decoder);
    }
}

}

int main(int argc, char** argv) {
    ArgumentErrors();
    ArgumentSizes();
    GuestArgumentWrites();
    SecondaryVersionNoRead();
    if (argc == 2 && std::strcmp(argv[1], "--arguments") == 0) return 0;
    GuestMetadataWrites();
    Clips();
    Clips(true);
    Single(HevcFixture::HevcMain10Stream, 2, 0, true);
    Single(HevcFixture::HevcCroppedStream, 0, 2, false);
    UnsupportedPicture();
    ParameterReplacements();
    ExtendedMetadata();
    CabacPadding();
    MalformedParameterHistory();
    TransferAssociation();
}
