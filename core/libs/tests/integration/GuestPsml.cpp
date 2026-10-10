#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement1100(void* requirement, const void* param);
std::int32_t APS5_VABI scePsmlMfsrCreateContext1100(void** context, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1100(void* context, void* commandBuffer, const void* param);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::int32_t errNotInitialized = static_cast<std::int32_t>(0x8A810001);

struct CommandBuffer {
    std::uint32_t* cursor;
    std::uint32_t sizeInDwords;
};

struct DispatchFixture {
    DispatchFixture() {
        dwords.fill(0xa5a5a5a5u);
        untouched = dwords;
        commandBuffer = CommandBuffer{dwords.data(), static_cast<std::uint32_t>(dwords.size())};
    }

    std::array<std::uint8_t, 0x158> param{};
    std::array<std::uint32_t, 64> dwords{};
    std::array<std::uint32_t, 64> untouched{};
    CommandBuffer commandBuffer{};
};

const Case requirement{"GetContextBufferRequirement_Uninitialized_ReturnsNotInitializedWithoutWriting", [] {
    const std::array<std::uint8_t, 0x158> param{};
    std::array<std::uint8_t, 0x18> requirementBuffer{};
    requirementBuffer.fill(0xa5);
    const auto untouched = requirementBuffer;
    RequireEqual(scePsmlMfsrGetContextBufferRequirement1100(requirementBuffer.data(), param.data()), errNotInitialized, "requirement query");
    Require(requirementBuffer == untouched, "the requirement buffer was modified");
}};

const Case requirementNull{"GetContextBufferRequirement_NullArguments_ReturnsNotInitialized", [] {
    RequireEqual(scePsmlMfsrGetContextBufferRequirement1100(nullptr, nullptr), errNotInitialized, "requirement query with null arguments");
}};

const Case createContext{"CreateContext_Uninitialized_ReturnsNotInitializedWithoutWriting", [] {
    const std::array<std::uint8_t, 0x158> param{};
    std::uint64_t contextStorage = 0;
    void* const sentinel = &contextStorage;
    void* context = sentinel;
    RequireEqual(scePsmlMfsrCreateContext1100(&context, param.data()), errNotInitialized, "create context");
    Require(context == sentinel, "the context pointer was modified");
}};

const Case createContextNull{"CreateContext_NullArguments_ReturnsNotInitialized", [] {
    RequireEqual(scePsmlMfsrCreateContext1100(nullptr, nullptr), errNotInitialized, "create context with null arguments");
}};

const Case dispatch{"GetDispatchMfsrPacket_Uninitialized_ReturnsNotInitializedWithoutWriting", [] {
    DispatchFixture fixture;
    std::uint64_t contextStorage = 0;
    RequireEqual(scePsmlMfsrGetDispatchMfsrPacket1100(&contextStorage, &fixture.commandBuffer, fixture.param.data()), errNotInitialized, "dispatch packet");
    Require(fixture.commandBuffer.cursor == fixture.dwords.data(), "the command buffer cursor moved");
    RequireEqual(fixture.commandBuffer.sizeInDwords, static_cast<std::uint32_t>(fixture.dwords.size()), "command buffer size");
    Require(fixture.dwords == fixture.untouched, "the command buffer contents were modified");
}};

const Case dispatchNullContext{"GetDispatchMfsrPacket_NullContext_ReturnsNotInitializedWithoutWriting", [] {
    DispatchFixture fixture;
    RequireEqual(scePsmlMfsrGetDispatchMfsrPacket1100(nullptr, &fixture.commandBuffer, fixture.param.data()), errNotInitialized, "dispatch packet with a null context");
    Require(fixture.commandBuffer.cursor == fixture.dwords.data(), "the command buffer cursor moved");
    Require(fixture.dwords == fixture.untouched, "the command buffer contents were modified");
}};

} // namespace
