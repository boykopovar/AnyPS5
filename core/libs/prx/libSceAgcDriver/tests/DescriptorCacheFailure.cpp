#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
thread_local bool failNextAllocation = false;
}

void* operator new(std::size_t size) {
    if (failNextAllocation) {
        failNextAllocation = false;
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
using namespace AgcDriver::Graphics;

struct VulkanObjects {
    std::uintptr_t next = 1;
    std::uint32_t layouts = 0;
    std::uint32_t pools = 0;
    std::uint32_t createdLayouts = 0;
    std::uint32_t createdPools = 0;
    bool failAfterLayout = false;
    bool failAfterPool = false;
    std::array<bool, 32> poolUsed{};
} objects;

void RequireTest(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

VKAPI_ATTR VkResult VKAPI_CALL CreateLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout* layout) {
    *layout = reinterpret_cast<VkDescriptorSetLayout>(objects.next++);
    ++objects.layouts;
    ++objects.createdLayouts;
    failNextAllocation = objects.failAfterLayout;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL DestroyLayout(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*) {
    RequireTest(objects.layouts != 0, "layout destroyed twice");
    --objects.layouts;
}

VKAPI_ATTR VkResult VKAPI_CALL CreatePool(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool* pool) {
    RequireTest(objects.next < objects.poolUsed.size(), "mock pool capacity exceeded");
    *pool = reinterpret_cast<VkDescriptorPool>(objects.next++);
    ++objects.pools;
    ++objects.createdPools;
    failNextAllocation = objects.failAfterPool;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL DestroyPool(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*) {
    RequireTest(objects.pools != 0, "pool destroyed twice");
    --objects.pools;
}

VKAPI_ATTR VkResult VKAPI_CALL AllocateSets(VkDevice, const VkDescriptorSetAllocateInfo* info, VkDescriptorSet* set) {
    auto& used = objects.poolUsed.at(reinterpret_cast<std::uintptr_t>(info->descriptorPool));
    if (used) return VK_ERROR_OUT_OF_POOL_MEMORY;
    used = true;
    *set = reinterpret_cast<VkDescriptorSet>(objects.next++);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL ResetPool(VkDevice, VkDescriptorPool pool, VkDescriptorPoolResetFlags) {
    objects.poolUsed.at(reinterpret_cast<std::uintptr_t>(pool)) = false;
    return VK_SUCCESS;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice, const char* name) {
    if (std::strcmp(name, "vkCreateDescriptorSetLayout") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreateLayout);
    if (std::strcmp(name, "vkDestroyDescriptorSetLayout") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyLayout);
    if (std::strcmp(name, "vkCreateDescriptorPool") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreatePool);
    if (std::strcmp(name, "vkDestroyDescriptorPool") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyPool);
    if (std::strcmp(name, "vkAllocateDescriptorSets") == 0) return reinterpret_cast<PFN_vkVoidFunction>(AllocateSets);
    if (std::strcmp(name, "vkResetDescriptorPool") == 0) return reinterpret_cast<PFN_vkVoidFunction>(ResetPool);
    return nullptr;
}

Context TestContext() {
    Context context{};
    context.deviceProc = Resolve;
    context.limits.maxPerStageResources = 128;
    context.limits.maxDescriptorSetStorageBuffers = 4096;
    context.descriptorIndexingLimits.maxPerStageUpdateAfterBindResources = 128;
    context.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindStorageBuffers = 4096;
    return context;
}

const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
const std::array<std::uint32_t, 4> key{binding.binding, binding.descriptorType, binding.descriptorCount, binding.stageFlags};
const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};

bool LayoutFailure(bool updateAfterBind) {
    objects = {};
    bool failed = false;
    {
        DescriptorCache cache(TestContext());
        objects.failAfterLayout = true;
        try {
            cache.Layout(key, std::span(&binding, 1), updateAfterBind);
        } catch (const std::bad_alloc&) {
            failed = true;
        }
        failNextAllocation = false;
        objects.failAfterLayout = false;
        RequireTest(failed && objects.createdLayouts == 1, "layout publication failure was not exercised");
        const auto layout = cache.Layout(key, std::span(&binding, 1), updateAfterBind);
        RequireTest(layout != VK_NULL_HANDLE && objects.createdLayouts == 2, "layout creation did not recover");
        RequireTest(cache.Layout(key, std::span(&binding, 1), updateAfterBind) == layout && objects.createdLayouts == 2, "layout cache hit created another object");
    }
    std::printf("layout updateAfterBind=%d: unowned=%u\n", updateAfterBind, objects.layouts);
    return objects.layouts == 0;
}

bool PoolFailure(bool updateAfterBind, bool grow) {
    objects = {};
    {
        DescriptorCache cache(TestContext());
        const auto layout = cache.Layout(key, std::span(&binding, 1), updateAfterBind);
        DescriptorCache::SetAllocation first{};
        if (grow) first = cache.Allocate(layout, std::span(&size, 1), updateAfterBind);
        objects.failAfterPool = true;
        DescriptorCache::SetAllocation allocation{};
        try {
            allocation = cache.Allocate(layout, std::span(&size, 1), updateAfterBind);
        } catch (const std::bad_alloc&) {
        }
        failNextAllocation = false;
        objects.failAfterPool = false;
        RequireTest(objects.createdPools == (grow ? 2u : 1u), "pool publication path was not exercised");
        if (allocation.set != VK_NULL_HANDLE) cache.Free(allocation);
        if (grow) cache.Free(first);
        const auto recovered = cache.Allocate(layout, std::span(&size, 1), updateAfterBind);
        RequireTest(recovered.set != VK_NULL_HANDLE, "pool allocation did not recover");
        cache.Free(recovered);
    }
    std::printf("pool updateAfterBind=%d grow=%d: unowned=%u\n", updateAfterBind, grow, objects.pools);
    return objects.layouts == 0 && objects.pools == 0;
}
}

int main() {
    int failures = 0;
    for (const bool updateAfterBind : {false, true}) {
        if (!LayoutFailure(updateAfterBind)) ++failures;
        for (const bool grow : {false, true}) if (!PoolFailure(updateAfterBind, grow)) ++failures;
    }
    return failures == 0 ? 0 : 1;
}
