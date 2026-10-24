#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_BDATESTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_BDATESTS_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <functional>
#include <optional>
#include <span>

struct BdaTestAccess {
    std::function<std::span<std::byte>(VkBuffer)> bytes;
    std::function<VkDescriptorBufferInfo(std::uint32_t)> descriptor;
    std::function<std::span<std::byte>(VkDeviceAddress)> addressBytes;
    std::function<void(std::optional<VkDeviceSize>)> limitMemory;
    std::function<std::uint64_t()> allocationAttempts;
    std::function<void(std::optional<VkDeviceSize>)> limitImports;
};

void RunBdaResourceTests(const AgcDriver::Graphics::Context& context, const BdaTestAccess& access);
bool RunSparseBufferHoleTests(const AgcDriver::Graphics::Context& context, const BdaTestAccess& access);
void RunHeapMirrorUnwatchTests(const AgcDriver::Graphics::Context& context, const BdaTestAccess& access, bool throughImport);
void RunSparseHeapMirrorUnwatchTests(const AgcDriver::Graphics::Context& context, const BdaTestAccess& access);
void RunSparseHeapMirrorBudgetTests(const AgcDriver::Graphics::Context& context, const BdaTestAccess& access);
void RunHeapMirrorRunUnwatchTests(const AgcDriver::Graphics::Context& context, const BdaTestAccess& access);
void RunGuestAllocationTests();
void RunGuestLeaseWaitTests();
void RunUnmappedGapTests();
void RunColorTargetLayoutTests();
void RunLiveStackAccessTests();

#endif
