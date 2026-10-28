#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VRAMBUDGET_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_VRAMBUDGET_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
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
    static VkDeviceSize TargetFor(VkDeviceSize size, VkDeviceSize available);
    bool Enabled() const { return heap.has_value(); }
    void Read(const VkPhysicalDeviceMemoryBudgetPropertiesEXT& reported);
    VkDeviceSize Excess() const;
    void Released(VkDeviceSize bytes);
    VramReport Report() const;

private:
    VkDeviceSize target() const;
    std::optional<std::uint32_t> heap;
    VkDeviceSize heapBytes = 0;
    VkDeviceSize limit = 0;
    bool announced = false;
    VkDeviceSize budget = 0;
    VkDeviceSize usage = 0;
    VkDeviceSize released = 0;
};

}

#endif
