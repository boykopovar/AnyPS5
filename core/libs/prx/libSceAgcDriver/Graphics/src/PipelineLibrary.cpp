#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <array>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <functional>
#include <thread>
#include <map>
#include <mutex>
#include <string_view>

namespace AgcDriver::Graphics {

namespace {

constexpr std::array<VkDynamicState, 15> dynamicStates{
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_DEPTH_BIAS,
    VK_DYNAMIC_STATE_DEPTH_BOUNDS,
    VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
    VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    VK_DYNAMIC_STATE_CULL_MODE_EXT,
    VK_DYNAMIC_STATE_FRONT_FACE_EXT,
    VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
    VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
    VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
    VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT,
    VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
    VK_DYNAMIC_STATE_STENCIL_OP_EXT,
};

using Key = std::vector<std::byte>;

struct DeviceLibraries {
    PFN_vkDestroyPipeline destroyPipeline = nullptr;
    PFN_vkDestroyPipelineLayout destroyLayout = nullptr;
    std::map<Key, VkPipelineLayout> layouts;
    std::array<std::map<Key, VkPipeline>, 4> parts;
    PipelineLibraryCounters counters;
};

struct LibraryStore {
    std::mutex mutex;
    std::map<VkDevice, DeviceLibraries> devices;
    std::condition_variable idle;
    std::condition_variable work;
    std::deque<std::pair<VkDevice, std::function<void()>>> jobs;
    std::map<VkDevice, std::size_t> pending;
    bool started = false;
};

LibraryStore& Libraries() {
    static auto* store = new LibraryStore();
    return *store;
}

void runJobs(LibraryStore& store) {
    std::unique_lock lock(store.mutex);
    for (;;) {
        store.work.wait(lock, [&] { return !store.jobs.empty(); });
        auto [device, job] = std::move(store.jobs.front());
        store.jobs.pop_front();
        lock.unlock();
        job();
        lock.lock();
        if (--store.pending[device] == 0) store.idle.notify_all();
    }
}

void enqueue(LibraryStore& store, VkDevice device, std::function<void()> job) {
    if (!store.started) {
        std::thread(runJobs, std::ref(store)).detach();
        store.started = true;
    }
    ++store.pending[device];
    store.jobs.emplace_back(device, std::move(job));
    store.work.notify_one();
}

Key Combined(const Key& part, const Key& context) {
    Key key;
    const auto append = [&](const Key& value) {
        const auto size = value.size();
        const auto bytes = std::as_bytes(std::span(&size, 1));
        key.insert(key.end(), bytes.begin(), bytes.end());
        key.insert(key.end(), value.begin(), value.end());
    };
    append(part);
    append(context);
    return key;
}

}

std::span<const VkDynamicState> PipelineLibraryDynamicStates() {
    return dynamicStates;
}

VkPipeline LinkPipelineFromLibraries(const Context& context, const VkGraphicsPipelineCreateInfo& info, const VkPipelineRenderingCreateInfoKHR& rendering, const VkPipelineLayoutCreateInfo& layout, const PipelineLibraryKeys& keys, std::shared_ptr<OptimizedPipeline>* optimized) {
    Require(info.renderPass == VK_NULL_HANDLE && info.pDynamicState != nullptr, "pipeline library parts need dynamic rendering and the library dynamic states");
    auto& store = Libraries();
    std::lock_guard lock(store.mutex);
    auto& device = store.devices[context.device];
    if (device.destroyPipeline == nullptr) {
        device.destroyPipeline = context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline");
        device.destroyLayout = context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout");
    }
    auto pipelineLayout = device.layouts.find(keys.layout);
    if (pipelineLayout == device.layouts.end()) {
        VkPipelineLayout handle = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layout, nullptr, &handle), "vkCreatePipelineLayout library");
        pipelineLayout = device.layouts.emplace(keys.layout, handle).first;
    }
    std::vector<VkPipelineShaderStageCreateInfo> preRasterization;
    const VkPipelineShaderStageCreateInfo* fragment = nullptr;
    bool mesh = false;
    for (std::uint32_t index = 0; index < info.stageCount; ++index) {
        const auto& stage = info.pStages[index];
        if (stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT) fragment = &stage;
        else preRasterization.push_back(stage);
        mesh |= stage.stage == VK_SHADER_STAGE_MESH_BIT_EXT;
    }
    Require(fragment != nullptr && !preRasterization.empty(), "a pipeline built from libraries needs a fragment and a pre-rasterization stage");
    constexpr std::array<VkGraphicsPipelineLibraryFlagsEXT, 4> flags{
        VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
        VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
    };
    const std::array<const Key*, 4> partKeys{&keys.vertexInput, &keys.preRasterization, &keys.fragmentShader, &keys.fragmentOutput};
    const Key none;
    const std::array<const Key*, 4> contextKeys{&none, &keys.layout, &keys.layout, &keys.renderPass};
    const VkPipelineRenderingCreateInfoKHR shaderRendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
    VkPipelineDepthStencilStateCreateInfo dynamicDepth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    std::vector<VkPipeline> libraries;
    for (std::size_t part = 0; part < flags.size(); ++part) {
        if (part == 0 && mesh) continue;
        const auto key = Combined(*partKeys[part], *contextKeys[part]);
        auto& cache = device.parts[part];
        if (const auto found = cache.find(key); found != cache.end()) {
            libraries.push_back(found->second);
            continue;
        }
        PerformanceTimer timing("Vulkan.GraphicsPipelineLibrary");
        VkGraphicsPipelineLibraryCreateInfoEXT library{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, flags[part] == VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT ? &rendering : &shaderRendering};
        library.flags = flags[part];
        VkGraphicsPipelineCreateInfo create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &library};
        create.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
        create.pDynamicState = info.pDynamicState;
        create.layout = pipelineLayout->second;
        switch (flags[part]) {
        case VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT:
            create.pVertexInputState = info.pVertexInputState;
            create.pInputAssemblyState = info.pInputAssemblyState;
            create.layout = VK_NULL_HANDLE;
            break;
        case VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT:
            create.stageCount = static_cast<std::uint32_t>(preRasterization.size());
            create.pStages = preRasterization.data();
            create.pViewportState = info.pViewportState;
            create.pRasterizationState = info.pRasterizationState;
            create.pTessellationState = info.pTessellationState;
            break;
        case VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT:
            create.stageCount = 1;
            create.pStages = fragment;
            create.pMultisampleState = info.pMultisampleState;
            create.pDepthStencilState = info.pDepthStencilState != nullptr ? info.pDepthStencilState : &dynamicDepth;
            break;
        default:
            create.pMultisampleState = info.pMultisampleState;
            create.pColorBlendState = info.pColorBlendState;
            create.layout = VK_NULL_HANDLE;
            break;
        }
        VkPipeline handle = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &create, nullptr, &handle), "vkCreateGraphicsPipelines library");
        cache.emplace(key, handle);
        ++device.counters.built[part];
        libraries.push_back(handle);
    }
    PerformanceTimer timing("Vulkan.GraphicsPipelineLink");
    VkPipelineLibraryCreateInfoKHR linked{VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR};
    linked.libraryCount = static_cast<std::uint32_t>(libraries.size());
    linked.pLibraries = libraries.data();
    VkGraphicsPipelineCreateInfo create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &linked};
    create.layout = info.layout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1, &create, nullptr, &pipeline), "vkCreateGraphicsPipelines link");
    ++device.counters.linked;
    static const bool optimize = std::getenv("APS5_NO_PIPELINE_LTO") == nullptr;
    if (optimize && optimized != nullptr) {
        auto slot = std::make_shared<OptimizedPipeline>();
        slot->device = context.device;
        slot->destroy = device.destroyPipeline;
        *optimized = slot;
        const auto createPipelines = context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines");
        const auto cache = context.pipelineCache;
        const auto pipelineLayout = info.layout;
        enqueue(store, context.device, [slot, createPipelines, cache, pipelineLayout, libraries, &store, devicePointer = &device] {
            if (slot->released.load()) return;
            VkPipelineLibraryCreateInfoKHR linked{VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR};
            linked.libraryCount = static_cast<std::uint32_t>(libraries.size());
            linked.pLibraries = libraries.data();
            VkGraphicsPipelineCreateInfo create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &linked};
            create.flags = VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
            create.layout = pipelineLayout;
            VkPipeline result = VK_NULL_HANDLE;
            if (createPipelines(slot->device, cache, 1, &create, nullptr, &result) != VK_SUCCESS) return;
            slot->handle.store(result);
            if (slot->released.load()) {
                if (const auto handle = slot->handle.exchange(VK_NULL_HANDLE)) slot->destroy(slot->device, handle, nullptr);
            }
            std::lock_guard lock(store.mutex);
            ++devicePointer->counters.optimized;
        });
    }
    return pipeline;
}

void OptimizedPipeline::Release() noexcept {
    released.store(true);
    if (const auto pipeline = handle.exchange(VK_NULL_HANDLE)) destroy(device, pipeline, nullptr);
}

void WaitForOptimizedPipelines(VkDevice device) {
    auto& store = Libraries();
    std::unique_lock lock(store.mutex);
    store.idle.wait(lock, [&] { return store.pending[device] == 0; });
}

void ClearPipelineLibraries(VkDevice device) {
    auto& store = Libraries();
    std::unique_lock lock(store.mutex);
    store.idle.wait(lock, [&] { return store.pending[device] == 0; });
    const auto found = store.devices.find(device);
    if (found == store.devices.end()) return;
    auto& libraries = found->second;
    for (auto& part : libraries.parts) {
        for (const auto& [key, handle] : part) libraries.destroyPipeline(device, handle, nullptr);
    }
    for (const auto& [key, handle] : libraries.layouts) libraries.destroyLayout(device, handle, nullptr);
    store.devices.erase(found);
}

PipelineLibraryCounters PipelineLibraryCountersOf(VkDevice device) {
    auto& store = Libraries();
    std::lock_guard lock(store.mutex);
    const auto found = store.devices.find(device);
    return found == store.devices.end() ? PipelineLibraryCounters{} : found->second.counters;
}

}
