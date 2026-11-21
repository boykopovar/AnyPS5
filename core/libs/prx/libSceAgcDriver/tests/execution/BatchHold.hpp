#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_BATCHHOLD_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_BATCHHOLD_HPP

#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <SDL_loadso.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

class BatchHold {
public:
    explicit BatchHold(VkDevice holdDevice) : device(holdDevice) {
#ifdef _WIN32
        constexpr const char* libraryName = "vulkan-1.dll";
#elif defined(__APPLE__)
        constexpr const char* libraryName = "libvulkan.1.dylib";
#else
        constexpr const char* libraryName = "libvulkan.so.1";
#endif
        library.reset(SDL_LoadObject(libraryName));
        if (library == nullptr) throw std::runtime_error("batch hold: cannot load Vulkan");
        resolve = reinterpret_cast<PFN_vkGetDeviceProcAddr>(SDL_LoadFunction(library.get(), "vkGetDeviceProcAddr"));
        if (resolve == nullptr) throw std::runtime_error("batch hold: cannot resolve vkGetDeviceProcAddr");
        const auto createSemaphore = function<PFN_vkCreateSemaphore>("vkCreateSemaphore");
        destroySemaphore = function<PFN_vkDestroySemaphore>("vkDestroySemaphore");
        signalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(resolve(device, "vkSignalSemaphoreKHR"));
        if (signalSemaphore == nullptr) signalSemaphore = function<PFN_vkSignalSemaphore>("vkSignalSemaphore");
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type};
        if (createSemaphore(device, &info, nullptr, &semaphore) != VK_SUCCESS) throw std::runtime_error("batch hold: cannot create the timeline semaphore");
    }

    BatchHold(const BatchHold&) = delete;
    BatchHold& operator=(const BatchHold&) = delete;

    ~BatchHold() {
        static_cast<void>(Release());
        destroySemaphore(device, semaphore, nullptr);
    }

    VkDevice Device() const {
        return device;
    }

    void Arm(AgcDriver::Graphics::Recorder& recorder) {
        recorder.HoldNextSubmit(semaphore, armed.fetch_add(1, std::memory_order_acq_rel) + 1);
    }

    VkResult Release() {
        std::lock_guard lock(signalMutex);
        const auto value = armed.load(std::memory_order_acquire);
        if (value <= signaled) return VK_SUCCESS;
        VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
        info.semaphore = semaphore;
        info.value = value;
        const auto result = signalSemaphore(device, &info);
        if (result == VK_SUCCESS) signaled = value;
        return result;
    }

private:
    template <typename TFunction>
    TFunction function(const char* name) const {
        const auto found = reinterpret_cast<TFunction>(resolve(device, name));
        if (found == nullptr) throw std::runtime_error(std::string("batch hold: cannot resolve ") + name);
        return found;
    }

    VkDevice device = VK_NULL_HANDLE;
    std::unique_ptr<void, decltype(&SDL_UnloadObject)> library{nullptr, &SDL_UnloadObject};
    PFN_vkGetDeviceProcAddr resolve = nullptr;
    PFN_vkDestroySemaphore destroySemaphore = nullptr;
    PFN_vkSignalSemaphore signalSemaphore = nullptr;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    std::atomic<std::uint64_t> armed{0};
    std::uint64_t signaled = 0;
    std::mutex signalMutex;
};

#endif
