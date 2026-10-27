#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace AgcDriver::Graphics {

namespace {

constexpr std::size_t DrainObjects = 512;
constexpr auto DrainAge = std::chrono::milliseconds(100);

struct Deferred {
    PFN_vkDestroyBuffer destroyBuffer = nullptr;
    PFN_vkFreeMemory freeMemory = nullptr;
    std::vector<VkBuffer> buffers;
    std::vector<VkDeviceMemory> memory;
    std::chrono::steady_clock::time_point oldest{};
};

struct Registry {
    std::mutex mutex;
    std::unordered_map<VkDevice, Deferred> devices;
    std::atomic<PFN_vkGetDeviceProcAddr> resolver{nullptr};
};

Registry& registry() {
    static auto& state = *new Registry();
    return state;
}

void destroyAll(VkDevice device, const Deferred& deferred) noexcept {
    for (const auto buffer : deferred.buffers) deferred.destroyBuffer(device, buffer, nullptr);
    for (const auto memory : deferred.memory) deferred.freeMemory(device, memory, nullptr);
}

template<typename THandle>
bool defer(VkDevice device, THandle handle, std::vector<THandle> Deferred::*list) noexcept {
    try {
        auto& state = registry();
        std::lock_guard lock(state.mutex);
        const auto found = state.devices.find(device);
        if (found == state.devices.end()) return false;
        auto& deferred = found->second;
        if (deferred.buffers.empty() && deferred.memory.empty()) deferred.oldest = std::chrono::steady_clock::now();
        (deferred.*list).push_back(handle);
        return true;
    } catch (...) {
        return false;
    }
}

VKAPI_ATTR void VKAPI_CALL deferredDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* allocator) {
    if (buffer == VK_NULL_HANDLE || defer(device, buffer, &Deferred::buffers)) return;
    reinterpret_cast<PFN_vkDestroyBuffer>(registry().resolver.load(std::memory_order_acquire)(device, "vkDestroyBuffer"))(device, buffer, allocator);
}

VKAPI_ATTR void VKAPI_CALL deferredFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
    if (memory == VK_NULL_HANDLE || defer(device, memory, &Deferred::memory)) return;
    reinterpret_cast<PFN_vkFreeMemory>(registry().resolver.load(std::memory_order_acquire)(device, "vkFreeMemory"))(device, memory, allocator);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL deferringDeviceProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkDestroyBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&deferredDestroyBuffer);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&deferredFreeMemory);
    return registry().resolver.load(std::memory_order_acquire)(device, name);
}

}

PFN_vkGetDeviceProcAddr DeferDestructionToIdle(VkDevice device, PFN_vkGetDeviceProcAddr resolver) {
    if (device == VK_NULL_HANDLE || resolver == nullptr) throw std::invalid_argument("AGC graphics: deferred destruction needs a device and its resolver");
    Deferred deferred;
    deferred.destroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(resolver(device, "vkDestroyBuffer"));
    deferred.freeMemory = reinterpret_cast<PFN_vkFreeMemory>(resolver(device, "vkFreeMemory"));
    if (deferred.destroyBuffer == nullptr || deferred.freeMemory == nullptr) throw std::runtime_error("AGC graphics: missing Vulkan function: vkDestroyBuffer or vkFreeMemory");
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto current = state.resolver.load(std::memory_order_acquire);
    if (current != nullptr && current != resolver) throw std::runtime_error("AGC graphics: deferred destruction found two different device resolvers");
    if (!state.devices.emplace(device, std::move(deferred)).second) throw std::runtime_error("AGC graphics: deferred destruction was set up twice for one device");
    state.resolver.store(resolver, std::memory_order_release);
    return &deferringDeviceProc;
}

void DestroyDeferredAtIdle(const Context& context, std::chrono::steady_clock::time_point now) {
    auto& state = registry();
    {
        std::lock_guard lock(state.mutex);
        const auto found = state.devices.find(context.device);
        if (found == state.devices.end()) return;
        const auto objects = found->second.buffers.size() + found->second.memory.size();
        if (objects == 0 || (objects < DrainObjects && now - found->second.oldest < DrainAge)) return;
    }
    const auto idle = context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue);
    if (idle != VK_SUCCESS && idle != VK_ERROR_DEVICE_LOST) throw std::runtime_error("AGC graphics: vkQueueWaitIdle before destroying deferred objects: Vulkan result " + std::to_string(idle));
    Deferred taken;
    {
        std::lock_guard lock(state.mutex);
        const auto found = state.devices.find(context.device);
        if (found == state.devices.end()) return;
        taken.destroyBuffer = found->second.destroyBuffer;
        taken.freeMemory = found->second.freeMemory;
        taken.buffers.swap(found->second.buffers);
        taken.memory.swap(found->second.memory);
    }
    destroyAll(context.device, taken);
}

void DestroyDeferredObjects(VkDevice device) noexcept {
    Deferred taken;
    {
        auto& state = registry();
        std::lock_guard lock(state.mutex);
        const auto found = state.devices.find(device);
        if (found == state.devices.end()) return;
        taken = std::move(found->second);
        state.devices.erase(found);
    }
    destroyAll(device, taken);
}

std::size_t DeferredObjects(VkDevice device) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.devices.find(device);
    return found == state.devices.end() ? 0 : found->second.buffers.size() + found->second.memory.size();
}

}
