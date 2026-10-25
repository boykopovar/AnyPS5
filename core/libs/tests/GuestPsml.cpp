#include "prx/libScePsml_debug/libScePsml_debug.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr std::int32_t kErrNotInitialized = static_cast<std::int32_t>(0x8A810001);
constexpr std::int32_t kErrInvalidArgument = static_cast<std::int32_t>(0x8A810005);
constexpr std::int32_t kErrNullOutput = static_cast<std::int32_t>(0x8A810009);

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

template <typename TCall>
bool Throws(TCall call, const char* text) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find(text) != std::string::npos;
    }
    return false;
}

struct ContextParam {
    std::array<std::uint8_t, 0x60> bytes{};

    void Set(const MfsrBlock* blocks, std::uint64_t count, void* shared) {
        std::memcpy(bytes.data() + 0x08, &blocks, sizeof(blocks));
        std::memcpy(bytes.data() + 0x10, &count, sizeof(count));
        std::memcpy(bytes.data() + 0x38, &shared, sizeof(shared));
    }

    void SetInputDimensions(std::uint16_t width, std::uint16_t height) {
        std::memcpy(bytes.data() + 0x18, &width, sizeof(width));
        std::memcpy(bytes.data() + 0x1a, &height, sizeof(height));
    }

    void SetDescriptor0(const void* descriptor) {
        std::memcpy(bytes.data() + 0x20, &descriptor, sizeof(descriptor));
    }
};

std::vector<MfsrBlock> Blocks(const MfsrRequirement& requirement, std::uint64_t first) {
    std::vector<MfsrBlock> blocks(requirement.blockCount);
    for (std::size_t i = 0; i < blocks.size(); ++i) blocks[i] = {first + i * requirement.blockBytes, requirement.blockBytes};
    return blocks;
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

void SetupAfterInit() {
    Require(scePsmlMfsrInit() == 0);
    Require(scePsmlMfsrInit() == 0);

    MfsrSharedResourcesParam sharedParam{};
    MfsrRequirement shared{};
    Require(scePsmlMfsrGetSharedResourcesInitRequirement(&shared, &sharedParam) == 0);
    Require(shared.blockBytes == 0x200000 && shared.alignment == 0x200000 && shared.blockCount == 52);

    const auto sharedBlocks = Blocks(shared, 0x10000000);
    sharedParam.blocks = sharedBlocks.data();
    sharedParam.blockCount = sharedBlocks.size();
    void* sharedResources = nullptr;
    Require(scePsmlMfsrCreateSharedResources(&sharedResources, &sharedParam) == 0);
    Require(sharedResources != nullptr);

    ContextParam contextParam;
    contextParam.Set(nullptr, 0, sharedResources);
    MfsrRequirement context{};
    Require(scePsmlMfsrGetContextBufferRequirement800M3_2(&context, reinterpret_cast<const MfsrContextParam*>(contextParam.bytes.data())) == 0);
    Require(context.blockBytes == 0x200000 && context.alignment == 0x200000 && context.blockCount == 4);

    const auto contextBlocks = Blocks(context, 0x20000000);
    contextParam.Set(contextBlocks.data(), contextBlocks.size(), sharedResources);
    void* mfsrContext = nullptr;
    Require(scePsmlMfsrCreateContext800M3_2(&mfsrContext, reinterpret_cast<const MfsrContextParam*>(contextParam.bytes.data())) == 0);
    Require(mfsrContext != nullptr && mfsrContext != sharedResources);

    std::uint32_t packetDwords = 0;
    Require(scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(mfsrContext, &packetDwords) == 0);
    Require(packetDwords == 0x600);
    Require(scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(mfsrContext, nullptr) == kErrNullOutput);
    Require(scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(sharedResources, &packetDwords) == 0);

    std::uint64_t foreign = 0;
    Require(scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(&foreign, &packetDwords) == kErrInvalidArgument);
    Require(scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(nullptr, &packetDwords) == kErrInvalidArgument);

    float bias = 42.0f;
    Require(scePsmlMfsrGetMipmapBias(mfsrContext, &bias, 3840) == kErrInvalidArgument);
    Require(bias == 42.0f);
    Require(scePsmlMfsrGetMipmapBias(&foreign, &bias, 3840) == kErrInvalidArgument);
    Require(scePsmlMfsrGetMipmapBias(sharedResources, &bias, 3840) == kErrInvalidArgument);

    Require(scePsmlMfsrSelectConfig(mfsrContext) == 0);
    Require(scePsmlMfsrSelectConfig(&foreign) == kErrInvalidArgument);
    Require(scePsmlMfsrRequestCapture(mfsrContext) == 0);
    Require(scePsmlMfsrRequestCapture(nullptr) == kErrInvalidArgument);
    Require(scePsmlMfsrIsCaptureInProgress(mfsrContext) == 0);
    Require(scePsmlMfsrIsCaptureInProgress(&foreign) == kErrInvalidArgument);

    ContextParam dimensionsParam;
    dimensionsParam.Set(contextBlocks.data(), contextBlocks.size(), sharedResources);
    dimensionsParam.SetInputDimensions(1920, 1080);
    void* dimensionContext = nullptr;
    Require(scePsmlMfsrCreateContext800M3_2(&dimensionContext, reinterpret_cast<const MfsrContextParam*>(dimensionsParam.bytes.data())) == 0);
    Require(scePsmlMfsrGetMipmapBias(dimensionContext, &bias, 3840) == 0);
    Require(bias == 0.0f);
    Require(scePsmlMfsrGetMipmapBias(dimensionContext, &bias, 1920) == 0);
    Require(bias == -1.0f);
    Require(scePsmlMfsrGetMipmapBias(dimensionContext, nullptr, 1920) == kErrNullOutput);
    Require(scePsmlMfsrReleaseContext(dimensionContext) == 0);

    std::array<std::uint8_t, 0x40> descriptor{};
    const std::uint16_t descriptorWidth = 1280, descriptorHeight = 720;
    std::memcpy(descriptor.data() + 0x20, &descriptorWidth, sizeof(descriptorWidth));
    std::memcpy(descriptor.data() + 0x22, &descriptorHeight, sizeof(descriptorHeight));
    ContextParam descriptorParam;
    descriptorParam.Set(contextBlocks.data(), contextBlocks.size(), sharedResources);
    descriptorParam.SetDescriptor0(descriptor.data());
    void* descriptorContext = nullptr;
    Require(scePsmlMfsrCreateContext800M3_2(&descriptorContext, reinterpret_cast<const MfsrContextParam*>(descriptorParam.bytes.data())) == 0);
    Require(scePsmlMfsrGetMipmapBias(descriptorContext, &bias, 2560) == 0);
    Require(bias == 0.0f);
    Require(scePsmlMfsrReleaseContext(descriptorContext) == 0);

    Require(Throws([&] { scePsmlMfsrReleaseSharedResources(sharedResources); }, "still used by a context"));
    Require(Throws([&] { scePsmlMfsrReleaseContext(sharedResources); }, "not created by this library"));
    Require(scePsmlMfsrReleaseContext(mfsrContext) == 0);
    Require(Throws([&] { scePsmlMfsrReleaseContext(mfsrContext); }, "not created by this library"));
    Require(scePsmlMfsrReleaseSharedResources(sharedResources) == 0);
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

void RefusedAfterInit() {
    MfsrSharedResourcesParam sharedParam{};
    MfsrRequirement requirement{};
    sharedParam.type = 1;
    Require(Throws([&] { scePsmlMfsrGetSharedResourcesInitRequirement(&requirement, &sharedParam); }, "type 1 is not supported"));

    sharedParam.type = 0;
    Require(scePsmlMfsrGetSharedResourcesInitRequirement(&requirement, &sharedParam) == 0);
    const auto blocks = Blocks(requirement, 0x10000000);
    sharedParam.blocks = blocks.data();
    sharedParam.blockCount = 51;
    void* sharedResources = nullptr;
    Require(Throws([&] { scePsmlMfsrCreateSharedResources(&sharedResources, &sharedParam); }, "fewer memory blocks"));
    auto small = blocks;
    small[2].bytes = 0x100000;
    sharedParam.blocks = small.data();
    sharedParam.blockCount = small.size();
    Require(Throws([&] { scePsmlMfsrCreateSharedResources(&sharedResources, &sharedParam); }, "smaller or less aligned"));
    Require(sharedResources == nullptr);

    std::uint64_t foreign = 0;
    ContextParam contextParam;
    contextParam.Set(blocks.data(), blocks.size(), &foreign);
    void* context = nullptr;
    Require(Throws([&] { scePsmlMfsrCreateContext800M3_2(&context, reinterpret_cast<const MfsrContextParam*>(contextParam.bytes.data())); }, "shared resources handle was not created"));
    Require(context == nullptr);

    std::array<std::uint8_t, 0x158> param{};
    std::array<std::uint32_t, 64> dwords{};
    MfsrCommandBuffer commandBuffer{dwords.data(), static_cast<std::uint32_t>(dwords.size())};
    Require(Throws([&] { scePsmlMfsrGetContextBufferRequirement1100(&requirement, param.data()); }, "not implemented"));
    Require(Throws([&] { scePsmlMfsrCreateContext1300(&context, param.data()); }, "not implemented"));
    Require(Throws([&] { scePsmlMfsrGetDispatchMfsrPacket900(&foreign, &commandBuffer, param.data()); }, "not implemented"));
    Require(Throws([&] { scePsmlMfsrGetDispatchMfsrPacket1000(&foreign, &commandBuffer, param.data()); }, "not implemented"));
    Require(Throws([&] { scePsmlMfsrGetDispatchMfsrPacket1300(&foreign, &commandBuffer, param.data()); }, "not implemented"));
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
    Mfsr2OnBaseConsole();
    SetupAfterInit();
    RefusedAfterInit();
}
