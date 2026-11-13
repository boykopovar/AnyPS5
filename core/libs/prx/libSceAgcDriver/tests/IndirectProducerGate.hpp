#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_INDIRECTPRODUCERGATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_INDIRECTPRODUCERGATE_HPP
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <SDL_loadso.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

namespace AgcDriver::Test {

class ProducerSubmissionGate {
public:
    explicit ProducerSubmissionGate(AgcDriver::VulkanDevice& device) : device(device) {
#ifdef _WIN32
        constexpr const char* libraryName = "vulkan-1.dll";
#elif defined(__APPLE__)
        constexpr const char* libraryName = "libvulkan.1.dylib";
#else
        constexpr const char* libraryName = "libvulkan.so.1";
#endif
        library.reset(SDL_LoadObject(libraryName));
        Graphics::Require(library != nullptr, "cannot load Vulkan for the producer submission gate");
        resolve = reinterpret_cast<PFN_vkGetDeviceProcAddr>(SDL_LoadFunction(library.get(), "vkGetDeviceProcAddr"));
        Graphics::Require(resolve != nullptr, "cannot resolve vkGetDeviceProcAddr for the producer submission gate");
        const auto createSemaphore = function<PFN_vkCreateSemaphore>("vkCreateSemaphore");
        destroySemaphore = function<PFN_vkDestroySemaphore>("vkDestroySemaphore");
        signalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(resolve(device.Device(), "vkSignalSemaphoreKHR"));
        if (signalSemaphore == nullptr) signalSemaphore = function<PFN_vkSignalSemaphore>("vkSignalSemaphore");
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type};
        Graphics::Require(createSemaphore(device.Device(), &info, nullptr, &semaphore) == VK_SUCCESS, "cannot create the producer submission gate");
    }

    ~ProducerSubmissionGate() {
        if (worker.joinable()) {
            worker.request_stop();
            worker.join();
        }
        Release();
        try {
            device.WaitIdle();
        } catch (...) {
        }
        destroySemaphore(device.Device(), semaphore, nullptr);
    }

    void Arm(AgcDriver::Graphics::Recorder& recorder) {
        recorder.Commands();
        recorder.HoldNextSubmit(semaphore, 1);
    }

    void ReleaseAfterSubmission(AgcDriver::Graphics::Recorder& recorder, std::uint64_t serial, const std::atomic<bool>* consumerRead = nullptr) {
        worker = std::jthread([this, &recorder, serial, consumerRead](std::stop_token stop) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!stop.stop_requested() && (recorder.NewestSubmitted() < serial || (consumerRead != nullptr && !consumerRead->load())) && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::microseconds(100));
            producerSubmitted = recorder.NewestSubmitted() >= serial;
            const bool consumerObserved = consumerRead == nullptr || consumerRead->load();
            Release();
            if ((!producerSubmitted || !consumerObserved) && !stop.stop_requested()) std::fprintf(stderr, "pending producer gate: watchdog expired for producer batch %llu (submitted %u, consumer observed %u, vkSignalSemaphore %d)\n", static_cast<unsigned long long>(serial), producerSubmitted ? 1u : 0u, consumerObserved ? 1u : 0u, static_cast<int>(releaseResult));
        });
    }

    bool Released() const { return signaled.load(); }

    void Finish() {
        worker.join();
        Graphics::Require(releaseResult == VK_SUCCESS, "cannot release the producer submission gate");
        Graphics::Require(producerSubmitted, "the CPU argument read did not submit the later leased producer before waiting for its earlier argument label");
        device.WaitIdle();
    }

private:
    template <typename TFunction>
    TFunction function(const char* name) const {
        const auto function = reinterpret_cast<TFunction>(resolve(device.Device(), name));
        Graphics::Require(function != nullptr, std::string("cannot resolve ") + name + " for the producer submission gate");
        return function;
    }

    void Release() {
        if (signaled.exchange(true)) return;
        VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
        info.semaphore = semaphore;
        info.value = 1;
        releaseResult = signalSemaphore(device.Device(), &info);
    }

    AgcDriver::VulkanDevice& device;
    std::unique_ptr<void, decltype(&SDL_UnloadObject)> library{nullptr, &SDL_UnloadObject};
    PFN_vkGetDeviceProcAddr resolve = nullptr;
    PFN_vkDestroySemaphore destroySemaphore = nullptr;
    PFN_vkSignalSemaphore signalSemaphore = nullptr;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    std::jthread worker;
    std::atomic<bool> signaled{false};
    bool producerSubmitted = false;
    VkResult releaseResult = VK_NOT_READY;
};

}
#endif
