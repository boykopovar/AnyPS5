#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PatchVertexLayer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineSpecialization.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <list>
#include <mutex>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
std::shared_ptr<Pipeline> TouchCachedPipeline(const Context& context, std::uint64_t hash, const std::weak_ptr<Pipeline>& expected) {
    auto& store = Pipelines();
    std::lock_guard lock(store.mutex);
    reportPipelines(store);
    const auto found = store.index.find(hash);
    if (found == store.index.end()) return nullptr;
    const auto it = found->second;
    if (it->pipeline.owner_before(expected) || expected.owner_before(it->pipeline) || !alive(*it, context)) return nullptr;
    store.entries.splice(store.entries.end(), store.entries, it);
    ++store.hits;
    ++store.totals.hits;
    return it->pipeline;
}

namespace {

struct DeferredPipelineBuild {
    Context context;
    State state;
    VertexInputLayout vertexInput;
    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout fragmentLayout = VK_NULL_HANDLE;
    bool split = false;
    std::vector<std::uint32_t> mainKey;
    std::vector<std::uint32_t> fragmentKey;
    std::deque<ShaderRecompiler::RecompileResult> programs;
    std::deque<std::vector<ShaderRecompiler::RegisteredImageView>> views;
    std::vector<CompiledShader> shaders;
    VkImageLayout attachmentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    std::uint64_t hash = 0;
};

struct DeferredPipelineBuilds {
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable idle;
    std::deque<std::unique_ptr<DeferredPipelineBuild>> queue;
    std::unordered_set<std::uint64_t> pending;
    std::unordered_set<std::uint64_t> failed;
    unsigned running = 0;
    bool started = false;
};

DeferredPipelineBuilds& deferredBuilds() {
    static auto* builds = new DeferredPipelineBuilds();
    return *builds;
}

bool profileDeferredBuilds() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

std::size_t deferredPipelineWords() {
    static const std::size_t words = [] {
        const char* value = std::getenv("APS5_ASYNC_PIPELINE_WORDS");
        return value != nullptr ? static_cast<std::size_t>(std::strtoull(value, nullptr, 10)) : std::size_t{0};
    }();
    return words;
}

void runDeferredBuilds() {
    auto& builds = deferredBuilds();
    for (;;) {
        std::unique_ptr<DeferredPipelineBuild> job;
        {
            std::unique_lock lock(builds.mutex);
            builds.wake.wait(lock, [&] { return !builds.queue.empty(); });
            job = std::move(builds.queue.front());
            builds.queue.pop_front();
            ++builds.running;
        }
        bool failed = false;
        const auto started = std::chrono::steady_clock::now();
        try {
            const PipelineSetLayouts layouts{job->mainLayout, job->fragmentLayout, job->split, job->mainKey, job->fragmentKey};
            static_cast<void>(CachedPipeline(job->context, job->state, job->vertexInput, layouts, job->shaders, job->attachmentLayout));
        } catch (const std::exception& error) {
            failed = true;
            if (profileDeferredBuilds()) AgcDriver::ProfilePrint_nid_no_patch("[pipecache] deferred build 0x%016llx failed: %s\n", static_cast<unsigned long long>(job->hash), error.what());
        }
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        if (profileDeferredBuilds()) AgcDriver::ProfilePrint_nid_no_patch("[pipecache] deferred build 0x%016llx finished in %.0f ms\n", static_cast<unsigned long long>(job->hash), elapsed);
        {
            std::lock_guard lock(builds.mutex);
            builds.pending.erase(job->hash);
            if (failed) builds.failed.insert(job->hash);
            job.reset();
            --builds.running;
        }
        builds.idle.notify_all();
    }
}

void awaitDeferredBuilds(VkDevice device) {
    auto& builds = deferredBuilds();
    std::unique_lock lock(builds.mutex);
    for (auto it = builds.queue.begin(); it != builds.queue.end();) {
        if ((*it)->context.device != device) {
            ++it;
            continue;
        }
        builds.pending.erase((*it)->hash);
        it = builds.queue.erase(it);
    }
    builds.idle.wait(lock, [&] { return builds.running == 0; });
    builds.failed.clear();
}

}

bool DefersPipelineBuild(std::span<const CompiledShader> shaders) {
    const auto threshold = deferredPipelineWords();
    if (threshold == 0) return false;
    std::size_t words = 0;
    for (const auto& shader : shaders) {
        if (shader.program != nullptr) words += shader.program->spirv.size();
    }
    return words >= threshold;
}

std::shared_ptr<Pipeline> ResidentPipelineOrDeferredBuild(const Context& context, const State& state, const VertexInputLayout& vertexInput, const PipelineSetLayouts& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout, std::uint64_t* storeHash) {
    if (storeHash != nullptr) *storeHash = 0;
    static const bool disabled = std::getenv("APS5_NO_PIPELINE_CACHE") != nullptr;
    struct DeferredKeyStorage {};
    auto& storage = HostThreadLocal<std::vector<std::byte>, DeferredKeyStorage>();
    const auto length = disabled ? std::size_t{0} : pipelineKey(storage, context, state, vertexInput, resources, shaders, attachmentLayout, narrowLibraryKeysNow());
    const std::span<const std::byte> key(storage.data(), length);
    if (disabled || key.empty()) return CachedPipeline(context, state, vertexInput, resources, shaders, attachmentLayout, nullptr, storeHash);
    const auto hash = hashKey(key);
    bool synchronous = false;
    {
        auto& store = Pipelines();
        std::lock_guard lock(store.mutex);
        const auto found = store.index.find(hash);
        synchronous = found != store.index.end() && (found->second->key.size() != key.size() || std::memcmp(found->second->key.data(), key.data(), key.size()) != 0 || alive(*found->second, context));
    }
    auto& builds = deferredBuilds();
    if (!synchronous) {
        std::lock_guard lock(builds.mutex);
        if (builds.pending.contains(hash)) return nullptr;
        synchronous = builds.failed.erase(hash) != 0;
    }
    if (synchronous) return CachedPipeline(context, state, vertexInput, resources, shaders, attachmentLayout, nullptr, storeHash);
    auto job = std::make_unique<DeferredPipelineBuild>();
    job->context = context;
    job->state = state;
    job->vertexInput = vertexInput;
    job->mainLayout = resources.main;
    job->fragmentLayout = resources.fragment;
    job->split = resources.split;
    job->mainKey.assign(resources.mainKey.begin(), resources.mainKey.end());
    job->fragmentKey.assign(resources.fragmentKey.begin(), resources.fragmentKey.end());
    job->attachmentLayout = attachmentLayout;
    job->hash = hash;
    for (const auto& shader : shaders) {
        auto copy = shader;
        if (shader.program != nullptr) copy.program = &job->programs.emplace_back(*shader.program);
        if (shader.registeredImageViews != nullptr) copy.registeredImageViews = &job->views.emplace_back(*shader.registeredImageViews);
        job->shaders.push_back(copy);
    }
    {
        std::lock_guard lock(builds.mutex);
        if (!builds.pending.insert(hash).second) return nullptr;
        builds.queue.push_back(std::move(job));
        if (!builds.started) {
            builds.started = true;
            const auto workers = std::max(2u, std::thread::hardware_concurrency() / 4u);
            for (unsigned worker = 0; worker < workers; ++worker) std::thread(runDeferredBuilds).detach();
        }
    }
    builds.wake.notify_one();
    if (profileDeferredBuilds()) AgcDriver::ProfilePrint_nid_no_patch("[pipecache] deferred build 0x%016llx queued, its draws are skipped until it is resident\n", static_cast<unsigned long long>(hash));
    return nullptr;
}

void BuildPipelineStoreKey(std::vector<std::byte>& key, const Context& context, const State& state, const VertexInputLayout& vertexInput, const PipelineSetLayouts& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout) {
    struct PipelineScratchStorage {};
    auto& storage = HostThreadLocal<std::vector<std::byte>, PipelineScratchStorage>();
    const auto length = pipelineKey(storage, context, state, vertexInput, resources, shaders, attachmentLayout, narrowLibraryKeysNow());
    key.assign(storage.begin(), storage.begin() + static_cast<std::ptrdiff_t>(length));
}

PipelineStoreCounters PipelineStoreTotals() {
    auto& store = Pipelines();
    std::lock_guard lock(store.mutex);
    auto totals = store.totals;
    totals.cached = store.entries.size();
    totals.bound = storeBound(store);
    return totals;
}

bool PipelineStoreCanGrow() {
    auto& store = Pipelines();
    std::lock_guard lock(store.mutex);
    return storeBound(store) < PipelineStore::MaximumEntries && !store.fixedCapacity;
}

void ClearCachedPipelines(VkDevice device) {
    awaitDeferredBuilds(device);
    auto& store = Pipelines();
    std::lock_guard lock(store.mutex);
    for (auto it = store.entries.begin(); it != store.entries.end();) {
        if (it->device != device) {
            ++it;
            continue;
        }
        store.index.erase(it->hash);
        it = store.entries.erase(it);
    }
    if (store.entries.empty() && !store.fixedCapacity) {
        store.capacity = 0;
        store.evictedOrder.clear();
        store.evictedKeys.clear();
        store.evictionsSinceCheck = 0;
        store.returnsSinceCheck = 0;
    }
    ClearPipelineLibraries(device);
    ReleasePixelFunctionLds(device);
    ReleaseMeshWindow(device);
}

}
}
}
}
}
