#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MEMORYACCOUNTING_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MEMORYACCOUNTING_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <string>
#include <vector>

namespace AgcDriver::Graphics {

class MemoryAccounting {
public:
    struct Allocation {
        VkDeviceMemory memory;
        VkDeviceSize bytes;
        VkDeviceSize retained;
        std::uint32_t heap;
        std::string category;
    };
    struct Snapshot {
        std::vector<Allocation> allocations;
        bool complete = true;
    };

    static bool Enabled();
    static void Record(VkDevice device, VkDeviceMemory memory, const VkMemoryAllocateInfo& allocation, const VkPhysicalDeviceMemoryProperties& properties, const char* category) noexcept;
    static VkResult Allocate(const Context& context, const VkMemoryAllocateInfo& allocation, const char* category, VkDeviceMemory* memory);
    static void Free(PFN_vkFreeMemory freeMemory, VkDevice device, VkDeviceMemory memory);
    static void Free(const Context& context, VkDeviceMemory memory);
    static void Use(VkDevice device, VkDeviceMemory memory, const char* category = nullptr) noexcept;
    static void Retain(VkDevice device, VkDeviceMemory memory, VkDeviceSize bytes) noexcept;
    static void RetainPart(VkDevice device, VkDeviceMemory memory, VkDeviceSize bytes, bool retained) noexcept;
    static void ForgetDevice(VkDevice device) noexcept;
    static Snapshot Read(VkDevice device);
    static void Report(const Context& context, const VkPhysicalDeviceMemoryBudgetPropertiesEXT& reported, std::uint64_t sampledBytes, std::uint64_t storageBytes, std::uint64_t storageBudget) noexcept;
};

}

#endif
