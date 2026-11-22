#include "prx/libSceVdecSavc2/libSceVdecSavc2.hpp"
#include "ProcessParametersFixture.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

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
    void* handle = nullptr;

    for (const std::int32_t sdk : {0x4000000, 0x6500000}) {
        SetSdkVersion(process, sdk);
        SdecMemory memory1{};
        std::uint32_t dpbFrames = 0;
        Require(sceSdecQueryMemorySw2(&config, &qRecord, 1, &memory1, &dpbFrames, 1, nullptr) == 0, "create setup query flag=1");
        memory1.blocks[0].base = 0x100000;
        memory1.blocks[1].base = 0x200000;
        memory1.blocks[2].base = 0x300000;

        Require(ThrowsRuntimeError([&] { sceSdecCreateSw2(&config, &attributes, &cRecord, &memory1, &handle); }), "create must reach setup throw with exact flag=1 sizes");

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
        Require(ThrowsRuntimeError([&] { sceSdecCreateSw2(&config, &attributes, &cRecord, &memory0, &handle); }), "create must accept larger flag=0 memory");
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
        SdecMemory mode2Memory{};
        const SdecRecord q2 = QueryRecord(2, 16, 0);
        Require(sceSdecQueryMemorySw2(&config, &q2, 1, &mode2Memory, &dpbFrames, 1, nullptr) == 0, "mode 2 query");
        mode2Memory.blocks[0].base = 0x100000;
        Require(ThrowsRuntimeError([&] { sceSdecCreateSw2(&config, &attributes, &goodMode2, &mode2Memory, &handle); }), "accepted r+0x18 and mode 2");
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

} // namespace

int main() {
    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "cannot install the process parameters");
    CheckQuerySizes(process);
    CheckQueryRefusals();
    CheckCreate(process);
    std::puts("Sdec query memory 2 tests passed");
    return 0;
}
