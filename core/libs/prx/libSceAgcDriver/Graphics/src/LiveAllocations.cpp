#include "prx/libSceAgcDriver/Graphics/include/LiveAllocations.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace AgcDriver::Graphics {

namespace {

constexpr std::size_t LabelsListed = 8;

thread_local const char* currentLabel = nullptr;

struct Allocation {
    std::uint32_t type = 0;
    VkDeviceSize bytes = 0;
    std::string label;
};

struct Tally {
    std::uint64_t count = 0;
    std::uint64_t bytes = 0;
};

struct Counted {
    PFN_vkGetDeviceProcAddr resolver = nullptr;
    PFN_vkAllocateMemory allocate = nullptr;
    PFN_vkFreeMemory free = nullptr;
    std::uint32_t limit = 0;
    std::uint64_t warnAt = 0;
    LiveAllocations live;
    std::unordered_map<VkDeviceMemory, Allocation> allocations;
    std::map<std::pair<std::uint32_t, std::string>, Tally> labels;
};

struct Registry {
    std::mutex mutex;
    std::unordered_map<VkDevice, Counted> devices;
};

Registry& registry() {
    static auto& state = *new Registry();
    return state;
}

VKAPI_ATTR VkResult VKAPI_CALL countedAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* allocator, VkDeviceMemory* memory) {
    auto& state = registry();
    PFN_vkAllocateMemory allocate = nullptr;
    {
        std::lock_guard lock(state.mutex);
        allocate = state.devices.at(device).allocate;
    }
    const auto result = allocate(device, info, allocator, memory);
    if (result != VK_SUCCESS || info->memoryTypeIndex >= VK_MAX_MEMORY_TYPES) return result;
    const std::string label = currentLabel != nullptr ? currentLabel : "unlabelled";
    std::uint64_t count = 0;
    std::uint32_t limit = 0;
    bool warn = false;
    {
        std::lock_guard lock(state.mutex);
        auto& counted = state.devices.at(device);
        counted.allocations.insert_or_assign(*memory, Allocation{info->memoryTypeIndex, info->allocationSize, label});
        auto& tally = counted.labels[{info->memoryTypeIndex, label}];
        ++tally.count;
        tally.bytes += info->allocationSize;
        auto& live = counted.live;
        ++live.count;
        ++live.countByType[info->memoryTypeIndex];
        live.bytesByType[info->memoryTypeIndex] += info->allocationSize;
        const auto size = SizeClass(info->allocationSize);
        ++live.countBySize[info->memoryTypeIndex][size];
        live.bytesBySize[info->memoryTypeIndex][size] += info->allocationSize;
        live.peak = std::max(live.peak, live.count);
        if (counted.limit != 0 && live.count > counted.warnAt) {
            warn = true;
            count = live.count;
            limit = counted.limit;
            counted.warnAt = counted.warnAt * 2u;
        }
    }
    if (warn) std::fprintf(stderr, "[gpu] %llu live device memory allocations exceed maxMemoryAllocationCount %u\n", static_cast<unsigned long long>(count), limit);
    return result;
}

VKAPI_ATTR void VKAPI_CALL countedFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
    auto& state = registry();
    PFN_vkFreeMemory release = nullptr;
    {
        std::lock_guard lock(state.mutex);
        auto& counted = state.devices.at(device);
        release = counted.free;
        const auto found = counted.allocations.find(memory);
        if (found != counted.allocations.end()) {
            auto& live = counted.live;
            --live.count;
            --live.countByType[found->second.type];
            live.bytesByType[found->second.type] -= found->second.bytes;
            const auto size = SizeClass(found->second.bytes);
            --live.countBySize[found->second.type][size];
            live.bytesBySize[found->second.type][size] -= found->second.bytes;
            const auto tally = counted.labels.find({found->second.type, found->second.label});
            if (tally != counted.labels.end()) {
                --tally->second.count;
                tally->second.bytes -= found->second.bytes;
                if (tally->second.count == 0) counted.labels.erase(tally);
            }
            counted.allocations.erase(found);
        }
    }
    release(device, memory, allocator);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL countingDeviceProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkAllocateMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&countedAllocateMemory);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&countedFreeMemory);
    auto& state = registry();
    PFN_vkGetDeviceProcAddr resolver = nullptr;
    {
        std::lock_guard lock(state.mutex);
        resolver = state.devices.at(device).resolver;
    }
    return resolver(device, name);
}

}

AllocationLabel::AllocationLabel(const char* label) noexcept : previous(currentLabel) {
    currentLabel = label;
}

AllocationLabel::~AllocationLabel() {
    currentLabel = previous;
}

PFN_vkGetDeviceProcAddr CountAllocations(VkDevice device, PFN_vkGetDeviceProcAddr resolver, std::uint32_t limit) {
    if (device == VK_NULL_HANDLE || resolver == nullptr) throw std::invalid_argument("AGC graphics: counting allocations needs a device and its resolver");
    Counted counted;
    counted.resolver = resolver;
    counted.allocate = reinterpret_cast<PFN_vkAllocateMemory>(resolver(device, "vkAllocateMemory"));
    counted.free = reinterpret_cast<PFN_vkFreeMemory>(resolver(device, "vkFreeMemory"));
    if (counted.allocate == nullptr || counted.free == nullptr) throw std::runtime_error("AGC graphics: missing Vulkan function: vkAllocateMemory or vkFreeMemory");
    counted.limit = limit;
    counted.warnAt = limit;
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    if (!state.devices.emplace(device, std::move(counted)).second) throw std::runtime_error("AGC graphics: allocation counting was set up twice for one device");
    return &countingDeviceProc;
}

LiveAllocations CountedAllocations(VkDevice device) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.devices.find(device);
    if (found == state.devices.end()) return {};
    auto live = found->second.live;
    for (const auto& [key, tally] : found->second.labels) live.labels.push_back({key.first, key.second, tally.count, tally.bytes});
    return live;
}

std::vector<std::string> LiveAllocationReport(const LiveAllocations& live, const VkPhysicalDeviceMemoryProperties& memory, std::uint32_t limit) {
    std::vector<std::string> lines;
    char line[256];
    std::snprintf(line, sizeof(line), "[device-fault] live device memory: %llu allocations (peak %llu, maxMemoryAllocationCount %u)", static_cast<unsigned long long>(live.count), static_cast<unsigned long long>(live.peak), limit);
    lines.emplace_back(line);
    for (std::uint32_t type = 0; type < memory.memoryTypeCount && type < VK_MAX_MEMORY_TYPES; ++type) {
        if (live.countByType[type] == 0) continue;
        std::snprintf(line, sizeof(line), "[device-fault] memory type %u (heap %u, flags 0x%x): %llu allocations, %llu MiB", type, memory.memoryTypes[type].heapIndex, static_cast<unsigned>(memory.memoryTypes[type].propertyFlags), static_cast<unsigned long long>(live.countByType[type]), static_cast<unsigned long long>(live.bytesByType[type] >> 20u));
        lines.emplace_back(line);
        std::string sizes = "[device-fault] memory type " + std::to_string(type) + " by size:";
        for (std::size_t size = 0; size < SizeClasses.size(); ++size) {
            if (live.countBySize[type][size] == 0) continue;
            std::snprintf(line, sizeof(line), " %s %llu/%llu MiB", SizeClassNames[size], static_cast<unsigned long long>(live.countBySize[type][size]), static_cast<unsigned long long>(live.bytesBySize[type][size] >> 20u));
            sizes += line;
        }
        lines.push_back(std::move(sizes));
        std::vector<const LabelTally*> labels;
        for (const auto& tally : live.labels) {
            if (tally.type == type) labels.push_back(&tally);
        }
        if (labels.empty()) continue;
        std::sort(labels.begin(), labels.end(), [](const LabelTally* left, const LabelTally* right) { return left->bytes != right->bytes ? left->bytes > right->bytes : left->label < right->label; });
        std::string callers = "[device-fault] memory type " + std::to_string(type) + " by caller:";
        for (std::size_t index = 0; index < labels.size() && index < LabelsListed; ++index) {
            std::snprintf(line, sizeof(line), "%s %s %llu/%llu MiB", index == 0 ? "" : ";", labels[index]->label.c_str(), static_cast<unsigned long long>(labels[index]->count), static_cast<unsigned long long>(labels[index]->bytes >> 20u));
            callers += line;
        }
        lines.push_back(std::move(callers));
    }
    return lines;
}

}
