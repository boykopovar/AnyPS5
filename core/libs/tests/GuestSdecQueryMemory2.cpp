#include "prx/libSceVdecSavc2/libSceVdecSavc2.hpp"
#include "ProcessParametersFixture.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int Invalid = 3;
constexpr int MemoryMismatch = 4;
constexpr std::uint32_t Untouched = 0xA5A5A5A5u;
constexpr std::uint8_t UntouchedByte = 0x5Au;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Sdec query memory 2: %s\n", message);
    std::abort();
}

template <typename Fn>
bool ThrowsRuntimeError(Fn&& fn) {
    try {
        fn();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

void SetSdkVersion(std::array<std::byte, 64>& process, std::int32_t version) {
    std::memcpy(process.data() + 0x10, &version, sizeof(version));
}

SdecConfig2 Config(std::uint32_t level, std::int32_t width, std::int32_t height, std::int32_t frameCount, std::uint32_t fieldMode = 1) {
    SdecConfig2 config{};
    config.level = level;
    config.frameCount = frameCount;
    config.widthInMbs = width;
    config.heightInMbs = height;
    config.depthLuma = 8;
    config.depthChroma = 8;
    config.fieldMode = fieldMode;
    return config;
}

SdecRecord QueryRecord(std::uint32_t instanceMode, std::uint32_t instanceCount, std::int32_t extraFrames) {
    SdecRecord record{};
    record.instanceMode = instanceMode;
    record.unknown8 = instanceCount;
    record.extraFrames = extraFrames;
    return record;
}

SdecRecord2 CreateRecord(std::uint32_t instanceMode, std::uint32_t instanceCount, std::int32_t extraFrames) {
    SdecRecord2 record{};
    record.instanceMode = instanceMode;
    record.instanceCount = instanceCount;
    record.extraFrames = extraFrames;
    record.unknown10 = 1;
    record.unknown18 = -1;
    record.unknown20 = 0x7fu;
    return record;
}

SdecMemory FilledMemory() {
    SdecMemory memory{};
    std::memset(&memory, 0xA5, sizeof(memory));
    return memory;
}

bool IsFilled(const SdecMemory& memory) {
    const SdecMemory filled = FilledMemory();
    return std::memcmp(&memory, &filled, sizeof(memory)) == 0;
}

bool IsAllZero(const std::byte* data, std::size_t size) {
    return std::all_of(data, data + size, [](std::byte b) { return b == std::byte{0}; });
}

void CheckQuerySizes(std::array<std::byte, 64>& process) {
    SetSdkVersion(process, 0x4000000);
    SdecMemory memory = FilledMemory();
    std::uint32_t dpbFrames = Untouched;
    std::uint8_t tooBig = UntouchedByte;
    const SdecConfig2 baseConfig = Config(51, 16, 16, 1, 1);
    const SdecRecord baseRecord = QueryRecord(0, 0, 0);
    Require(sceSdecQueryMemorySw2(&baseConfig, &baseRecord, 1, &memory, &dpbFrames, 0, &tooBig) == 0, "16x16 query failed");
    Require(dpbFrames == 1, "wrong explicit DPB frames");
    Require(tooBig == UntouchedByte, "accepted query modified tooBig");
    Require(memory.blocks[0].base == 0 && memory.blocks[0].size == 7846968 && memory.blocks[0].alignment == 0 && memory.blocks[0].reserved == 0, "wrong 16x16 SDK 4 first size");
    Require(memory.blocks[1].base == 0 && memory.blocks[1].size == 1156864 && memory.blocks[1].alignment == 256 && memory.blocks[1].reserved == 0, "wrong 16x16 SDK 4 second size");
    Require(memory.blocks[2].base == 0 && memory.blocks[2].size == 396544 && memory.blocks[2].alignment == 256 && memory.blocks[2].reserved == 0, "wrong 16x16 third size");

    SetSdkVersion(process, 0x64fffff);
    SdecMemory boundaryBefore = FilledMemory();
    Require(sceSdecQueryMemorySw2(&baseConfig, &baseRecord, 2, &boundaryBefore, &dpbFrames, 0, nullptr) == 0, "SDK 6.49 query failed");
    Require(boundaryBefore.blocks[0].size == 7846968, "SDK 6.49 must use pre-6.50 first size");

    SetSdkVersion(process, 0x6500000);
    SdecMemory boundaryAt = FilledMemory();
    Require(sceSdecQueryMemorySw2(&baseConfig, &baseRecord, -1, &boundaryAt, &dpbFrames, 0, nullptr) == 0, "SDK 6.50 query failed");
    Require(boundaryAt.blocks[0].size == 7845240, "wrong 16x16 SDK 6.50 first size");
    Require(boundaryAt.blocks[1].size == 1156864 && boundaryAt.blocks[2].size == 396544, "SDK switch must not alter second or third size");

    SetSdkVersion(process, 0x9000000);
    SdecMemory late = FilledMemory();
    Require(sceSdecQueryMemorySw2(&baseConfig, &baseRecord, 1, &late, &dpbFrames, 0, nullptr) == 0, "SDK 9.00 query failed");
    Require(late.blocks[0].size == 7845240, "SDK 9.00 must match SDK 6.50");

    SetSdkVersion(process, 0x4000000);
    SdecMemory flagged = FilledMemory();
    Require(sceSdecQueryMemorySw2(&baseConfig, &baseRecord, 1, &flagged, &dpbFrames, 1, nullptr) == 0, "flag=1 query failed");
    Require(flagged.blocks[0].size == 7732280, "wrong 16x16 flag=1 first size");
    Require(flagged.blocks[1].size == 1156864, "wrong 16x16 flag=1 second size");
    Require(flagged.blocks[2].size == 339712, "wrong 16x16 flag=1 third size");

    const SdecRecord threeInstances = QueryRecord(2, 3, 0);
    SdecMemory threeFlag0{};
    SdecMemory threeFlag1{};
    Require(sceSdecQueryMemorySw2(&baseConfig, &threeInstances, 1, &threeFlag0, &dpbFrames, 0, nullptr) == 0, "3-instance flag=0 failed");
    Require(sceSdecQueryMemorySw2(&baseConfig, &threeInstances, 1, &threeFlag1, &dpbFrames, 1, nullptr) == 0, "3-instance flag=1 failed");
    Require(threeFlag0.blocks[1].size > threeFlag1.blocks[1].size, "flag=0 must use un-capped k and m for 3 instances");

    const SdecConfig2 fieldConfig = Config(51, 16, 16, 1, 0);
    SdecMemory fieldMemory{};
    Require(sceSdecQueryMemorySw2(&fieldConfig, &baseRecord, 1, &fieldMemory, &dpbFrames, 1, nullptr) == 0, "fieldMode=0 query failed");
    Require(fieldMemory.blocks[0].size >= flagged.blocks[0].size, "fieldMode=0 first size must be max over field pass");
    Require(fieldMemory.blocks[1].size >= flagged.blocks[1].size, "fieldMode=0 second size must be max over field pass");
    Require(fieldMemory.blocks[2].size >= flagged.blocks[2].size, "fieldMode=0 third size must be max over field pass");

    const SdecConfig2 level52 = Config(52, 16, 16, -1, 1);
    SdecMemory level52Memory{};
    dpbFrames = Untouched;
    Require(sceSdecQueryMemorySw2(&level52, &baseRecord, 1, &level52Memory, &dpbFrames, 1, nullptr) == 0, "level 52 must be accepted");
    Require(dpbFrames == 16, "level 52 at 16x16 must clamp DPB frames to 16");
}

void CheckQueryRefusals() {
    const SdecConfig2 validConfig = Config(40, 120, 68, -1, 1);
    const SdecRecord validRecord = QueryRecord(1, 0, 0);
    SdecMemory memory = FilledMemory();
    std::uint32_t dpbFrames = Untouched;
    std::uint8_t tooBig = UntouchedByte;

    Require(sceSdecQueryMemorySw2(nullptr, &validRecord, 1, &memory, &dpbFrames, 1, &tooBig) == Invalid, "null config");
    Require(sceSdecQueryMemorySw2(&validConfig, nullptr, 1, &memory, &dpbFrames, 1, &tooBig) == Invalid, "null record");
    Require(sceSdecQueryMemorySw2(&validConfig, &validRecord, 1, nullptr, &dpbFrames, 1, &tooBig) == Invalid, "null memory");
    Require(sceSdecQueryMemorySw2(&validConfig, &validRecord, 1, &memory, nullptr, 1, &tooBig) == Invalid, "null dpbFrames");
    Require(IsFilled(memory) && dpbFrames == Untouched && tooBig == UntouchedByte, "null check modified outputs");

    Require(sceSdecQueryMemorySw2(&validConfig, &validRecord, 0, &memory, &dpbFrames, 1, &tooBig) == Invalid, "zero a3 mode");
    Require(dpbFrames == Untouched && IsFilled(memory), "a3 check happened after DPB write");

    SdecConfig2 depthMismatch = validConfig;
    depthMismatch.depthLuma = 8;
    depthMismatch.depthChroma = 10;
    Require(sceSdecQueryMemorySw2(&depthMismatch, &validRecord, 1, &memory, &dpbFrames, 1, &tooBig) == Invalid && dpbFrames == Untouched, "8/10 depth mismatch");
    depthMismatch.depthLuma = 10;
    depthMismatch.depthChroma = 8;
    Require(sceSdecQueryMemorySw2(&depthMismatch, &validRecord, 1, &memory, &dpbFrames, 1, &tooBig) == Invalid && dpbFrames == Untouched, "10/8 depth mismatch");
    for (const auto& depths : std::array<std::array<std::uint8_t, 2>, 3>{{{0, 0}, {10, 12}, {12, 10}}}) {
        SdecConfig2 acceptedDepths = validConfig;
        acceptedDepths.depthLuma = depths[0];
        acceptedDepths.depthChroma = depths[1];
        Require(sceSdecQueryMemorySw2(&acceptedDepths, &validRecord, 1, &memory, &dpbFrames, 1, &tooBig) == 0, "accepted depth pair failed");
    }

    memory = FilledMemory();
    dpbFrames = Untouched;
    tooBig = UntouchedByte;
    const SdecRecord tooManyInstances = QueryRecord(1, 17, 0);
    Require(sceSdecQueryMemorySw2(&validConfig, &tooManyInstances, 1, &memory, &dpbFrames, 1, &tooBig) == Invalid, "instance count > 16");
    Require(dpbFrames == 4 && IsFilled(memory) && tooBig == UntouchedByte, "dpbFrames must be written before instance count check");

    SdecConfig2 badReserved = validConfig;
    badReserved.reserved0 = 1;
    Require(sceSdecQueryMemorySw2(&badReserved, &validRecord, 1, &memory, &dpbFrames, 1, &tooBig) == Invalid && tooBig == UntouchedByte, "non-zero reserved0");
    const SdecConfig2 zeroWidth = Config(40, 0, 68, -1, 1);
    const SdecConfig2 zeroHeight = Config(40, 120, 0, -1, 1);
    Require(sceSdecQueryMemorySw2(&zeroWidth, &validRecord, 1, &memory, &dpbFrames, 1, &tooBig) == Invalid, "zero width");
    Require(sceSdecQueryMemorySw2(&zeroHeight, &validRecord, 1, &memory, &dpbFrames, 1, &tooBig) == Invalid, "zero height");

    memory = FilledMemory();
    dpbFrames = Untouched;
    tooBig = UntouchedByte;
    const SdecConfig2 oversized = Config(40, 121, 68, 2, 1);
    Require(sceSdecQueryMemorySw2(&oversized, &validRecord, 1, &memory, &dpbFrames, 1, &tooBig) == Invalid, "frame size above level MaxFS");
    Require(dpbFrames == 2 && tooBig == 1 && IsFilled(memory), "oversized check must set tooBig and leave memory untouched");
    Require(sceSdecQueryMemorySw2(&oversized, &validRecord, 1, &memory, &dpbFrames, 1, nullptr) == Invalid, "oversized with null tooBig");

    errno = 71;
    Require(sceSdecQueryMemorySw2(reinterpret_cast<const SdecConfig2*>(0x10), &validRecord, 1, &memory, &dpbFrames, 1, &tooBig) == Invalid, "unmapped config");
    Require(sceSdecQueryMemorySw2(&validConfig, reinterpret_cast<const SdecRecord*>(0x10), 1, &memory, &dpbFrames, 1, &tooBig) == Invalid, "unmapped record");
    Require(sceSdecQueryMemorySw2(&validConfig, &validRecord, 1, reinterpret_cast<SdecMemory*>(0x10), &dpbFrames, 1, &tooBig) == Invalid, "unmapped memory");
    Require(sceSdecQueryMemorySw2(&validConfig, &validRecord, 1, &memory, reinterpret_cast<std::uint32_t*>(0x10), 1, &tooBig) == Invalid, "unmapped dpbFrames");
    Require(errno == 71, "errno changed across unmapped queries");
}

void CheckCreate(std::array<std::byte, 64>& process) {
    const int attributes = 0;
    const SdecConfig2 config = Config(40, 120, 68, -1, 1);
    const SdecRecord qRecord = QueryRecord(1, 0, 0);
    const SdecRecord2 cRecord = CreateRecord(1, 0, 0);
    SdecRecord2 defaultAffinity = cRecord;
    defaultAffinity.unknown20 = 0;
    void* handle = nullptr;

    for (const std::int32_t sdk : {0x4000000, 0x6500000}) {
        SetSdkVersion(process, sdk);
        SdecMemory memory1{};
        std::uint32_t dpbFrames = 0;
        Require(sceSdecQueryMemorySw2(&config, &qRecord, 1, &memory1, &dpbFrames, 1, nullptr) == 0, "create setup query flag=1");
        memory1.blocks[0].base = 0x100000;
        memory1.blocks[1].base = 0x200000;
        memory1.blocks[2].base = 0x300000;

        Require(ThrowsRuntimeError([&] { sceSdecCreateSw2(&config, &attributes, &defaultAffinity, &memory1, &handle); }), "create must reach affinity check with exact flag=1 sizes");
        Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &memory1, &handle) == Invalid, "unmapped blocks[0] must return Invalid");

        for (std::size_t block = 0; block < 3; ++block) {
            SdecMemory shortBlock = memory1;
            shortBlock.blocks[block].size -= 1;
            Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &shortBlock, &handle) == MemoryMismatch, "short block must return 4 without slack");
        }

        for (const auto& [block, alignment] : std::array<std::array<std::uint32_t, 2>, 6>{{{0, 1}, {0, 256}, {1, 0}, {1, 512}, {2, 128}, {2, 4096}}}) {
            SdecMemory misaligned = memory1;
            misaligned.blocks[block].alignment = alignment;
            Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &misaligned, &handle) == MemoryMismatch, "mismatched alignment must return 4");
        }

        SdecMemory memory0{};
        Require(sceSdecQueryMemorySw2(&config, &qRecord, 1, &memory0, &dpbFrames, 0, nullptr) == 0, "create setup query flag=0");
        memory0.blocks[0].base = 0x100000;
        memory0.blocks[1].base = 0x200000;
        memory0.blocks[2].base = 0x300000;
        Require(ThrowsRuntimeError([&] { sceSdecCreateSw2(&config, &attributes, &defaultAffinity, &memory0, &handle); }), "create must accept larger flag=0 memory");
        Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &memory0, &handle) == Invalid, "unmapped blocks[0] with flag=0 must return Invalid");
    }

    SetSdkVersion(process, 0x4000000);
    SdecMemory validMemory{};
    std::uint32_t dpbFrames = 0;
    Require(sceSdecQueryMemorySw2(&config, &qRecord, 1, &validMemory, &dpbFrames, 1, nullptr) == 0, "refusal base query");
    validMemory.blocks[0].base = 0x100000;

    SdecConfig2 badReserved = config;
    badReserved.reserved0 = 1;
    Require(sceSdecCreateSw2(&badReserved, nullptr, nullptr, nullptr, nullptr) == Invalid, "create checks c+0x00 before null pointers");
    Require(sceSdecCreateSw2(nullptr, &attributes, &cRecord, &validMemory, &handle) == Invalid, "null config");
    Require(sceSdecCreateSw2(&config, nullptr, &cRecord, &validMemory, &handle) == Invalid, "null attributes");
    Require(sceSdecCreateSw2(&config, &attributes, nullptr, &validMemory, &handle) == Invalid, "null record");
    Require(sceSdecCreateSw2(&config, &attributes, &cRecord, nullptr, &handle) == Invalid, "null memory");
    Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &validMemory, nullptr) == Invalid, "null handle");

    SdecMemory missingBase = validMemory;
    missingBase.blocks[0].base = 0;
    Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &missingBase, &handle) == Invalid, "null first block base");
    SdecMemory tinyFirst = validMemory;
    tinyFirst.blocks[0].size = 0x37;
    Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &tinyFirst, &handle) == Invalid, "0x37 first block");

    SdecRecord2 badUnknown10 = cRecord;
    badUnknown10.unknown10 = 0;
    Require(sceSdecCreateSw2(&config, &attributes, &badUnknown10, &validMemory, &handle) == Invalid, "zero r+0x10");

    for (const std::uint32_t badMode : {3u, 0xffffffffu}) {
        SdecRecord2 bad = cRecord;
        bad.instanceMode = badMode;
        Require(sceSdecCreateSw2(&config, &attributes, &bad, &validMemory, &handle) == Invalid, "create instance mode above 2");
    }

    SdecRecord2 mode2TooMany = CreateRecord(2, 17, 0);
    Require(sceSdecCreateSw2(&config, &attributes, &mode2TooMany, &validMemory, &handle) == Invalid, "create mode 2 instance count above 16");

    for (const std::int32_t bad18 : {0, 0xff, 0x300, -2}) {
        SdecRecord2 bad = cRecord;
        bad.unknown18 = bad18;
        Require(sceSdecCreateSw2(&config, &attributes, &bad, &validMemory, &handle) == Invalid, "create r+0x18 out of range");
    }

    SdecRecord2 bad20 = cRecord;
    bad20.unknown20 = 0x80u;
    Require(sceSdecCreateSw2(&config, &attributes, &bad20, &validMemory, &handle) == Invalid, "create r+0x20 above 0x7f");

    for (const std::int32_t good18 : {0x100, 0x2ff, -1}) {
        SdecRecord2 goodMode2 = CreateRecord(2, 16, 0);
        goodMode2.unknown18 = good18;
        goodMode2.unknown20 = 0;
        SdecMemory mode2Memory{};
        const SdecRecord q2 = QueryRecord(2, 16, 0);
        Require(sceSdecQueryMemorySw2(&config, &q2, 1, &mode2Memory, &dpbFrames, 1, nullptr) == 0, "mode 2 query");
        mode2Memory.blocks[0].base = 0x100000;
        Require(ThrowsRuntimeError([&] { sceSdecCreateSw2(&config, &attributes, &goodMode2, &mode2Memory, &handle); }), "accepted r+0x18 and mode 2");
        goodMode2.unknown20 = 0x7fu;
        Require(sceSdecCreateSw2(&config, &attributes, &goodMode2, &mode2Memory, &handle) == Invalid, "unmapped mode 2 blocks[0] must return Invalid");
    }

    SdecConfig2 mismatch = config;
    mismatch.depthLuma = 8;
    mismatch.depthChroma = 10;
    const SdecConfig2 oversizedCreate = Config(40, 121, 68, -1, 1);
    Require(sceSdecCreateSw2(&mismatch, &attributes, &cRecord, &validMemory, &handle) == Invalid, "create depth mismatch");
    Require(sceSdecCreateSw2(&oversizedCreate, &attributes, &cRecord, &validMemory, &handle) == Invalid, "create oversized");

    errno = 71;
    Require(sceSdecCreateSw2(reinterpret_cast<const SdecConfig2*>(0x10), &attributes, &cRecord, &validMemory, &handle) == Invalid, "unmapped config");
    Require(sceSdecCreateSw2(&config, &attributes, reinterpret_cast<const SdecRecord2*>(0x10), &validMemory, &handle) == Invalid, "unmapped record");
    Require(sceSdecCreateSw2(&config, &attributes, &cRecord, reinterpret_cast<const SdecMemory*>(0x10), &handle) == Invalid, "unmapped memory");
    Require(errno == 71, "errno changed across unmapped creates");
}

void CheckCreateAndDestroyLifecycle(std::array<std::byte, 64>& process) {
    SetSdkVersion(process, 0x4000000);
    SdecConfig2 config = Config(51, 16, 16, 1, 1);
    config.unknown28 = 256;
    config.unknown2C = 256;
    const SdecRecord qRecord = QueryRecord(1, 0, 0);
    const SdecRecord2 cRecord = CreateRecord(1, 0, 0);
    SdecMemory memory{};
    std::uint32_t dpbFrames = 0;
    Require(sceSdecQueryMemorySw2(&config, &qRecord, 1, &memory, &dpbFrames, 1, nullptr) == 0, "lifecycle query failed");

    std::vector<std::byte> block0(memory.blocks[0].size, std::byte{0xCC});
    std::vector<std::byte> block1(memory.blocks[1].size, std::byte{UntouchedByte});
    std::vector<std::byte> block2(memory.blocks[2].size, std::byte{UntouchedByte});
    memory.blocks[0].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block0.data()));
    memory.blocks[1].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block1.data()));
    memory.blocks[2].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block2.data()));

    const int attributes = 0;
    void* handle = nullptr;
    errno = 71;
    Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &memory, &handle) == 0, "sceSdecCreateSw2 lifecycle failed");
    Require(errno == 71, "sceSdecCreateSw2 must preserve errno");
    Require(handle == block0.data(), "handle must equal blocks[0].base");

    const std::uint64_t expectedWs = memory.blocks[0].base + 0x18;
    const std::uint64_t expectedCodec = ((expectedWs + 0x20) & ~std::uint64_t{0x1f}) + 0x20;
    std::array<std::uint64_t, 3> handleHeader{};
    std::memcpy(handleHeader.data(), block0.data(), sizeof(handleHeader));
    Require(handleHeader[0] == 1, "handle header[0] must be 1");
    Require(handleHeader[1] == expectedWs, "handle header[1] must point to workspace");
    Require(handleHeader[2] == expectedCodec, "handle header[2] must point to codec");

    std::array<std::uint64_t, 4> wsHeader{};
    std::memcpy(wsHeader.data(), block0.data() + 0x18, sizeof(wsHeader));
    Require(wsHeader[0] == expectedWs + 0x20, "workspace header[0] start mismatch");
    Require(wsHeader[1] == expectedCodec + 0x180, "workspace header[1] bump mismatch");
    Require(wsHeader[2] == memory.blocks[0].base + memory.blocks[0].size, "workspace header[2] end mismatch");
    Require(wsHeader[3] == 0, "workspace header[3] must be 0");

    const std::byte* codecPtr = block0.data() + static_cast<std::size_t>(expectedCodec - memory.blocks[0].base);
    std::uint64_t codecFlag = 0;
    std::uint64_t codecWs = 0;
    std::uint32_t codecFrames = 0;
    std::uint32_t codecUnknown28 = 0;
    std::uint32_t codecUnknown2C = 0;
    SdecMemory codecMemory{};
    std::uint64_t codecUnknown10 = 0;
    std::uint32_t codecInstanceMode = 0;
    std::memcpy(&codecFlag, codecPtr + 0x00, sizeof(codecFlag));
    std::memcpy(&codecWs, codecPtr + 0x10, sizeof(codecWs));
    std::memcpy(&codecFrames, codecPtr + 0x30, sizeof(codecFrames));
    std::memcpy(&codecUnknown28, codecPtr + 0x34, sizeof(codecUnknown28));
    std::memcpy(&codecUnknown2C, codecPtr + 0x38, sizeof(codecUnknown2C));
    std::memcpy(&codecMemory, codecPtr + 0x78, sizeof(codecMemory));
    std::memcpy(&codecUnknown10, codecPtr + 0xc0, sizeof(codecUnknown10));
    std::memcpy(&codecInstanceMode, codecPtr + 0x118, sizeof(codecInstanceMode));
    Require(codecFlag == 1, "codec flag must be 1");
    Require(codecWs == expectedWs, "codec workspace back-pointer mismatch");
    Require(codecFrames == 1 && codecUnknown28 == 256 && codecUnknown2C == 256, "codec frame dimensions mismatch");
    Require(std::memcmp(&codecMemory, &memory, sizeof(memory)) == 0, "codec memory copy mismatch");
    Require(codecUnknown10 == cRecord.unknown10, "codec unknown10 mismatch");
    Require(codecInstanceMode == 1, "codec instanceMode mismatch");

    Require(std::all_of(block1.begin(), block1.end(), [](std::byte b) { return b == std::byte{0}; }), "blocks[1] must be zeroed");

    errno = 73;
    Require(sceSdecDestroySw2(handle) == 0, "sceSdecDestroySw2 lifecycle failed");
    Require(errno == 73, "sceSdecDestroySw2 must preserve errno");
    std::memcpy(handleHeader.data(), block0.data(), sizeof(handleHeader));
    Require(handleHeader[0] == 1 && handleHeader[1] == 0 && handleHeader[2] == 0, "destroy must clear workspace and codec fields");

    Require(sceSdecDestroySw2(handle) == 0, "idempotent sceSdecDestroySw2 failed");
    Require(errno == 73, "idempotent sceSdecDestroySw2 must preserve errno");
}

void CheckCreateRollback(std::array<std::byte, 64>& process) {
    SetSdkVersion(process, 0x4000000);
    SdecConfig2 config = Config(51, 16, 16, 1, 1);
    config.unknown28 = 256;
    config.unknown2C = 256;
    const SdecRecord qRecord = QueryRecord(1, 0, 0);
    const SdecRecord2 cRecord = CreateRecord(1, 0, 0);
    SdecMemory memory{};
    std::uint32_t dpbFrames = 0;
    Require(sceSdecQueryMemorySw2(&config, &qRecord, 1, &memory, &dpbFrames, 1, nullptr) == 0, "rollback query failed");

    std::vector<std::byte> block0(memory.blocks[0].size, std::byte{0xCC});
    std::vector<std::byte> block1(memory.blocks[1].size, std::byte{UntouchedByte});
    std::vector<std::byte> block2(memory.blocks[2].size, std::byte{UntouchedByte});

    const int attributes = 0;
    void* const sentinel = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x12345));

    SdecMemory badBlock1 = memory;
    badBlock1.blocks[0].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block0.data()));
    badBlock1.blocks[1].base = 0x200000;
    badBlock1.blocks[2].base = 0x300000;
    const std::uint64_t expectedWs = badBlock1.blocks[0].base + 0x18;
    const std::uint64_t expectedCodec = ((expectedWs + 0x20) & ~std::uint64_t{0x1f}) + 0x20;
    const std::size_t codecOffset = static_cast<std::size_t>(expectedCodec - badBlock1.blocks[0].base);

    void* handle = sentinel;
    errno = 71;
    Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &badBlock1, &handle) == Invalid, "unwritable blocks[1] must return Invalid");
    Require(errno == 71, "unwritable blocks[1] must preserve errno");
    Require(handle == sentinel, "unwritable blocks[1] must leave output handle untouched");
    Require(IsAllZero(block0.data(), 0x18), "unwritable blocks[1] must clear handle header");
    Require(IsAllZero(block0.data() + 0x18, 0x20), "unwritable blocks[1] must clear workspace header");
    Require(IsAllZero(block0.data() + codecOffset, 0x180), "unwritable blocks[1] must clear codec object");

    std::fill(block0.begin(), block0.end(), std::byte{0xCC});
    SdecMemory goodMem = memory;
    goodMem.blocks[0].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block0.data()));
    goodMem.blocks[1].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block1.data()));
    goodMem.blocks[2].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block2.data()));
    errno = 73;
    Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &goodMem, reinterpret_cast<void**>(0x10)) == Invalid, "unwritable handle must return Invalid");
    Require(errno == 73, "unwritable handle must preserve errno");
    Require(IsAllZero(block0.data(), 0x18), "unwritable handle must clear handle header");
    Require(IsAllZero(block0.data() + 0x18, 0x20), "unwritable handle must clear workspace header");
    Require(IsAllZero(block0.data() + codecOffset, 0x180), "unwritable handle must clear codec object");

#ifdef _WIN32
    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    const std::size_t pageSize = sysInfo.dwPageSize;
    const std::size_t allocBytes = ((pageSize + static_cast<std::size_t>(memory.blocks[0].size) + pageSize - 1) / pageSize) * pageSize;
    auto* const pages = static_cast<std::byte*>(VirtualAlloc(nullptr, allocBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr, "VirtualAlloc failed");
    const auto protectReadOnly = [](void* addr, std::size_t len) {
        DWORD oldProtect = 0;
        Require(VirtualProtect(addr, len, PAGE_READONLY, &oldProtect) != 0, "VirtualProtect PAGE_READONLY failed");
    };
    const auto protectReadWrite = [](void* addr, std::size_t len) {
        DWORD oldProtect = 0;
        Require(VirtualProtect(addr, len, PAGE_READWRITE, &oldProtect) != 0, "VirtualProtect PAGE_READWRITE failed");
    };
#else
    const long nativePage = sysconf(_SC_PAGESIZE);
    Require(nativePage > 0, "sysconf _SC_PAGESIZE failed");
    const std::size_t pageSize = static_cast<std::size_t>(nativePage);
    const std::size_t allocBytes = ((pageSize + static_cast<std::size_t>(memory.blocks[0].size) + pageSize - 1) / pageSize) * pageSize;
    auto* const pages = static_cast<std::byte*>(mmap(nullptr, allocBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(pages != MAP_FAILED, "mmap failed");
    const auto protectReadOnly = [](void* addr, std::size_t len) {
        Require(mprotect(addr, len, PROT_READ) == 0, "mprotect PROT_READ failed");
    };
    const auto protectReadWrite = [](void* addr, std::size_t len) {
        Require(mprotect(addr, len, PROT_READ | PROT_WRITE) == 0, "mprotect PROT_READ|PROT_WRITE failed");
    };
#endif

    std::byte* const splitCodecBase = pages + pageSize - 0x38;
    std::memset(splitCodecBase, 0xCC, 0x200);
    protectReadOnly(pages + pageSize, allocBytes - pageSize);
    SdecMemory splitCodecMem = memory;
    splitCodecMem.blocks[0].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(splitCodecBase));
    splitCodecMem.blocks[1].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block1.data()));
    splitCodecMem.blocks[2].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block2.data()));
    handle = sentinel;
    errno = 75;
    Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &splitCodecMem, &handle) == Invalid, "unwritable codec object must return Invalid");
    Require(errno == 75, "unwritable codec object must preserve errno");
    Require(handle == sentinel, "unwritable codec object must leave output handle untouched");
    Require(IsAllZero(splitCodecBase + 0x18, 0x20), "unwritable codec object must clear workspace header");

    protectReadWrite(pages + pageSize, allocBytes - pageSize);
    std::byte* const splitHeaderBase = pages + pageSize - 0x18;
    std::memset(splitHeaderBase, 0xCC, 0x200);
    protectReadOnly(pages, pageSize);
    SdecMemory splitHeaderMem = memory;
    splitHeaderMem.blocks[0].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(splitHeaderBase));
    splitHeaderMem.blocks[1].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block1.data()));
    splitHeaderMem.blocks[2].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block2.data()));
    const std::uint64_t splitWs = splitHeaderMem.blocks[0].base + 0x18;
    const std::uint64_t splitCodecAddr = ((splitWs + 0x20) & ~std::uint64_t{0x1f}) + 0x20;
    const std::size_t splitCodecOff = static_cast<std::size_t>(splitCodecAddr - splitHeaderMem.blocks[0].base);
    handle = sentinel;
    errno = 77;
    Require(sceSdecCreateSw2(&config, &attributes, &cRecord, &splitHeaderMem, &handle) == Invalid, "unwritable handle header must return Invalid");
    Require(errno == 77, "unwritable handle header must preserve errno");
    Require(handle == sentinel, "unwritable handle header must leave output handle untouched");
    Require(IsAllZero(splitHeaderBase + 0x18, 0x20), "unwritable handle header must clear workspace header");
    Require(IsAllZero(splitHeaderBase + splitCodecOff, 0x180), "unwritable handle header must clear codec object");

#ifdef _WIN32
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, allocBytes);
#endif
}

} // namespace

int main() {
    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "cannot install the process parameters");
    CheckQuerySizes(process);
    CheckQueryRefusals();
    CheckCreate(process);
    CheckCreateAndDestroyLifecycle(process);
    CheckCreateRollback(process);
    std::puts("Sdec query memory 2 tests passed");
    return 0;
}
