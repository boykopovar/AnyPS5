#include "prx/libSceVdecSavc/libSceVdecSavc.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr int Invalid = 3;
constexpr int MemoryMismatch = 4;
constexpr std::uint32_t Untouched = 0xA5A5A5A5u;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Sdec query memory: %s\n", message);
    std::abort();
}

struct SizeRow {
    std::uint32_t level;
    std::int32_t width;
    std::int32_t height;
    std::int32_t frameCount;
    std::uint32_t instanceMode;
    std::int32_t extraFrames;
    std::uint32_t dpbFrames;
    std::uint64_t first;
    std::uint64_t second;
    std::uint64_t third;
};

constexpr std::array<SizeRow, 12> Sizes{{
    {40, 120, 68, -1, 1, 0, 4, 0xc94b18, 0xe4ff00, 0x1cebd00},
    {40, 120, 68, -1, 0, 0, 4, 0xc94b18, 0x1778600, 0x1cebd00},
    {51, 1024, 36, -1, 1, 0, 5, 0x28cc518, 0x31ac700, 0x7400d00},
    {51, 54, 682, 4, 0, 3, 4, 0x31a6158, 0x5b56a00, 0x92de100},
    {10, 11, 9, -1, 1, 0, 4, 0x609358, 0x462b00, 0x847b00},
    {111, 11, 9, 2, 0, -5, 2, 0x601a78, 0x482200, 0x833700},
    {42, 120, 68, -1, 1, 16, 4, 0x1ca7298, 0xe4ff00, 0x4cbbd00},
    {13, 22, 18, -1, 1, 0, 6, 0x662558, 0x4beb00, 0x966900},
    {51, 1, 1, -1, 0, 0, 16, 0x5f13f8, 0x442800, 0x808100},
    {51, 1024, 1, 0, 1, 0, 0, 0x742df8, 0x583700, 0x920d00},
    {51, 1, 682, 0, 0, 0, 0, 0x684e78, 0x78c500, 0xb55500},
    {51, 4, 4, 0, 1, 8191, 0, 0x1ae2338, 0x446000, 0x3804500},
}};

struct ModuleSize {
    std::int32_t width;
    std::int32_t height;
    std::uint64_t oneInstance;
    std::uint64_t twoInstances;
};

constexpr std::array<ModuleSize, 5> ModuleSecondSizes{{
    {45, 30, 6225408, 7836416},
    {80, 45, 9135360, 13394944},
    {120, 68, 15007488, 24610304},
    {160, 90, 23063040, 39997440},
    {240, 135, 46344960, 84473344},
}};

SdecConfig Config(std::uint32_t level, std::int32_t width, std::int32_t height, std::int32_t frameCount) {
    SdecConfig config{};
    config.level = level;
    config.frameCount = frameCount;
    config.widthInMbs = width;
    config.heightInMbs = height;
    return config;
}

SdecRecord Record(std::uint32_t instanceMode, std::int32_t extraFrames) {
    SdecRecord record{};
    record.instanceMode = instanceMode;
    record.extraFrames = extraFrames;
    return record;
}

SdecMemory Filled() {
    SdecMemory memory{};
    std::memset(&memory, 0xA5, sizeof(memory));
    return memory;
}

bool IsFilled(const SdecMemory& memory) {
    const SdecMemory filled = Filled();
    return std::memcmp(&memory, &filled, sizeof(memory)) == 0;
}

int Query(const SdecConfig& config, const SdecRecord& record, SdecMemory* memory, std::uint32_t* dpbFrames) {
    return sceSdecQueryMemorySw(&config, &record, memory, dpbFrames);
}

int Refused(const SdecConfig& config, const SdecRecord& record, std::uint32_t* dpbFrames) {
    SdecMemory memory = Filled();
    const int result = Query(config, record, &memory, dpbFrames);
    Require(IsFilled(memory), "a refused query wrote the memory record");
    return result;
}

void CheckQuerySizes() {
    for (const SizeRow& row : Sizes) {
        SdecMemory memory = Filled();
        std::uint32_t dpbFrames = Untouched;
        const SdecConfig config = Config(row.level, row.width, row.height, row.frameCount);
        Require(Query(config, Record(row.instanceMode, row.extraFrames), &memory, &dpbFrames) == 0, "a query within the limits was refused");
        Require(dpbFrames == row.dpbFrames, "wrong DPB frame count");
        Require(memory.blocks[0].base == 0 && memory.blocks[0].size == row.first && memory.blocks[0].alignment == 0 && memory.blocks[0].reserved == 0, "wrong first block");
        Require(memory.blocks[1].base == 0 && memory.blocks[1].size == row.second && memory.blocks[1].alignment == 256 && memory.blocks[1].reserved == 0, "wrong second block");
        Require(memory.blocks[2].base == 0 && memory.blocks[2].size == row.third && memory.blocks[2].alignment == 256 && memory.blocks[2].reserved == 0, "wrong third block");
    }
    for (const ModuleSize& size : ModuleSecondSizes) {
        for (const std::uint32_t mode : {0u, 1u, 2u, 0xffffffffu}) {
            SdecMemory memory = Filled();
            std::uint32_t dpbFrames = Untouched;
            Require(Query(Config(51, size.width, size.height, 3), Record(mode, 0), &memory, &dpbFrames) == 0, "a common picture size was refused");
            Require(memory.blocks[1].size == (mode == 1 ? size.oneInstance : size.twoInstances), "the second size differs from the module's");
        }
    }
    SdecConfig other = Config(40, 120, 68, -1);
    other.unknown4 = 7;
    other.unknown10 = 9;
    other.unknown18 = 11;
    SdecRecord otherRecord = Record(1, 0);
    otherRecord.unknown8 = 5;
    SdecMemory memory = Filled();
    std::uint32_t dpbFrames = Untouched;
    Require(Query(other, otherRecord, &memory, &dpbFrames) == 0 && memory.blocks[0].size == Sizes[0].first && memory.blocks[1].size == Sizes[0].second && memory.blocks[2].size == Sizes[0].third, "fields the sizes do not depend on changed them");
}

void CheckQueryDpbFrames() {
    const SdecRecord record = Record(1, 0);
    SdecMemory memory = Filled();
    for (const auto& [frameCount, expected] : std::array<std::array<std::int32_t, 2>, 4>{{{0, 0}, {7, 7}, {16, 16}, {20, 16}}}) {
        std::uint32_t dpbFrames = Untouched;
        Require(Query(Config(40, 120, 68, frameCount), record, &memory, &dpbFrames) == 0, "a frame override was refused");
        Require(dpbFrames == static_cast<std::uint32_t>(expected), "the frame override was not capped at 16");
    }
    std::uint32_t dpbFrames = Untouched;
    Require(Query(Config(111, 11, 9, -1), record, &memory, &dpbFrames) == 0 && dpbFrames == 16, "level 111 did not use the default DPB size");
}

void CheckQueryRefusals() {
    const SdecRecord record = Record(1, 0);
    SdecMemory memory = Filled();
    Require(Query(Config(40, 120, 68, -1), record, &memory, nullptr) == Invalid && IsFilled(memory), "a query without a DPB output was accepted");

    std::uint32_t dpbFrames = Untouched;
    SdecConfig reserved = Config(40, 120, 68, -1);
    reserved.reserved0 = 1;
    Require(Refused(reserved, record, &dpbFrames) == Invalid, "a non-zero first word was accepted");
    Require(dpbFrames == 4, "the DPB count was not written before the first-word check");

    dpbFrames = Untouched;
    Require(Refused(Config(40, 0, 68, 3), record, &dpbFrames) == Invalid && dpbFrames == 3, "a zero width was accepted");
    dpbFrames = Untouched;
    Require(Refused(Config(40, 120, 0, 3), record, &dpbFrames) == Invalid && dpbFrames == 3, "a zero height was accepted");
    Require(Refused(Config(40, 0, 68, -1), record, &dpbFrames) == Invalid, "a zero width with no frame override was accepted");

    for (const std::uint32_t level : {0u, 9u, 14u, 23u, 52u, 110u, 112u}) {
        dpbFrames = Untouched;
        Require(Refused(Config(level, 4, 4, -1), record, &dpbFrames) == Invalid, "an unknown level was accepted");
        Require(dpbFrames == 16, "an unknown level did not use the default DPB size");
    }
    Require(Refused(Config(40, 121, 68, -1), record, &dpbFrames) == Invalid, "a picture above the level's frame size was accepted");
    Require(Refused(Config(10, 10, 10, -1), record, &dpbFrames) == Invalid, "a level 1.0 picture above 99 macroblocks was accepted");
    Require(Refused(Config(51, 1025, 1, 0), record, &dpbFrames) == Invalid, "a width above 1024 macroblocks was accepted");
    Require(Refused(Config(51, 1, 683, 0), record, &dpbFrames) == Invalid, "a height above 682 macroblocks was accepted");
    Require(Refused(Config(51, 4, 4, 0), Record(1, 0x2000), &dpbFrames) == Invalid, "more than 0x2000 frames were accepted");
    Require(Refused(Config(51, 4, 4, 16), Record(1, 0x1ff0), &dpbFrames) == Invalid, "more than 0x2000 frames with the override were accepted");

    dpbFrames = Untouched;
    SdecConfig negative = Config(40, -120, 68, -1);
    negative.reserved0 = 1;
    Require(Refused(negative, record, &dpbFrames) == Invalid && dpbFrames == 16, "a negative width did not give 16 frames");
    Require(Refused(Config(40, -120, 0, -1), record, &dpbFrames) == Invalid, "a negative width with a zero height was accepted");
    Require(Refused(Config(52, 120, -68, -1), record, &dpbFrames) == Invalid, "a negative height with an unknown level was accepted");

    errno = 71;
    dpbFrames = Untouched;
    Require(sceSdecQueryMemorySw(reinterpret_cast<const SdecConfig*>(0x10), &record, &memory, &dpbFrames) == Invalid, "an unmapped configuration was accepted");
    const SdecConfig config = Config(40, 120, 68, -1);
    Require(sceSdecQueryMemorySw(&config, reinterpret_cast<const SdecRecord*>(0x10), &memory, &dpbFrames) == Invalid, "an unmapped record was accepted");
    Require(sceSdecQueryMemorySw(&config, &record, &memory, reinterpret_cast<std::uint32_t*>(0x10)) == Invalid, "an unwritable DPB output was accepted");
    Require(sceSdecQueryMemorySw(&config, &record, reinterpret_cast<SdecMemory*>(0x10), &dpbFrames) == Invalid, "an unwritable memory record was accepted");
    Require(errno == 71, "errno changed across unmapped queries");
}

SdecMemory Queried(const SdecConfig& config, const SdecRecord& record) {
    SdecMemory memory = Filled();
    std::uint32_t dpbFrames = Untouched;
    Require(Query(config, record, &memory, &dpbFrames) == 0, "the query for a create row was refused");
    memory.blocks[0].base = 0x10000;
    memory.blocks[1].base = 0x20000;
    memory.blocks[2].base = 0x30000;
    return memory;
}

int Create(const SdecConfig& config, const SdecRecord& record, const SdecMemory& memory, void** handle) {
    static const std::uint32_t attributes = 0;
    return sceSdecCreateSw(&config, &attributes, &record, &memory, handle);
}

void* const Sentinel = reinterpret_cast<void*>(0x5A5A5A5Aull);

void CheckCreateRefusals() {
    const SdecConfig config = Config(40, 120, 68, -1);
    const SdecRecord record = Record(1, 0);
    const SdecMemory memory = Queried(config, record);
    const std::uint32_t attributes = 0;

    void* handle = Sentinel;
    Require(sceSdecCreateSw(nullptr, &attributes, &record, &memory, &handle) == Invalid, "create accepted no configuration");
    Require(sceSdecCreateSw(&config, nullptr, &record, &memory, &handle) == Invalid, "create accepted no second argument");
    Require(sceSdecCreateSw(&config, &attributes, nullptr, &memory, &handle) == Invalid, "create accepted no record");
    Require(sceSdecCreateSw(&config, &attributes, &record, nullptr, &handle) == Invalid, "create accepted no memory");
    Require(sceSdecCreateSw(&config, &attributes, &record, &memory, nullptr) == Invalid, "create accepted no handle output");

    SdecConfig reserved = config;
    reserved.reserved0 = 1;
    Require(Create(reserved, record, memory, &handle) == Invalid, "create accepted a non-zero first word");
    SdecMemory noBase = memory;
    noBase.blocks[0].base = 0;
    Require(Create(config, record, noBase, &handle) == Invalid, "create accepted a first block without an address");
    SdecMemory header = memory;
    header.blocks[0].size = 0x17;
    Require(Create(config, record, header, &handle) == Invalid, "create accepted a first block smaller than its header");
    Require(handle == Sentinel, "a check before the decoder set-up wrote the handle");

    const auto refusedWithHandle = [&](const SdecConfig& rowConfig, const SdecRecord& rowRecord, const SdecMemory& rowMemory, int expected, const char* message) {
        void* rowHandle = Sentinel;
        Require(Create(rowConfig, rowRecord, rowMemory, &rowHandle) == expected, message);
        Require(rowHandle == nullptr, "a refused decoder set-up did not clear the handle");
    };
    SdecMemory small = memory;
    small.blocks[0].size = 0x37;
    refusedWithHandle(config, record, small, Invalid, "create accepted a first block with no room after its header");
    for (const std::uint32_t mode : {2u, 3u, 0x7fffffffu, 0xffffffffu}) {
        refusedWithHandle(config, Record(mode, 0), memory, Invalid, "create accepted an instance mode above 1");
    }
    refusedWithHandle(Config(52, 120, 68, -1), record, memory, Invalid, "create accepted an unknown level");
    refusedWithHandle(Config(40, 121, 68, -1), record, memory, Invalid, "create accepted a picture above the level's frame size");
    refusedWithHandle(Config(40, 0, 68, -1), record, memory, Invalid, "create accepted a zero width");

    for (const auto& [block, alignment] : std::array<std::array<std::uint32_t, 2>, 6>{{{0, 1}, {0, 256}, {1, 128}, {1, 512}, {2, 0}, {2, 4096}}}) {
        SdecMemory misaligned = memory;
        misaligned.blocks[block].alignment = alignment;
        refusedWithHandle(config, record, misaligned, MemoryMismatch, "create accepted an alignment the query did not give");
    }
    SdecMemory first = memory;
    first.blocks[0].size -= 0x21;
    refusedWithHandle(config, record, first, MemoryMismatch, "create accepted a first block 0x21 bytes short");
    SdecMemory second = memory;
    second.blocks[1].size -= 1;
    refusedWithHandle(config, record, second, MemoryMismatch, "create accepted a short second block");
    SdecMemory third = memory;
    third.blocks[2].size -= 1;
    refusedWithHandle(config, record, third, MemoryMismatch, "create accepted a short third block");
    const SdecRecord moreFrames = Record(1, 1);
    refusedWithHandle(config, moreFrames, memory, MemoryMismatch, "create accepted memory queried for fewer frames");
    const SdecRecord twoInstances = Record(0, 0);
    refusedWithHandle(config, twoInstances, memory, MemoryMismatch, "create accepted memory queried for one instance with two");

    errno = 71;
    Require(sceSdecCreateSw(reinterpret_cast<const SdecConfig*>(0x10), &attributes, &record, &memory, &handle) == Invalid, "create accepted an unmapped configuration");
    Require(sceSdecCreateSw(&config, &attributes, &record, reinterpret_cast<const SdecMemory*>(0x10), &handle) == Invalid, "create accepted an unmapped memory record");
    Require(Create(config, record, small, reinterpret_cast<void**>(0x10)) == Invalid, "create failed on an unwritable handle");
    Require(errno == 71, "errno changed across unmapped creates");
}

void CheckCreateAndDestroyLifecycle() {
    const SdecConfig config = Config(10, 11, 9, -1);
    const SdecRecord record = Record(1, 0);
    SdecMemory memory = Filled();
    std::uint32_t dpbFrames = Untouched;
    Require(Query(config, record, &memory, &dpbFrames) == 0, "lifecycle query refused");

    std::vector<std::uint8_t> buf0(static_cast<std::size_t>(memory.blocks[0].size), 0xCD);
    std::vector<std::uint8_t> buf1(static_cast<std::size_t>(memory.blocks[1].size) + 256, 0xCD);
    std::vector<std::uint8_t> buf2(static_cast<std::size_t>(memory.blocks[2].size) + 256, 0xCD);
    const auto align256 = [](std::uint8_t* ptr) -> std::uint64_t {
        const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
        return static_cast<std::uint64_t>((addr + 255u) & ~std::uintptr_t{255u});
    };
    memory.blocks[0].base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(buf0.data()));
    memory.blocks[1].base = align256(buf1.data());
    memory.blocks[2].base = align256(buf2.data());

    void* handle = nullptr;
    errno = 71;
    Require(Create(config, record, memory, &handle) == 0, "create rejected queried buffers");
    Require(errno == 71, "errno changed across create");
    Require(handle == buf0.data(), "create returned wrong handle");

    std::array<std::uint64_t, 3> header{};
    std::memcpy(header.data(), handle, sizeof(header));
    Require(header[0] == 1, "wrong handle header tag");
    Require(header[1] == memory.blocks[0].base + 0x18, "wrong workspace pointer");
    Require(header[2] >= header[1] + 0x20, "wrong codec pointer");

    std::uint32_t codecState = 0;
    const auto* codecBytes = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(header[2]));
    std::memcpy(&codecState, codecBytes + 0xf0, sizeof(codecState));
    Require(codecState == 1, "codec state not initialized");

    errno = 71;
    Require(sceSdecDestroySw(handle) == 0, "destroy returned error");
    Require(errno == 71, "errno changed across destroy");
    std::memcpy(header.data(), handle, sizeof(header));
    Require(header[1] == 0 && header[2] == 0, "destroy did not clear workspace and codec fields");

    std::array<std::uint64_t, 8> standaloneCodec{};
    standaloneCodec[6] = 0x12345678u;
    std::array<std::uint64_t, 4> standaloneDecoder{1, 0x2000, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(standaloneCodec.data())), 0x4444};
    Require(sceSdecDestroySw(standaloneDecoder.data()) == 0, "destroy standalone codec returned error");
    Require(standaloneDecoder[1] == 0 && standaloneDecoder[2] == 0 && standaloneCodec[6] == 0, "destroy did not clear codec fields");
}

}

int main() {
    CheckQuerySizes();
    CheckQueryDpbFrames();
    CheckQueryRefusals();
    CheckCreateRefusals();
    CheckCreateAndDestroyLifecycle();
    std::puts("Sdec query memory tests passed");
    return 0;
}
