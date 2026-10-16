#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement1100(void* requirement, const void* param);
std::int32_t APS5_VABI scePsmlMfsrCreateContext1100(void** context, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1100(void* context, void* commandBuffer, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement800M3_2(void* requirement, const void* param);
std::int32_t APS5_VABI scePsmlMfsrCreateContext800M3_2(void** context, const void* param);
std::int32_t APS5_VABI scePsmlMfsrCreateContext1300(void** context, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket900(void* context, void* commandBuffer, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1000(void* context, void* commandBuffer, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1300(void* context, void* commandBuffer, const void* param);
std::int32_t APS5_VABI scePsmlMfsrInit();
std::int32_t APS5_VABI scePsmlMfsrGetSharedResourcesInitRequirement(void* requirement, const void* param);
std::int32_t APS5_VABI scePsmlMfsrCreateSharedResources(void** sharedResources, const void* param);
std::int32_t APS5_VABI scePsmlMfsrReleaseSharedResources(void* sharedResources);
std::int32_t APS5_VABI scePsmlMfsrReleaseContext(void* context);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr std::int32_t kErrNotInitialized = static_cast<std::int32_t>(0x8A810001);

struct CommandBuffer {
    std::uint32_t* cursor;
    std::uint32_t sizeInDwords;
};

using CreateContext = std::int32_t (APS5_VABI*)(void**, const void*);
using GetDispatchPacket = std::int32_t (APS5_VABI*)(void*, void*, const void*);

void CreateContextUntouched(CreateContext create) {
    std::array<std::uint8_t, 0x158> param{};
    std::uint64_t contextStorage = 0;
    void* const sentinel = &contextStorage;
    void* context = sentinel;
    Require(create(&context, param.data()) == kErrNotInitialized);
    Require(context == sentinel);
    Require(create(nullptr, nullptr) == kErrNotInitialized);
}

void DispatchPacketUntouched(GetDispatchPacket dispatch) {
    std::array<std::uint8_t, 0x158> param{};
    std::uint64_t contextStorage = 0;
    std::array<std::uint32_t, 64> dwords;
    dwords.fill(0xa5a5a5a5u);
    const auto untouchedDwords = dwords;
    CommandBuffer commandBuffer{dwords.data(), static_cast<std::uint32_t>(dwords.size())};
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

struct Requirement {
    std::uint64_t blockBytes;
    std::uint64_t alignment;
    std::uint64_t blockCount;
};

struct Block {
    std::uint64_t address;
    std::uint64_t bytes;
};

struct SharedParam {
    std::uint32_t type;
    std::uint32_t reserved;
    const Block* blocks;
    std::uint64_t blockCount;
    std::uint64_t reserved2;
};

struct ContextParam {
    std::array<std::uint8_t, 0x60> bytes{};

    void Set(const Block* blocks, std::uint64_t count, void* shared) {
        std::memcpy(bytes.data() + 0x08, &blocks, sizeof(blocks));
        std::memcpy(bytes.data() + 0x10, &count, sizeof(count));
        std::memcpy(bytes.data() + 0x38, &shared, sizeof(shared));
    }
};

std::array<Block, 4> Blocks(const Requirement& requirement, std::uint64_t first) {
    std::array<Block, 4> blocks{};
    for (std::size_t i = 0; i < blocks.size(); ++i) blocks[i] = {first + i * requirement.blockBytes, requirement.blockBytes};
    return blocks;
}

void SetupBeforeInit() {
    std::uint64_t storage = 0;
    void* const sentinel = &storage;
    void* handle = sentinel;
    Requirement requirement{1, 2, 3};
    SharedParam sharedParam{};
    Require(scePsmlMfsrGetSharedResourcesInitRequirement(&requirement, &sharedParam) == kErrNotInitialized);
    Require(requirement.blockBytes == 1 && requirement.alignment == 2 && requirement.blockCount == 3);
    Require(scePsmlMfsrCreateSharedResources(&handle, &sharedParam) == kErrNotInitialized);
    Require(handle == sentinel);
    Require(scePsmlMfsrReleaseContext(sentinel) == kErrNotInitialized);
    Require(scePsmlMfsrReleaseSharedResources(sentinel) == kErrNotInitialized);
}

void SetupAfterInit() {
    Require(scePsmlMfsrInit() == 0);
    Require(scePsmlMfsrInit() == 0);

    SharedParam sharedParam{};
    Requirement shared{};
    Require(scePsmlMfsrGetSharedResourcesInitRequirement(&shared, &sharedParam) == 0);
    Require(shared.blockBytes == 0x200000 && shared.alignment == 0x200000 && shared.blockCount == 4);

    const auto sharedBlocks = Blocks(shared, 0x10000000);
    sharedParam.blocks = sharedBlocks.data();
    sharedParam.blockCount = sharedBlocks.size();
    void* sharedResources = nullptr;
    Require(scePsmlMfsrCreateSharedResources(&sharedResources, &sharedParam) == 0);
    Require(sharedResources != nullptr);

    ContextParam contextParam;
    contextParam.Set(nullptr, 0, sharedResources);
    Requirement context{};
    Require(scePsmlMfsrGetContextBufferRequirement800M3_2(&context, contextParam.bytes.data()) == 0);
    Require(context.blockBytes == 0x200000 && context.alignment == 0x200000 && context.blockCount == 4);

    const auto contextBlocks = Blocks(context, 0x20000000);
    contextParam.Set(contextBlocks.data(), contextBlocks.size(), sharedResources);
    void* mfsrContext = nullptr;
    Require(scePsmlMfsrCreateContext800M3_2(&mfsrContext, contextParam.bytes.data()) == 0);
    Require(mfsrContext != nullptr && mfsrContext != sharedResources);

    Require(Throws([&] { scePsmlMfsrReleaseSharedResources(sharedResources); }, "still used by a context"));
    Require(Throws([&] { scePsmlMfsrReleaseContext(sharedResources); }, "not created by this library"));
    Require(scePsmlMfsrReleaseContext(mfsrContext) == 0);
    Require(Throws([&] { scePsmlMfsrReleaseContext(mfsrContext); }, "not created by this library"));
    Require(scePsmlMfsrReleaseSharedResources(sharedResources) == 0);
}

void RefusedAfterInit() {
    SharedParam sharedParam{};
    Requirement requirement{};
    sharedParam.type = 1;
    Require(Throws([&] { scePsmlMfsrGetSharedResourcesInitRequirement(&requirement, &sharedParam); }, "type 1 is not supported"));

    sharedParam.type = 0;
    Require(scePsmlMfsrGetSharedResourcesInitRequirement(&requirement, &sharedParam) == 0);
    const auto blocks = Blocks(requirement, 0x10000000);
    sharedParam.blocks = blocks.data();
    sharedParam.blockCount = 3;
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
    Require(Throws([&] { scePsmlMfsrCreateContext800M3_2(&context, contextParam.bytes.data()); }, "shared resources handle was not created"));
    Require(context == nullptr);

    std::array<std::uint8_t, 0x158> param{};
    std::array<std::uint32_t, 64> dwords{};
    CommandBuffer commandBuffer{dwords.data(), static_cast<std::uint32_t>(dwords.size())};
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
    CommandBuffer commandBuffer{dwords.data(), static_cast<std::uint32_t>(dwords.size())};
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
    Require(scePsmlMfsrGetContextBufferRequirement800M3_2(olderRequirement.data(), param.data()) == kErrNotInitialized);
    Require(olderRequirement == untouchedOlderRequirement);
    Require(scePsmlMfsrGetContextBufferRequirement800M3_2(nullptr, nullptr) == kErrNotInitialized);

    CreateContextUntouched(scePsmlMfsrCreateContext800M3_2);
    CreateContextUntouched(scePsmlMfsrCreateContext1300);
    DispatchPacketUntouched(scePsmlMfsrGetDispatchMfsrPacket900);
    DispatchPacketUntouched(scePsmlMfsrGetDispatchMfsrPacket1000);
    DispatchPacketUntouched(scePsmlMfsrGetDispatchMfsrPacket1300);
    SetupBeforeInit();
    SetupAfterInit();
    RefusedAfterInit();
}
