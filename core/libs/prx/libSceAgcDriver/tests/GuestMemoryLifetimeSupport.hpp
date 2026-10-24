#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_GUESTMEMORYLIFETIMESUPPORT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_GUESTMEMORYLIFETIMESUPPORT_HPP

#include <SDL_error.h>
#include <SDL_loadso.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

static inline bool GuestMemoryLifetimeHasHostImports(VkDevice device) {
    if (std::getenv("APS5_NO_HOST_IMPORT") != nullptr) {
        std::puts("skipped, host imports are disabled by APS5_NO_HOST_IMPORT");
        return false;
    }
#ifdef _WIN32
    constexpr const char* libraryName = "vulkan-1.dll";
#else
    constexpr const char* libraryName = "libvulkan.so.1";
#endif
    const std::unique_ptr<void, decltype(&SDL_UnloadObject)> library(SDL_LoadObject(libraryName), &SDL_UnloadObject);
    if (!library) throw std::runtime_error(std::string("cannot load Vulkan for the host import capability query: ") + SDL_GetError());
    const auto deviceProc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(SDL_LoadFunction(library.get(), "vkGetDeviceProcAddr"));
    if (deviceProc == nullptr) throw std::runtime_error(std::string("cannot resolve vkGetDeviceProcAddr for the host import capability query: ") + SDL_GetError());
    if (deviceProc(device, "vkGetMemoryHostPointerPropertiesEXT") == nullptr) {
        std::puts("skipped, the device has no VK_EXT_external_memory_host");
        return false;
    }
    return true;
}

#endif
