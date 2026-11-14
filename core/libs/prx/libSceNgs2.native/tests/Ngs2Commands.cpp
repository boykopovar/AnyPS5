#include "Ngs2Test.hpp"
#include "prx/libSceNgs2.native/include/Api.hpp"
#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <vector>

namespace {

struct CommandRecord {
    std::uint32_t id;
    std::uint8_t flags;
    std::uint8_t type;
    std::uint16_t count;
    union {
        std::uint64_t u64;
        std::uint32_t u32;
        std::int32_t i32;
        float f;
        const void* ptr;
    } value;
};

struct StreamOption {
    std::uint64_t size;
    char name[64];
    std::uint32_t flags;
    std::uint32_t num_blocks;
    std::uint32_t buffer_size_1;
    std::uint32_t reserved1;
    std::uint32_t buffer_size_2;
    std::uint32_t reserved2;
    std::uint32_t buffer_size_3;
    std::uint32_t reserved3;
    float default_param;
    std::uint32_t reserved4;
    std::uint8_t reserved_tail[24];
};

constexpr int InvalidOutAddress = static_cast<int>(0x804a8010u);
constexpr int InvalidOutSize = static_cast<int>(0x804a8011u);
constexpr int InvalidOptionSize = static_cast<int>(0x804a8013u);
constexpr int InvalidBufferInfo = static_cast<int>(0x804a8020u);
constexpr int InvalidBufferAllocator = static_cast<int>(0x804a8023u);
constexpr int InvalidStreamCommands = static_cast<int>(0x804a8030u);
constexpr int InvalidStreamQueryId = static_cast<int>(0x804a8031u);
constexpr int InvalidSampleRate = static_cast<int>(0x804a8051u);
constexpr int InvalidNumBlocks = static_cast<int>(0x804a8053u);
constexpr int NullStreamBlockPtr = static_cast<int>(0x804a8055u);
constexpr int InvalidSystemHandle = static_cast<int>(0x804a8201u);
constexpr int InvalidRackHandle = static_cast<int>(0x804a8202u);
constexpr int InvalidStreamHandle = static_cast<int>(0x804a8204u);
constexpr int InvalidRackModuleIndex = static_cast<int>(0x804a8256u);
constexpr int InvalidRackQueryId = static_cast<int>(0x804a8304u);
constexpr int InvalidNumGrainSamples = static_cast<int>(0x804a8310u);
constexpr int InvalidParamId = static_cast<int>(0x804a835au);
constexpr int InvalidSystemQueryId = static_cast<int>(0x804a835bu);
constexpr int InvalidStreamStateCmd = static_cast<int>(0x804a8400u);
constexpr int InvalidStreamCmdId = static_cast<int>(0x804a8401u);
constexpr int StreamBufSize3Zero = static_cast<int>(0x804a8402u);
constexpr int StreamBufSize1Zero = static_cast<int>(0x804a8403u);
constexpr int StreamBufSize2Zero = static_cast<int>(0x804a8404u);
constexpr int SystemIndexRange = static_cast<int>(0x804a8410u);

template <typename TFunc>
void AssertThrows(TFunc&& func) {
    bool threw = false;
    try {
        func();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

void TestSystemCommandsAndQueries() {
    Require(sceNgs2SystemRunCommands(0xdead, nullptr, 0) == InvalidSystemHandle);
    Require(sceNgs2SystemQueryInfo(0xdead, 0xf04000, nullptr, 0) == InvalidSystemHandle);

    const auto system = CreateSystem();

    Require(sceNgs2SystemRunCommands(system, nullptr, 0) == SCE_NGS2_OK);

    CommandRecord badCmd{};
    badCmd.id = 0x123456;
    Require(sceNgs2SystemRunCommands(system, &badCmd, 1) == InvalidParamId);

    CommandRecord badRateCmd{};
    badRateCmd.id = 0xf00004;
    badRateCmd.value.u32 = 47999;
    Require(sceNgs2SystemRunCommands(system, &badRateCmd, 1) == InvalidSampleRate);

    CommandRecord badGrainCmd{};
    badGrainCmd.id = 0xf00005;
    badGrainCmd.value.u32 = 63;
    Require(sceNgs2SystemRunCommands(system, &badGrainCmd, 1) == InvalidNumGrainSamples);

    CommandRecord badSlotCmd{};
    badSlotCmd.id = 0x20000003;
    Require(sceNgs2SystemRunCommands(system, &badSlotCmd, 1) == SystemIndexRange);

    std::array<CommandRecord, 2> atomicBatch{};
    atomicBatch[0].id = 0xf00002;
    atomicBatch[0].value.u64 = 0xbeef;
    atomicBatch[1].id = 0xf00004;
    atomicBatch[1].value.u32 = 99999;
    Require(sceNgs2SystemRunCommands(system, atomicBatch.data(), atomicBatch.size()) == InvalidSampleRate);

    std::uint64_t queriedUserData = 0;
    Require(sceNgs2SystemQueryInfo(system, 0xf04001, &queriedUserData, sizeof(queriedUserData)) == SCE_NGS2_OK);
    Require(queriedUserData == 0);

    std::array<CommandRecord, 4> goodBatch{};
    goodBatch[0].id = 0xf00000;
    goodBatch[1].id = 0xf00001;
    goodBatch[2].id = 0xf00002;
    goodBatch[2].value.u64 = 0x12345678ULL;
    goodBatch[3].id = 0xf00004;
    goodBatch[3].value.u32 = 44100;
    Require(sceNgs2SystemRunCommands(system, goodBatch.data(), goodBatch.size()) == SCE_NGS2_OK);

    queriedUserData = 0;
    Require(sceNgs2SystemQueryInfo(system, 0xf04001, nullptr, sizeof(queriedUserData)) == InvalidOutAddress);
    Require(sceNgs2SystemQueryInfo(system, 0xf04001, &queriedUserData, 4) == InvalidOutSize);
    Require(sceNgs2SystemQueryInfo(system, 0xf04001, &queriedUserData, sizeof(queriedUserData)) == SCE_NGS2_OK);
    Require(queriedUserData == 0x12345678ULL);

    Ngs2SystemInfo sysInfo{};
    Require(sceNgs2SystemQueryInfo(system, 0xf04000, nullptr, sizeof(sysInfo)) == InvalidOutAddress);
    Require(sceNgs2SystemQueryInfo(system, 0xf04000, &sysInfo, 0x10) == InvalidOutSize);
    Require(sceNgs2SystemQueryInfo(system, 0xf04000, &sysInfo, sizeof(sysInfo)) == SCE_NGS2_OK);
    Require(sysInfo.sample_rate == 44100);

    std::array<std::uint8_t, 24> slotPayload{};
    for (std::size_t i = 0; i < slotPayload.size(); ++i) slotPayload[i] = static_cast<std::uint8_t>(0x55 + i);

    CommandRecord slotCmd{};
    slotCmd.id = (3u << 24) | 0xf00003;
    slotCmd.value.ptr = slotPayload.data();
    Require(sceNgs2SystemRunCommands(system, &slotCmd, 1) == SCE_NGS2_OK);

    std::array<std::uint8_t, 24> slotReadback{};
    Require(sceNgs2SystemQueryInfo(system, (3u << 24) | 0xf04002, nullptr, sizeof(slotReadback)) == InvalidOutAddress);
    Require(sceNgs2SystemQueryInfo(system, (3u << 24) | 0xf04002, slotReadback.data(), 16) == InvalidOutSize);
    Require(sceNgs2SystemQueryInfo(system, (32u << 24) | 0xf04002, slotReadback.data(), sizeof(slotReadback)) == SystemIndexRange);
    Require(sceNgs2SystemQueryInfo(system, (3u << 24) | 0xf04002, slotReadback.data(), sizeof(slotReadback)) == SCE_NGS2_OK);
    Require(std::memcmp(slotReadback.data(), slotPayload.data(), sizeof(slotReadback)) == 0);

    const auto otherSystem = CreateSystem();
    std::array<std::uint8_t, 24> otherSlotReadback{};
    Require(sceNgs2SystemQueryInfo(otherSystem, (3u << 24) | 0xf04002, otherSlotReadback.data(), sizeof(otherSlotReadback)) == SCE_NGS2_OK);
    std::array<std::uint8_t, 24> zeroSlot{};
    Require(std::memcmp(otherSlotReadback.data(), zeroSlot.data(), sizeof(otherSlotReadback)) == 0);
    Require(sceNgs2SystemDestroy(otherSystem, nullptr) == SCE_NGS2_OK);

    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_REVERB);
    uintptr_t rackHandles[4] = {};
    const int rackCount = sceNgs2SystemQueryInfo(system, 0xf04003, rackHandles, sizeof(rackHandles));
    Require(rackCount == 1);
    Require(rackHandles[0] == rack);

    Require(sceNgs2SystemQueryInfo(system, 0xf04004, nullptr, 8) == InvalidOutAddress);
    AssertThrows([&] { sceNgs2SystemQueryInfo(system, 0xf04004, nullptr, 0); });
    AssertThrows([&] { sceNgs2SystemQueryInfo(system, 0xf04070, nullptr, 0); });
    Require(sceNgs2SystemQueryInfo(system, 0xdeadbeef, nullptr, 0) == InvalidSystemQueryId);

    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

void TestRackCommandsAndQueries() {
    Require(sceNgs2RackRunCommands(0xdead, nullptr, 0) == InvalidRackHandle);
    Require(sceNgs2RackQueryInfo(0xdead, 0xf14000, nullptr, 0) == InvalidRackHandle);

    const auto system = CreateSystem();
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_REVERB);

    Require(sceNgs2RackRunCommands(rack, nullptr, 0) == SCE_NGS2_OK);

    CommandRecord badRackCmd{};
    badRackCmd.id = 0xf00000;
    Require(sceNgs2RackRunCommands(rack, &badRackCmd, 1) == InvalidParamId);

    std::array<CommandRecord, 3> goodRackBatch{};
    goodRackBatch[0].id = 0xf10000;
    goodRackBatch[1].id = 0xf10001;
    goodRackBatch[2].id = 0xf10002;
    goodRackBatch[2].value.u64 = 0xcafebabeULL;
    Require(sceNgs2RackRunCommands(rack, goodRackBatch.data(), goodRackBatch.size()) == SCE_NGS2_OK);

    std::uint64_t rackUserData = 0;
    Require(sceNgs2RackQueryInfo(rack, 0xf14001, &rackUserData, 4) == InvalidOutSize);
    Require(sceNgs2RackQueryInfo(rack, 0xf14001, nullptr, sizeof(rackUserData)) == InvalidOutAddress);
    Require(sceNgs2RackQueryInfo(rack, 0xf14001, &rackUserData, sizeof(rackUserData)) == SCE_NGS2_OK);
    Require(rackUserData == 0xcafebabeULL);

    Ngs2RackInfo rackInfo{};
    Require(sceNgs2RackQueryInfo(rack, 0xf14000, nullptr, sizeof(rackInfo)) == InvalidOutAddress);
    Require(sceNgs2RackQueryInfo(rack, 0xf14000, &rackInfo, 0x20) == InvalidOutSize);
    Require(sceNgs2RackQueryInfo(rack, 0xf14000, &rackInfo, sizeof(rackInfo)) == SCE_NGS2_OK);

    std::array<std::uint8_t, 32> modRecord{};
    Require(sceNgs2RackQueryInfo(rack, (5u << 24) | 0xf14002, modRecord.data(), sizeof(modRecord)) == InvalidRackModuleIndex);
    AssertThrows([&] { sceNgs2RackQueryInfo(rack, 0xf14070, nullptr, 0); });
    Require(sceNgs2RackQueryInfo(rack, 0x99999999, nullptr, 0) == InvalidRackQueryId);

    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

void TestStreamExports() {
    Require(sceNgs2StreamResetOption(nullptr) == InvalidOutAddress);
    StreamOption opt{};
    Require(sceNgs2StreamResetOption(&opt) == SCE_NGS2_OK);
    Require(opt.size == sizeof(StreamOption));
    Require(opt.num_blocks == 4);
    Require(opt.buffer_size_1 == 0x100);
    Require(opt.buffer_size_2 == 0x10);
    Require(opt.buffer_size_3 == 0x10000);
    Require(opt.default_param == 0.5f);

    Require(sceNgs2StreamQueryBufferSize(&opt, nullptr) == InvalidOutAddress);

    Ngs2ContextBufferInfo bufInfo{};
    Require(sceNgs2StreamQueryBufferSize(nullptr, &bufInfo) == SCE_NGS2_OK);
    Require(bufInfo.host_buffer_size == 0x106c0ULL);

    StreamOption badBlocks = opt;
    badBlocks.num_blocks = 0;
    Require(sceNgs2StreamQueryBufferSize(&badBlocks, &bufInfo) == InvalidNumBlocks);
    badBlocks.num_blocks = 33;
    Require(sceNgs2StreamQueryBufferSize(&badBlocks, &bufInfo) == InvalidNumBlocks);

    StreamOption badSize3 = opt;
    badSize3.buffer_size_3 = 0;
    Require(sceNgs2StreamQueryBufferSize(&badSize3, &bufInfo) == StreamBufSize3Zero);

    StreamOption badSize1 = opt;
    badSize1.buffer_size_1 = 0;
    Require(sceNgs2StreamQueryBufferSize(&badSize1, &bufInfo) == StreamBufSize1Zero);

    StreamOption badSize2 = opt;
    badSize2.buffer_size_2 = 0;
    Require(sceNgs2StreamQueryBufferSize(&badSize2, &bufInfo) == StreamBufSize2Zero);

    StreamOption badOptSize = opt;
    badOptSize.size = 0x80;
    Require(sceNgs2StreamQueryBufferSize(&badOptSize, &bufInfo) == InvalidOptionSize);

    const auto system = CreateSystem();
    uintptr_t streamHandle = 0;

    Require(sceNgs2StreamCreate(system, &opt, nullptr, &streamHandle) == InvalidBufferInfo);
    Ngs2ContextBufferInfo emptyHost{};
    Require(sceNgs2StreamCreate(system, &opt, &emptyHost, &streamHandle) == InvalidBufferInfo);
    bufInfo.host_buffer = reinterpret_cast<void*>(0x1000);
    Require(sceNgs2StreamCreate(system, &opt, &bufInfo, nullptr) == InvalidOutAddress);
    Require(sceNgs2StreamCreate(0xdead, &opt, &bufInfo, &streamHandle) == InvalidSystemHandle);
    AssertThrows([&] { sceNgs2StreamCreate(system, &opt, &bufInfo, &streamHandle); });

    Ngs2BufferAllocator alloc{};
    Require(sceNgs2StreamCreateWithAllocator(system, &opt, nullptr, &streamHandle) == InvalidBufferAllocator);
    Require(sceNgs2StreamCreateWithAllocator(system, &opt, &alloc, &streamHandle) == InvalidBufferAllocator);
    alloc.alloc_handler = [](Ngs2ContextBufferInfo*) -> int { return 0; };
    Require(sceNgs2StreamCreateWithAllocator(system, &opt, &alloc, nullptr) == InvalidOutAddress);
    Require(sceNgs2StreamCreateWithAllocator(0xdead, &opt, &alloc, &streamHandle) == InvalidSystemHandle);
    AssertThrows([&] { sceNgs2StreamCreateWithAllocator(system, &opt, &alloc, &streamHandle); });

    Require(sceNgs2StreamDestroy(0, nullptr) == InvalidStreamHandle);
    Require(sceNgs2StreamRunCommands(0, nullptr, 0) == InvalidStreamHandle);
    Require(sceNgs2StreamQueryInfo(0, 0xf24001, nullptr, 0) == InvalidStreamHandle);

    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

void TestModuleEnumerations() {
    void* configTable[4] = {};
    AssertThrows([&] { sceNgs2ModuleEnumConfigs(configTable, 4); });
    Require(sceNgs2ModuleArrayEnumItems(nullptr, nullptr, 0) == 0);
    Require(sceNgs2ModuleQueueEnumItems(nullptr, nullptr, 0) == 0);
    std::uint8_t dummyQ[16]{};
    AssertThrows([&] { sceNgs2ModuleQueueEnumItems(dummyQ, nullptr, 0); });

    alignas(8) std::uint8_t arrayData[64] = {};
    const std::uint32_t itemSize = 4;
    const std::uint32_t count = 3;
    const std::uint8_t flag = 1;
    arrayData[8] = flag;
    std::memcpy(arrayData + 0xc, &itemSize, sizeof(itemSize));
    std::memcpy(arrayData + 0x14 + 4 * flag, &count, sizeof(count));
    const std::uint32_t sampleVal1 = 0x11111111;
    const std::uint32_t sampleVal2 = 0x22222222;
    const std::uint32_t sampleVal3 = 0x33333333;
    std::memcpy(arrayData + 0x20, &sampleVal1, 4);
    std::memcpy(arrayData + 0x24, &sampleVal2, 4);
    std::memcpy(arrayData + 0x28, &sampleVal3, 4);

    std::uint32_t outputItems[4] = {};
    const int enumerated = sceNgs2ModuleArrayEnumItems(arrayData, outputItems, 2);
    Require(enumerated == 3);
    Require(outputItems[0] == sampleVal1);
    Require(outputItems[1] == sampleVal2);
    Require(outputItems[2] == 0);
}

}

int main() {
    TestSystemCommandsAndQueries();
    TestRackCommandsAndQueries();
    TestStreamExports();
    TestModuleEnumerations();
    return 0;
}
