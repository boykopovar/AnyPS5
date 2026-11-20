#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "ShaderCacheDirectory.hpp"
#include "ShaderDiskCache.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <span>
#include <system_error>
#include <thread>
#include <type_traits>
#include <zstd.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <pthread.h>
#include <sys/qos.h>
#endif

namespace AgcDriver::Graphics {
namespace {
std::uint64_t validTail(const std::filesystem::path& path, std::uint64_t from, std::uint64_t size) {
    if (size <= from) return from;
    std::ifstream file(path, std::ios::binary);
    if (!file) return from;
    file.seekg(static_cast<std::streamoff>(from));
    std::vector<std::byte> bytes(static_cast<std::size_t>(size - from));
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (static_cast<std::uint64_t>(file.gcount()) != bytes.size()) return from;
    const auto span = std::span<const std::byte>(bytes);
    std::uint64_t cursor = 0;
    std::uint64_t valid = 0;
    while (cursor + sizeof(RecordHeader) <= span.size()) {
        RecordHeader record;
        std::memcpy(&record, span.data() + cursor, sizeof(record));
        const auto payloadOffset = cursor + sizeof(RecordHeader);
        if (record.kind == 0 || record.kind > ComputeRecord || record.payloadBytes > span.size() - payloadOffset) break;
        const auto end = padded(payloadOffset + record.payloadBytes);
        if (end > span.size()) break;
        if (ShaderRecompiler::HashBytes(span.subspan(static_cast<std::size_t>(payloadOffset), static_cast<std::size_t>(record.payloadBytes))) != record.payloadHash) break;
        valid = end;
        cursor = end;
    }
    return from + valid;
}

struct ReplayCounters {
    std::atomic<std::uint64_t> graphics{0}, compute{0}, present{0}, skipped{0}, rejected{0}, failed{0};
    std::atomic<std::uint64_t> held{0}, heldPeak{0};
    std::atomic<std::uint32_t> threads{0};
    std::atomic<double> ms{0};
};

ReplayCounters& replayCounters() {
    static ReplayCounters counters;
    return counters;
}

// Builds the journal's pipelines for one device on a pool: the first thread loads the file, every
// thread then takes the next entry. A pipeline the store already holds counts as present; an entry
// whose modules are missing or whose set layouts differ from what its modules describe is rejected.
class Replay : public PipelineJournalReplay {
public:
    // The context keeps the device's buffer pool: a store entry is alive for a draw only while its
    // pool is the draw's (the pool tells device instances apart), so the entries built here must
    // name it. The pool is released with the replay, before the device's teardown resets it.
    Replay(Context context, PipelineJournalHooks hooks, unsigned threads) : context(std::move(context)), hooks(std::move(hooks)), active(threads) {
        started = std::chrono::steady_clock::now();
        replayCounters().threads.store(threads, std::memory_order_relaxed);
        replayCounters().heldPeak.store(0, std::memory_order_relaxed);
        workers.reserve(threads);
        for (unsigned index = 0; index < threads; ++index) workers.push_back(Libc::StartHostThread([this] { run(); }));
    }

    ~Replay() override {
        Cancel();
        Wait();
    }

    void Cancel() override { cancelled.store(true, std::memory_order_relaxed); }

    void Wait() override {
        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }
        workers.clear();
    }

    [[nodiscard]] bool Done() const override { return finished.load(std::memory_order_acquire); }

private:
    void run() {
#ifdef _WIN32
        if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL)) std::fprintf(stderr, "[pipeline-journal] replay thread priority not lowered (error %lu)\n", GetLastError());
#elif defined(__APPLE__)
        if (const int error = pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0); error != 0) std::fprintf(stderr, "[pipeline-journal] replay thread priority not lowered (error %d)\n", error);
#endif
        std::call_once(loadOnce, [&] {
            try {
                auto journal = Journal::Instance().Load();
                const auto graphicsLimit = configured("APS5_PIPELINE_JOURNAL_ENTRIES", 4096u);
                const auto computeLimit = configured("APS5_PIPELINE_JOURNAL_COMPUTE_ENTRIES", 1024u);
                const auto compute = static_cast<std::uint64_t>(std::ranges::count_if(journal.entries, [](const LoadedEntry& entry) { return entry.kind == ComputeRecord; }));
                const auto graphics = journal.entries.size() - compute;
                auto graphicsLeft = graphics > graphicsLimit ? graphics - graphicsLimit : 0u;
                auto computeLeft = compute > computeLimit ? compute - computeLimit : 0u;
                if (graphicsLeft + computeLeft != 0) {
                    std::fprintf(stderr, "[pipeline-journal] replaying the newest %llu of %llu graphics and %llu of %llu compute pipelines (APS5_PIPELINE_JOURNAL_ENTRIES, APS5_PIPELINE_JOURNAL_COMPUTE_ENTRIES)\n", static_cast<unsigned long long>(graphics - graphicsLeft), static_cast<unsigned long long>(graphics), static_cast<unsigned long long>(compute - computeLeft), static_cast<unsigned long long>(compute));
                    std::vector<LoadedEntry> kept;
                    kept.reserve(static_cast<std::size_t>(journal.entries.size() - graphicsLeft - computeLeft));
                    for (auto& entry : journal.entries) {
                        auto& left = entry.kind == ComputeRecord ? computeLeft : graphicsLeft;
                        if (left != 0) {
                            --left;
                            continue;
                        }
                        kept.push_back(std::move(entry));
                    }
                    journal.entries = std::move(kept);
                }
                loaded = std::move(journal);
            } catch (const std::exception& error) {
                std::fprintf(stderr, "[pipeline-journal] cannot load %s: %s\n", Journal::Instance().Path().string().c_str(), error.what());
            }
        });
        for (;;) {
            if (cancelled.load(std::memory_order_relaxed)) break;
            const auto index = cursor.fetch_add(1, std::memory_order_relaxed);
            if (index >= loaded.entries.size()) break;
            try {
                replay(loaded.entries[index]);
            } catch (const std::exception& error) {
                replayCounters().failed.fetch_add(1, std::memory_order_relaxed);
                static std::atomic<std::uint64_t> reported{0};
                if (reported.fetch_add(1) < 8) std::fprintf(stderr, "[pipeline-journal] replay failed: %s\n", error.what());
            }
        }
        if (active.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            loaded = LoadedJournal{};
            const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            replayCounters().ms.store(ms, std::memory_order_relaxed);
            finished.store(true, std::memory_order_release);
            ReportPipelineJournal();
        }
    }

    std::shared_ptr<const ShaderRecompiler::RecompileResult> readModule(const ShaderRecompiler::PipelineIdentity& identity, const LoadedModule& module) const {
        std::vector<std::byte> payload(static_cast<std::size_t>(module.payloadBytes));
        if (loaded.file == nullptr || !loaded.file->Read(module.payloadOffset, payload) || ShaderRecompiler::HashBytes(payload) != module.payloadHash) return nullptr;
        auto bytes = std::make_shared<std::vector<std::byte>>();
        if (!expandModule(payload, *bytes)) return nullptr;
        auto result = std::make_shared<ShaderRecompiler::RecompileResult>();
        if (!ShaderRecompiler::ShaderDiskCache::DecodeResult(*bytes, *result, bytes) || !((module.stage == ShaderRecompiler::ShaderStage::Compute ? DescribedPipelineIdentity(*result) : result->Identity()) == identity)) return nullptr;
        return result;
    }

    struct Held {
        explicit Held(std::uint64_t bytes) : bytes(bytes) {
            auto& counters = replayCounters();
            const auto now = counters.held.fetch_add(bytes, std::memory_order_relaxed) + bytes;
            auto peak = counters.heldPeak.load(std::memory_order_relaxed);
            while (peak < now && !counters.heldPeak.compare_exchange_weak(peak, now, std::memory_order_relaxed)) {}
        }
        ~Held() { replayCounters().held.fetch_sub(bytes, std::memory_order_relaxed); }
        Held(const Held&) = delete;
        Held& operator=(const Held&) = delete;
        std::uint64_t bytes;
    };

    void replay(const LoadedEntry& entry) {
        struct Replaying {
            Replaying() { replayingHere = true; }
            ~Replaying() { replayingHere = false; }
        } replaying;
        auto& counters = replayCounters();
        if (entry.kind == ComputeRecord) {
            const auto module = loaded.modules.find(entry.compute);
            if (module == loaded.modules.end() || module->second.stage != ShaderRecompiler::ShaderStage::Compute || !hooks.compute) {
                counters.rejected.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            const Held held(module->second.moduleBytes);
            const auto result = readModule(module->first, module->second);
            if (result == nullptr) {
                counters.rejected.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            (hooks.compute(*result, entry.pushConstants) ? counters.compute : counters.present).fetch_add(1, std::memory_order_relaxed);
            return;
        }
        std::vector<const std::pair<const ShaderRecompiler::PipelineIdentity, LoadedModule>*> modules;
        std::uint64_t bytes = 0;
        for (const auto& stage : entry.graphics.stages) {
            const auto module = loaded.modules.find(stage.identity);
            if (module == loaded.modules.end() || module->second.stage != stage.stage) {
                counters.rejected.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            modules.push_back(&*module);
            bytes += module->second.moduleBytes;
        }
        if (modules.empty() || context.descriptorCache == nullptr) {
            counters.rejected.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const auto store = PipelineStoreTotals();
        if (store.cached >= store.bound && !PipelineStoreCanGrow()) {
            counters.skipped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const Held held(bytes);
        std::vector<std::shared_ptr<const ShaderRecompiler::RecompileResult>> results;
        std::vector<CompiledShader> shaders;
        for (std::size_t index = 0; index < modules.size(); ++index) {
            auto result = readModule(modules[index]->first, modules[index]->second);
            if (result == nullptr) {
                counters.rejected.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            shaders.push_back({entry.graphics.stages[index].stage, result.get(), entry.graphics.stages[index].pushConstantOffset});
            results.push_back(std::move(result));
        }
        const auto layouts = DescribeSetLayouts(context, shaders);
        if (!std::ranges::equal(layouts.mainKey, entry.graphics.mainLayoutKey) || !std::ranges::equal(layouts.fragmentKey, entry.graphics.fragmentLayoutKey) || layouts.layouts.split != entry.graphics.splitSets) {
            counters.rejected.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        bool created = false;
        static_cast<void>(CachedPipeline(context, entry.graphics.state, entry.graphics.vertexInput, layouts.View(), shaders, entry.graphics.attachmentLayout, &created));
        (created ? counters.graphics : counters.present).fetch_add(1, std::memory_order_relaxed);
    }

    Context context;
    PipelineJournalHooks hooks;
    std::vector<std::thread> workers;
    std::once_flag loadOnce;
    LoadedJournal loaded;
    std::atomic<std::size_t> cursor{0};
    std::atomic<unsigned> active{0};
    std::atomic<bool> finished{false};
    std::atomic<bool> cancelled{false};
    std::chrono::steady_clock::time_point started;
};

}

State CanonicalPipelineState(const State& state) {
    State canonical{};
    canonical.stages.path = state.stages.path;
    canonical.stages.vertexWaveSize = state.stages.vertexWaveSize;
    canonical.stages.fragmentWaveSize = state.stages.fragmentWaveSize;
    canonical.meshLayerBase = state.meshLayerBase;
    canonical.meshLayerCount = state.meshLayerCount;
    canonical.stages.mesh = state.stages.mesh;
    canonical.stages.tessellation = state.stages.tessellation;
    canonical.stages.hullWaveSize = state.stages.hullWaveSize;
    canonical.stages.tessellationGeometry = state.stages.tessellationGeometry;
    if (state.depth) {
        DepthTarget depth{};
        depth.format = state.depth->format;
        depth.samples = state.depth->samples;
        canonical.depth = depth;
    }
    canonical.depthTest = state.depthTest;
    canonical.depthWrite = state.depthWrite;
    canonical.depthCompare = state.depthCompare;
    canonical.depthBoundsTest = state.depthBoundsTest;
    canonical.depthBias = state.depthBias;
    canonical.depthBiasClamp = state.depthBiasClamp;
    canonical.depthBiasPerFace = state.depthBiasPerFace;
    canonical.stencilTest = state.stencilTest;
    canonical.stencilFront = state.stencilFront;
    canonical.stencilBack = state.stencilBack;
    if (state.stencilReplayReference) canonical.stencilReplayReference = 0u;
    for (const auto& color : state.colors) {
        ColorTarget canonicalColor{};
        canonicalColor.format = color.format;
        canonicalColor.exportIndex = color.exportIndex;
        canonicalColor.componentMapping = color.componentMapping;
        canonicalColor.partialWriteMask = color.partialWriteMask;
        canonicalColor.mergeBlend = color.mergeBlend;
        canonical.colors.push_back(canonicalColor);
    }
    canonical.blends = state.blends;
    canonical.blendConstants = state.blendConstants;
    canonical.hasColorTarget = state.hasColorTarget;
    canonical.rectList = state.rectList;
    canonical.dualSourceBlend = state.dualSourceBlend;
    canonical.reversedBlend = state.reversedBlend;
    canonical.colorRop = state.colorRop;
    canonical.topology = state.topology;
    canonical.primitiveRestart = state.primitiveRestart;
    canonical.negativeOneToOne = state.negativeOneToOne;
    canonical.depthClamp = state.depthClamp;
    canonical.conservativeRasterization = state.conservativeRasterization;
    canonical.cullMode = state.cullMode;
    canonical.polygonMode = state.polygonMode;
    canonical.lineWidth = state.lineWidth;
    canonical.frontFace = state.frontFace;
    canonical.provokingVertexMode = state.provokingVertexMode;
    canonical.samples = state.samples;
    canonical.sampleLocations = state.sampleLocations;
    canonical.customSampleLocations = state.customSampleLocations;
    canonical.sampleLocationGrid = state.sampleLocationGrid;
    canonical.perSampleShading = state.perSampleShading;
    canonical.mergedCoverage = state.mergedCoverage;
    canonical.truncatedCoverage = state.truncatedCoverage;
    canonical.quadSampleMask = state.quadSampleMask;
    canonical.depthRemap.mode = state.depthRemap.mode;
    canonical.vertexPoints = state.vertexPoints;
    canonical.pointSize = state.pointSize;
    canonical.pointSizeMin = state.pointSizeMin;
    canonical.pointSizeMax = state.pointSizeMax;
    canonical.rasterizerDiscard = state.rasterizerDiscard;
    canonical.extraViewports.assign(state.extraViewports.size(), VkViewport{0, 0, 1, 1, 0, 1});
    return canonical;
}

bool GeneratedPipelineStage(const State& state, ShaderRecompiler::ShaderStage stage) {
    using Stage = ShaderRecompiler::ShaderStage;
    return (state.rectList && (stage == Stage::TessellationControl || stage == Stage::TessellationEvaluation)) || stage == Stage::Geometry;
}

GraphicsPipelineRecipe MakeGraphicsPipelineRecipe(const State& state, const VertexInputLayout& vertexInput, const PipelineSetLayouts& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout) {
    GraphicsPipelineRecipe recipe;
    recipe.attachmentLayout = attachmentLayout;
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        recipe.stages.push_back({shader.stage, shader.program->Identity(), shader.pushConstantOffset, GeneratedPipelineStage(state, shader.stage)});
    }
    recipe.vertexInput = vertexInput;
    recipe.mainLayoutKey.assign(resources.mainKey.begin(), resources.mainKey.end());
    recipe.fragmentLayoutKey.assign(resources.fragmentKey.begin(), resources.fragmentKey.end());
    recipe.splitSets = resources.split;
    recipe.state = CanonicalPipelineState(state);
    return recipe;
}

std::vector<std::byte> EncodeGraphicsPipelineRecipe(const GraphicsPipelineRecipe& recipe) {
    std::vector<std::byte> bytes;
    Writer writer(bytes);
    encodeRecipe(writer, recipe);
    return bytes;
}

bool DecodeGraphicsPipelineRecipe(std::span<const std::byte> bytes, GraphicsPipelineRecipe& recipe) {
    Reader reader(bytes);
    return decodeRecipe(reader, recipe);
}

std::uint64_t PipelineSwitch(const char* name, std::uint64_t fallback) {
    return configured(name, fallback);
}

bool PipelineJournalEnabled() {
    return Journal::Instance().Enabled();
}

bool PipelineJournalReplaying() {
    return replayingHere;
}

std::filesystem::path PipelineJournalPath() {
    return Journal::Instance().Path();
}

PipelineJournalCounters PipelineJournalTotals() {
    auto totals = Journal::Instance().Totals();
    const auto& replay = replayCounters();
    totals.replayedGraphics = replay.graphics.load(std::memory_order_relaxed);
    totals.replayedCompute = replay.compute.load(std::memory_order_relaxed);
    totals.replayPresent = replay.present.load(std::memory_order_relaxed);
    totals.replaySkipped = replay.skipped.load(std::memory_order_relaxed);
    totals.replayRejected = replay.rejected.load(std::memory_order_relaxed);
    totals.replayFailed = replay.failed.load(std::memory_order_relaxed);
    totals.replayMs = replay.ms.load(std::memory_order_relaxed);
    totals.replayThreads = replay.threads.load(std::memory_order_relaxed);
    totals.replayHeldPeakBytes = replay.heldPeak.load(std::memory_order_relaxed);
    return totals;
}

void RecordGraphicsPipeline(const GraphicsPipelineRecipe& recipe, std::span<const CompiledShader> shaders) {
    Journal::Instance().RecordGraphics(recipe, shaders);
}

void RecordComputePipeline(const ShaderRecompiler::RecompileResult& shader, bool pushConstants) {
    Journal::Instance().RecordCompute(shader, pushConstants);
}

void FlushPipelineJournal() {
    auto* journal = Journal::Existing();
    if (journal != nullptr && journal->Enabled()) journal->Flush();
}

void ReportPipelineJournal() {
    if (!profiling() || !Journal::Instance().Enabled()) return;
    const auto totals = PipelineJournalTotals();
    AgcDriver::ProfilePrint_nid_no_patch("[pipeline-journal] loaded %llu pipelines, %llu modules (%.1f MiB, %.0f ms); replay on %u threads: %llu graphics and %llu compute built, %llu present, %llu skipped (store full), %llu rejected, %llu failed in %.0f ms, holding at most %.1f MiB of modules; recorded this run: %llu graphics, %llu compute, %llu modules (%.1f MiB), %llu write failures, %llu records evicted (%.1f MiB)\n",
                                         static_cast<unsigned long long>(totals.loadedRecipes), static_cast<unsigned long long>(totals.loadedModules), static_cast<double>(totals.loadedBytes) / (1024.0 * 1024.0), totals.loadMs, totals.replayThreads,
                                         static_cast<unsigned long long>(totals.replayedGraphics), static_cast<unsigned long long>(totals.replayedCompute), static_cast<unsigned long long>(totals.replayPresent), static_cast<unsigned long long>(totals.replaySkipped), static_cast<unsigned long long>(totals.replayRejected), static_cast<unsigned long long>(totals.replayFailed), totals.replayMs, static_cast<double>(totals.replayHeldPeakBytes) / (1024.0 * 1024.0),
                                         static_cast<unsigned long long>(totals.recordedGraphics), static_cast<unsigned long long>(totals.recordedCompute), static_cast<unsigned long long>(totals.recordedModules), static_cast<double>(totals.writtenBytes) / (1024.0 * 1024.0), static_cast<unsigned long long>(totals.writeFailures), static_cast<unsigned long long>(totals.evictedRecords), static_cast<double>(totals.evictedBytes) / (1024.0 * 1024.0));
}

std::uint64_t PipelineJournalDeviceFingerprint(const VkPhysicalDeviceProperties& properties) {
    std::array<std::uint32_t, 7> words{properties.vendorID, properties.deviceID, properties.driverVersion, 0u, 0u, 0u, 0u};
    static_assert(VK_UUID_SIZE == 16);
    std::memcpy(words.data() + 3, properties.pipelineCacheUUID, VK_UUID_SIZE);
    return ShaderRecompiler::HashSpirvWords(words, 0x6a6f75726e616c01ull);
}

std::shared_ptr<PipelineJournalReplay> StartPipelineJournalReplay(const Context& context, std::uint64_t deviceFingerprint, PipelineJournalHooks hooks) {
    if (!Journal::Instance().Enabled() || !Journal::Instance().Bind(deviceFingerprint)) return nullptr;
    const auto threads = static_cast<unsigned>(configured("APS5_PIPELINE_JOURNAL_THREADS", std::max(1u, std::thread::hardware_concurrency() / 2u)));
    if (threads == 0) return nullptr;
    return std::make_shared<Replay>(context, std::move(hooks), threads);
}

}
