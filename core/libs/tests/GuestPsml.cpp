#include "prx/libScePsml_debug/libScePsml_debug.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr std::int32_t kErrNotInitialized = static_cast<std::int32_t>(0x8A810001);

using GetDispatchPacket = std::int32_t (APS5_VABI*)(void*, void*, const void*);

template <typename TParam>
void CreateContextUntouched(std::int32_t (APS5_VABI* create)(void**, const TParam*)) {
    std::array<std::uint8_t, 0x158> param{};
    std::uint64_t contextStorage = 0;
    void* const sentinel = &contextStorage;
    void* context = sentinel;
    Require(create(&context, reinterpret_cast<const TParam*>(param.data())) == kErrNotInitialized);
    Require(context == sentinel);
    Require(create(nullptr, nullptr) == kErrNotInitialized);
}

void DispatchPacketUntouched(GetDispatchPacket dispatch) {
    std::array<std::uint8_t, 0x158> param{};
    std::uint64_t contextStorage = 0;
    std::array<std::uint32_t, 64> dwords;
    dwords.fill(0xa5a5a5a5u);
    const auto untouchedDwords = dwords;
    MfsrCommandBuffer commandBuffer{dwords.data(), static_cast<std::uint32_t>(dwords.size())};
    Require(dispatch(&contextStorage, &commandBuffer, param.data()) == kErrNotInitialized);
    Require(dispatch(nullptr, &commandBuffer, nullptr) == kErrNotInitialized);
    Require(commandBuffer.cursor == dwords.data());
    Require(commandBuffer.sizeInDwords == dwords.size());
    Require(dwords == untouchedDwords);
}



void SetupBeforeInit() {
    std::uint64_t storage = 0;
    void* const sentinel = &storage;
    void* handle = sentinel;
    MfsrRequirement requirement{1, 2, 3};
    MfsrSharedResourcesParam sharedParam{};
    Require(scePsmlMfsrGetSharedResourcesInitRequirement(&requirement, &sharedParam) == kErrNotInitialized);
    Require(requirement.blockBytes == 1 && requirement.alignment == 2 && requirement.blockCount == 3);
    Require(scePsmlMfsrCreateSharedResources(&handle, &sharedParam) == kErrNotInitialized);
    Require(handle == sentinel);
    Require(scePsmlMfsrReleaseContext(sentinel) == kErrNotInitialized);
    Require(scePsmlMfsrReleaseSharedResources(sentinel) == kErrNotInitialized);

    std::uint32_t packetDwords = 0xa5a5a5a5;
    Require(scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(sentinel, &packetDwords) == kErrNotInitialized);
    Require(packetDwords == 0xa5a5a5a5);
    float bias = 0.5f;
    Require(scePsmlMfsrGetMipmapBias(sentinel, &bias, 3840) == kErrNotInitialized);
    Require(bias == 0.5f);
    Require(scePsmlMfsrSelectConfig(sentinel) == kErrNotInitialized);
    Require(scePsmlMfsrRequestCapture(sentinel) == kErrNotInitialized);
    Require(scePsmlMfsrIsCaptureInProgress(sentinel) == kErrNotInitialized);
}

void EnableFlagsReturnZero() {
    Require(scePsmlMfsrEnableSpecularAA(0) == 0);
    Require(scePsmlMfsrEnableSpecularAA(1) == 0);
    Require(scePsmlMfsrEnableQuality8K(0) == 0);
    Require(scePsmlMfsrEnableQuality8K(1) == 0);
    Require(scePsmlMfsrEnableLightWeight(0) == 0);
    Require(scePsmlMfsrEnableLightWeight(1) == 0);
}

void CreateContext1300SizeCheck() {
    constexpr std::int32_t unsupportedResolution = static_cast<std::int32_t>(0x8A810013);
    std::array<std::uint8_t, 0x30> param{};
    const auto setType = [&](std::uint32_t value) { std::memcpy(param.data(), &value, sizeof(value)); };
    const auto setDims = [&](std::uint16_t width, std::uint16_t height) {
        std::memcpy(param.data() + 0x18, &width, sizeof(width));
        std::memcpy(param.data() + 0x1a, &height, sizeof(height));
    };
    std::uint64_t contextStorage = 0;
    void* const sentinel = &contextStorage;
    void* context = sentinel;

    setType(0);
    for (const auto [w, h] : {std::pair{3841, 2160}, {3840, 2161}, {7680, 4320}}) {
        setDims(static_cast<std::uint16_t>(w), static_cast<std::uint16_t>(h));
        Require(scePsmlMfsrCreateContext1300(&context, param.data()) == unsupportedResolution);
    }
    for (const auto [w, h] : {std::pair{3840, 2160}, {1920, 1080}}) {
        setDims(static_cast<std::uint16_t>(w), static_cast<std::uint16_t>(h));
        Require(scePsmlMfsrCreateContext1300(&context, param.data()) == kErrNotInitialized);
    }
    setType(1);
    setDims(7680, 4320);
    Require(scePsmlMfsrCreateContext1300(&context, param.data()) == kErrNotInitialized);
    Require(scePsmlMfsrCreateContext1300(&context, nullptr) == kErrNotInitialized);
    Require(context == sentinel);
}

void Mfsr1OnBaseConsole() {
    constexpr std::int32_t unsupportedConsole = static_cast<std::int32_t>(0x8A810012);
    EnableFlagsReturnZero();
    Require(scePsmlMfsrInit() == unsupportedConsole);
    Require(scePsmlMfsrInit() == unsupportedConsole);
    SetupBeforeInit();
    EnableFlagsReturnZero();
    CreateContext1300SizeCheck();
}

void ConcurrentCreatesUninitialized() {
    std::atomic<int> errors{0};
    std::vector<std::thread> pool;
    for (int i = 0; i < 8; ++i) {
        pool.emplace_back([&] {
            for (int round = 0; round < 64; ++round) {
                MfsrSharedResourcesParam sharedParam{};
                void* handle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xdeadbeef));
                if (scePsmlMfsrCreateSharedResources(&handle, &sharedParam) != kErrNotInitialized || handle != reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xdeadbeef))) ++errors;
            }
        });
    }
    for (auto& worker : pool) worker.join();
    Require(errors == 0);
}

void Mfsr2OnBaseConsole() {
    constexpr std::int32_t notInitialized = static_cast<std::int32_t>(0x8A8A0001);
    constexpr std::int32_t unsupportedConsole = static_cast<std::int32_t>(0x8A8A0012);
    Require(scePsmlMfsr2Init() == unsupportedConsole);
    Require(scePsmlMfsr2Init() == unsupportedConsole);

    std::array<std::uint8_t, 0x30> param{};
    std::array<std::uint8_t, 0x18> requirement;
    requirement.fill(0xa5);
    const auto untouchedRequirement = requirement;
    Require(scePsmlMfsr2GetSharedResourcesInitRequirement(reinterpret_cast<MfsrRequirement*>(requirement.data()), reinterpret_cast<const Mfsr2SharedResourcesParam*>(param.data())) == notInitialized);
    Require(scePsmlMfsr2GetContextInitRequirement(reinterpret_cast<MfsrRequirement*>(requirement.data()), reinterpret_cast<const Mfsr2ContextParam*>(param.data())) == notInitialized);
    Require(requirement == untouchedRequirement);

    std::uint64_t storage = 0;
    void* const sentinel = &storage;
    void* handle = sentinel;
    Require(scePsmlMfsr2CreateSharedResources(&handle, reinterpret_cast<const Mfsr2SharedResourcesParam*>(param.data())) == notInitialized);
    Require(scePsmlMfsr2CreateContext(&handle, reinterpret_cast<const Mfsr2ContextParam*>(param.data())) == notInitialized);
    Require(handle == sentinel);
    Require(scePsmlMfsr2ReleaseContext(sentinel) == notInitialized);
    Require(scePsmlMfsr2ReleaseSharedResources(sentinel) == notInitialized);

    std::uint32_t packetDwords = 0xa5a5a5a5;
    Require(scePsmlMfsr2GetDispatchPacketsSizeInDwords(sentinel, &packetDwords) == notInitialized);
    Require(packetDwords == 0xa5a5a5a5);

    std::array<std::uint8_t, 112> dispatchParam{};
    std::array<std::uint32_t, 64> dwords;
    dwords.fill(0xa5a5a5a5u);
    const auto untouchedDwords = dwords;
    MfsrCommandBuffer commandBuffer{dwords.data(), static_cast<std::uint32_t>(dwords.size())};
    Require(scePsmlMfsr2GetDispatchPackets(sentinel, &commandBuffer, reinterpret_cast<const Mfsr2DispatchParam*>(dispatchParam.data())) == notInitialized);
    Require(commandBuffer.cursor == dwords.data());
    Require(commandBuffer.sizeInDwords == dwords.size());
    Require(dwords == untouchedDwords);
}

}

int main() {
    std::array<std::uint8_t, 0x158> param{};

    std::array<std::uint8_t, 0x18> requirement;
    requirement.fill(0xa5);
    const auto untouchedRequirement = requirement;
    Require(scePsmlMfsrGetContextBufferRequirement1100(requirement.data(), param.data()) == kErrNotInitialized);
    Require(requirement == untouchedRequirement);
    Require(scePsmlMfsrGetContextBufferRequirement1100(nullptr, nullptr) == kErrNotInitialized);

    std::uint64_t contextStorage = 0;
    void* const sentinel = &contextStorage;
    void* context = sentinel;
    Require(scePsmlMfsrCreateContext1100(&context, param.data()) == kErrNotInitialized);
    Require(context == sentinel);
    Require(scePsmlMfsrCreateContext1100(nullptr, nullptr) == kErrNotInitialized);

    std::array<std::uint32_t, 64> dwords;
    dwords.fill(0xa5a5a5a5u);
    const auto untouchedDwords = dwords;
    MfsrCommandBuffer commandBuffer{dwords.data(), static_cast<std::uint32_t>(dwords.size())};
    Require(scePsmlMfsrGetDispatchMfsrPacket1100(&contextStorage, &commandBuffer, param.data()) == kErrNotInitialized);
    Require(commandBuffer.cursor == dwords.data());
    Require(commandBuffer.sizeInDwords == dwords.size());
    Require(dwords == untouchedDwords);
    Require(scePsmlMfsrGetDispatchMfsrPacket1100(nullptr, &commandBuffer, param.data()) == kErrNotInitialized);
    Require(commandBuffer.cursor == dwords.data());
    Require(dwords == untouchedDwords);

    std::array<std::uint8_t, 0x18> olderRequirement;
    olderRequirement.fill(0xa5);
    const auto untouchedOlderRequirement = olderRequirement;
    Require(scePsmlMfsrGetContextBufferRequirement800M3_2(reinterpret_cast<MfsrRequirement*>(olderRequirement.data()), reinterpret_cast<const MfsrContextParam*>(param.data())) == kErrNotInitialized);
    Require(olderRequirement == untouchedOlderRequirement);
    Require(scePsmlMfsrGetContextBufferRequirement800M3_2(nullptr, nullptr) == kErrNotInitialized);

    CreateContextUntouched(scePsmlMfsrCreateContext800M3_2);
    CreateContextUntouched(scePsmlMfsrCreateContext1300);
    DispatchPacketUntouched(scePsmlMfsrGetDispatchMfsrPacket900);
    DispatchPacketUntouched(scePsmlMfsrGetDispatchMfsrPacket1000);
    DispatchPacketUntouched(scePsmlMfsrGetDispatchMfsrPacket1300);
    SetupBeforeInit();
    Mfsr1OnBaseConsole();
    Mfsr2OnBaseConsole();
    ConcurrentCreatesUninitialized();
}
