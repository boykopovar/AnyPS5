#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

inline VkCommandBuffer FakeCommandBuffer(std::uintptr_t index) noexcept {
    constexpr std::uintptr_t dispatchableAlignment = 16;
    return reinterpret_cast<VkCommandBuffer>(index * dispatchableAlignment);
}
