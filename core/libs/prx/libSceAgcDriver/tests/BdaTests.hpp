#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_BDATESTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_BDATESTS_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <functional>
#include <span>

struct BdaTestAccess {
    std::function<std::span<std::byte>(VkBuffer)> bytes;
    std::function<VkDescriptorBufferInfo(std::uint32_t)> descriptor;
    std::function<std::span<std::byte>(VkDeviceAddress)> addressBytes;
};

void RunBdaResourceTests(const AgcDriver::Graphics::Context& context, const BdaTestAccess& access);
void RunGuestAllocationTests();
void RunGuestLeaseWaitTests();
void RunUnmappedGapTests();
void RunColorTargetLayoutTests();
void RunLiveStackAccessTests();

#endif
