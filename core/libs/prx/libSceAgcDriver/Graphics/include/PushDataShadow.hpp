#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PUSHDATASHADOW_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PUSHDATASHADOW_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

namespace AgcDriver::Graphics {

template<std::size_t Bytes>
class PushDataShadow {
public:
    static constexpr std::size_t Unit = sizeof(VkDeviceAddress);
    static_assert(Bytes % Unit == 0);

    std::pair<std::size_t, std::size_t> Changed(VkCommandBuffer target, std::uint64_t currentMark, std::span<const std::byte, Bytes> next) {
        if (target != commands || currentMark != mark) {
            commands = target;
            std::memcpy(bytes.data(), next.data(), Bytes);
            return {0, Bytes};
        }
        std::size_t first = Bytes;
        std::size_t end = 0;
        for (std::size_t offset = 0; offset < Bytes; offset += Unit) {
            if (std::memcmp(next.data() + offset, bytes.data() + offset, Unit) == 0) continue;
            first = std::min(first, offset);
            end = offset + Unit;
        }
        if (first >= end) return {0, 0};
        std::memcpy(bytes.data() + first, next.data() + first, end - first);
        return {first, end};
    }

    void Pushed(std::uint64_t newMark) { mark = newMark; }

private:
    VkCommandBuffer commands = VK_NULL_HANDLE;
    std::uint64_t mark = 0;
    std::array<std::byte, Bytes> bytes{};
};

}

#endif
