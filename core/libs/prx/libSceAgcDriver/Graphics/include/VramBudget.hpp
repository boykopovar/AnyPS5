#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VRAMBUDGET_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VRAMBUDGET_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>

namespace AgcDriver::Graphics {

struct VramReport {
    std::uint32_t heap = 0;
    VkDeviceSize heapBytes = 0;
    VkDeviceSize budget = 0;
    VkDeviceSize usage = 0;
    VkDeviceSize target = 0;
};

class VramBudget {
public:
    explicit VramBudget(const Context& context);
    VramBudget(const VramBudget&) = delete;
    VramBudget& operator=(const VramBudget&) = delete;
    static VkDeviceSize HeadroomFor(VkDeviceSize size);
    static VkDeviceSize TargetFor(VkDeviceSize size, VkDeviceSize available, VkDeviceSize reserve);
    bool Enabled() const { return heap.has_value(); }
    void Refresh();
    VkDeviceSize Excess();
    void Released(VkDeviceSize bytes);
    VramReport Report() const;

private:
    VkDeviceSize target() const;
    PFN_vkGetPhysicalDeviceMemoryProperties2 query = nullptr;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    std::optional<std::uint32_t> heap;
    VkDeviceSize heapBytes = 0;
    VkDeviceSize limit = 0;
    VkDeviceSize headroom = 0;
    std::mutex refreshMutex;
    bool announced = false;
    std::atomic<std::int64_t> readAt{0};
    std::atomic<VkDeviceSize> budget{0};
    std::atomic<VkDeviceSize> usage{0};
    std::atomic<VkDeviceSize> released{0};
};

}

#endif
