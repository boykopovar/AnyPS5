#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>
#include <string_view>
#include <thread>

using namespace AgcDriver::Graphics;
using namespace std::chrono_literals;

namespace {

void Check(bool value, const char* reason) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", reason);
    std::fflush(stderr);
    std::_Exit(1);
}

std::atomic<std::uintptr_t> next{16};
std::atomic<unsigned> live{0};
std::atomic<unsigned> attempts{0};
std::mutex gate;
std::condition_variable changed;
bool entered = false;
bool released = false;
bool failing = false;
bool throwing = false;
bool blocking = false;

template<typename THandle>
THandle MakeHandle() {
    ++live;
    return reinterpret_cast<THandle>(next.fetch_add(1));
}

VKAPI_ATTR VkResult VKAPI_CALL CreateModule(VkDevice, const VkShaderModuleCreateInfo*, const VkAllocationCallbacks*, VkShaderModule* output) {
    *output = MakeHandle<VkShaderModule>();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL CreateLayout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout* output) {
    *output = MakeHandle<VkPipelineLayout>();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL CreatePass(VkDevice, const VkRenderPassCreateInfo*, const VkAllocationCallbacks*, VkRenderPass* output) {
    *output = MakeHandle<VkRenderPass>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL DestroyModule(VkDevice, VkShaderModule, const VkAllocationCallbacks*) { --live; }
VKAPI_ATTR void VKAPI_CALL DestroyLayout(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*) { --live; }
VKAPI_ATTR void VKAPI_CALL DestroyPass(VkDevice, VkRenderPass, const VkAllocationCallbacks*) { --live; }
VKAPI_ATTR void VKAPI_CALL DestroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*) { --live; }

VKAPI_ATTR VkResult VKAPI_CALL CreatePipeline(VkDevice, VkPipelineCache, std::uint32_t count, const VkGraphicsPipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline* output) {
    Check(count == 1, "expected a single graphics pipeline");
    const auto attempt = ++attempts;
    std::unique_lock lock(gate);
    if (blocking && !entered) {
        entered = true;
        changed.notify_all();
        changed.wait(lock, [] { return released; });
        if (throwing) throw std::runtime_error("injected pipeline exception");
        if (failing) return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    Check(attempt != 0, "pipeline attempt counter wrapped");
    *output = MakeHandle<VkPipeline>();
    changed.notify_all();
    return VK_SUCCESS;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice, const char* name) {
    if (std::strcmp(name, "vkCreateShaderModule") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreateModule);
    if (std::strcmp(name, "vkCreatePipelineLayout") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreateLayout);
    if (std::strcmp(name, "vkCreateRenderPass") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreatePass);
    if (std::strcmp(name, "vkCreateGraphicsPipelines") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreatePipeline);
    if (std::strcmp(name, "vkDestroyShaderModule") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyModule);
    if (std::strcmp(name, "vkDestroyPipelineLayout") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyLayout);
    if (std::strcmp(name, "vkDestroyRenderPass") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyPass);
    if (std::strcmp(name, "vkDestroyPipeline") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyPipeline);
    return nullptr;
}

}

int main(int argc, char** argv) {
    Check(argc == 2, "expected shared, independent, cached, failure or exception");
    const std::string_view mode(argv[1]);
    Check(mode == "shared" || mode == "independent" || mode == "cached" || mode == "failure" || mode == "exception", "unknown mode");
    Context context{};
    context.device = reinterpret_cast<VkDevice>(std::uintptr_t{1});
    context.deviceProc = Resolve;
    context.limits.framebufferNoAttachmentsSampleCounts = VK_SAMPLE_COUNT_1_BIT;
    ShaderRecompiler::RecompileResult vertex, fragment;
    vertex.spirv = {0x07230203u, 0x00010000u, 1, 1, 0};
    fragment.spirv = {0x07230203u, 0x00010000u, 2, 1, 0};
    const std::array shaders{CompiledShader{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, CompiledShader{ShaderRecompiler::ShaderStage::Fragment, &fragment, 0}};
    const auto build = [&](VkCullModeFlags cull, bool* created) {
        State state{};
        state.cullMode = cull;
        return CachedPipeline(context, state, VertexInputLayout{}, PipelineSetLayouts{}, shaders, VK_IMAGE_LAYOUT_GENERAL, created);
    };
    std::shared_ptr<Pipeline> warm;
    if (mode == "cached") warm = build(VK_CULL_MODE_BACK_BIT, nullptr);
    const auto before = attempts.load();
    blocking = true;
    failing = mode == "failure";
    throwing = mode == "exception";
    std::array<std::shared_ptr<Pipeline>, 2> results;
    std::array<bool, 2> created{};
    std::array<std::exception_ptr, 2> errors;
    std::atomic<bool> secondStarted{false};
    std::atomic<bool> secondDone{false};
    std::thread first([&] {
        try { results[0] = build(0, &created[0]); }
        catch (...) { errors[0] = std::current_exception(); }
    });
    {
        std::unique_lock lock(gate);
        Check(changed.wait_for(lock, 3s, [] { return entered; }), "first pipeline build did not start");
    }
    std::thread second([&] {
        secondStarted = true;
        changed.notify_all();
        try { results[1] = build(mode == "independent" || mode == "cached" ? VK_CULL_MODE_BACK_BIT : 0, &created[1]); }
        catch (...) { errors[1] = std::current_exception(); }
        secondDone = true;
        changed.notify_all();
    });
    bool early = false;
    {
        std::unique_lock lock(gate);
        Check(changed.wait_for(lock, 3s, [&] { return secondStarted.load(); }), "second lookup did not start");
        early = changed.wait_for(lock, mode == "independent" || mode == "cached" ? 3s : 200ms, [&] { return secondDone.load(); });
        released = true;
    }
    changed.notify_all();
    first.join();
    second.join();
    Check(static_cast<bool>(errors[0]) == (failing || throwing), "unexpected first build failure");
    Check(!errors[1] && results[1] != nullptr, "second lookup failed");
    Check((mode == "independent" || mode == "cached") ? early : !early, "lookup did not wait only for its own in-flight pipeline");
    Check(mode != "shared" || (results[0] == results[1] && created[0] && !created[1]), "identical lookups did not share one created pipeline");
    Check(mode != "cached" || (results[1] == warm && !created[1]), "a cached hit was held behind a cold build");
    Check(mode != "independent" || (results[0] != results[1] && created[0] && created[1]), "independent keys were not built separately");
    const auto expected = mode == "shared" || mode == "cached" ? 1u : 2u;
    Check(attempts - before == expected, "redundant pipeline creation");
    const auto previous = attempts.load();
    Check(build(mode == "independent" || mode == "cached" ? VK_CULL_MODE_BACK_BIT : 0, nullptr) == results[1], "completed pipeline was not cached");
    Check(attempts == previous, "cached pipeline was rebuilt");
    results = {};
    warm.reset();
    ClearCachedPipelines(context.device);
    Check(live == 0, "pipeline store leaked Vulkan objects");
    std::printf("%.*s: shared misses, independent builds, cached hits and failure cleanup checked\n", static_cast<int>(mode.size()), mode.data());
}
