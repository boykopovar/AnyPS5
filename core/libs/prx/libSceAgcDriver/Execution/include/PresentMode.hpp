#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTMODE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PRESENTMODE_HPP

#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>

namespace AgcDriver {

inline VkPresentModeKHR SelectPresentMode(const char* requested, std::span<const VkPresentModeKHR> supported) {
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    if (requested != nullptr) {
        const std::string_view name(requested);
        if (name == "mailbox") mode = VK_PRESENT_MODE_MAILBOX_KHR;
        else if (name == "immediate") mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        else if (name == "fifo_latest_ready") mode = VK_PRESENT_MODE_FIFO_LATEST_READY_KHR;
        else if (name != "fifo") throw std::runtime_error("Vulkan presentation: ANYPS5_PRESENT_MODE must be fifo, fifo_latest_ready, mailbox or immediate");
    }
    if (std::find(supported.begin(), supported.end(), mode) == supported.end()) throw std::runtime_error("Vulkan presentation: the surface does not support the requested present mode");
    return mode;
}

inline std::optional<std::uint32_t> SelectPresentQueue(std::uint32_t familyQueues, std::uint32_t queuesTaken) {
    if (queuesTaken >= familyQueues) return std::nullopt;
    return queuesTaken;
}

}

#endif
