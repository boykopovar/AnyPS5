#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string_view>
#include <thread>

using namespace AgcDriver::Graphics;
using namespace std::chrono_literals;

namespace {

std::atomic<std::uintptr_t> nextHandle{1};
std::atomic<unsigned> attempts{0};
std::atomic<unsigned> live{0};
std::mutex gateMutex;
std::condition_variable changed;
bool entered = false;
bool released = false;
bool failFirst = false;
bool throwFirst = false;

void Check(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::_Exit(1);
}

template<typename THandle>
THandle MakeHandle() {
    ++live;
    return reinterpret_cast<THandle>(nextHandle.fetch_add(1));
}

VKAPI_ATTR VkResult VKAPI_CALL CreateLayout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout* result) {
    *result = MakeHandle<VkPipelineLayout>();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL CreatePipelines(VkDevice, VkPipelineCache, std::uint32_t count, const VkGraphicsPipelineCreateInfo* info, const VkAllocationCallbacks*, VkPipeline* result) {
    Check(count == 1 && (info->flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) != 0, "expected one library part");
    const auto attempt = ++attempts;
    std::unique_lock lock(gateMutex);
    changed.notify_all();
    if (attempt == 1) {
        entered = true;
        changed.notify_all();
        changed.wait(lock, [] { return released; });
        if (throwFirst) throw std::runtime_error("injected pipeline build exception");
        if (failFirst) return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    *result = MakeHandle<VkPipeline>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL DestroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*) { --live; }
VKAPI_ATTR void VKAPI_CALL DestroyLayout(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*) { --live; }
VKAPI_ATTR void VKAPI_CALL DestroyModule(VkDevice, VkShaderModule, const VkAllocationCallbacks*) { --live; }

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice, const char* name) {
    if (std::strcmp(name, "vkCreateGraphicsPipelines") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreatePipelines);
    if (std::strcmp(name, "vkCreatePipelineLayout") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreateLayout);
    if (std::strcmp(name, "vkDestroyPipeline") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyPipeline);
    if (std::strcmp(name, "vkDestroyPipelineLayout") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyLayout);
    if (std::strcmp(name, "vkDestroyShaderModule") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyModule);
    return nullptr;
}

void Build(const Context& context, bool different) {
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.pDynamicState = &dynamic;
    info.stageCount = static_cast<std::uint32_t>(stages.size());
    info.pStages = stages.data();
    VkPipelineRenderingCreateInfoKHR rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    PipelineLibraryKeys keys;
    keys.vertexInput = {different ? std::byte{2} : std::byte{1}};
    LinkPipelineFromLibraries(context, info, rendering, layout, keys, nullptr, true);
}

}

int main(int argc, char** argv) {
    Check(argc == 2, "expected shared, independent, failure, exception or clear");
    const std::string_view mode(argv[1]);
    Check(mode == "shared" || mode == "independent" || mode == "failure" || mode == "exception" || mode == "clear", "unknown test mode");
    failFirst = mode == "failure";
    throwFirst = mode == "exception";
    Context context{};
    context.device = reinterpret_cast<VkDevice>(std::uintptr_t{1});
    context.deviceProc = Resolve;
    context.pipelineLinkTimeOptimization = false;
    std::atomic<unsigned> failures{0};
    std::atomic<bool> secondStarted{false};
    std::atomic<bool> secondFinished{false};
    const auto build = [&](bool different) {
        try { Build(context, different); }
        catch (const std::runtime_error&) { ++failures; }
    };
    std::thread first([&] { build(false); });
    {
        std::unique_lock lock(gateMutex);
        Check(changed.wait_for(lock, 3s, [] { return entered; }), "the first part build did not start");
    }
    std::thread second([&] {
        secondStarted = true;
        changed.notify_all();
        build(mode == "independent");
        secondFinished = true;
        changed.notify_all();
    });
    bool independentCompleted = false;
    bool duplicateEntered = false;
    std::thread clear;
    std::atomic<bool> clearStarted{false};
    std::atomic<bool> cleared{false};
    {
        std::unique_lock lock(gateMutex);
        Check(changed.wait_for(lock, 3s, [&] { return secondStarted.load(); }), "the second builder did not start");
        if (mode == "independent") {
            independentCompleted = changed.wait_for(lock, 3s, [&] { return secondFinished.load(); });
        } else {
            duplicateEntered = changed.wait_for(lock, 200ms, [] { return attempts.load() > 1; });
        }
        if (mode == "clear") {
            clear = std::thread([&] {
                clearStarted = true;
                changed.notify_all();
                ClearPipelineLibraries(context.device);
                cleared = true;
                changed.notify_all();
            });
            Check(changed.wait_for(lock, 3s, [&] { return clearStarted.load(); }), "the clearing thread did not start");
            Check(!changed.wait_for(lock, 100ms, [&] { return cleared.load(); }), "libraries were cleared while their build was blocked");
        }
        released = true;
    }
    changed.notify_all();
    first.join();
    second.join();
    if (clear.joinable()) clear.join();
    if (mode == "clear") {
        Check(cleared && live == 0, "clearing concurrent library builds leaked Vulkan objects");
        Check(!duplicateEntered && attempts == 4, "clearing allowed duplicate part builds");
        std::puts("clear: waited for concurrent builds and released every Vulkan object");
        return 0;
    }
    const auto compiled = attempts.load();
    Build(context, false);
    Check(attempts == compiled, "a completed library part was compiled again");
    const auto counters = PipelineLibraryCountersOf(context.device);
    ClearPipelineLibraries(context.device);
    Check(live == 0, "pipeline-library builds leaked Vulkan objects");
    Check(!duplicateEntered, "the same library part was compiled concurrently more than once");
    Check(mode != "independent" || independentCompleted, "a different library key waited for the blocked build");
    Check(failures == (failFirst || throwFirst ? 1u : 0u), "unexpected library build failure count");
    Check(attempts == (mode == "shared" ? 4u : 5u), "unexpected number of library part compilations");
    Check(counters.built == std::array<std::uint64_t, 4>{mode == "independent" ? 2u : 1u, 1, 1, 1}, "library counters do not match successful unique builds");
    std::printf("%.*s: %u compilations, no duplicate in-flight part builds, no live objects\n", static_cast<int>(mode.size()), mode.data(), attempts.load());
    return 0;
}
