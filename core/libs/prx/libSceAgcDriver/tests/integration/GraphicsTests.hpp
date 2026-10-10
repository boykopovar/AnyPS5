#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_GRAPHICSTESTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_GRAPHICSTESTS_HPP

#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

template<typename TAction>
void RequireRejection(const TAction& action, std::string_view reason, std::source_location location = std::source_location::current()) {
    const auto error = Testing::RequireThrows<std::runtime_error>(action, "expected a rejection naming '" + std::string(reason) + "'", location);
    Testing::Require(std::string_view(error.what()).find(reason) != std::string_view::npos, "unexpected failure: " + std::string(error.what()) + ", expected '" + std::string(reason) + "'", location);
}

struct BdaTestAccess {
    std::function<std::span<std::byte>(VkBuffer)> bytes;
    std::function<VkDescriptorBufferInfo(std::uint32_t)> descriptor;
    std::function<std::span<std::byte>(VkDeviceAddress)> addressBytes;
    std::function<void(std::optional<VkDeviceSize>)> limitMemory;
    std::function<std::uint64_t()> allocationAttempts;
};

struct DetilerCapture {
    std::uint32_t groupsX;
    std::uint32_t groupsY;
    std::uint32_t groupsZ;
    std::vector<std::byte> pushConstants;
    VkBuffer sourceBuffer;
    VkDeviceSize sourceOffset;
    VkDeviceSize sourceRange;
    VkBuffer destinationBuffer;
    VkDeviceSize destinationOffset;
    VkDeviceSize destinationRange;
};

struct TextureDetilerTestAccess {
    std::function<VkBuffer(std::uint64_t)> makeBuffer;
    std::function<std::vector<std::byte>&(VkBuffer)> bytes;
    std::function<DetilerCapture()> lastDispatch;
    std::function<std::uint32_t()> pipelineCount;
    std::function<std::array<std::uint32_t, 3>()> lastSpecialization;
};

class MockVulkanSession {
public:
    MockVulkanSession();
    ~MockVulkanSession();
    MockVulkanSession(const MockVulkanSession&) = delete;
    MockVulkanSession& operator=(const MockVulkanSession&) = delete;

    std::int64_t LiveObjects() const;
};

template<typename TBody>
void WithMockVulkan(const TBody& body, std::string_view what, std::source_location location = std::source_location::current()) {
    MockVulkanSession session;
    body();
    Testing::RequireEqual(session.LiveObjects(), std::int64_t{0}, std::string(what) + " leaked Vulkan objects", location);
}

AgcDriver::Graphics::Context BdaMockContext();
BdaTestAccess BdaMockAccess();
AgcDriver::Graphics::Context DetilerMockContext();
TextureDetilerTestAccess DetilerMockAccess();

#ifdef _WIN32
void MainImageRegistrationCase();
#endif

#endif // CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_GRAPHICSTESTS_HPP
