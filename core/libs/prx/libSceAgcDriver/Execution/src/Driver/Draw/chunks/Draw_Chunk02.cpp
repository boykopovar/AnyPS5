#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DrawWriteRanges.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawProgramTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DrawWriteRanges.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
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
#include <algorithm>
#include <map>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <atomic>
#include <exception>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <typeinfo>

namespace AgcDriver::DriverDetail {
namespace {
namespace {
namespace {
DrawVerdict Driver::finishDraw(DrawTail& tail, PerformanceTimer& timing) {
    using Stage = ShaderRecompiler::ShaderStage;
    using Role = ShaderRecompiler::ProgramRole;
    const bool profile = tail.profile;
    const auto traceToken = tail.traceToken;
    const auto& trace = tail.trace;
    const auto* tracePtr = trace.token != 0 ? &trace : nullptr;
    const auto packetOffset = tail.packetOffset;
    const auto packet = tail.packet;
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
    auto& writablePrograms = tail.writablePrograms;
    const auto& programs = *tail.programs;
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
        auto& words = writablePrograms.front().userData;
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
        if (vertexInfos[i]) {
            vertexInfos[i]->paClVsOutCntl = PositionExportLayout(program.snapshot.get(), program.codeOffset, program.binary.code, tail.reg207, program.tailContinuation.get()) | graphics.layerExports | graphics.viewportExports;
            vertexInfos[i]->depthRemapMode = graphics.depthRemap.mode;
            Graphics::ApplyPointSize(graphics, *vertexInfos[i]);
        }
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
    const auto joinedRuns = [](const std::vector<std::pair<std::uint64_t, std::uint64_t>>& runs) {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> joined;
        for (const auto& run : runs) {
            if (!joined.empty() && joined.back().second == run.first) joined.back().second = run.second;
            else joined.push_back(run);
        }
        return joined;
    };
    const auto listRuns = [](const std::vector<std::pair<std::uint64_t, std::uint64_t>>& runs) {
        std::string text;
        for (std::size_t run = 0; run < runs.size() && run < 4; ++run) {
            char item[48];
            std::snprintf(item, sizeof(item), " [0x%llx,0x%llx)", static_cast<unsigned long long>(runs[run].first), static_cast<unsigned long long>(runs[run].second));
            text += item;
        }
        return text;
    };
    const auto verifyDataStage = [&](std::size_t i, const DispatchVariant& kept) {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
        std::vector<std::uint32_t> words;
        freshRuns(i, runs, words);
        const auto& compiled = *stageCaptures[i].compiled;
        const char* what = joinedRuns(runs) != joinedRuns(kept.runs) ? "runs" : words != kept.words ? "words" : vertexInfos[i].has_value() != (kept.vertexInfo != nullptr) || (vertexInfos[i] && !sameVertexInfo(*vertexInfos[i], *kept.vertexInfo)) ? "vertex stage info" : compiled.variantId != kept.compiled->variantId ? "variant" : compiled.pushConstants != kept.compiled->pushConstants ? "push constants" : nullptr;
        ++dataVerified;
        if (what == nullptr) return;
        char message[640];
        std::snprintf(message, sizeof(message), "APS5_VERIFY_DRAW_DATA_HITS: stage %zu (program 0x%llx) of a data-only hit differs from its full build in its %s (%zu runs / %zu words kept, %zu / %zu fresh; variant 0x%llx / 0x%llx; key 0x%llx, shape 0x%llx; kept%s; fresh%s)", i, static_cast<unsigned long long>(programs[i].binary.codeAddress), what, kept.runs.size(), kept.words.size(), runs.size(), words.size(), static_cast<unsigned long long>(kept.compiled->variantId), static_cast<unsigned long long>(compiled.variantId), static_cast<unsigned long long>(drawKey), static_cast<unsigned long long>(shapeKey), listRuns(joinedRuns(kept.runs)).c_str(), listRuns(joinedRuns(runs)).c_str());
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
        const char* what = joinedRuns(runs) != joinedRuns(kept.runs) ? "runs" : words != hit.words ? "words" : fresh.variantId != patched.variantId ? "variant" : fresh.pushConstants != patched.pushConstants ? "push constants" : !sameBindings ? "bindings" : nullptr;
        ++stageDataVerified;
        if (what == nullptr) return;
        char message[256];
        std::snprintf(message, sizeof(message), "APS5_VERIFY_DRAW_DATA_HITS: stage %zu (program 0x%llx) of a stage data hit differs from its full build in its %s (%zu data positions refreshed)", i, static_cast<unsigned long long>(programs[i].binary.codeAddress), what, kept.dataPositions.size());
        throw std::runtime_error(message);
    };
    std::optional<Graphics::IndirectDrawPath> indirectCpu;
    std::size_t completedSubdraws = 0;
    std::vector<std::uint32_t> pushOffsets(programs.size(), 0);
    std::optional<ShaderRecompiler::MeshBarycentricLayout> meshBarycentrics;
    bool forcedBarycentrics = false;
    std::vector<std::size_t> resultIndex(programs.size(), 0);
    const bool fixedSlots = localDevice->GraphicsPipelineLibraries();
    const bool hasFragment = std::ranges::any_of(programs, [](const DrawProgram& program) { return program.binary.stage == ShaderRecompiler::ShaderStage::Fragment; });
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (roles[i] == Role::GeometryBack) continue;
        const auto& program = programs[i];
        pushCursorBytes = Graphics::StagePushOffset(pushCursorBytes, program.binary.stage, fixedSlots);
        require(pushCursorBytes <= Graphics::PushBlockBytes(fixedSlots), "stage push constant offset exceeds the pipeline push constant block");
        const auto pushBudget = Graphics::StagePushBudget(Graphics::StagePushLimit(graphics.stages, pushCursorBytes, graphics.depthRemap.mode != 0u), pushCursorBytes, program.binary.stage, fixedSlots, hasFragment);
        pushOffsets[i] = pushCursorBytes;
        if (drawHit) {

            programResults[i] = stageData[i].compiled != nullptr ? stageData[i].compiled.get() : matched[i]->compiled.get();
            memory.insert(memory.end(), matchedRegions[i].begin(), matchedRegions[i].end());
        } else {
            const auto candidate = std::find_if(dataCandidates[i].begin(), dataCandidates[i].end(), [&](const auto& kept) { return kept.first->pushOffset == pushCursorBytes && kept.first->compiled->pushConstants.size() <= pushBudget; });
            if (candidate != dataCandidates[i].end() && !verifyDrawDataHits()) {
                matched[i] = candidate->first;
                stageCaptures[i].registeredViews = matched[i]->registeredViews;
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
                compiledStages.push_back(materializeDrawStage(i, pushCursorBytes, queue, submission, packetOffset, trace.token, programs, graphics, pixel, vertexInfos, memory, linked, drawParameters, localDevice, *shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected, meshBarycentrics));
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
        try {
            require(result.pushConstants.size() <= pushBudget, "stage push constants exceed the pipeline push constant block");
            stages.push_back({program.binary.stage, &result, result.pushConstants.empty() ? 0u : pushCursorBytes, stageCaptures[i].registeredViews.get()});
            pushCursorBytes += static_cast<std::uint32_t>(result.pushConstants.size());
        } catch (...) {
            TraceMaterializationFailure(trace.token, submission, packetOffset, "push-constant-validation", completedSubdraws, std::current_exception());
            throw;
        }
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
    if (recordQueuedLabelsAfterCapture(submission.queue, memory)) return draw(*tail.queue, packet, submission, packetOffset, rejected);

    bool rectListBuilt = false;

    std::size_t rectIndex = 0;
    const auto buildRectList = [&] {
        try {
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
            results.reserve(programs.size() + 2u);
            rectIndex = results.size();
            results.push_back(std::move(control));
            results.push_back(std::move(rectangle.evaluation));
            stages.insert(stages.begin() + 1, {{Stage::TessellationControl, &results[rectIndex], 0}, {Stage::TessellationEvaluation, &results[rectIndex + 1], 0}});
            rectListBuilt = true;
            phaseTiming.Phase(DrawRowRectList);
        } catch (...) {
            TraceMaterializationFailure(trace.token, submission, packetOffset, "rect-list", completedSubdraws, std::current_exception());
            throw;
        }
    };
    if (graphics.rectList) buildRectList();
    if (graphics.stages.tessellationGeometry) {
        static std::mutex captureMutex;
        static std::map<std::pair<std::uint32_t, std::uint32_t>, ShaderRecompiler::RecompileResult> captureShaders;
        const auto& geometry = *graphics.stages.tessellationGeometry;
        results.reserve(programs.size() + 2u);
        {
            std::lock_guard captureLock(captureMutex);
            auto found = captureShaders.find({geometry.esgsItemSize, geometry.primitivesPerGroup});
            if (found == captureShaders.end()) found = captureShaders.emplace(std::pair{geometry.esgsItemSize, geometry.primitivesPerGroup}, ShaderRecompiler::BuildTessellationCaptureGeometryShader(geometry, localDevice->Target(), localDevice->GeometryLimits())).first;
            results.push_back(found->second);
        }
        const auto evaluation = std::ranges::find_if(stages, [](const Graphics::CompiledShader& stage) { return stage.stage == Stage::TessellationEvaluation; });
        require(evaluation != stages.end(), "tessellation followed by a geometry stage has no domain program");
        stages.insert(std::next(evaluation), Graphics::CompiledShader{Stage::Geometry, &results.back(), 0});
    }
    const bool lastVertexPatches = programs.size() == 2u && programResults[0] != nullptr && programResults[1] != nullptr && Graphics::LastVertexPatches(graphics, stages, localDevice->ProvokingVertexLast(), localDevice->Target().tessellation.has_value(), localDevice->PrimitiveListRestart(), drawParameters.indexed);
    if (lastVertexPatches && !programResults[1]->barycentricEmulation.active) {
        forcedBarycentrics = true;
        const auto pushBytes = programResults[1]->pushConstants.size();
        pixel = Graphics::DefaultUnexportedInputs(decode->pixel, programResults[0]->parameterExports);
        compiledStages.push_back(materializeDrawStage(1, pushOffsets[1], queue, submission, packetOffset, traceToken, programs, graphics, pixel, vertexInfos, memory, linked, drawParameters, localDevice, *shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected, meshBarycentrics, true));
        if (!rejected.empty()) return DrawVerdict::Rejected;
        programResults[1] = compiledStages.back().get();
        stages[1].program = programResults[1];
        stages[1].registeredImageViews = stageCaptures[1].registeredViews.get();
        require(programResults[1]->barycentricEmulation.active && programResults[1]->pushConstants.size() == pushBytes, "emulating barycentrics for a last provoking vertex changed the pixel shader's push constants or left it without emulated barycentrics");
    }
    if (!programs.empty() && programResults.back() != nullptr && programResults.back()->barycentricEmulation.active && graphics.stages.path == Graphics::ShaderPath::Geometry) {
        require(localDevice->Target().mesh.has_value(), "a pixel shader on the mesh path reads barycentrics, and this device has neither VK_KHR_fragment_shader_barycentric nor mesh shaders (VK_EXT_mesh_shader), so they cannot be provided or emulated");
        require(!graphics.rectList && graphics.stages.mesh && programs.size() == 2 && programResults[0] != nullptr && programResults[1] != nullptr && stages.size() == 2, "a pixel shader that reads barycentrics needs exactly one mesh program before it to emulate them");
        meshBarycentrics = ShaderRecompiler::MeshBarycentricLayoutOf(*programResults[1]);
        if (programResults[0]->meshBarycentrics != meshBarycentrics) {
            const auto pushBytes = programResults[0]->pushConstants.size();
            const auto exports = programResults[0]->parameterExports;
            compiledStages.push_back(materializeDrawStage(0, pushOffsets[0], queue, submission, packetOffset, traceToken, programs, graphics, pixel, vertexInfos, memory, linked, drawParameters, localDevice, *shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected, meshBarycentrics));
            if (!rejected.empty()) return DrawVerdict::Rejected;
            programResults[0] = compiledStages.back().get();
            stages[0].program = programResults[0];
            stages[0].registeredImageViews = stageCaptures[0].registeredViews.get();
            require(programResults[0]->pushConstants.size() == pushBytes && programResults[0]->parameterExports == exports, "pairing the mesh program with its pixel shader changed the push constants or the parameter exports the pixel shader was built for");
        }
        require(programResults[0]->meshBarycentrics == meshBarycentrics, "the mesh program was compiled for a different barycentric layout than its pixel shader");
    } else if (!graphics.rectList && !programs.empty() && programResults.back() != nullptr && programResults.back()->barycentricEmulation.active) {
        require(graphics.stages.path == Graphics::ShaderPath::Vertex && programs.size() == 2 && programResults[0] != nullptr && stages.size() == 2, "a pixel shader that reads barycentrics without VK_KHR_fragment_shader_barycentric needs a vertex shader before it");
        const auto geometryLimits = localDevice->GeometryLimits();
        results.reserve(programs.size() + 2u);
        if (!lastVertexPatches && (geometryLimits || !localDevice->Target().tessellation)) {
            results.push_back(ShaderRecompiler::BuildBarycentricGeometryShader(*programResults[0], *programResults[1], localDevice->Target(), geometryLimits));
            stages.insert(stages.begin() + 1, Graphics::CompiledShader{Stage::Geometry, &results.back(), 0});
        } else {
            require((graphics.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST && (!graphics.primitiveRestart || !localDevice->PrimitiveListRestart())) || (graphics.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP && (!graphics.primitiveRestart || !drawParameters.indexed)), "a pixel shader that reads barycentrics without VK_KHR_fragment_shader_barycentric or geometry shaders needs a triangle list, with primitive restart only on a device without VK_EXT_primitive_topology_list_restart, or a triangle strip, with primitive restart only in a non-indexed draw");
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
        results.reserve(programs.size() + 2u);
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
        FinishLeaseWritesBeforeRead();
        const auto count = std::min(indirect.countIndirect ? Pm4::ReadDrawCount(indirect) : indirect.count, indirect.count);
        std::vector<Pm4::DrawArguments> records;
        for (std::uint32_t record = 0; record < count; ++record) records.push_back(Pm4::ReadDrawArguments(indirect, record));
        Graphics::CountIndirectDraw(*indirectCpu, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
        const auto baseVertexWords = UserDataWords(programs, roles, indirect.baseVertexLocation);
        const auto startInstanceWords = UserDataWords(programs, roles, indirect.startInstanceLocation);
        const auto drawIndexWords = indirect.drawIndexEnabled ? UserDataWords(programs, roles, indirect.drawIndexLocation) : std::vector<std::pair<std::size_t, std::size_t>>{};
        const auto drawable = [&](const Pm4::DrawArguments& arguments) { return arguments.count != 0 && arguments.instances != 0 && (!drawParameters.indexed || !indirect.indexLimitKnown || arguments.firstVertexOrIndex < drawParameters.indexCount); };
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
        bool recordedOne = false;
        for (std::uint32_t record = 0; record < records.size(); ++record) {
            const auto& arguments = records[record];
            if (traceIndirect) std::fprintf(stderr, "[draw]   record %u: count %u instances %u first %u vertexOffset %u startInstance %u\n", record, arguments.count, arguments.instances, arguments.firstVertexOrIndex, arguments.vertexOffset, arguments.firstInstance);
            if (arguments.count == 0 || arguments.instances == 0) continue;
            std::set<std::size_t> patched;
            recordWords.clear();
            const auto patch = [&](const std::vector<std::pair<std::size_t, std::size_t>>& words, std::uint32_t value) {
                for (const auto& [program, word] : words) {
                    writablePrograms[program].userData[word] = value;
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
                if (indirect.indexLimitKnown) {
                    if (arguments.firstVertexOrIndex >= drawParameters.indexCount) continue;
                    direct.indexCount = std::min(arguments.count, drawParameters.indexCount - arguments.firstVertexOrIndex);
                } else {
                    const auto end = static_cast<std::uint64_t>(arguments.firstVertexOrIndex) + arguments.count;
                    require(end <= std::numeric_limits<std::uint32_t>::max(), "indirect draw index range overflow");
                    require(end * drawParameters.indexSize <= std::numeric_limits<std::uint64_t>::max() - drawParameters.indexAddress, "indirect draw index range address overflow");
                    direct.indexCount = arguments.count;
                }
                direct.indexAddress = drawParameters.indexAddress + static_cast<std::uint64_t>(arguments.firstVertexOrIndex) * drawParameters.indexSize;
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
                    const auto fresh = materializeDrawStage(programIndex, pushOffsets[programIndex], queue, submission, packetOffset, trace.token, programs, graphics, pixel, vertexInfos, memory, linked, drawParameters, localDevice, *shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected, meshBarycentrics, forcedBarycentrics);
                    for (auto& stage : stages) if (stage.program == &result) stage.registeredImageViews = stageCaptures[programIndex].registeredViews.get();
                    if (!rejected.empty()) return DrawVerdict::Rejected;
                    verifyForwardedWords(programIndex, result, *fresh);
                    continue;
                }
                if (pushWord) wordsRefused += static_cast<std::uint64_t>(std::count_if(recordWords.begin(), recordWords.end(), [&](const RecordWord& word) { return word.program == programIndex; }));
                result = *materializeDrawStage(programIndex, pushOffsets[programIndex], queue, submission, packetOffset, trace.token, programs, graphics, pixel, vertexInfos, memory, linked, programs[programIndex].binary.stage == Stage::TessellationControl || programs[programIndex].binary.stage == Stage::TessellationEvaluation ? direct : drawParameters, localDevice, *shaderMemory, stageCaptures, recompiled, drawHit, matched, matchedRegions, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected, meshBarycentrics, forcedBarycentrics);
                for (auto& stage : stages) if (stage.program == &result) stage.registeredImageViews = stageCaptures[programIndex].registeredViews.get();
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
                runCaptureCheckHook();
                if (tail.queue != nullptr && capturedMemoryChanged(memory)) {
                    const RecaptureScope recapture;
                    return draw(*tail.queue, tail.packet, submission, tail.packetOffset, rejected);
                }
                phaseTiming.Phase(DrawRowVectors);
                auto writes = drawWriteRanges(graphics, stages);
                const auto commitQueue = submission.queue;
                const auto commitPacket = tail.packetOpcode;
                const auto epoch = DrawPipeline::EpochToken().load(std::memory_order_relaxed);
                auto commit = [this, localDevice = std::move(localDevice), decode = std::move(decode), direct, stages = std::move(stages), snapshots = std::move(snapshots), commitQueue, commitPacket, epoch, writablePrograms = std::move(writablePrograms), results = std::move(results), memory = std::move(memory), linked = std::move(linked), stageCaptures = std::move(stageCaptures), matched = std::move(matched), matchedRegions = std::move(matchedRegions), fresh = std::move(fresh), decodeReads = std::move(decodeReads), shaderMemory = std::move(shaderMemory), entry = std::move(entry), dataEntry = std::move(dataEntry), compiledStages = std::move(compiledStages), stageData = std::move(stageData)] {
                    DrawPipeline::FollowEpoch(epoch);
                    GuestMemory::SetCurrentPacket(commitPacket, commitQueue);
                    commitDraw(localDevice, commitQueue, decode->state, direct, stages, snapshots, nullptr, {}, 0, nullptr);
                };
                auto held = std::make_shared<decltype(commit)>(std::move(commit));
                DrawPipeline::Queue0().Enqueue(DrawPipeline::CommitKind::IndirectDraw, [held] { (*held)(); }, std::move(writes));
                phaseTiming.Phase(DrawRowLockWait);
                countRecordWords();
                timing.Mark("draw_enqueued");
                return drawn();
            }
            drainBeforeInline();
            lockForDraw();
            if (!recordedOne) {
                runCaptureCheckHook();
                if (tail.queue != nullptr && capturedMemoryChanged(memory, snapshots)) {
                    gpuLock.unlock();
                    const RecaptureScope recapture;
                    return draw(*tail.queue, tail.packet, submission, tail.packetOffset, rejected);
                }
            }
            recordedOne = true;
            noteDrawWriters(stages, submission.queue);
            phaseTiming.Phase(DrawRowVectors);
            localDevice->Draw(graphics, direct, stages, snapshots, nullptr, tracePtr);
            phaseTiming.Phase(DrawRowGraphics);
        }
        countRecordWords();
        timing.Mark("draw_and_resource_release");
        return drawn();
    }

    std::vector<std::shared_ptr<DispatchVariant>> recipeStages;
    if (registerKey && !drawParameters.indirect && !patchedStages && !graphics.stages.tessellationGeometry && Graphics::DrawRecipes()) {
        recipeStages.reserve(programs.size());
        for (std::size_t i = 0; i < programs.size(); ++i) recipeStages.push_back(drawHit ? (matched[i] != nullptr && matched[i]->donor != nullptr ? matched[i]->donor : matched[i]) : fresh[i]);
        if (std::all_of(recipeStages.begin(), recipeStages.end(), [](const std::shared_ptr<DispatchVariant>& variant) { return variant == nullptr; })) recipeStages.clear();
    }
    const auto recipeDonor = drawHit && entry != nullptr ? entry->donor : nullptr;
    std::shared_ptr<const DrawRecipe> recipe;
    if (drawHit && !recipeStages.empty()) {
        recipe = findDrawRecipe(drawKey, recipeStages, recipeDonor);
        if (recipe == nullptr) VulkanDevice::NoteDrawRecipeMiss(VulkanDevice::DrawRecipePrecheck::NoRecipe);
    }
    if (recipe == nullptr) {
        if (auto known = localDevice->KnownDrawRejection(graphics, stages)) {
            rejected = std::move(*known);
            return DrawVerdict::Rejected;
        }
    }
    if (static const bool nameVariants = std::getenv("APS5_PROFILE_GPU_DRAWS") != nullptr; nameVariants && stages.size() == programs.size()) {
        static std::mutex namedMutex;
        static std::set<std::uint64_t> named;
        std::lock_guard namedLock(namedMutex);
        for (std::size_t i = 0; i < stages.size(); ++i) {
            if (stages[i].program == nullptr || !named.insert(stages[i].program->variantId).second) continue;
            std::fprintf(stderr, "[drawvariant] 0x%llx stage %d code 0x%llx\n", static_cast<unsigned long long>(stages[i].program->variantId), static_cast<int>(stages[i].stage), static_cast<unsigned long long>(programs[i].binary.codeAddress));
        }
    }
    if (pipelined && (!drawParameters.indirect || pipelinedIndirect) && (tail.queue == nullptr || deferredLabels().labels.empty())) {
        runCaptureCheckHook();
        if (capturedMemoryChanged(memory)) {
            const RecaptureScope recapture;
            if (tail.queue != nullptr) return draw(*tail.queue, tail.packet, submission, tail.packetOffset, rejected);
            restartFinishedDraw(tail);
            return finishDraw(tail, timing);
        }
        phaseTiming.Phase(DrawRowVectors);
        auto writes = drawWriteRanges(graphics, stages);
        const auto commitQueue = submission.queue;
        const auto commitPacket = tail.packetOpcode;
        const auto epoch = DrawFinisher::OnFinisherThread() ? tail.epoch : DrawPipeline::EpochToken().load(std::memory_order_relaxed);
        auto reads = QueuedDrawReadRanges(drawParameters, stages);
        auto contentKey = recipe == nullptr && DrawFinisher::OnFinisherThread() ? localDevice->DrawContentKey(decode->state, drawParameters, stages) : std::vector<std::uint32_t>{};
        auto commit = [this, contentKey = std::move(contentKey), localDevice = std::move(localDevice), decode = std::move(decode), drawParameters, stages = std::move(stages), snapshots = std::move(snapshots), recipe = std::move(recipe), recipeStages = std::move(recipeStages), drawKey, recipeDonor, shape = tail.shapeKey, commitQueue, commitPacket, epoch, trace = trace, writablePrograms = std::move(writablePrograms), results = std::move(results), memory = std::move(memory), linked = std::move(linked), stageCaptures = std::move(stageCaptures), matched = std::move(matched), matchedRegions = std::move(matchedRegions), fresh = std::move(fresh), decodeReads = std::move(decodeReads), shaderMemory = std::move(shaderMemory), entry = std::move(entry), dataEntry = std::move(dataEntry), compiledStages = std::move(compiledStages), stageData = std::move(stageData)]() mutable {
            DrawPipeline::FollowEpoch(epoch);
            GuestMemory::SetCurrentPacket(commitPacket, commitQueue);
            commitDraw(localDevice, commitQueue, decode->state, drawParameters, stages, snapshots, recipe, recipeStages, drawKey, recipeDonor, trace, shape, contentKey.empty() ? nullptr : &contentKey);
        };
        auto held = std::make_shared<decltype(commit)>(std::move(commit));
        DrawPipeline::Queue0().Enqueue(DrawPipeline::CommitKind::Draw, [held] { (*held)(); }, std::move(writes), 0, {}, std::move(reads), true);
        phaseTiming.Phase(DrawRowLockWait);
        timing.Mark("draw_enqueued");
        return drawn();
    }
    if (drawParameters.indirect) drainBeforeInline();
    lockForDraw();
    runCaptureCheckHook();
    if (tail.queue != nullptr && capturedMemoryChanged(memory, snapshots)) {
        gpuLock.unlock();
        const RecaptureScope recapture;
        return draw(*tail.queue, tail.packet, submission, tail.packetOffset, rejected);
    }
    noteDrawWriters(stages, submission.queue);
    phaseTiming.Phase(DrawRowVectors);
    if (recipe != nullptr) {
        if (localDevice->DrawFromRecipe(graphics, drawParameters, stages, snapshots, recipe, tracePtr) == RecipeOutcome::Recorded) {
            phaseTiming.Phase(DrawRowGraphics);
            timing.Mark("draw_and_resource_release");
            return drawn();
        }

        VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Restart, VulkanDevice::RecipeKind::Draw);
    }
    std::shared_ptr<const DrawRecipe> built;
    localDevice->Draw(graphics, drawParameters, stages, snapshots, recipeStages.empty() ? nullptr : &built, tracePtr, tail.shapeKey);
    phaseTiming.Phase(DrawRowGraphics);
    if (built != nullptr) attachDrawRecipe(drawKey, recipeStages, std::move(built), recipeDonor);
    timing.Mark("draw_and_resource_release");
    return drawn();
}

}
}
}
}
