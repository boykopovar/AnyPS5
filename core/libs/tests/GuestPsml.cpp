#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

extern "C" {
std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement1100(void* requirement, const void* param);
std::int32_t APS5_VABI scePsmlMfsrCreateContext1100(void** context, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1100(void* context, void* commandBuffer, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetSharedResourcesInitRequirement(void* requirement, const void* param);
std::int32_t APS5_VABI scePsmlMfsrCreateSharedResources(void* sharedResources, const void* param);
std::int32_t APS5_VABI scePsmlMfsrReleaseSharedResources(void* sharedResources);
std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement800M3_2(void* requirement, const void* param);
std::int32_t APS5_VABI scePsmlMfsrCreateContext800M3_2(void* context, const void* param);
std::int32_t APS5_VABI scePsmlMfsrReleaseContext(void* context);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(const void* context, std::uint32_t* sizeInDwords);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket900(void* context, void* commandBuffer, const void* param);
std::int32_t APS5_VABI scePsmlMfsrRequestCapture(const void* object);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr std::int32_t kErrNotInitialized = static_cast<std::int32_t>(0x8A810001);

struct CommandBuffer {
    std::uint32_t* cursor;
    std::uint32_t sizeInDwords;
};

struct SharedResourcesParam {
    std::uint32_t type;
    std::uint32_t reserved;
    const std::uint64_t* blocks;
    std::uint64_t blockCount;
    std::uint64_t virtualAddressStart;
};

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

    std::array<std::uint64_t, 3> memoryRequirement;
    memoryRequirement.fill(0xa5a5a5a5a5a5a5a5ull);
    const auto untouchedMemoryRequirement = memoryRequirement;
    std::array<std::uint32_t, 2> memoryParam{0, 0};
    Require(scePsmlMfsrGetSharedResourcesInitRequirement(memoryRequirement.data(), memoryParam.data()) == kErrNotInitialized);
    Require(memoryRequirement == untouchedMemoryRequirement);
    Require(scePsmlMfsrGetSharedResourcesInitRequirement(nullptr, nullptr) == kErrNotInitialized);

    Require(scePsmlMfsrGetContextBufferRequirement800M3_2(memoryRequirement.data(), param.data()) == kErrNotInitialized);
    Require(memoryRequirement == untouchedMemoryRequirement);
    Require(scePsmlMfsrGetContextBufferRequirement800M3_2(nullptr, nullptr) == kErrNotInitialized);

    std::array<std::uint64_t, 2> block{0x10000000ull, 0x200000ull};
    const SharedResourcesParam sharedParam{0, 0, block.data(), 1, 0};
    std::array<std::uint8_t, 0x30> sharedResources;
    sharedResources.fill(0xa5);
    const auto untouchedSharedResources = sharedResources;
    Require(scePsmlMfsrCreateSharedResources(sharedResources.data(), &sharedParam) == kErrNotInitialized);
    Require(sharedResources == untouchedSharedResources);
    Require(scePsmlMfsrCreateSharedResources(nullptr, nullptr) == kErrNotInitialized);
    Require(scePsmlMfsrReleaseSharedResources(sharedResources.data()) == kErrNotInitialized);
    Require(sharedResources == untouchedSharedResources);
    Require(scePsmlMfsrReleaseSharedResources(nullptr) == kErrNotInitialized);

    std::array<std::uint8_t, 0x370> contextMemory;
    contextMemory.fill(0xa5);
    const auto untouchedContextMemory = contextMemory;
    Require(scePsmlMfsrCreateContext800M3_2(contextMemory.data(), param.data()) == kErrNotInitialized);
    Require(contextMemory == untouchedContextMemory);
    Require(scePsmlMfsrCreateContext800M3_2(nullptr, nullptr) == kErrNotInitialized);
    Require(scePsmlMfsrReleaseContext(contextMemory.data()) == kErrNotInitialized);
    Require(contextMemory == untouchedContextMemory);
    Require(scePsmlMfsrReleaseContext(nullptr) == kErrNotInitialized);

    std::uint32_t sizeInDwords = 0xa5a5a5a5u;
    Require(scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(contextMemory.data(), &sizeInDwords) == kErrNotInitialized);
    Require(sizeInDwords == 0xa5a5a5a5u);
    Require(scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(nullptr, nullptr) == kErrNotInitialized);

    Require(scePsmlMfsrGetDispatchMfsrPacket900(contextMemory.data(), &commandBuffer, param.data()) == kErrNotInitialized);
    Require(commandBuffer.cursor == dwords.data());
    Require(commandBuffer.sizeInDwords == dwords.size());
    Require(dwords == untouchedDwords);
    Require(contextMemory == untouchedContextMemory);
    Require(scePsmlMfsrGetDispatchMfsrPacket900(nullptr, nullptr, nullptr) == kErrNotInitialized);

    Require(scePsmlMfsrRequestCapture(contextMemory.data()) == kErrNotInitialized);
    Require(contextMemory == untouchedContextMemory);
    Require(scePsmlMfsrRequestCapture(nullptr) == kErrNotInitialized);
}
