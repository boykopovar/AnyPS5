#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_LIVEALLOCATIONS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_LIVEALLOCATIONS_HPP

#include <vulkan/vulkan.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {

inline constexpr std::array<VkDeviceSize, 5> SizeClasses{1ull << 20u, 16ull << 20u, 64ull << 20u, 256ull << 20u, ~0ull};
inline constexpr std::array<const char*, 5> SizeClassNames{"<1MiB", "<16MiB", "<64MiB", "<256MiB", ">=256MiB"};

inline std::size_t SizeClass(VkDeviceSize bytes) {
    std::size_t size = 0;
    while (bytes >= SizeClasses[size] && size + 1 < SizeClasses.size()) ++size;
    return size;
}

struct LabelTally {
    std::uint32_t type = 0;
    std::string label;
    std::uint64_t count = 0;
    std::uint64_t bytes = 0;
};

class AllocationLabel {
public:
    explicit AllocationLabel(const char* label) noexcept;
    ~AllocationLabel();
    AllocationLabel(const AllocationLabel&) = delete;
    AllocationLabel& operator=(const AllocationLabel&) = delete;

private:
    const char* previous;
};

struct LiveAllocations {
    std::uint64_t count = 0;
    std::uint64_t peak = 0;
    std::array<std::uint64_t, VK_MAX_MEMORY_TYPES> countByType{};
    std::array<std::uint64_t, VK_MAX_MEMORY_TYPES> bytesByType{};
    std::array<std::array<std::uint64_t, SizeClasses.size()>, VK_MAX_MEMORY_TYPES> countBySize{};
    std::array<std::array<std::uint64_t, SizeClasses.size()>, VK_MAX_MEMORY_TYPES> bytesBySize{};
    std::vector<LabelTally> labels;
};

PFN_vkGetDeviceProcAddr CountAllocations(VkDevice device, PFN_vkGetDeviceProcAddr resolver, std::uint32_t limit);
LiveAllocations CountedAllocations(VkDevice device);
std::vector<std::string> LiveAllocationReport(const LiveAllocations& live, const VkPhysicalDeviceMemoryProperties& memory, std::uint32_t limit);

}

#endif
