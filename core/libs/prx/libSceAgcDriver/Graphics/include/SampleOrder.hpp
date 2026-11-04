#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SAMPLEORDER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SAMPLEORDER_HPP

#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {

struct HostSampleLocations {
    VkExtent2D grid{1, 1};
    SampleOrder order{};
};

enum class SampleOrderMethod : std::uint8_t {
    Storage,
    Depth,
    DepthStencilExport,
    DepthStencilBits
};

struct SampleOrderStep {
    std::uint32_t attachment = 0;
    SampleOrderMethod method = SampleOrderMethod::Storage;
    bool toHost = false;
    bool toGuest = false;
};

struct SampleOrderPass {
    SampleOrder order{};
    VkExtent2D extent{};
    std::array<VkSampleLocationEXT, 32> locations{};
    bool located = false;
};

struct SampleOrderTarget {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    std::uint32_t samples = 1;
    SampleOrderMethod method = SampleOrderMethod::Storage;
    std::shared_ptr<void> keep;
};

struct DepthSampleLocations {
    VkImage image = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    std::uint32_t samples = 1;
    VkExtent2D grid{1, 1};
    std::array<VkSampleLocationEXT, 32> locations{};
    std::shared_ptr<void> keep;
};

HostSampleLocations HostSampleLocationsFor(const Context& context, const State& state);
bool HostsSampleLocationGrid(VkExtent2D largestGrid);
bool MayReorderSamples(const Context& context, std::uint32_t samples);
SampleOrderMethod DepthSampleOrderMethod(const Context& context, const DepthTarget& depth);
VkFormat SampleOrderStorageFormat(const Context& context, VkFormat format, std::uint32_t elementBytes, std::uint32_t samples);
std::string SampleOrderRefusal(const Context& context, const State& state);
std::vector<SampleOrderStep> PlanSampleOrder(const Context& context, const State& state, const SampleOrder& order, VkAttachmentLoadOp load = VK_ATTACHMENT_LOAD_OP_LOAD, VkAttachmentStoreOp store = VK_ATTACHMENT_STORE_OP_STORE);
SampleOrderPass SampleOrderPassFor(const Context& context, const State& state, const SampleOrder& order);
void RecordSampleOrder(const Context& context, VkCommandBuffer commands, const SampleOrderPass& pass, std::span<const SampleOrderTarget> targets, bool toHost, std::vector<std::shared_ptr<void>>& kept);
void RecordDepthSampleLocations(const Context& context, VkCommandBuffer commands, const DepthSampleLocations& depth);
std::array<std::span<const std::uint32_t>, 9> SampleOrderShaderModules();
void ClearSampleOrders(VkDevice device);

}

#endif
