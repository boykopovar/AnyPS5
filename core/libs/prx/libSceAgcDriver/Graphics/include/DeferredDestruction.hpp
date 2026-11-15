#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEFERREDDESTRUCTION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEFERREDDESTRUCTION_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <chrono>
#include <cstddef>

namespace AgcDriver::Graphics {

PFN_vkGetDeviceProcAddr DeferDestructionToIdle(VkDevice device, PFN_vkGetDeviceProcAddr resolver);
void DestroyDeferredAtIdle(const Context& context, std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
void DestroyDeferredObjects(VkDevice device) noexcept;
bool IsDeferredMemoryFree(PFN_vkFreeMemory freeMemory);
std::size_t DeferredObjects(VkDevice device);

}

#endif
