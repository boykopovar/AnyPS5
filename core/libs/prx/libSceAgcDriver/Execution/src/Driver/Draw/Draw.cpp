#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DrawWriteRanges.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawProgramTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DrawWriteRanges.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <atomic>
#include <array>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdio>
#include <unordered_set>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace AgcDriver::DriverDetail {

struct DrawTail {
    using GpuLock = std::unique_lock<std::remove_reference_t<decltype(GuestMemory::GpuMutex())>>;
    bool profile = false;
    std::array<double, DrawDriverPhaseCount> phaseMs{};
    std::uint64_t captures = 0;
    std::chrono::steady_clock::time_point phaseLap{};
    DrawPhaseTiming phaseTiming{false, phaseMs, phaseLap};
    Pm4::DrawParameters drawParameters{};
    bool pipelined = false;
    bool traceIndirect = false;
    std::uint64_t dumpTarget = 0;
    std::uint64_t dumpSlot1 = 0;
    std::shared_ptr<VulkanDevice> localDevice;
    bool registerKey = false;
    std::uint64_t drawKey = 0;
    std::shared_ptr<DrawEntry> entry;
    std::shared_ptr<const DrawDecode> decode;
    std::shared_ptr<const DrawDecode> relocatedDecode;
    std::shared_ptr<DrawEntry> dataEntry;
    std::uint64_t shapeKey = 0;
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    std::vector<DrawProgram> programs;
    std::vector<ShaderRecompiler::MemoryRegion> memory;
    std::vector<ShaderRecompiler::LinkedProgram> linked;
    std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>> vertexInfos;
    std::vector<std::vector<Graphics::DecodeRead>> decodeReads;
    std::unique_ptr<ShaderMemory> shaderMemory;
    std::vector<ShaderRecompiler::RecompileResult> results;
    std::vector<std::shared_ptr<const ShaderRecompiler::RecompileResult>> compiledStages;
    std::vector<Graphics::CompiledShader> stages;
    std::uint32_t pushCursorBytes = 0;
    std::vector<const ShaderRecompiler::RecompileResult*> programResults;
    std::vector<StageCapture> stageCaptures;
    std::vector<std::shared_ptr<DispatchVariant>> matched;
    std::vector<std::vector<ShaderRecompiler::MemoryRegion>> matchedRegions;
    std::vector<std::shared_ptr<DispatchVariant>> fresh;
    std::vector<bool> recompiled;
    DrawDataCandidates dataCandidates;
    std::vector<bool> reused;
    std::vector<StageDataHit> stageData;
    bool drawHit = false;
    bool verifyHit = false;
    bool stageEntries = false;
    bool patchedStages = false;
    const Submission* submission = nullptr;
    std::uint32_t submissionQueue = 0;
    std::uint64_t submissionSerial = 0;
    std::size_t packetOffset = 0;
    std::uint32_t packetOpcode = 0;
    std::uint32_t reg207 = 0;
    std::uint64_t epoch = 0;
    QueueState* queue = nullptr;
    std::span<const std::uint32_t> packet;
    std::string* rejected = nullptr;
    std::string ownRejected;
    GpuLock* gpuLock = nullptr;
    GpuLock ownLock{GuestMemory::GpuMutex(), std::defer_lock};
};

namespace {

void noteFinisherHandOff(bool handed) {
    static std::atomic<std::uint64_t> handedOff{0}, kept{0};
    static std::atomic<std::int64_t> reportedAt{0};
    (handed ? handedOff : kept).fetch_add(1, std::memory_order_relaxed);
    static const bool stats = std::getenv("APS5_PIPELINE_STATS") != nullptr || std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!stats) return;
    const auto tenSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 10;
    if (reportedAt.exchange(tenSeconds, std::memory_order_relaxed) == tenSeconds) return;
    std::fprintf(stderr, "[finisher] draws (10 s): %llu handed to the draw finisher, %llu finished on the worker%s\n", static_cast<unsigned long long>(handedOff.exchange(0)), static_cast<unsigned long long>(kept.exchange(0)), DrawFinisher::DrainText().c_str());
}

}

namespace {

enum class HeldReason : std::size_t { Known, NoDecode, Size, NullVertex, NullFragment, NullOther, Bda, Buffer, Storage, Count };
std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(HeldReason::Count)> heldReasons{};

void noteHeld(HeldReason reason) {
    heldReasons[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
    static const bool profile = std::getenv("APS5_PIPELINE_STATS") != nullptr || std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    static std::atomic<std::uint64_t> total{0};
    const auto count = ++total;
    if (count % 262144u != 0u && (count > 262144u || (count & (count - 1u)) != 0u)) return;
    constexpr std::array<const char*, static_cast<std::size_t>(HeldReason::Count)> names{"targets only", "no decode", "stage count", "no variant (vertex)", "no variant (fragment)", "no variant (other)", "BDA stores", "written buffer", "storage image"};
    std::string text;
    for (std::size_t i = 0; i < names.size(); ++i) {
        char item[64];
        std::snprintf(item, sizeof(item), "%s %s %llu", i == 0 ? "" : ",", names[i], static_cast<unsigned long long>(count >= 262144u ? heldReasons[i].exchange(0) : heldReasons[i].load()));
        text += item;
    }
    std::fprintf(stderr, "[finisher-held] held draws' writes:%s\n", text.c_str());
}

std::optional<HeldReason> writesBeyondTargets(const ShaderRecompiler::RecompileResult& compiled) {
    using Role = ShaderRecompiler::DescriptorRole;
    for (const auto& binding : compiled.bindings) {
        if (binding.role == Role::BdaPagetable && Graphics::ShaderStoresThroughAddresses(compiled.spirv)) return HeldReason::Bda;
        if (binding.role == Role::GuestBuffers) {
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                if (element >= binding.bufferWritten.size() || binding.bufferWritten[element]) return HeldReason::Buffer;
            }
        }
        if (binding.role == Role::GuestImages && binding.kind == ShaderRecompiler::DescriptorKind::StorageImage) {
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                if (element >= binding.imageWritten.size() || binding.imageWritten[element]) return HeldReason::Storage;
            }
        }
    }
    return std::nullopt;
}

DrawFinisher::Pending heldWrites(const DrawTail& tail) {
    DrawFinisher::Pending held;
    held.key = tail.drawKey;
    held.shape = tail.shapeKey;
    if (tail.decode == nullptr) {
        noteHeld(HeldReason::NoDecode);
        return held;
    }
    if (tail.matched.size() != tail.programs.size()) {
        noteHeld(HeldReason::Size);
        return held;
    }
    const auto& roles = tail.decode->roles;
    for (std::size_t i = 0; i < tail.matched.size(); ++i) {
        if (i < roles.size() && roles[i] == ShaderRecompiler::ProgramRole::GeometryBack) continue;
        const auto& variant = tail.matched[i];
        if (variant == nullptr || variant->compiled == nullptr) {
            const bool candidates = !tail.drawHit && i < tail.dataCandidates.size() && !tail.dataCandidates[i].empty();
            const auto& program = tail.programs[i];
            if (!candidates && program.snapshot != nullptr && !ProgramWritesMemory(*program.snapshot, program.codeOffset) && (program.tailContinuation == nullptr || !ProgramWritesMemory(*program.tailContinuation, 0))) continue;
            if (!candidates) {
                const auto stage = tail.programs[i].binary.stage;
                noteHeld(stage == ShaderRecompiler::ShaderStage::Fragment ? HeldReason::NullFragment : stage == ShaderRecompiler::ShaderStage::Vertex ? HeldReason::NullVertex : HeldReason::NullOther);
                return held;
            }
            for (const auto& [candidate, regions] : tail.dataCandidates[i]) {
                if (candidate == nullptr || candidate->compiled == nullptr) {
                    noteHeld(HeldReason::NullOther);
                    return held;
                }
                if (const auto reason = writesBeyondTargets(*candidate->compiled)) {
                    noteHeld(*reason);
                    return held;
                }
            }
            continue;
        }
        if (const auto reason = writesBeyondTargets(*variant->compiled)) {
            noteHeld(*reason);
            return held;
        }
        if (i < tail.stageData.size() && tail.stageData[i].compiled != nullptr) {
            if (const auto reason = writesBeyondTargets(*tail.stageData[i].compiled)) {
                noteHeld(*reason);
                return held;
            }
        }
    }
    noteHeld(HeldReason::Known);
    held.writes = DrawWriteRanges(tail.decode->state, {}, ExactDrawWrites());
    held.unknown = false;
    return held;
}

bool EmptyIndirectSkip() {
    static const bool enabled = std::getenv("APS5_NO_EMPTY_INDIRECT_SKIP") == nullptr;
    return enabled;
}

struct CpuIndirectProgramsStorage {};

std::unordered_map<std::uint64_t, bool>& CpuIndirectPrograms() {
    return HostThreadLocal<std::unordered_map<std::uint64_t, bool>, CpuIndirectProgramsStorage>();
}

}

DrawVerdict Driver::draw(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission, std::string& rejected) {
    PerformanceTimer timing("Driver.Draw");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto tailHolder = std::make_shared<DrawTail>();
    auto& tail = *tailHolder;
    auto& phaseMs = tail.phaseMs;
    auto& captures = tail.captures;
    auto& phaseLap = tail.phaseLap;
    phaseLap = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    tail.profile = profile;
    tail.phaseTiming.profile = profile;
    auto& phaseTiming = tail.phaseTiming;
    if (profile && packetStartedAt() != std::chrono::steady_clock::time_point{}) phaseMs[DrawRowPrologue] = std::chrono::duration<double, std::milli>(phaseLap - packetStartedAt()).count();

    const auto drawn = [&] {
        phaseTiming.Phase(DrawRowVectors);
        if (!profile) return DrawVerdict::Drawn;
        auto& pending = pendingDrawPhases();
        pending.phases = true;
        pending.captures = captures;
        pending.ms = phaseMs;
        pending.tailAt = phaseLap;
        return DrawVerdict::Drawn;
    };
    auto& drawParameters = tail.drawParameters;
    drawParameters = Pm4::ResolveDraw(packet, queue);
    tail.pipelined = DrawPipeline::Active();
    const bool pipelined = tail.pipelined;
    static const bool pipelinedIndirect = std::getenv("APS5_NO_PIPELINED_INDIRECT") == nullptr;
    if (pipelined && drawParameters.indirect) {
        const auto& indirect = *drawParameters.indirect;
        auto& pipeline = DrawPipeline::Queue0();
        const bool overlaps = pipeline.Busy() && (pipeline.Overlaps(indirect.arguments, static_cast<std::size_t>(std::max<std::uint64_t>(indirect.RangeBytes(), indirect.recordBytes))) || (indirect.countIndirect && pipeline.Overlaps(indirect.countAddress, 4)));
        if (!pipelinedIndirect || overlaps) pipeline.Drain(DrawPipeline::DrainReason::Indirect);
    }
    auto& traceIndirect = tail.traceIndirect;
    if (const auto verdict = precheckDraw(queue, submission, packet, drawParameters, rejected, traceIndirect)) return *verdict;
    phaseTiming.Phase(DrawRowPrecheck);
    using Stage = ShaderRecompiler::ShaderStage;
    using Role = ShaderRecompiler::ProgramRole;

    static const std::uint64_t dumpTarget = [] { const char* text = std::getenv("APS5_DUMP_DRAW_SHADERS"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();

    static const std::uint64_t dumpSlot1 = [] { const char* text = std::getenv("APS5_DUMP_DRAW_SLOT1"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();

    static const bool lockedPrepare = std::getenv("APS5_LOCKED_DRAW_PREPARE") != nullptr;
    std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
    auto& localDevice = tail.localDevice;
    if (lockedPrepare) {

        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
        gpuLock.lock();
        timing.Mark("gpu_mutex_wait");
        phaseTiming.Phase(DrawRowLockWait);
        if (device == nullptr) device = std::make_shared<VulkanDevice>();
        localDevice = device;

        recordLabelsForPacket(localDevice.get(), submission.queue);
        phaseTiming.Phase(DrawRowLabels);
    } else if ((localDevice = device.Load()) == nullptr) {
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
        std::lock_guard createLock(GuestMemory::GpuMutex());
        if (device == nullptr) device = std::make_shared<VulkanDevice>();
        localDevice = device;
    }
    timing.Mark("device_setup");
    phaseTiming.Phase(DrawRowVectors);

    const bool useDrawEntries = drawEntries() && !ShaderRecompiler::DebugProbeActive() && dumpTarget == 0 && dumpSlot1 == 0;
    tail.registerKey = useDrawEntries && registerKeyEnabled();
    const bool registerKey = tail.registerKey;
    auto& drawKey = tail.drawKey;
    std::array<std::uint64_t, 5> drawKeyComponents{};
    auto& entry = tail.entry;
    auto& decode = tail.decode;
    auto& dataEntry = tail.dataEntry;
    auto& shapeKey = tail.shapeKey;
    std::uint64_t drawIdentity = 0, drawAlias = 0;
    std::vector<DrawUserPatch> userPatches;
    if (registerKey) {
        const auto keyStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        std::shared_ptr<const DrawRelocation> relocation;
        bool aliasFirst = false;
        if (drawRelocationEnabled()) {
            if (DrawFinisher::Busy() && drawRelocationPending.key != 0) DrawFinisher::Get().Drain();
            drawIdentity = drawIdentityKey(queue, *submission.shaders);
            std::lock_guard cacheLock(drawCacheMutex);
            learnPendingRelocation();
            const auto known = drawRelocations.find(drawIdentity);
            if (known != drawRelocations.end()) {
                auto& trial = drawRelocationTrials[drawIdentity];
                if (trial.idle != 0) --trial.idle;
                else relocation = known->second;
                aliasFirst = relocation != nullptr && trial.productive;
            }
        }
        drawKey = drawRegisterKey(queue, *submission.shaders, localDevice->Serial(), &shapeKey, traceDrawCache() ? &drawKeyComponents : nullptr, aliasFirst ? relocation.get() : nullptr, aliasFirst ? &drawAlias : nullptr);
        if (DrawFinisher::Busy()) {
            bool shapeMatters = false;
            if (DrawFinisher::Overlap() && drawDataHits()) {
                std::lock_guard peekLock(drawCacheMutex);
                const auto shape = drawShapes.find(shapeKey);
                shapeMatters = !drawCache.contains(drawKey) && (shape == drawShapes.end() || !drawCache.contains(shape->second));
            }
            const bool drain = !DrawFinisher::Overlap() || DrawFinisher::Get().PendingKey(drawKey, shapeMatters ? shapeKey : 0);
            if (drain) DrawFinisher::Get().Drain();
            if (DrawFinisher::Overlap()) DrawFinisher::NoteDrain(DrawFinisher::DrainSite::Lookup, drain);
        }
        std::unique_lock cacheLock(drawCacheMutex);
        ++drawEntryCounters.lookups;
        ++drawEntryCounters.registerKeyLookups;
        if (profile) drawEntryCounters.keyUs += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - keyStart).count();
        const auto found = drawCache.find(drawKey);
        if (found != drawCache.end()) {
            entry = found->second;
            decode = entry->decode;
        } else {
            ++drawEntryCounters.absent;
            if (relocation != nullptr && !aliasFirst) {
                cacheLock.unlock();
                drawRegisterKey(queue, *submission.shaders, localDevice->Serial(), nullptr, nullptr, relocation.get(), &drawAlias);
                cacheLock.lock();
            }
            if (drawIdentity != 0) drawRelocationPending = {drawKey, drawAlias, drawIdentity};
            if (drawAlias != 0) relocateDrawEntry(queue, drawAlias, entry, decode, userPatches);
            if (relocation != nullptr) {
                auto& trial = drawRelocationTrials[drawIdentity];
                ++trial.lookups;
                if (trial.lookups >= DrawRelocationTrial::Window) {
                    trial.productive = trial.hits * DrawRelocationTrial::MinimumHitShare >= trial.lookups;
                    if (!trial.productive) trial.idle = DrawRelocationTrial::IdleDraws;
                    trial.lookups = trial.hits = 0;
                }
            }
            if (traceDrawCache()) {
                auto& counters = drawEntryCounters;
                if (traceKeysEvicted.contains(drawKey)) ++counters.absentEvicted;
                else if (traceKeysSeen.contains(drawKey)) ++counters.absentNeverInserted;
                else if (traceShapes.contains(shapeKey)) ++counters.absentUserWords;
                else ++counters.absentNewRegisters;
            }
            if (decode == nullptr && drawDataHits() && !drawParameters.indirect) {
                if (const auto shape = drawShapes.find(shapeKey); shape != drawShapes.end()) {
                    const auto kept = drawCache.find(shape->second);
                    if (kept != drawCache.end() && kept->second->decode != nullptr && kept->second->shape == shapeKey) dataEntry = kept->second;
                }
                if (dataEntry != nullptr) ++drawEntryCounters.dataHits;
            }
        }
        if (traceDrawCache()) traceDrawKeyLookup(drawKeyComponents, found == drawCache.end());
        if (traceDrawCache()) {
            traceInsert(traceKeysSeen, drawKey);
            traceInsert(traceShapes, shapeKey);
        }
        maybeReportDrawCache(profile);
    }
    if (decode != nullptr && !DecodeProgramsCurrent(*decode, *submission.shaders)) {
        userPatches.clear();
        std::lock_guard cacheLock(drawCacheMutex);
        eraseDrawEntry(drawKey, entry);
        ++drawEntryCounters.absent;
        entry = nullptr;
        decode = nullptr;
    }
    if (dataEntry != nullptr && !DecodeProgramsCurrent(*dataEntry->decode, *submission.shaders)) dataEntry = nullptr;
    phaseTiming.Phase(DrawRowKeyLookupValidate);

    if (dataEntry != nullptr) {
        auto refreshed = std::make_shared<DrawDecode>(*dataEntry->decode);
        for (auto& program : refreshed->programs) {
            ReadUserWords(queue, program, false);
            if (program.merged) InitializeMerged(queue, program, program.mergedPointer, program.mergedPointerRequired, false);
        }
        if (verifyDrawDataHits()) {
            const auto full = decodeDraw(queue, submission, localDevice->graphicsContext());
            if (!sameDecode(*refreshed, *full)) {
                std::size_t differing = 0;
                while (differing < full->programs.size() && differing < refreshed->programs.size() && full->programs[differing].userData == refreshed->programs[differing].userData) ++differing;
                char message[320];
                std::snprintf(message, sizeof(message), "APS5_VERIFY_DRAW_DATA_HITS: a data-only hit's decode differs from the full decode (key 0x%llx, shape 0x%llx, %zu / %zu programs, first with differing user words %zu, path %u / %u, target 0x%llx / 0x%llx)", static_cast<unsigned long long>(drawKey), static_cast<unsigned long long>(shapeKey), refreshed->programs.size(), full->programs.size(), differing, static_cast<unsigned>(refreshed->state.stages.path), static_cast<unsigned>(full->state.stages.path), static_cast<unsigned long long>(refreshed->state.color.address), static_cast<unsigned long long>(full->state.color.address));
                throw std::runtime_error(message);
            }
        }
        decode = std::move(refreshed);
    }
    resolveDrawDecode(queue, submission, localDevice->graphicsContext(), decode, registerKey, drawKey, profile);
    const auto& graphics = decode->state;
    auto& pixel = tail.pixel;
    pixel = decode->pixel;
    auto& programs = tail.programs;
    programs = decode->programs;
    for (const auto& patch : userPatches) programs[patch.program].userData[patch.index] = patch.value;
    if (const auto skipped = SkippedShader(); skipped != 0 && std::any_of(programs.begin(), programs.end(), [skipped](const DrawProgram& program) { return program.binary.codeAddress == skipped; })) return drawn();
    const bool emptyIndirectSkip = EmptyIndirectSkip();
    auto& cpuIndirectPrograms = CpuIndirectPrograms();
    const auto recordDraws = [&](const Pm4::DrawArguments& arguments) { return arguments.count != 0 && arguments.instances != 0 && (!drawParameters.indexed || arguments.firstVertexOrIndex < drawParameters.indexCount); };
    if (emptyIndirectSkip && drawParameters.indirect && !programs.empty()) {
        const auto known = cpuIndirectPrograms.find(programs.front().binary.codeAddress);
        if (known != cpuIndirectPrograms.end() && known->second) {
            const auto& indirect = *drawParameters.indirect;
            recordQueuedLabelsBeforeRead(submission.queue);
            const auto readStart = std::chrono::steady_clock::now();
            const auto count = std::min(indirect.countIndirect ? Pm4::ReadDrawCount(indirect) : indirect.count, indirect.count);
            bool any = false;
            for (std::uint32_t record = 0; record < count && !any; ++record) any = recordDraws(Pm4::ReadDrawArguments(indirect, record));
            if (!any) {
                Graphics::CountEmptyIndirectDraw(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
                timing.Mark("empty_indirect_skipped");
                return drawn();
            }
        }
    }
    const auto setMeshIndexBuffer = [&](const Pm4::DrawParameters& parameters) {
        if (!graphics.stages.mesh) return;
        auto& words = programs.front().userData;
        require(programs.front().firstUserSgpr == 0 && words.size() >= ShaderRecompiler::MeshIndexBufferUserWord + 4, "mesh program lacks the hidden user words");
        const auto descriptor = Graphics::MeshIndexBufferDescriptor(parameters);
        std::copy(descriptor.begin(), descriptor.end(), words.begin() + ShaderRecompiler::MeshIndexBufferUserWord);
    };
    if (!drawParameters.indirect) setMeshIndexBuffer(drawParameters);
    else if (graphics.stages.mesh) setMeshIndexBuffer(Pm4::DrawParameters{drawParameters.indexAddress, std::max(drawParameters.indexCount, 1u), drawParameters.indexSize, 1, 0, drawParameters.indexed});
    const std::vector<Role>& roles = decode->roles;
    static const bool tracePrograms = std::getenv("APS5_TRACE_DRAW_PROGRAMS") != nullptr;
    if (tracePrograms) {
        std::vector<DrawProgramTraceEntry> identities;
        identities.reserve(programs.size());
        for (std::size_t i = 0; i < programs.size(); ++i) {
            identities.push_back({programs[i].binary.stage, roles[i], programs[i].snapshot->codeAddress, programs[i].binary.codeAddress});
        }
        const auto line = FormatDrawProgramTrace(submission.serial, submission.queue, identities);
        std::fprintf(stderr, "%s\n", line.c_str());
    }
    phaseTiming.Phase(DrawRowDecode);

    const auto locate = [&](std::uint32_t location) -> std::optional<std::pair<std::size_t, std::size_t>> {
        const auto words = UserDataWords(programs, roles, location);
        if (words.empty()) return std::nullopt;
        return words.front();
    };
    if (drawParameters.indirect) {
        auto& indirect = *drawParameters.indirect;
        const auto sgprOf = [&](std::uint32_t location) -> std::int32_t {
            const auto word = locate(location);
            if (!word || word->first != 0) return -1;
            return static_cast<std::int32_t>(programs.front().firstUserSgpr + word->second);
        };
        indirect.baseVertexSgpr = sgprOf(indirect.baseVertexLocation);
        indirect.startInstanceSgpr = sgprOf(indirect.startInstanceLocation);
        indirect.drawIndexSgpr = sgprOf(indirect.drawIndexLocation);
    }
    if (DrawFinisher::Busy()) {
        const bool drain = !DrawFinisher::Overlap();
        if (drain) DrawFinisher::Get().Drain();
        else DrawFinisher::NoteDrain(DrawFinisher::DrainSite::Reads, false);
    }
    auto& memory = tail.memory;
    auto& linked = tail.linked;
    memory.reserve(2 * programs.size() + 32);
    AppendProgramMemory(memory, programs);
    linked.reserve(programs.size());
    for (std::size_t i = 0; i < programs.size(); ++i) {
        const auto& program = programs[i];
        linked.push_back({roles[i], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
    }
    timing.Mark("prepare");
    phaseTiming.Phase(DrawRowProgramPrepare);

    auto& vertexInfos = tail.vertexInfos;
    vertexInfos.resize(programs.size());
    auto& decodeReads = tail.decodeReads;
    decodeReads.resize(programs.size());
    const auto decodeVertexInfo = [&](std::size_t i) {
        const auto& program = programs[i];
        if (program.binary.stage == Stage::Fragment || roles[i] == Role::GeometryBack) return;
        vertexInfos[i] = DecodeOrderedVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, decodeReads[i], 8u - program.firstUserSgpr);
        if (vertexInfos[i]) {
            vertexInfos[i]->paClVsOutCntl = PositionExportLayout(program.snapshot.get(), program.codeOffset, program.binary.code, readRegister(queue.context, 0x207), program.tailContinuation.get()) | graphics.layerExports;
            vertexInfos[i]->depthRemapMode = graphics.depthRemap.mode;
            Graphics::ApplyPointSize(graphics, *vertexInfos[i]);
        }
    };
    if (!registerKey) {
        for (std::size_t i = 0; i < programs.size(); ++i) decodeVertexInfo(i);
        phaseTiming.Phase(DrawRowDecode);
    }
    auto& shaderMemory = tail.shaderMemory;
    shaderMemory = std::make_unique<ShaderMemory>(memory, &queryPendingWrite, &observePendingWrite, hookWaitCounter(), &QueryPendingSpan);
    auto& results = tail.results;
    auto& compiledStages = tail.compiledStages;
    compiledStages.reserve(programs.size());
    auto& stages = tail.stages;
    results.reserve(programs.size() + 2u);
    stages.reserve(programs.size());
    auto& pushCursorBytes = tail.pushCursorBytes;

    auto& programResults = tail.programResults;
    programResults.assign(programs.size(), nullptr);

    auto& stageCaptures = tail.stageCaptures;
    stageCaptures.resize(programs.size());
    auto& matched = tail.matched;
    matched.resize(programs.size());
    auto& matchedRegions = tail.matchedRegions;
    matchedRegions.resize(programs.size());

    auto& fresh = tail.fresh;
    fresh.resize(programs.size());

    auto& recompiled = tail.recompiled;
    recompiled.assign(programs.size(), false);
    auto& dataCandidates = tail.dataCandidates;
    dataCandidates.resize(programs.size());
    auto& reused = tail.reused;
    reused.assign(programs.size(), false);
    auto& stageData = tail.stageData;
    stageData.resize(programs.size());
    auto& drawHit = tail.drawHit;
    auto& verifyHit = tail.verifyHit;
    tail.stageEntries = useDrawEntries && !(graphics.stages.tessellation && !drawParameters.indirect && drawParameters.instanceCount > 1u);
    const bool stageEntries = tail.stageEntries;
    lookupDraw(submission, localDevice, graphics, pixel, programs, roles, vertexInfos, stageEntries, registerKey, profile, drawKey, entry, matched, matchedRegions, drawHit, verifyHit, phaseTiming, phaseMs, dataEntry, dataCandidates, stageData);
    for (std::size_t i = 0; i < matched.size(); ++i) {
        if (matched[i] == nullptr || matched[i]->shaderDataPatches.empty()) continue;
        applyShaderDataPatches(*matched[i]);
        if (stageData[i].compiled != nullptr) stageData[i].compiled = patchDataWords(*matched[i], stageData[i].words);
    }
    const auto relocatedDecode = decode;
    if (!userPatches.empty()) {
        std::lock_guard cacheLock(drawCacheMutex);
        if (drawHit || verifyHit) {
            ++drawRelocationCounters.hits;
            ++drawRelocationTrials[drawIdentity].hits;
        }
        if (profile && drawRelocationCounters.lookups >= 100000) reportDrawRelocation();
    }
    if (!userPatches.empty() && !drawHit) {
        auto current = std::make_shared<DrawDecode>(*decode);
        for (const auto& patch : userPatches) current->programs[patch.program].userData[patch.index] = patch.value;
        decode = std::move(current);
    }
    tail.patchedStages = std::any_of(stageData.begin(), stageData.end(), [](const StageDataHit& hit) { return hit.compiled != nullptr; });
    tail.relocatedDecode = relocatedDecode;

    if (registerKey) {

        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (matched[i] != nullptr && drawHit && !verifyDrawRecipe()) {
                if (matched[i]->vertexInfo != nullptr) vertexInfos[i] = *matched[i]->vertexInfo;
                continue;
            }
            if (!dataCandidates[i].empty() && !verifyDrawDataHits()) {
                const auto& info = dataCandidates[i].front().first->vertexInfo;
                if (info != nullptr) vertexInfos[i] = *info;
                continue;
            }
            decodeVertexInfo(i);
            if (matched[i] != nullptr && verifyDrawRecipe() && (vertexInfos[i].has_value() != (matched[i]->vertexInfo != nullptr) || (vertexInfos[i] && !sameVertexInfo(*vertexInfos[i], *matched[i]->vertexInfo)))) {
                static std::atomic<std::uint64_t> reports{0};
                if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: stage %zu (program 0x%llx) of a hit has a vertex stage info unlike its variant's\n", i, static_cast<unsigned long long>(programs[i].binary.codeAddress));
                std::lock_guard cacheLock(drawCacheMutex);
                ++drawEntryCounters.verifyDecodeMismatches;
            }
        }
        phaseTiming.Phase(DrawRowDecode);
    }

    tail.submission = &submission;
    tail.submissionQueue = submission.queue;
    tail.packetOpcode = (packet[0] >> 8u) & 0xffu;
    tail.reg207 = readRegister(queue.context, 0x207);
    tail.dumpTarget = dumpTarget;
    tail.dumpSlot1 = dumpSlot1;
    const bool handOff = pipelined && !drawParameters.indirect && !profile && !lockedPrepare && dumpTarget == 0 && dumpSlot1 == 0 && !tracePrograms && !verifyDrawDataHits() && deferredLabels().labels.empty() && DrawFinisher::Enabled();
    if (handOff) {
        tail.epoch = DrawPipeline::EpochToken().load(std::memory_order_relaxed);
        tail.submissionSerial = submission.serial;
        tail.packetOffset = static_cast<std::size_t>(packet.data() - submission.commands.data());
        noteFinisherHandOff(true);
        DrawFinisher::Pending held;
        if (DrawFinisher::Overlap()) held = heldWrites(tail);
        DrawFinisher::Get().Enqueue([this, tailHolder] {
            auto& job = *tailHolder;
            DrawPipeline::FollowEpoch(job.epoch);
            GuestMemory::SetCurrentPacket(job.packetOpcode, job.submissionQueue);
            PerformanceTimer finishTiming("Driver.DrawFinish");
            try {
                if (finishDraw(job, finishTiming) == DrawVerdict::Rejected) throw std::runtime_error(job.ownRejected);
            } catch (const std::exception& error) {
                CaptureTrace::Log("draw-error submission=%llu offset=%zu finisher=1 reason=%.256s", static_cast<unsigned long long>(job.submissionSerial), job.packetOffset, error.what());
                throw std::runtime_error("draw at submission " + std::to_string(job.submissionSerial) + " offset " + std::to_string(job.packetOffset) + " (finished on the draw finisher): " + error.what());
            }
        }, std::move(held));
        timing.Mark("draw_handed_off");
        return drawn();
    }
    noteFinisherHandOff(false);
    if (DrawFinisher::Busy()) {
        DrawFinisher::Get().Drain();
        if (DrawFinisher::Overlap()) DrawFinisher::NoteDrain(DrawFinisher::DrainSite::Kept, true);
    }
    tail.queue = &queue;
    tail.packet = packet;
    tail.rejected = &rejected;
    tail.gpuLock = &gpuLock;
    return finishDraw(tail, timing);
}

DrawVerdict Driver::finishDraw(DrawTail& tail, PerformanceTimer& timing) {
    using Stage = ShaderRecompiler::ShaderStage;
    using Role = ShaderRecompiler::ProgramRole;
    const bool profile = tail.profile;
    auto& phaseMs = tail.phaseMs;
    auto& phaseLap = tail.phaseLap;
    auto& captures = tail.captures;
    auto& phaseTiming = tail.phaseTiming;
    auto& drawParameters = tail.drawParameters;
    const bool pipelined = tail.pipelined;
    static const bool pipelinedIndirect = std::getenv("APS5_NO_PIPELINED_INDIRECT") == nullptr;
    const auto drainBeforeInline = [&] {
        if (pipelined && DrawPipeline::Queue0().Busy()) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Indirect);
    };
    auto& traceIndirect = tail.traceIndirect;
    const auto dumpTarget = tail.dumpTarget;
    const auto dumpSlot1 = tail.dumpSlot1;
    auto& gpuLock = tail.gpuLock != nullptr ? *tail.gpuLock : tail.ownLock;
    auto& localDevice = tail.localDevice;
    const bool registerKey = tail.registerKey;
    auto& drawKey = tail.drawKey;
    auto& entry = tail.entry;
    auto& decode = tail.decode;
    auto& dataEntry = tail.dataEntry;
    auto& shapeKey = tail.shapeKey;
    const auto& graphics = decode->state;
    auto& pixel = tail.pixel;
    auto& programs = tail.programs;
    const std::vector<Role>& roles = decode->roles;
    auto& memory = tail.memory;
    auto& linked = tail.linked;
    auto& vertexInfos = tail.vertexInfos;
    auto& decodeReads = tail.decodeReads;
    auto& shaderMemory = tail.shaderMemory;
    auto& results = tail.results;
    auto& compiledStages = tail.compiledStages;
    auto& stages = tail.stages;
    auto& pushCursorBytes = tail.pushCursorBytes;
    auto& programResults = tail.programResults;
    auto& stageCaptures = tail.stageCaptures;
    auto& matched = tail.matched;
    auto& matchedRegions = tail.matchedRegions;
    auto& fresh = tail.fresh;
    auto& recompiled = tail.recompiled;
    auto& dataCandidates = tail.dataCandidates;
    auto& reused = tail.reused;
    auto& stageData = tail.stageData;
    std::uint64_t dataStagesReused = 0, dataStagesCompiled = 0, dataVerified = 0;
    auto& drawHit = tail.drawHit;
    auto& verifyHit = tail.verifyHit;
    const bool stageEntries = tail.stageEntries;
    const bool patchedStages = tail.patchedStages;
    std::uint64_t stageDataVerified = 0;
    const Submission& submission = *tail.submission;
    std::string& rejected = tail.rejected != nullptr ? *tail.rejected : tail.ownRejected;
    static const QueueState noQueue{};
    const QueueState& queue = tail.queue != nullptr ? *tail.queue : noQueue;
    const auto drawn = [&] {
        phaseTiming.Phase(DrawRowVectors);
        if (!profile) return DrawVerdict::Drawn;
        auto& pending = pendingDrawPhases();
        pending.phases = true;
        pending.captures = captures;
        pending.ms = phaseMs;
        pending.tailAt = phaseLap;
        return DrawVerdict::Drawn;
    };
    const auto setMeshIndexBuffer = [&](const Pm4::DrawParameters& parameters) {
        if (!graphics.stages.mesh) return;
        auto& words = programs.front().userData;
        require(programs.front().firstUserSgpr == 0 && words.size() >= ShaderRecompiler::MeshIndexBufferUserWord + 4, "mesh program lacks the hidden user words");
        const auto descriptor = Graphics::MeshIndexBufferDescriptor(parameters);
        std::copy(descriptor.begin(), descriptor.end(), words.begin() + ShaderRecompiler::MeshIndexBufferUserWord);
    };
    const auto locate = [&](std::uint32_t location) -> std::optional<std::pair<std::size_t, std::size_t>> {
        if (location == 0x280u) return std::nullopt;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (roles[i] == Role::Fragment || roles[i] == Role::GeometryBack || location < programs[i].userDataBase) continue;
            const auto word = location - programs[i].userDataBase + (8u - programs[i].firstUserSgpr);
            if (word < programs[i].userData.size()) return std::make_pair(i, static_cast<std::size_t>(word));
        }
        return std::nullopt;
    };
    const auto decodeVertexInfo = [&](std::size_t i) {
        const auto& program = programs[i];
        if (program.binary.stage == Stage::Fragment || roles[i] == Role::GeometryBack) return;
        decodeReads[i].clear();
        vertexInfos[i] = Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, &decodeReads[i], false, 8u - program.firstUserSgpr);
        if (vertexInfos[i]) vertexInfos[i]->paClVsOutCntl = PositionExportLayout(program.snapshot.get(), program.codeOffset, program.binary.code, tail.reg207, program.tailContinuation.get()) | graphics.layerExports;
    };
    const auto fold = [&](const ShaderRecompiler::RecompileResult& main, Pm4::DrawParameters& parameters) { FoldDrawOffsets(main, programs.front(), parameters); };

    const auto freshRuns = [&](std::size_t i, std::vector<std::pair<std::uint64_t, std::uint64_t>>& runs, std::vector<std::uint32_t>& words) {
        std::vector<ShaderRecompiler::MemoryRegion> regions(stageCaptures[i].regions.begin(), stageCaptures[i].regions.end());
        for (const auto& read : decodeReads[i]) regions.push_back({read.address, read.Bytes()});
        std::stable_sort(regions.begin(), regions.end(), [](const ShaderRecompiler::MemoryRegion& a, const ShaderRecompiler::MemoryRegion& b) { return a.guestAddress < b.guestAddress; });
        for (const auto& region : regions) {
            runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
            const auto count = region.bytes.size() / sizeof(std::uint32_t);
            const auto offset = words.size();
            words.resize(offset + count);
            std::memcpy(words.data() + offset, region.bytes.data(), count * sizeof(std::uint32_t));
        }
    };
    const auto verifyDataStage = [&](std::size_t i, const DispatchVariant& kept) {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
        std::vector<std::uint32_t> words;
        freshRuns(i, runs, words);
        const auto& compiled = *stageCaptures[i].compiled;
        const char* what = runs != kept.runs ? "runs" : words != kept.words ? "words" : vertexInfos[i].has_value() != (kept.vertexInfo != nullptr) || (vertexInfos[i] && !sameVertexInfo(*vertexInfos[i], *kept.vertexInfo)) ? "vertex stage info" : compiled.variantId != kept.compiled->variantId ? "variant" : compiled.pushConstants != kept.compiled->pushConstants ? "push constants" : nullptr;
        ++dataVerified;
        if (what == nullptr) return;
        char message[320];
        std::snprintf(message, sizeof(message), "APS5_VERIFY_DRAW_DATA_HITS: stage %zu (program 0x%llx) of a data-only hit differs from its full build in its %s (%zu runs / %zu words kept, %zu / %zu fresh; variant 0x%llx / 0x%llx; key 0x%llx, shape 0x%llx)", i, static_cast<unsigned long long>(programs[i].binary.codeAddress), what, kept.runs.size(), kept.words.size(), runs.size(), words.size(), static_cast<unsigned long long>(kept.compiled->variantId), static_cast<unsigned long long>(compiled.variantId), static_cast<unsigned long long>(drawKey), static_cast<unsigned long long>(shapeKey));
        throw std::runtime_error(message);
    };
    const auto verifyStageData = [&](std::size_t i) {
        const auto& hit = stageData[i];
        const auto& kept = *matched[i];
        std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
        std::vector<std::uint32_t> words;
        freshRuns(i, runs, words);
        const auto& fresh = *stageCaptures[i].compiled;
        const auto& patched = *hit.compiled;
        const bool sameBindings = fresh.bindings.size() == patched.bindings.size() && std::equal(fresh.bindings.begin(), fresh.bindings.end(), patched.bindings.begin(), [](const auto& a, const auto& b) { return a.kind == b.kind && a.role == b.role && a.binding == b.binding && a.count == b.count && a.guestDescriptor == b.guestDescriptor; });
        const char* what = runs != kept.runs ? "runs" : words != hit.words ? "words" : fresh.variantId != patched.variantId ? "variant" : fresh.pushConstants != patched.pushConstants ? "push constants" : !sameBindings ? "bindings" : nullptr;
        ++stageDataVerified;
        if (what == nullptr) return;
        char message[256];
        std::snprintf(message, sizeof(message), "APS5_VERIFY_DRAW_DATA_HITS: stage %zu (program 0x%llx) of a stage data hit differs from its full build in its %s (%zu data positions refreshed)", i, static_cast<unsigned long long>(programs[i].binary.codeAddress), what, kept.dataPositions.size());
        throw std::runtime_error(message);
    };
    std::optional<Graphics::IndirectDrawPath> indirectCpu;
    std::vector<std::uint32_t> pushOffsets(programs.size(), 0);
    std::optional<ShaderRecompiler::MeshBarycentricLayout> meshBarycentrics;
    std::vector<std::size_t> resultIndex(programs.size(), 0);
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (roles[i] == Role::GeometryBack) continue;
        const auto& program = programs[i];
        pushCursorBytes = Graphics::StagePushOffset(pushCursorBytes, program.binary.stage, localDevice->GraphicsPipelineLibraries());
        pushOffsets[i] = pushCursorBytes;
        if (drawHit) {

            programResults[i] = stageData[i].compiled != nullptr ? stageData[i].compiled.get() : matched[i]->compiled.get();
            memory.insert(memory.end(), matchedRegions[i].begin(), matchedRegions[i].end());
        } else {
            const auto candidate = std::find_if(dataCandidates[i].begin(), dataCandidates[i].end(), [&](const auto& kept) { return kept.first->pushOffset == pushCursorBytes; });
            if (candidate != dataCandidates[i].end() && !verifyDrawDataHits()) {
                matched[i] = candidate->first;
                matchedRegions[i] = std::move(candidate->second);
                reused[i] = true;
                programResults[i] = stageData[i].compiled != nullptr ? stageData[i].compiled.get() : matched[i]->compiled.get();
                memory.insert(memory.end(), matchedRegions[i].begin(), matchedRegions[i].end());
            } else {
                if (!dataCandidates[i].empty() && !verifyDrawDataHits()) decodeVertexInfo(i);
                if (program.binary.stage == ShaderRecompiler::ShaderStage::Fragment) {
                    for (auto previous = i; previous-- > 0;) {
                        if (programResults[previous] == nullptr) continue;
                        pixel = Graphics::DefaultUnexportedInputs(decode->pixel, programResults[previous]->parameterExports);
                        break;
                    }
                }
                compiledStages.push_back(materializeDrawStage(i, pushCursorBytes, queue, submission, programs, graphics, pixel, vertexInfos, memory, linked, drawParameters, localDevice, *shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected, meshBarycentrics));
                if (!rejected.empty()) return DrawVerdict::Rejected;
                programResults[i] = compiledStages.back().get();
                if (candidate != dataCandidates[i].end()) verifyDataStage(i, *candidate->first);
                if (stageData[i].compiled != nullptr && verifyDrawDataHits()) verifyStageData(i);
            }
            if (dataEntry != nullptr && reused[i]) ++dataStagesReused;
            else if (dataEntry != nullptr) ++dataStagesCompiled;
        }
        const auto& result = *programResults[i];
        if (i == 0 && drawParameters.indirect) {
            indirectCpu = ClassifyIndirectDraw(result, graphics, programs.front(), localDevice, drawParameters, traceIndirect);
            if (EmptyIndirectSkip()) CpuIndirectPrograms()[programs.front().binary.codeAddress] = indirectCpu.has_value();
        } else if (i == 0) {
            fold(result, drawParameters);
        }
        const auto slotEnd = (pushCursorBytes / Graphics::PipelinePushSlotBytes + 1u) * Graphics::PipelinePushSlotBytes;
        require(result.pushConstants.size() <= slotEnd - pushCursorBytes, "stage push constants exceed the pipeline push constant block");
        stages.push_back({program.binary.stage, &result, result.pushConstants.empty() ? 0u : pushCursorBytes});
        pushCursorBytes += static_cast<std::uint32_t>(result.pushConstants.size());
    }

    if (stageDataVerified != 0) {
        std::lock_guard cacheLock(drawCacheMutex);
        drawEntryCounters.stageDataVerified += stageDataVerified;
    }
    if (dataEntry != nullptr) {
        std::lock_guard cacheLock(drawCacheMutex);
        drawEntryCounters.dataStagesReused += dataStagesReused;
        drawEntryCounters.dataStagesCompiled += dataStagesCompiled;
        drawEntryCounters.dataVerified += dataVerified;
    }
    const bool shapeRefresh = Graphics::DrawPipelineEnabled();
    const auto refreshSeenTwice = [&] {
        std::uint64_t data = shapeKey;
        bool stored = false;
        const auto mix = [&data](std::uint64_t value) {
            data ^= value + 0x9e3779b97f4a7c15ull + (data << 6u) + (data >> 2u);
        };
        for (std::size_t i = 0; i < programs.size(); ++i) {
            const auto& capture = stageCaptures[i];
            if (reused[i] || capture.compiled == nullptr || !CacheableResult(*capture.compiled)) continue;
            stored = true;
            const auto& words = programs[i].userData;
            for (std::size_t w = 0; w < words.size(); ++w) {
                if (w >= 64u || (capture.userDataMask >> w & 1u) != 0) mix((static_cast<std::uint64_t>(i) << 40u) ^ (static_cast<std::uint64_t>(w) << 32u) ^ words[w]);
            }
        }
        if (!stored) return false;
        static std::array<std::atomic<std::uint64_t>, 4096> lastData{};
        return lastData[shapeKey % lastData.size()].exchange(data, std::memory_order_relaxed) == data;
    };
    const bool shapeNeedsEntry = shapeRefresh && registerKey && shapeKey != 0 && drawDataHits() && !drawParameters.indirect && dataEntry != nullptr && dataStagesCompiled != 0 && refreshSeenTwice();
    cacheDrawStages(stageEntries, drawHit, drawParameters, indirectCpu, programs, stageCaptures, vertexInfos, decodeReads, verifyHit, matched, stageData, fresh, drawKey, registerKey, decode, phaseTiming, reused, entry, shapeKey, shapeNeedsEntry);
    timing.Mark("shader_compile_and_link");

    for (const auto& reads : decodeReads) {
        for (const auto& read : reads) memory.push_back({read.address, read.Bytes()});
    }

    if (tail.queue != nullptr && pipelined && !deferredLabels().labels.empty()) DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Labels);
    if (tail.queue != nullptr && recordQueuedLabelsAfterCapture(submission.queue, memory)) return draw(*tail.queue, tail.packet, submission, rejected);

    bool rectListBuilt = false;

    std::size_t rectIndex = 0;
    const auto buildRectList = [&] {
        phaseTiming.Phase(DrawRowVectors);
        require(programs.size() == 2 && programResults[0] != nullptr && programResults[1] != nullptr, "rect-list requires vertex and fragment programs");
        auto rectangle = DrawRectangle(*programs[0].snapshot, programs[1].snapshot, programResults[0]->variantId, programResults[1]->variantId, localDevice->Target());
        auto& control = graphics.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT ? rectangle.lastVertexControl : rectangle.control;
        if (rectListBuilt) {
            results[rectIndex] = std::move(control);
            results[rectIndex + 1] = std::move(rectangle.evaluation);
            phaseTiming.Phase(DrawRowRectList);
            return;
        }
        require(stages.size() == 2, "rect-list requires vertex and fragment programs");
        rectIndex = results.size();
        results.push_back(std::move(control));
        results.push_back(std::move(rectangle.evaluation));
        stages.insert(stages.begin() + 1, {{Stage::TessellationControl, &results[rectIndex], 0}, {Stage::TessellationEvaluation, &results[rectIndex + 1], 0}});
        rectListBuilt = true;
        phaseTiming.Phase(DrawRowRectList);
    };
    if (graphics.rectList) buildRectList();
    if (!programs.empty() && programResults.back() != nullptr && programResults.back()->barycentricEmulation.active && graphics.stages.path == Graphics::ShaderPath::Geometry) {
        require(localDevice->Target().mesh.has_value(), "a pixel shader on the mesh path reads barycentrics, and this device has neither VK_KHR_fragment_shader_barycentric nor mesh shaders (VK_EXT_mesh_shader), so they cannot be provided or emulated");
        require(!graphics.rectList && graphics.stages.mesh && programs.size() == 2 && programResults[0] != nullptr && programResults[1] != nullptr && stages.size() == 2, "a pixel shader that reads barycentrics needs exactly one mesh program before it to emulate them");
        meshBarycentrics = ShaderRecompiler::MeshBarycentricLayoutOf(*programResults[1]);
        if (programResults[0]->meshBarycentrics != meshBarycentrics) {
            const auto pushBytes = programResults[0]->pushConstants.size();
            const auto exports = programResults[0]->parameterExports;
            compiledStages.push_back(materializeDrawStage(0, pushOffsets[0], queue, submission, programs, graphics, pixel, vertexInfos, memory, linked, drawParameters, localDevice, *shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected, meshBarycentrics));
            if (!rejected.empty()) return DrawVerdict::Rejected;
            programResults[0] = compiledStages.back().get();
            stages[0].program = programResults[0];
            require(programResults[0]->pushConstants.size() == pushBytes && programResults[0]->parameterExports == exports, "pairing the mesh program with its pixel shader changed the push constants or the parameter exports the pixel shader was built for");
        }
        require(programResults[0]->meshBarycentrics == meshBarycentrics, "the mesh program was compiled for a different barycentric layout than its pixel shader");
    } else if (!graphics.rectList && !programs.empty() && programResults.back() != nullptr && programResults.back()->barycentricEmulation.active) {
        require(graphics.stages.path == Graphics::ShaderPath::Vertex && programs.size() == 2 && programResults[0] != nullptr && stages.size() == 2, "a pixel shader that reads barycentrics without VK_KHR_fragment_shader_barycentric needs a vertex shader before it");
        const auto geometryLimits = localDevice->GeometryLimits();
        if (geometryLimits || !localDevice->Target().tessellation) {
            results.push_back(ShaderRecompiler::BuildBarycentricGeometryShader(*programResults[0], *programResults[1], localDevice->Target(), geometryLimits));
            stages.insert(stages.begin() + 1, Graphics::CompiledShader{Stage::Geometry, &results.back(), 0});
        } else {
            require((graphics.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || graphics.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) && !graphics.primitiveRestart, "a pixel shader that reads barycentrics without VK_KHR_fragment_shader_barycentric or geometry shaders needs a triangle list or strip without primitive restart");
            auto patches = ShaderRecompiler::BuildBarycentricTessellationShaders(*programResults[0], *programResults[1], localDevice->Target(), localDevice->MaxFragmentInputComponents(), graphics.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT ? 2u : 0u);
            const auto first = results.size();
            results.push_back(std::move(patches.control));
            results.push_back(std::move(patches.evaluation));
            stages.insert(stages.begin() + 1, {{Stage::TessellationControl, &results[first], 0}, {Stage::TessellationEvaluation, &results[first + 1], 0}});
        }
    }
    std::vector<Graphics::GuestMemorySnapshot> snapshots;
    const auto snapshot = [&] {
        snapshots.clear();
        snapshots.reserve(memory.size());
        for (const auto& region : memory) snapshots.push_back({region.guestAddress, region.bytes});
    };
    snapshot();
    timing.Mark("post_compile_prepare");
    const auto lockForDraw = [&] {
        require(tail.queue != nullptr, "the draw finisher reached a draw it cannot commit through the pipeline");
        if (gpuLock.owns_lock()) return;
        phaseTiming.Phase(DrawRowVectors);
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
        gpuLock.lock();
        timing.Mark("gpu_mutex_wait");
        phaseTiming.Phase(DrawRowLockWait);

        if (auto current = device.Load(); current != nullptr && current != localDevice) {
            static std::atomic<std::uint64_t> replaced{0};
            std::fprintf(stderr, "[draw] device replaced during unlocked preparation (%llu)\n", static_cast<unsigned long long>(++replaced));
            localDevice = std::move(current);
        }

        recordLabelsForPacket(localDevice.get(), submission.queue);
        phaseTiming.Phase(DrawRowLabels);
    };
    if (drawParameters.indirect && indirectCpu) {

        const auto indirect = *drawParameters.indirect;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (programResults[i] == nullptr) continue;
            resultIndex[i] = results.size();
            results.push_back(ShaderRecompiler::RecompileResult(*programResults[i]));
            for (auto& stage : stages) {
                if (stage.program == programResults[i]) stage.program = &results[resultIndex[i]];
            }
            programResults[i] = &results[resultIndex[i]];
        }
        recordQueuedLabelsBeforeRead(submission.queue);
        const auto readStart = std::chrono::steady_clock::now();
        const auto count = std::min(indirect.countIndirect ? Pm4::ReadDrawCount(indirect) : indirect.count, indirect.count);
        std::vector<Pm4::DrawArguments> records;
        for (std::uint32_t record = 0; record < count; ++record) records.push_back(Pm4::ReadDrawArguments(indirect, record));
        Graphics::CountIndirectDraw(*indirectCpu, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
        const auto baseVertexWords = UserDataWords(programs, roles, indirect.baseVertexLocation);
        const auto startInstanceWords = UserDataWords(programs, roles, indirect.startInstanceLocation);
        const auto drawIndexWords = indirect.drawIndexEnabled ? UserDataWords(programs, roles, indirect.drawIndexLocation) : std::vector<std::pair<std::size_t, std::size_t>>{};
        const auto drawable = [&](const Pm4::DrawArguments& arguments) { return arguments.count != 0 && arguments.instances != 0 && (!drawParameters.indexed || arguments.firstVertexOrIndex < drawParameters.indexCount); };
        std::size_t lastDrawable = records.size();
        for (std::size_t record = 0; record < records.size(); ++record) {
            if (drawable(records[record])) lastDrawable = record;
        }
        const bool pushWord = drawCachePushWord();
        const bool verifyPushWord = pushWord && verifyDrawPushWord();
        std::uint64_t wordsForwarded = 0, stagesRecompiled = 0, wordsRefused = 0, wordsVerified = 0;
        struct RecordWord {
            std::size_t program;
            std::size_t index;
            std::uint32_t value;
        };
        std::vector<RecordWord> recordWords;
        const auto countRecordWords = [&] {
            if (wordsForwarded == 0 && stagesRecompiled == 0 && wordsRefused == 0 && wordsVerified == 0) return;
            std::lock_guard cacheLock(drawCacheMutex);
            drawEntryCounters.recordWordsForwarded += wordsForwarded;
            drawEntryCounters.recordStagesRecompiled += stagesRecompiled;
            drawEntryCounters.recordWordsRefused += wordsRefused;
            drawEntryCounters.recordWordsVerified += wordsVerified;
        };
        const auto sameInfo = [&](const std::optional<ShaderRecompiler::ShaderVertexStageInfo>& a, const std::optional<ShaderRecompiler::ShaderVertexStageInfo>& b) { return a.has_value() == b.has_value() && (!a || sameVertexInfo(*a, *b)); };
        const auto forwardRecordWords = [&](std::size_t programIndex, ShaderRecompiler::RecompileResult& result, const std::optional<ShaderRecompiler::ShaderVertexStageInfo>& previousInfo) {
            std::uint64_t walked = 0;
            if (stageCaptures[programIndex].capture != nullptr) walked = stageCaptures[programIndex].userDataUse.asBase | stageCaptures[programIndex].userDataUse.asValue;
            else if (matched[programIndex] != nullptr) walked = matched[programIndex]->userWordsWalked;
            else return false;
            if ((graphics.stages.mesh && programIndex == 0) || !sameInfo(previousInfo, vertexInfos[programIndex])) return false;
            std::vector<std::pair<std::uint32_t, std::uint32_t>> slots;
            bool any = false;
            for (const auto& word : recordWords) {
                if (word.program != programIndex) continue;
                any = true;
                if (word.index >= ShaderRecompiler::SrtUserDataUse::TrackedWords || (walked >> word.index & 1u) != 0) return false;
                const auto& forwarded = result.shaderDataUserWords;
                const auto slot = std::find(forwarded.begin(), forwarded.end(), static_cast<std::uint32_t>(word.index));
                if (slot != forwarded.end()) slots.emplace_back(static_cast<std::uint32_t>(slot - forwarded.begin()), word.value);
            }
            if (!any) return false;
            for (const auto& [slot, value] : slots) patchForwardedWord(result, slot, value);
            return true;
        };
        const auto verifyForwardedWords = [&](std::size_t programIndex, const ShaderRecompiler::RecompileResult& forwarded, const ShaderRecompiler::RecompileResult& fresh) {
            const bool sameBindings = forwarded.bindings.size() == fresh.bindings.size() && std::equal(forwarded.bindings.begin(), forwarded.bindings.end(), fresh.bindings.begin(), [](const auto& a, const auto& b) { return a.kind == b.kind && a.role == b.role && a.binding == b.binding && a.count == b.count && a.guestDescriptor == b.guestDescriptor; });
            std::uint64_t walked = 0;
            for (const auto& word : recordWords) {
                if (word.program == programIndex && word.index < ShaderRecompiler::SrtUserDataUse::TrackedWords) walked |= (stageCaptures[programIndex].userDataUse.asBase | stageCaptures[programIndex].userDataUse.asValue) >> word.index & 1u;
            }
            const char* what = forwarded.variantId != fresh.variantId ? "variant" : forwarded.pushConstants != fresh.pushConstants ? "push constants" : !sameBindings ? "bindings" : forwarded.shaderDataUserWords != fresh.shaderDataUserWords ? "forwarded words" : walked != 0 ? "walked user word" : nullptr;
            ++wordsVerified;
            if (what == nullptr) return;
            char message[256];
            std::snprintf(message, sizeof(message), "APS5_VERIFY_DRAW_PUSH_WORD: stage %zu (program 0x%llx) with its record's user word forwarded as push data differs from its full build in its %s", programIndex, static_cast<unsigned long long>(programs[programIndex].binary.codeAddress), what);
            throw std::runtime_error(message);
        };
        for (std::uint32_t record = 0; record < records.size(); ++record) {
            const auto& arguments = records[record];
            if (traceIndirect) std::fprintf(stderr, "[draw]   record %u: count %u instances %u first %u vertexOffset %u startInstance %u\n", record, arguments.count, arguments.instances, arguments.firstVertexOrIndex, arguments.vertexOffset, arguments.firstInstance);
            if (arguments.count == 0 || arguments.instances == 0) continue;
            std::set<std::size_t> patched;
            recordWords.clear();
            const auto patch = [&](const std::vector<std::pair<std::size_t, std::size_t>>& words, std::uint32_t value) {
                for (const auto& [program, word] : words) {
                    programs[program].userData[word] = value;
                    patched.insert(program);
                    recordWords.push_back({program, word, value});
                }
            };
            patch(baseVertexWords, indirect.recordBytes == 20 ? arguments.vertexOffset : arguments.firstVertexOrIndex);
            patch(startInstanceWords, arguments.firstInstance);
            patch(drawIndexWords, record);
            if (graphics.stages.tessellation) {
                for (std::size_t i = 0; i < programs.size(); ++i) {
                    if (programs[i].binary.stage == Stage::TessellationControl || programs[i].binary.stage == Stage::TessellationEvaluation) patched.insert(i);
                }
            }
            Pm4::DrawParameters direct{0, arguments.count, 0, arguments.instances, drawParameters.flags, drawParameters.indexed, 0, 0};
            if (drawParameters.indexed) {

                if (arguments.firstVertexOrIndex >= drawParameters.indexCount) continue;
                direct.indexAddress = drawParameters.indexAddress + static_cast<std::uint64_t>(arguments.firstVertexOrIndex) * drawParameters.indexSize;
                direct.indexCount = std::min(arguments.count, drawParameters.indexCount - arguments.firstVertexOrIndex);
                direct.indexSize = drawParameters.indexSize;
            } else {
                direct.firstVertex = indirect.indxOffset;
            }
            if (graphics.stages.mesh) {
                setMeshIndexBuffer(direct);
                patched.insert(0);
            }
            for (const auto programIndex : patched) {
                auto& result = results[resultIndex[programIndex]];
                const auto pushBytes = result.pushConstants.size();
                const auto previousInfo = vertexInfos[programIndex];
                decodeVertexInfo(programIndex);
                if (pushWord && forwardRecordWords(programIndex, result, previousInfo)) {
                    wordsForwarded += static_cast<std::uint64_t>(std::count_if(recordWords.begin(), recordWords.end(), [&](const RecordWord& word) { return word.program == programIndex; }));
                    if (!verifyPushWord) continue;
                    const auto fresh = materializeDrawStage(programIndex, pushOffsets[programIndex], queue, submission, programs, graphics, pixel, vertexInfos, memory, linked, drawParameters, localDevice, *shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected, meshBarycentrics);
                    if (!rejected.empty()) return DrawVerdict::Rejected;
                    verifyForwardedWords(programIndex, result, *fresh);
                    continue;
                }
                if (pushWord) wordsRefused += static_cast<std::uint64_t>(std::count_if(recordWords.begin(), recordWords.end(), [&](const RecordWord& word) { return word.program == programIndex; }));
                result = *materializeDrawStage(programIndex, pushOffsets[programIndex], queue, submission, programs, graphics, pixel, vertexInfos, memory, linked, programs[programIndex].binary.stage == Stage::TessellationControl || programs[programIndex].binary.stage == Stage::TessellationEvaluation ? direct : drawParameters, localDevice, *shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected, meshBarycentrics);
                if (!rejected.empty()) return DrawVerdict::Rejected;
                require(result.pushConstants.size() == pushBytes, "patched program changed its push constant layout");
                ++stagesRecompiled;
            }
            fold(*programResults[0], direct);
            if (graphics.rectList && patched.contains(0)) buildRectList();
            snapshot();
            if (auto known = localDevice->KnownDrawRejection(graphics, stages)) {
                rejected = std::move(*known);
                return DrawVerdict::Rejected;
            }
            if (pipelined && pipelinedIndirect && record == lastDrawable && !gpuLock.owns_lock() && deferredLabels().labels.empty()) {
                phaseTiming.Phase(DrawRowVectors);
                auto writes = drawWriteRanges(graphics, stages);
                const auto commitQueue = submission.queue;
                const auto commitPacket = tail.packetOpcode;
                const auto epoch = DrawPipeline::EpochToken().load(std::memory_order_relaxed);
                auto commit = [this, localDevice = std::move(localDevice), decode = std::move(decode), direct, stages = std::move(stages), snapshots = std::move(snapshots), commitQueue, commitPacket, epoch, programs = std::move(programs), results = std::move(results), memory = std::move(memory), linked = std::move(linked), stageCaptures = std::move(stageCaptures), matched = std::move(matched), matchedRegions = std::move(matchedRegions), fresh = std::move(fresh), decodeReads = std::move(decodeReads), shaderMemory = std::move(shaderMemory), entry = std::move(entry), dataEntry = std::move(dataEntry), compiledStages = std::move(compiledStages), stageData = std::move(stageData)] {
                    DrawPipeline::FollowEpoch(epoch);
                    GuestMemory::SetCurrentPacket(commitPacket, commitQueue);
                    commitDraw(localDevice, commitQueue, decode->state, direct, stages, snapshots, nullptr, {}, 0);
                };
                auto held = std::make_shared<decltype(commit)>(std::move(commit));
                DrawPipeline::Queue0().Enqueue([held] { (*held)(); }, std::move(writes));
                phaseTiming.Phase(DrawRowLockWait);
                countRecordWords();
                timing.Mark("draw_enqueued");
                return drawn();
            }
            drainBeforeInline();
            lockForDraw();
            noteDrawWriters(stages, submission.queue);
            phaseTiming.Phase(DrawRowVectors);
            localDevice->Draw(graphics, direct, stages, snapshots);
            phaseTiming.Phase(DrawRowGraphics);
        }
        countRecordWords();
        timing.Mark("draw_and_resource_release");
        return drawn();
    }

    std::vector<std::shared_ptr<DispatchVariant>> recipeStages;
    if (registerKey && !drawParameters.indirect && !patchedStages && Graphics::DrawRecipes()) {
        recipeStages.reserve(programs.size());
        for (std::size_t i = 0; i < programs.size(); ++i) recipeStages.push_back(drawHit ? matched[i] : fresh[i]);
        if (std::all_of(recipeStages.begin(), recipeStages.end(), [](const std::shared_ptr<DispatchVariant>& variant) { return variant == nullptr; })) recipeStages.clear();
    }
    std::shared_ptr<const DrawRecipe> recipe;
    if (drawHit && !recipeStages.empty()) {
        recipe = findDrawRecipe(drawKey, recipeStages);
        if (recipe == nullptr) VulkanDevice::NoteDrawRecipeMiss(VulkanDevice::DrawRecipePrecheck::NoRecipe);
    }
    if (recipe == nullptr) {
        if (auto known = localDevice->KnownDrawRejection(graphics, stages)) {
            rejected = std::move(*known);
            return DrawVerdict::Rejected;
        }
    }
    if (pipelined && !drawParameters.indirect && (tail.queue == nullptr || deferredLabels().labels.empty())) {
        phaseTiming.Phase(DrawRowVectors);
        auto writes = drawWriteRanges(graphics, stages);
        const auto commitQueue = submission.queue;
        const auto commitPacket = tail.packetOpcode;
        const auto epoch = DrawFinisher::OnFinisherThread() ? tail.epoch : DrawPipeline::EpochToken().load(std::memory_order_relaxed);
        auto reads = QueuedDrawReadRanges(drawParameters, stages);
        auto commit = [this, localDevice = std::move(localDevice), decode = std::move(decode), drawParameters, stages = std::move(stages), snapshots = std::move(snapshots), recipe = std::move(recipe), recipeStages = std::move(recipeStages), drawKey, commitQueue, commitPacket, epoch, programs = std::move(programs), results = std::move(results), memory = std::move(memory), linked = std::move(linked), stageCaptures = std::move(stageCaptures), matched = std::move(matched), matchedRegions = std::move(matchedRegions), fresh = std::move(fresh), decodeReads = std::move(decodeReads), shaderMemory = std::move(shaderMemory), entry = std::move(entry), dataEntry = std::move(dataEntry), compiledStages = std::move(compiledStages), stageData = std::move(stageData)]() mutable {
            DrawPipeline::FollowEpoch(epoch);
            GuestMemory::SetCurrentPacket(commitPacket, commitQueue);
            commitDraw(localDevice, commitQueue, decode->state, drawParameters, stages, snapshots, recipe, recipeStages, drawKey);
        };
        auto held = std::make_shared<decltype(commit)>(std::move(commit));
        DrawPipeline::Queue0().Enqueue([held] { (*held)(); }, std::move(writes), 0, {}, std::move(reads), true);
        phaseTiming.Phase(DrawRowLockWait);
        timing.Mark("draw_enqueued");
        return drawn();
    }
    if (drawParameters.indirect) drainBeforeInline();
    lockForDraw();
    noteDrawWriters(stages, submission.queue);
    phaseTiming.Phase(DrawRowVectors);
    if (recipe != nullptr) {
        if (localDevice->DrawFromRecipe(graphics, drawParameters, stages, snapshots, recipe) == RecipeOutcome::Recorded) {
            phaseTiming.Phase(DrawRowGraphics);
            timing.Mark("draw_and_resource_release");
            return drawn();
        }

        VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Restart, VulkanDevice::RecipeKind::Draw);
    }
    std::shared_ptr<const DrawRecipe> built;
    localDevice->Draw(graphics, drawParameters, stages, snapshots, recipeStages.empty() ? nullptr : &built);
    phaseTiming.Phase(DrawRowGraphics);
    if (built != nullptr) attachDrawRecipe(drawKey, recipeStages, std::move(built));
    timing.Mark("draw_and_resource_release");
    return drawn();
}

void Driver::commitDraw(std::shared_ptr<VulkanDevice> localDevice, std::uint32_t queue, const Graphics::State& graphics, const Pm4::DrawParameters& drawParameters, std::span<const Graphics::CompiledShader> stages, std::span<const Graphics::GuestMemorySnapshot> snapshots, const std::shared_ptr<const DrawRecipe>& recipe, const std::vector<std::shared_ptr<DispatchVariant>>& recipeStages, std::uint64_t drawKey) {
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    if (auto current = device.Load(); current != nullptr && current != localDevice) localDevice = std::move(current);
    struct SubmitDue {
        Driver& driver;
        const std::shared_ptr<VulkanDevice>& device;
        ~SubmitDue() noexcept(false) {
            if (std::uncaught_exceptions() == 0) driver.submitDueAfterCommit(device.get());
        }
    } submitDue{*this, localDevice};
    recordLabelsForPacket(localDevice.get(), queue);
    noteDrawWriters(stages, queue);
    if (recipe != nullptr) {
        if (localDevice->DrawFromRecipe(graphics, drawParameters, stages, snapshots, recipe) == RecipeOutcome::Recorded) return;
        VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Restart, VulkanDevice::RecipeKind::Draw);
    }
    std::shared_ptr<const DrawRecipe> built;
    localDevice->Draw(graphics, drawParameters, stages, snapshots, recipeStages.empty() ? nullptr : &built);
    if (built != nullptr) attachDrawRecipe(drawKey, recipeStages, std::move(built));
}

}
