#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <cstdlib>
#include <cstring>

namespace AgcDriver::DriverDetail {

DrawVerdict Driver::draw(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission, std::string& rejected) {
    PerformanceTimer timing("Driver.Draw");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    std::array<double, DrawDriverPhaseCount> phaseMs{};
    std::uint64_t captures = 0;
    auto phaseLap = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    DrawPhaseTiming phaseTiming{profile, phaseMs, phaseLap};
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
    auto drawParameters = Pm4::ResolveDraw(packet, queue);
    bool traceIndirect = false;
    if (const auto verdict = precheckDraw(queue, submission, packet, drawParameters, rejected, traceIndirect)) return *verdict;
    phaseTiming.Phase(DrawRowPrecheck);
    using Stage = ShaderRecompiler::ShaderStage;
    using Role = ShaderRecompiler::ProgramRole;

    static const std::uint64_t dumpTarget = [] { const char* text = std::getenv("APS5_DUMP_DRAW_SHADERS"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();

    static const std::uint64_t dumpSlot1 = [] { const char* text = std::getenv("APS5_DUMP_DRAW_SLOT1"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();

    static const bool lockedPrepare = std::getenv("APS5_LOCKED_DRAW_PREPARE") != nullptr;
    std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
    std::shared_ptr<VulkanDevice> localDevice;
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
    const bool registerKey = useDrawEntries && registerKeyEnabled();
    std::uint64_t drawKey = 0;
    std::shared_ptr<DrawEntry> entry;
    std::shared_ptr<const DrawPlan> decode;
    std::shared_ptr<DrawEntry> dataEntry;
    std::uint64_t shapeKey = 0;
    if (registerKey) {
        const auto keyStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        drawKey = drawRegisterKey(queue, *submission.shaders, localDevice->Serial(), &shapeKey);
        std::lock_guard cacheLock(drawCacheMutex);
        ++drawEntryCounters.lookups;
        ++drawEntryCounters.registerKeyLookups;
        if (profile) drawEntryCounters.keyUs += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - keyStart).count();
        const auto found = drawCache.find(drawKey);
        if (found != drawCache.end()) {
            entry = found->second.entry;
            decode = entry->plan;
        } else {
            ++drawEntryCounters.absent;
            if (traceDrawCache()) {
                auto& counters = drawEntryCounters;
                if (traceKeysEvicted.contains(drawKey)) ++counters.absentEvicted;
                else if (traceKeysSeen.contains(drawKey)) ++counters.absentNeverInserted;
                else if (traceShapes.contains(shapeKey)) ++counters.absentUserWords;
                else ++counters.absentNewRegisters;
            }
            if (drawDataHits() && !drawParameters.indirect) {
                dataEntry = drawPlans.Find(shapeKey);
                if (dataEntry != nullptr) ++drawEntryCounters.dataHits;
            }
        }
        if (traceDrawCache()) {
            traceInsert(traceKeysSeen, drawKey);
            traceInsert(traceShapes, shapeKey);
        }
        maybeReportDrawCache(profile);
    }
    phaseTiming.Phase(DrawRowKeyLookupValidate);

    if (dataEntry != nullptr) decode = dataEntry->plan;
    if (dataEntry != nullptr && verifyDrawDataHits()) {
        const auto full = decodeDraw(queue, submission);
        require(samePlan(*decode, *full), "APS5_VERIFY_DRAW_DATA_HITS: cached draw plan differs from full decode");
    }
    resolveDrawPlan(queue, submission, decode, registerKey, drawKey, profile);
    const auto& graphics = decode->state;
    const auto& pixel = decode->pixel;
    std::array<DrawProgram, MaxDrawPrograms> inputStorage;
    require(decode->programs.size() <= inputStorage.size(), "draw plan exceeds the graphics stage limit");
    auto programs = std::span(inputStorage).first(decode->programs.size());
    for (std::size_t i = 0; i < programs.size(); ++i) programs[i].Read(queue, decode->programs[i]);
    const auto setMeshIndexBuffer = [&](const Pm4::DrawParameters& parameters) {
        if (!graphics.stages.mesh) return;
        auto words = programs.front().UserData();
        require(programs.front().plan->firstUserSgpr == 0 && words.size() >= ShaderRecompiler::MeshIndexBufferUserWord + 4, "mesh program lacks the hidden user words");
        const auto descriptor = Graphics::MeshIndexBufferDescriptor(parameters);
        std::copy(descriptor.begin(), descriptor.end(), words.begin() + ShaderRecompiler::MeshIndexBufferUserWord);
    };
    if (!drawParameters.indirect) setMeshIndexBuffer(drawParameters);
    else if (graphics.stages.mesh) setMeshIndexBuffer(Pm4::DrawParameters{drawParameters.indexAddress, std::max(drawParameters.indexCount, 1u), drawParameters.indexSize, 1, 0, drawParameters.indexed});
    const std::vector<Role>& roles = decode->roles;
    phaseTiming.Phase(DrawRowDecode);

    const auto locate = [&](std::uint32_t location) -> std::optional<std::pair<std::size_t, std::size_t>> {
        if (location == 0x280u) return std::nullopt;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (roles[i] == Role::Fragment || roles[i] == Role::GeometryBack || location < programs[i].plan->userDataBase) continue;
            const auto word = location - programs[i].plan->userDataBase + (8u - programs[i].plan->firstUserSgpr);
            if (word < programs[i].UserData().size()) return std::make_pair(i, static_cast<std::size_t>(word));
        }
        return std::nullopt;
    };
    if (drawParameters.indirect) {
        auto& indirect = *drawParameters.indirect;
        const auto sgprOf = [&](std::uint32_t location) -> std::int32_t {
            const auto word = locate(location);
            if (!word || word->first != 0) return -1;
            return static_cast<std::int32_t>(programs.front().plan->firstUserSgpr + word->second);
        };
        indirect.baseVertexSgpr = sgprOf(indirect.baseVertexLocation);
        indirect.startInstanceSgpr = sgprOf(indirect.startInstanceLocation);
        indirect.drawIndexSgpr = sgprOf(indirect.drawIndexLocation);
    }
    std::vector<ShaderRecompiler::MemoryRegion> memory;
    std::array<ShaderRecompiler::LinkedProgram, MaxDrawPrograms> linkedStorage;
    auto linked = std::span(linkedStorage).first(programs.size());
    std::array<DrawStage, MaxDrawPrograms> stageStorage;
    auto work = std::span(stageStorage).first(programs.size());
    for (std::size_t i = 0; i < programs.size(); ++i) {
        const auto& program = programs[i];
        memory.insert(memory.end(), program.plan->memory.begin(), program.plan->memory.end());
        linked[i] = {roles[i], program.plan->binary, program.plan->userDataBase, program.plan->firstUserSgpr, program.UserData()};
    }
    timing.Mark("prepare");
    phaseTiming.Phase(DrawRowProgramPrepare);

    const auto decodeVertexInfo = [&](std::size_t i) {
        const auto& program = programs[i];
        if (program.plan->binary.stage == Stage::Fragment || roles[i] == Role::GeometryBack) return;
        work[i].decodeReads.clear();
        work[i].vertexInfo = program.plan->vertexInputs->Read(program.UserData(), &work[i].decodeReads);
    };
    if (!registerKey) {
        for (std::size_t i = 0; i < programs.size(); ++i) decodeVertexInfo(i);
        phaseTiming.Phase(DrawRowDecode);
    }
    ShaderMemory shaderMemory(memory, &queryPendingWrite, &observePendingWrite, hookWaitCounter());
    std::vector<ShaderRecompiler::RecompileResult> results;
    std::vector<Graphics::CompiledShader> stages;
    if (graphics.rectList) results.reserve(2);
    stages.reserve(programs.size());
    std::uint32_t pushCursorBytes = 0;

    std::uint64_t dataStagesReused = 0, dataStagesCompiled = 0, dataVerified = 0;
    bool drawHit = false;
    bool verifyHit = false;
    lookupDraw(submission, localDevice, graphics, pixel, programs, roles, work, useDrawEntries, registerKey, profile, drawKey, entry, drawHit, verifyHit, phaseTiming, phaseMs, dataEntry);

    if (registerKey) {

        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (work[i].matched != nullptr && drawHit && !verifyDrawRecipe()) {
                if (work[i].matched->vertexInfo != nullptr) work[i].vertexInfo = *work[i].matched->vertexInfo;
                continue;
            }
            if (!work[i].candidates.empty() && !verifyDrawDataHits()) {
                const auto& info = work[i].candidates.front().first->vertexInfo;
                if (info != nullptr) work[i].vertexInfo = *info;
                continue;
            }
            decodeVertexInfo(i);
            if (work[i].matched != nullptr && verifyDrawRecipe() && (work[i].vertexInfo.has_value() != (work[i].matched->vertexInfo != nullptr) || (work[i].vertexInfo && !sameVertexInfo(*work[i].vertexInfo, *work[i].matched->vertexInfo)))) {
                static std::atomic<std::uint64_t> reports{0};
                if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: stage %zu (program 0x%llx) of a hit has a vertex stage info unlike its variant's\n", i, static_cast<unsigned long long>(programs[i].plan->binary.codeAddress));
                std::lock_guard cacheLock(drawCacheMutex);
                ++drawEntryCounters.verifyDecodeMismatches;
            }
        }
        phaseTiming.Phase(DrawRowDecode);
    }

    static const bool indxOffsetSkipFold = std::getenv("APS5_INDX_OFFSET_SKIP_FOLD") != nullptr;
    static const bool indexedOffsetFold = std::getenv("APS5_NO_INDEXED_OFFSET_FOLD") == nullptr;
    const auto fold = [&](const ShaderRecompiler::RecompileResult& main, Pm4::DrawParameters& parameters) {
        if (parameters.indexed && !indexedOffsetFold) return;
        if (main.vertexOffsetSgpr >= 0 && (parameters.firstVertex == 0 || !indxOffsetSkipFold)) {
            const auto offset = drawUserWord(programs.front(), main.vertexOffsetSgpr);
            require(offset <= std::numeric_limits<std::uint32_t>::max() - parameters.firstVertex, "draw vertex offset overflow");
            parameters.firstVertex += offset;
        }
        if (main.instanceOffsetSgpr >= 0) parameters.firstInstance = drawUserWord(programs.front(), main.instanceOffsetSgpr);
    };

    const auto verifyDataStage = [&](std::size_t i, const DispatchVariant& kept) {
        std::vector<ShaderRecompiler::MemoryRegion> regions(work[i].capture.regions.begin(), work[i].capture.regions.end());
        for (const auto& read : work[i].decodeReads) regions.push_back({read.address, std::as_bytes(std::span(read.bytes))});
        std::stable_sort(regions.begin(), regions.end(), [](const ShaderRecompiler::MemoryRegion& a, const ShaderRecompiler::MemoryRegion& b) { return a.guestAddress < b.guestAddress; });
        std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
        std::vector<std::uint32_t> words;
        for (const auto& region : regions) {
            runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
            const auto count = region.bytes.size() / sizeof(std::uint32_t);
            const auto offset = words.size();
            words.resize(offset + count);
            std::memcpy(words.data() + offset, region.bytes.data(), count * sizeof(std::uint32_t));
        }
        const auto& compiled = *work[i].capture.compiled;
        const char* what = runs != kept.runs ? "runs" : words != kept.words ? "words" : work[i].vertexInfo.has_value() != (kept.vertexInfo != nullptr) || (work[i].vertexInfo && !sameVertexInfo(*work[i].vertexInfo, *kept.vertexInfo)) ? "vertex stage info" : compiled.variantId != kept.compiled->variantId ? "variant" : compiled.pushConstants != kept.compiled->pushConstants ? "push constants" : nullptr;
        ++dataVerified;
        if (what == nullptr) return;
        char message[320];
        std::snprintf(message, sizeof(message), "APS5_VERIFY_DRAW_DATA_HITS: stage %zu (program 0x%llx) of a data-only hit differs from its full build in its %s (%zu runs / %zu words kept, %zu / %zu fresh; variant 0x%llx / 0x%llx; key 0x%llx, shape 0x%llx)", i, static_cast<unsigned long long>(programs[i].plan->binary.codeAddress), what, kept.runs.size(), kept.words.size(), runs.size(), words.size(), static_cast<unsigned long long>(kept.compiled->variantId), static_cast<unsigned long long>(compiled.variantId), static_cast<unsigned long long>(drawKey), static_cast<unsigned long long>(shapeKey));
        throw std::runtime_error(message);
    };
    std::optional<Graphics::IndirectDrawPath> indirectCpu;
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (roles[i] == Role::GeometryBack) continue;
        const auto& program = programs[i];
        work[i].pushOffset = pushCursorBytes;
        if (drawHit) {

            work[i].result = work[i].matched->compiled.get();
            memory.insert(memory.end(), work[i].matchedRegions.begin(), work[i].matchedRegions.end());
        } else {
            const auto candidate = std::find_if(work[i].candidates.begin(), work[i].candidates.end(), [&](const auto& kept) { return kept.first->pushOffset == pushCursorBytes; });
            if (candidate != work[i].candidates.end() && !verifyDrawDataHits()) {
                work[i].matched = candidate->first;
                work[i].matchedRegions = std::move(candidate->second);
                work[i].reused = true;
                work[i].result = work[i].matched->compiled.get();
                memory.insert(memory.end(), work[i].matchedRegions.begin(), work[i].matchedRegions.end());
            } else {
                if (!work[i].candidates.empty() && !verifyDrawDataHits()) decodeVertexInfo(i);
                compileDrawStage(i, pushCursorBytes, queue, submission, programs, graphics, pixel, work, memory, linked, drawParameters, localDevice, shaderMemory, drawHit, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected);
                if (!rejected.empty()) return DrawVerdict::Rejected;
                work[i].result = work[i].capture.compiled.get();
                if (candidate != work[i].candidates.end()) verifyDataStage(i, *candidate->first);
            }
            if (dataEntry != nullptr && work[i].reused) ++dataStagesReused;
            else if (dataEntry != nullptr) ++dataStagesCompiled;
        }
        const auto& result = *work[i].result;
        if (i == 0 && drawParameters.indirect) {
            indirectCpu = classifyIndirectDraw(result, graphics, programs.front(), localDevice, drawParameters, traceIndirect);
        } else if (i == 0) {
            fold(result, drawParameters);
        }
        require(result.pushConstants.size() <= Graphics::PipelinePushConstantBytes - pushCursorBytes, "stage push constants exceed the pipeline push constant block");
        work[i].compiledIndex = stages.size();
        stages.push_back({program.plan->binary.stage, &result, result.pushConstants.empty() ? 0u : pushCursorBytes});
        pushCursorBytes += static_cast<std::uint32_t>(result.pushConstants.size());
    }

    if (dataEntry != nullptr) {
        std::lock_guard cacheLock(drawCacheMutex);
        drawEntryCounters.dataStagesReused += dataStagesReused;
        drawEntryCounters.dataStagesCompiled += dataStagesCompiled;
        drawEntryCounters.dataVerified += dataVerified;
    }
    cacheDrawStages(useDrawEntries, drawHit, drawParameters, indirectCpu, programs, work, verifyHit, drawKey, registerKey, decode, phaseTiming, entry, shapeKey);
    timing.Mark("shader_compile_and_link");

    for (const auto& stage : work) {
        for (const auto& read : stage.decodeReads) memory.push_back({read.address, std::as_bytes(std::span(read.bytes))});
    }

    if (recordQueuedLabelsAfterCapture(submission.queue, memory)) return draw(queue, packet, submission, rejected);

    bool rectListBuilt = false;

    std::size_t rectIndex = 0;
    const auto buildRectList = [&] {
        phaseTiming.Phase(DrawRowVectors);
        require(programs.size() == 2 && work[0].result != nullptr && work[1].result != nullptr, "rect-list requires vertex and fragment programs");
        auto rectangle = ShaderRecompiler::BuildRectListShaders(*work[0].result, *work[1].result, localDevice->Target());
        if (rectListBuilt) {
            results[rectIndex] = std::move(rectangle.control);
            results[rectIndex + 1] = std::move(rectangle.evaluation);
            phaseTiming.Phase(DrawRowRectList);
            return;
        }
        require(stages.size() == 2, "rect-list requires vertex and fragment programs");
        rectIndex = results.size();
        results.push_back(std::move(rectangle.control));
        results.push_back(std::move(rectangle.evaluation));
        stages.insert(stages.begin() + 1, {{Stage::TessellationControl, &results[rectIndex], 0}, {Stage::TessellationEvaluation, &results[rectIndex + 1], 0}});
        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (work[i].result != nullptr && work[i].compiledIndex != 0) work[i].compiledIndex += 2;
        }
        rectListBuilt = true;
        phaseTiming.Phase(DrawRowRectList);
    };
    if (graphics.rectList) buildRectList();
    std::vector<Graphics::GuestMemorySnapshot> snapshots;
    const auto snapshot = [&] {
        snapshots.clear();
        for (const auto& region : memory) snapshots.push_back({region.guestAddress, region.bytes});
    };
    snapshot();
    timing.Mark("post_compile_prepare");
    const auto lockForDraw = [&] {
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
        recordQueuedLabelsBeforeRead(submission.queue);
        const auto readStart = std::chrono::steady_clock::now();
        const auto count = std::min(indirect.countIndirect ? Pm4::ReadDrawCount(indirect) : indirect.count, indirect.count);
        std::vector<Pm4::DrawArguments> records;
        for (std::uint32_t record = 0; record < count; ++record) records.push_back(Pm4::ReadDrawArguments(indirect, record));
        Graphics::CountIndirectDraw(*indirectCpu, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
        const auto baseVertexWord = locate(indirect.baseVertexLocation);
        const auto startInstanceWord = locate(indirect.startInstanceLocation);
        const auto drawIndexWord = indirect.drawIndexEnabled ? locate(indirect.drawIndexLocation) : std::nullopt;
        for (std::uint32_t record = 0; record < records.size(); ++record) {
            const auto& arguments = records[record];
            if (traceIndirect) std::fprintf(stderr, "[draw]   record %u: count %u instances %u first %u vertexOffset %u startInstance %u\n", record, arguments.count, arguments.instances, arguments.firstVertexOrIndex, arguments.vertexOffset, arguments.firstInstance);
            if (arguments.count == 0 || arguments.instances == 0) continue;
            std::set<std::size_t> patched;
            const auto patch = [&](const std::optional<std::pair<std::size_t, std::size_t>>& word, std::uint32_t value) {
                if (!word) return;
                programs[word->first].UserData()[word->second] = value;
                patched.insert(word->first);
            };
            patch(baseVertexWord, indirect.recordBytes == 20 ? arguments.vertexOffset : arguments.firstVertexOrIndex);
            patch(startInstanceWord, arguments.firstInstance);
            patch(drawIndexWord, record);
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
                const auto pushBytes = work[programIndex].result->pushConstants.size();
                decodeVertexInfo(programIndex);
                compileDrawStage(programIndex, work[programIndex].pushOffset, queue, submission, programs, graphics, pixel, work, memory, linked, drawParameters, localDevice, shaderMemory, drawHit, profile, dumpTarget, dumpSlot1, captures, phaseTiming, phaseMs, rejected);
                if (!rejected.empty()) return DrawVerdict::Rejected;
                work[programIndex].result = work[programIndex].capture.compiled.get();
                stages[work[programIndex].compiledIndex].program = work[programIndex].result;
                require(work[programIndex].result->pushConstants.size() == pushBytes, "patched program changed its push constant layout");
            }
            fold(*work[0].result, direct);
            if (graphics.rectList && patched.contains(0)) buildRectList();
            snapshot();
            if (auto known = localDevice->KnownDrawRejection(graphics, stages)) {
                rejected = std::move(*known);
                return DrawVerdict::Rejected;
            }
            lockForDraw();
            noteDrawWriters(stages, submission.queue);
            phaseTiming.Phase(DrawRowVectors);
            localDevice->Draw(graphics, direct, stages, snapshots);
            phaseTiming.Phase(DrawRowGraphics);
        }
        timing.Mark("draw_and_resource_release");
        return drawn();
    }

    std::array<std::shared_ptr<DispatchVariant>, MaxDrawPrograms> recipeStorage;
    std::span<std::shared_ptr<DispatchVariant>> recipeStages;
    if (registerKey && !drawParameters.indirect && Graphics::DrawRecipes()) {
        recipeStages = std::span(recipeStorage).first(programs.size());
        for (std::size_t i = 0; i < programs.size(); ++i) recipeStages[i] = drawHit ? work[i].matched : work[i].fresh;
        if (std::all_of(recipeStages.begin(), recipeStages.end(), [](const std::shared_ptr<DispatchVariant>& variant) { return variant == nullptr; })) recipeStages = {};
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

}
