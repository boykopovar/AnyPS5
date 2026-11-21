#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexPlanScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/StagedMerge.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/TracyZone.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <unordered_map>
#include <type_traits>
#include <utility>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
std::shared_ptr<Buffer> directMeshArguments(const Context& context, const Pm4::DrawParameters& draw, std::span<const std::uint32_t> restartTable) {
    auto arguments = meshArgumentBuffer(context, restartTable);
    const MeshArguments resolved{0u, 0u, 0u, draw.indexCount, 0u};
    std::memcpy(arguments->Bytes().data(), &resolved, sizeof(resolved));
    return arguments;
}

std::shared_ptr<Buffer> recordMeshArguments(const Context& context, VkCommandBuffer commands, Recorder* recorder, bool recorded, const State& state, const Pm4::DrawParameters& draw, const IndirectRecord& indirect, const std::function<void(std::uint32_t)>& countBarrier, std::span<const std::uint32_t> restartTable) {
    const auto* args = indirect.args;
    const auto rules = MeshArgumentRulesFor(context, *state.stages.mesh, draw.indexCount);
    auto arguments = meshArgumentBuffer(context, restartTable);
    if (indirect.path == IndirectDrawPath::Gpu && recorder != nullptr && indirect.argumentImport->address != 0) {
        const std::array<std::uint32_t, 7> words{rules.indexCount, rules.inputSize, rules.step, rules.primitivesPerGroup, rules.maxGroups, rules.maxInstances, rules.maxTotal};
        if (recorder->RecordMeshArguments(commands, indirect.argumentImport->address + (args->arguments - indirect.argumentImport->base), arguments->DeviceAddress(), words)) {
            countBarrier(2);
            if (recorded) recorder->NotePendingRead(args->arguments, static_cast<std::size_t>(args->RangeBytes()), Recorder::ReadKind::Indirect);
            return arguments;
        }
    }
    const auto record = !indirect.records.empty() ? indirect.records.front() : Pm4::ReadDrawArguments(*args, 0);
    const auto resolved = ResolveMeshArguments(record, rules);
    std::memcpy(arguments->Bytes().data(), &resolved, sizeof(resolved));
    return arguments;
}

std::function<void()> indirectRecordCheck(const IndirectRecord* indirect) {
    static const bool checkArguments = std::getenv("APS5_CHECK_INDIRECT_ARGS") != nullptr;
    if (indirect == nullptr || indirect->args == nullptr || indirect->path != IndirectDrawPath::Gpu || !checkArguments) return {};
    return [args = *indirect->args] {
        using Rule = Pm4::DrawParameters::IndirectDraw::Rule;
        static std::atomic<std::uint64_t> printed{0};
        static std::atomic<std::uint64_t> ignoredValues{0};
        static std::atomic<std::uint64_t> checked{0};
        try {
            for (std::uint32_t record = 0; record < args.count; ++record) {
                const auto arguments = Pm4::ReadDrawArguments(args, record);
                const bool ignoredVertex = args.vertexRule == Rule::Constant && (args.recordBytes == 20 ? arguments.vertexOffset : arguments.firstVertexOrIndex) != 0;
                const bool ignoredInstance = args.instanceRule == Rule::Constant && arguments.firstInstance != 0;
                ++checked;
                if (ignoredVertex || ignoredInstance) ++ignoredValues;
                if (printed.fetch_add(1) < 16) std::fprintf(stderr, "[draw] indirect record 0x%llx: count %u instances %u first %u vertexOffset %u startInstance %u (vertex %s, instance %s)\n", static_cast<unsigned long long>(args.arguments + static_cast<std::uint64_t>(record) * args.stride), arguments.count, arguments.instances, arguments.firstVertexOrIndex, arguments.vertexOffset, arguments.firstInstance, args.vertexRule == Rule::Constant ? "const: record value ignored" : "in-place", args.instanceRule == Rule::Constant ? "const: record value ignored" : "in-place");
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[draw] indirect record read-back failed: %s\n", error.what());
        }
        if (checked % 64 == 0) std::fprintf(stderr, "[draw] indirect records checked %llu, with a value in a Constant dimension %llu\n", static_cast<unsigned long long>(checked.load()), static_cast<unsigned long long>(ignoredValues.load()));
    };
}

std::atomic<std::uint32_t> spaceKeepFailures{0};
std::atomic<int> stopAfterResolve{0};

std::shared_ptr<std::shared_ptr<const void>> reserveSpaceKeep(Recorder& recorder, const ShaderResources& resources) {
    if (!resources.SpaceReuse()) return nullptr;
    if (spaceKeepFailures.load(std::memory_order_relaxed) != 0) {
        spaceKeepFailures.fetch_sub(1, std::memory_order_relaxed);
        throw std::bad_alloc();
    }
    auto slot = std::make_shared<std::shared_ptr<const void>>();
    recorder.Keep(slot);
    return slot;
}

struct Kept {
    std::shared_ptr<ShaderResources> resources;
    std::shared_ptr<Pipeline> pipeline;
    std::shared_ptr<Framebuffer> framebuffer;
    std::shared_ptr<Buffer> indices;
    std::vector<std::shared_ptr<Buffer>> vertexBuffers;
    std::vector<std::shared_ptr<StorageTexture>> targets;
    std::unique_ptr<DeviceBuffer> scratch;
};

void keepRecordedDraw(Recorder& recorder, const std::shared_ptr<ShaderResources>& resources, std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Framebuffer> framebuffer, DrawInputs& inputs, std::vector<std::shared_ptr<StorageTexture>> targets, std::vector<std::pair<std::uint32_t, std::uint32_t>> slices, VkRect2D scissor, std::unique_ptr<DeviceBuffer> scratch, std::function<void()> checkRecords, bool listed, bool completion, const DrawOutcome& outcome, std::shared_ptr<ShaderResources::DrawUse> use, std::shared_ptr<const void>* spaceKeep) {
    auto kept = std::make_shared<Kept>();
    kept->resources = resources;
    kept->pipeline = std::move(pipeline);
    kept->framebuffer = std::move(framebuffer);
    kept->indices = std::move(inputs.indices);
    kept->vertexBuffers = std::move(inputs.vertexBuffers);
    kept->scratch = std::move(scratch);
    kept->targets = std::move(targets);
    const auto& residents = kept->targets;
    resources->KeepStorageImageBytes(recorder);
    for (const auto& target : residents) {
        if (target != nullptr && !target->Cached()) recorder.KeepBytes(target.get(), static_cast<std::size_t>(target->BudgetBytes()));
    }
    recorder.Keep(kept);
    if (resources != nullptr) recorder.KeepBytes(resources.get(), resources->CopiedBytes());
    if (checkRecords) recorder.OnComplete(std::move(checkRecords));
    if (resources->HoldsLease()) CountLeaseOutcome(outcome.waited, outcome.waited ? 0 : recorder.Submissions() + 1, !resources->BdaReadOnly());
    resources->MarkGpuWrites(recorder);
    if (spaceKeep != nullptr) *spaceKeep = resources->ReleaseSpace();
    if (listed && resources->HasCopiedWrites()) {
        auto writers = DrawCopiedWriters();
        recorder.OnComplete([resources, writers] {
            writers->erase(std::remove(writers->begin(), writers->end(), resources), writers->end());
            resources->WriteBackBuffers();
        });
        writers->push_back(resources);
        GuestMemory::BreakProofs();
    } else if (completion) {
        if (use != nullptr) recorder.OnComplete([resources, use] { resources->CompleteDrawUse(*use); });
        else if (resources->Reusable()) recorder.OnComplete([resources] { resources->CheckFaults(); });
        else recorder.OnComplete([resources] { resources->WriteBackBuffers(); });
    }
    std::vector<bool> storeNow(residents.size(), false);
    for (std::size_t index = 0; index < residents.size(); ++index) {
        if (residents[index] == nullptr) continue;
        if (residents[index]->Descriptor().tileMode == TextureTileMode::kLinear) storeNow[index] = !residents[index]->MarkDrawn(index < slices.size() ? slices[index].first : 0u, index < slices.size() ? slices[index].second : 0u, scissor) || !residents[index]->Cached();
        else if (index < slices.size()) residents[index]->MarkSliceDirty(slices[index].first, slices[index].second);
        else residents[index]->MarkDirty();
    }
    for (std::size_t index = 0; index < residents.size(); ++index) {
        if (storeNow[index]) StorageTexture::FlushPending(residents[index]->Descriptor().baseAddress, static_cast<std::size_t>(residents[index]->GuestBytes()), nullptr, "linear target");
    }
}

struct StagedWrite {
    std::shared_ptr<StagedTemporary> temporary;
    VkImageView target = VK_NULL_HANDLE;
    std::uint16_t mask = 0;
    std::uint32_t samples = 1;
    VkRect2D area{};
};

struct RecordedDraw {
    Recorder* recorder = nullptr;
    const State* masked = nullptr;
    std::shared_ptr<ShaderResources> resources;
    std::shared_ptr<Pipeline> pipeline;
    std::shared_ptr<Framebuffer> framebuffer;
    std::span<const VkImageView> targetViews;
    std::vector<std::shared_ptr<StorageTexture>> targets;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> targetSlices;
    std::vector<std::shared_ptr<void>> multisampleTargets;
    std::vector<std::shared_ptr<StorageTexture>> proxies;
    std::vector<VkImage> multisampleImages;
    std::vector<StagedWrite> staged;
    const IndirectRecord* indirect = nullptr;
    std::span<const ShaderResources::MovedBuffer> moved;
    bool listed = false;
    bool completion = false;
    bool waited = false;
    const std::array<std::byte, PipelinePushConstantBytes>* pushBytes = nullptr;
    VkShaderStageFlags pushStages = 0;
    std::vector<std::pair<std::uint32_t, std::shared_ptr<Pipeline>>> stencilReplay;
    std::vector<SampleOrderTarget> reordered;
    SampleOrderPass reorderPass;
    std::optional<DepthSampleLocations> depthLocations;
    std::shared_ptr<std::shared_ptr<const void>>* spaceKeep = nullptr;
};

void patchDepthRemapPush(std::array<std::byte, PipelinePushConstantBytes>& block, const State& state) {
    if (state.depthRemap.mode == 0u) return;
    static_assert(ShaderRecompiler::DepthRemapPushOffsetBytes + ShaderRecompiler::DepthRemapPushBytes <= PipelinePushSlotBytes);
    const std::array<float, ShaderRecompiler::DepthRemapPushBytes / 4> words{state.depthRemap.scale, state.depthRemap.offset};
    std::memcpy(block.data() + ShaderRecompiler::DepthRemapPushOffsetBytes, words.data(), sizeof(words));
}

void recordStencilReplay(const Context& context, CommandState& recording, const State& state, const Pm4::DrawParameters& draw, const DrawInputs& inputs, std::span<const std::pair<std::uint32_t, std::shared_ptr<Pipeline>>> replay, std::span<const CompiledShader> shaders, const ShaderResources& resources, const std::array<std::byte, PipelinePushConstantBytes>* bytes = nullptr, VkShaderStageFlags stages = 0) {
    const auto commands = recording.Commands();
    State selected = state;
    auto block = bytes != nullptr ? *bytes : AssemblePushConstants(shaders);
    if (bytes == nullptr) {
        resources.PatchPushConstants(block);
        stages = PushConstantStages(shaders);
    }
    patchDepthRemapPush(block, state);
    const auto fragment = std::ranges::find_if(shaders, [](const CompiledShader& shader) { return shader.stage == ShaderRecompiler::ShaderStage::Fragment && !shader.program->stencilReferences.empty(); });
    Require(fragment != shaders.end(), "stencil replay fragment stage is missing");
    const auto referenceOffset = fragment->pushConstantOffset + fragment->program->stencilReferenceDword * sizeof(std::uint32_t);
    Require(referenceOffset <= block.size() - sizeof(std::uint32_t), "stencil replay reference exceeds the push block");
    const auto run = [&](const Pm4::DrawParameters& piece, std::optional<std::uint32_t> firstIndex) {
        std::uint32_t unusedSubdraw = 0;
        for (const auto& [reference, pipeline] : replay) {
            selected.stencilReplayReference = reference;
            selected.stencilFront.reference = reference;
            selected.stencilBack.reference = reference;
            recording.SetStencilReference(context, VK_STENCIL_FACE_FRONT_AND_BACK, reference);
            std::memcpy(block.data() + referenceOffset, &reference, sizeof(reference));
            pipeline->PushConstants(commands, stages, block);
            recordDrawCommands(context, commands, selected, piece, inputs, nullptr, VK_NULL_HANDLE, 0, nullptr, unusedSubdraw, firstIndex);
        }
    };
    if (replay.size() == 1u) {
        run(draw, std::nullopt);
        return;
    }
    const auto indexCount = draw.indexed ? inputs.indexCount.value_or(draw.indexCount) : draw.indexCount;
    for (std::uint32_t instance = 0; instance < draw.instanceCount; ++instance) {
        for (std::uint32_t first = 0; first < indexCount - indexCount % 3u; first += 3u) {
            auto piece = draw;
            piece.instanceCount = 1u;
            piece.firstInstance = draw.firstInstance + instance;
            piece.indexCount = 3u;
            if (!draw.indexed) piece.firstVertex += first;
            run(piece, draw.indexed ? first : 0u);
        }
    }
}

void pushDrawConstants(const Pipeline& pipeline, VkCommandBuffer commands, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, const ShaderResources& resources, const std::array<std::byte, PipelinePushConstantBytes>* bytes, VkShaderStageFlags stages, VkDeviceAddress meshArguments = 0, bool restartTable = false, std::uint32_t meshLayerStride = 0) {
    auto block = bytes != nullptr ? *bytes : AssemblePushConstants(shaders);
    if (bytes == nullptr) {
        resources.PatchPushConstants(block);
        stages = PushConstantStages(shaders);
    }
    patchDepthRemapPush(block, state);
    if (state.stages.tessellation) {
        for (const auto& shader : shaders) Require(shader.program->pushConstants.empty() || shader.pushConstantOffset >= PipelinePushSlotBytes || shader.pushConstantOffset + shader.program->pushConstants.size() <= ShaderRecompiler::TessellationLevelPushOffsetBytes, "a stage's push constants overlap the tessellation levels");
        const std::array<float, ShaderRecompiler::TessellationLevelPushBytes / 4> levels{state.stages.minTessellationLevel, state.stages.maxTessellationLevel};
        static_assert(ShaderRecompiler::TessellationLevelPushOffsetBytes + ShaderRecompiler::TessellationLevelPushBytes == PipelinePushSlotBytes);
        std::memcpy(block.data() + ShaderRecompiler::TessellationLevelPushOffsetBytes, levels.data(), sizeof(levels));
    }
    const auto capturedItems = state.tessellationCapture != nullptr ? state.tessellationCapture->items->DeviceAddress() : 0;
    if (!state.stages.mesh) {
        if (state.tessellationCapture != nullptr) {
            for (const auto& shader : shaders) Require(shader.program->pushConstants.empty() || shader.pushConstantOffset >= PipelinePushSlotBytes || shader.pushConstantOffset + shader.program->pushConstants.size() <= ShaderRecompiler::MeshDrawPushOffsetBytes, "a stage's push constants overlap the tessellation capture addresses");
            const auto header = state.tessellationCapture->header->DeviceAddress();
            const std::array<std::uint32_t, 4> words{static_cast<std::uint32_t>(capturedItems), static_cast<std::uint32_t>(capturedItems >> 32u), static_cast<std::uint32_t>(header), static_cast<std::uint32_t>(header >> 32u)};
            static_assert(ShaderRecompiler::MeshDrawPushOffsetBytes + sizeof(std::uint32_t) * 4 <= ShaderRecompiler::TessellationLevelPushOffsetBytes);
            std::memcpy(block.data() + ShaderRecompiler::MeshDrawPushOffsetBytes, words.data(), sizeof(words));
            stages |= VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        pipeline.PushConstants(commands, stages, block);
        return;
    }
    const auto firstVertex = draw.indirect ? draw.indirect->vertexConstant : draw.firstVertex;
    const auto firstInstance = draw.indirect ? draw.indirect->instanceConstant : draw.firstInstance;
    std::array<std::uint32_t, ShaderRecompiler::MeshDrawPushBytes / 4> words{draw.indexCount, firstVertex, firstInstance, (draw.indexed ? draw.indexSize | (restartTable ? ShaderRecompiler::MeshIndexRestartTable : 0u) : 0u) | meshLayerStride << ShaderRecompiler::MeshLayerStrideShift, static_cast<std::uint32_t>(meshArguments), static_cast<std::uint32_t>(meshArguments >> 32u)};
    if (state.tessellationCapture != nullptr) words = {static_cast<std::uint32_t>(capturedItems), static_cast<std::uint32_t>(capturedItems >> 32u), 0u, 0u, static_cast<std::uint32_t>(meshArguments), static_cast<std::uint32_t>(meshArguments >> 32u)};
    static_assert(ShaderRecompiler::MeshDrawPushOffsetBytes + ShaderRecompiler::MeshDrawPushBytes == PipelinePushSlotBytes);
    std::memcpy(block.data() + ShaderRecompiler::MeshDrawPushOffsetBytes, words.data(), sizeof(words));
    pipeline.PushConstants(commands, stages | VK_SHADER_STAGE_MESH_BIT_EXT, block);
}

bool CaptureInputsEnabled() {
    static const bool enabled = std::getenv("APS5_CAPTURE_INPUTS") != nullptr;
    Require(!enabled || CaptureTrace::Enabled(), "APS5_CAPTURE_INPUTS requires APS5_CAPTURE_TRACE");
    return enabled;
}

void captureInputs(const Context& context, Recorder& recorder, VkCommandBuffer commands, const ShaderResources& resources, const std::shared_ptr<ShaderResources::DrawBindings>& bindings, std::uint64_t target) {
    struct Sample {
        std::uint64_t address;
        std::size_t offset;
        std::vector<std::byte> expected;
        VkBuffer source;
        VkDeviceSize sourceOffset;
    };
    static unsigned long long nextDraw = 0;
    const auto draw = ++nextDraw;
    const auto batch = static_cast<unsigned long long>(recorder.Submissions() + 1);
    std::vector<Sample> samples;
    std::size_t total = 0;
    const auto addSample = [&](std::uint64_t address, std::size_t bytes, VkBuffer source, VkDeviceSize offset, const std::byte* expected) {
        if (bytes == 0 || bytes > 512) return;
        Require(offset % 4 == 0 && bytes % 4 == 0, "capture input is not aligned for a Vulkan buffer copy");
        Require(total + bytes <= 65536, "capture inputs exceed 64 KiB per draw");
        Sample sample{address, total, std::vector<std::byte>(bytes), source, offset};
        std::memcpy(sample.expected.data(), expected, bytes);
        total += bytes;
        samples.push_back(std::move(sample));
    };
    if (bindings != nullptr) {
        for (const auto& snapshot : bindings->snapshots) {
            if (snapshot.buffer == nullptr) continue;
            const auto bytes = snapshot.Bytes();
            addSample(snapshot.address, bytes.size(), snapshot.buffer->Handle(), snapshot.offset, bytes.data());
        }
    }
    for (const auto& [begin, end] : resources.InPlaceReads()) {
        Require(end >= begin, "invalid capture input range");
        const auto bytes = static_cast<std::size_t>(end - begin);
        if (bytes == 0 || bytes > 512) continue;
        if (bindings != nullptr && std::any_of(bindings->snapshots.begin(), bindings->snapshots.end(), [&](const auto& snapshot) { return snapshot.address < end && begin < snapshot.address + snapshot.bytes; })) continue;
        if (resources.WritesOverlap(begin, bytes) || recorder.PendingWriteOverlaps(begin, bytes)) {
            CaptureTrace::Log("input-skip draw=%llu batch=%llu address=%llx bytes=%zu reason=gpu-writer", draw, batch, static_cast<unsigned long long>(begin), bytes);
            continue;
        }
        const auto* imported = HostImportFor(context, begin, bytes);
        Require(imported != nullptr, "capture input has no host import");
        addSample(begin, bytes, imported->buffer, HostImportOffset(*imported, begin), reinterpret_cast<const std::byte*>(begin));
    }
    CaptureTrace::Log("input-capture draw=%llu batch=%llu target=%llx ranges=%zu bytes=%zu", draw, batch, static_cast<unsigned long long>(target), samples.size(), total);
    if (samples.empty()) return;
    auto readback = std::make_shared<Buffer>(context, total, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    const auto copy = context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
    for (const auto& sample : samples) {
        const VkBufferCopy region{sample.sourceOffset, sample.offset, sample.expected.size()};
        copy(commands, sample.source, readback->Handle(), 1, &region);
    }
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
    recorder.OnComplete([draw, batch, target, readback, samples = std::move(samples)] {
        const auto actual = readback->Bytes();
        std::size_t differences = 0;
        for (const auto& sample : samples) {
            std::size_t changed = 0;
            for (std::size_t index = 0; index < sample.expected.size(); ++index) {
                const auto gpu = actual[sample.offset + index];
                if (sample.expected[index] == gpu) continue;
                if (changed < 16) CaptureTrace::Log("input-byte draw=%llu batch=%llu address=%llx offset=%zu cpu=%02x gpu=%02x", draw, batch, static_cast<unsigned long long>(sample.address), index, std::to_integer<unsigned>(sample.expected[index]), std::to_integer<unsigned>(gpu));
                ++changed;
            }
            if (changed != 0) {
                ++differences;
                CaptureTrace::Log("input-mismatch draw=%llu batch=%llu target=%llx address=%llx bytes=%zu changed=%zu", draw, batch, static_cast<unsigned long long>(target), static_cast<unsigned long long>(sample.address), sample.expected.size(), changed);
            }
        }
        CaptureTrace::Log("input-result draw=%llu batch=%llu ranges=%zu mismatched=%zu", draw, batch, samples.size(), differences);
    });
}

bool WriteOnlyPasses() {
    static const bool enabled = std::getenv("APS5_NO_WRITE_ONLY_PASSES") == nullptr;
    return enabled;
}

void recordDraw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, DrawInputs& inputs, RecordedDraw& record, DrawOutcome& outcome, DrawTimer& timer, double& ownWaitedMs, const DrawTraceContext* trace = nullptr) {
    std::uint32_t subdraw = 0;
    auto* recorder = record.recorder;
    auto& resources = *record.resources;
    const auto* args = record.indirect != nullptr ? record.indirect->args : nullptr;
    const bool gpuIndirect = args != nullptr && record.indirect->path == IndirectDrawPath::Gpu;
    using CommandClass = Recorder::CommandClass;
    const auto countBarrier = [&](std::uint32_t count) { Recorder::CountBarriers(CommandClass::Draw, count); };
    const auto passKey = DrawRenderPassKey(context, state, record.targetViews);
    const bool readsTarget = std::any_of(record.targets.begin(), record.targets.end(), [&](const std::shared_ptr<StorageTexture>& target) { return resources.ReadsImage(target.get()); });
    const auto touches = [&](std::uint64_t begin, std::uint64_t end) {
        const auto bytes = static_cast<std::size_t>(end - begin);
        if (resources.WritesOverlap(begin, bytes) || resources.ReadsOverlap(begin, bytes)) return true;
        if (!gpuIndirect) return false;
        return (begin < args->arguments + args->RangeBytes() && args->arguments < end) || (args->countIndirect && begin < args->countAddress + 4 && args->countAddress < end);
    };
    if (recorder->HasQueuedKeyStores() && (resources.HoldsLease() || recorder->AnyQueuedKeyStore(touches))) {
        recorder->FlushKeyStores();
        outcome.keyStoresFlushed = true;
    }
    if (recorder->HasQueuedStores() && (resources.HoldsLease() || recorder->AnyQueuedStore(touches))) {
        recorder->FlushStores();
        outcome.storesFlushed = true;
    }
    const bool capture = CaptureInputsEnabled();
    const bool meshIndirect = state.stages.mesh && args != nullptr;
    const bool captured = state.stages.mesh && state.tessellationCapture != nullptr;
    const bool addressBased = resources.HoldsLease();
    thread_local std::vector<std::pair<std::uint64_t, std::uint64_t>>* passReadsSlot = nullptr;
    auto& passReads = ShaderRecompiler::ThreadOwned(passReadsSlot);
    passReads.clear();
    if (!addressBased) resources.DeviceReads(passReads);
    const auto deviceReads = passReads.size();
    if (gpuIndirect) {
        passReads.emplace_back(args->arguments, args->arguments + args->RangeBytes());
        if (args->countIndirect) passReads.emplace_back(args->countAddress, args->countAddress + 4);
    }
    passReads.insert(passReads.end(), inputs.inPlaceReads.begin(), inputs.inPlaceReads.end());
    for (const auto& entry : record.moved) {
        if (entry.inPlace == VK_NULL_HANDLE) continue;
        const auto begin = entry.address - entry.adjustment;
        passReads.emplace_back(begin, begin + GuestBufferMemory::ViewBytes(entry.size, entry.adjustment));
    }
    const auto passImages = resources.StorageImages();
    std::vector<std::pair<VkImage, bool>> passAttachments;
    passAttachments.reserve(record.targets.size());
    for (const auto& target : record.targets) {
        if (target != nullptr) passAttachments.emplace_back(target->Image(), true);
    }
    for (const auto image : record.multisampleImages) passAttachments.emplace_back(image, true);
    const bool unknownMemory = addressBased || resources.UsesAddressTable() || resources.HasCopiedWrites();
    static_cast<void>(StorageTexture::StoreDrawnLinearTouching("draw access", [&](std::uint64_t begin, std::uint64_t end) { return std::any_of(passReads.begin(), passReads.end(), [&](const auto& read) { return read.first < end && begin < read.second; }) || resources.WritesOverlap(begin, static_cast<std::size_t>(end - begin)); }, unknownMemory));
    const auto drawUse = resources.BeginDrawUse(*recorder);
    const auto drawBindings = resources.PrepareDrawBindings(*recorder, record.moved, false, drawUse != nullptr ? drawUse->record.get() : nullptr);
    static const bool addressReadPasses = std::getenv("APS5_NO_ADDRESS_READ_PASSES") == nullptr;
    const bool readOnlyBda = addressReadPasses && resources.BdaReadOnly();
    outcome.addressDraw = addressBased;
    const bool unknownWrites = ((addressBased || resources.UsesAddressTable()) && !readOnlyBda) || resources.HasCopiedWrites();
    GuestRangeList passWrites = resources.GpuWrites();
    GuestRangeList passWriteOnly{};
    thread_local std::vector<std::pair<std::uint64_t, std::uint64_t>>* hazardWritesSlot = nullptr;
    if (const auto& writeOnly = resources.WriteOnlyRanges(); !writeOnly.empty() && !unknownMemory && WriteOnlyPasses()) {
        const auto listed = [&](const std::pair<std::uint64_t, std::uint64_t>& range, bool aligned) {
            return std::any_of(writeOnly.begin(), writeOnly.end(), [&](const auto& only) {
                if (!aligned) return only == range;
                const auto begin = only.first & ~std::uint64_t{3};
                const auto end = begin == only.first ? only.second : (only.second + 3) & ~std::uint64_t{3};
                return range.first == begin && range.second == end;
            });
        };
        const auto ownReads = passReads.begin() + static_cast<std::ptrdiff_t>(deviceReads);
        passReads.erase(std::remove_if(passReads.begin(), ownReads, [&](const auto& range) { return listed(range, true); }), ownReads);
        auto& hazardWrites = ShaderRecompiler::ThreadOwned(hazardWritesSlot);
        hazardWrites.clear();
        for (const auto& range : resources.GpuWrites()) {
            if (!listed(range, false)) hazardWrites.push_back(range);
        }
        passWrites = hazardWrites;
        passWriteOnly = writeOnly;
    }
    const PassAccess passAccess{passReads, passWrites, passImages, passAttachments, unknownMemory, unknownWrites, passWriteOnly};
    const bool fenced = state.depth.has_value() || !record.proxies.empty();
    resources.RequireGuestBuffers();
    if (UsesMeshWindow(shaders)) PrepareMeshWindow(context);
    const auto start = recorder->StartDrawPass(passKey, capture || readsTarget || gpuIndirect || meshIndirect || captured || !record.staged.empty() || PixelFunctionLdsSubgroup(shaders) != 0u, fenced, passAccess);
    if (record.spaceKeep != nullptr) *record.spaceKeep = reserveSpaceKeep(*recorder, resources);
    for (const auto& target : record.multisampleTargets) KeepMultisampleTarget_nid_no_patch(target);
    const bool continued = start.continued;
    outcome.passContinued = continued;
    outcome.passBegun = !continued;
    outcome.passBreak = start.reason;
    const auto commands = start.commands;
    auto& recording = recorder->GraphicsState();
    if (!continued) {
        recorder->PrepareSampleSlot();
        PerformanceTrace::Amount(PerformanceTrace::RenderPassScope, 1);
    }
    if (capture) captureInputs(context, *recorder, commands, resources, drawBindings, record.targets.empty() || record.targets.front() == nullptr ? 0 : record.targets.front()->Descriptor().baseAddress);
    const auto drawTiming = !continued ? recorder->BeginGpuTiming(CommandClass::Draw) : Recorder::NoTiming;
    if (!continued && Recorder::BarrierValidate()) {
        auto reads = resources.InPlaceReads();
        reads.insert(reads.end(), inputs.inPlaceReads.begin(), inputs.inPlaceReads.end());
        if (gpuIndirect) {
            reads.emplace_back(args->arguments, args->arguments + args->RangeBytes());
            if (args->countIndirect) reads.emplace_back(args->countAddress, args->countAddress + 4);
        }
        auto images = resources.StorageImages();
        for (const auto& target : record.targets) {
            if (target != nullptr) images.emplace_back(target->Image(), true);
        }
        recorder->NoteAccess(CommandClass::Draw, Recorder::Access{reads, resources.GpuWrites(), images, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | inputs.shaderStages, resources.HoldsLease()});
    }
    std::unique_ptr<DeviceBuffer> scratch;
    std::shared_ptr<Buffer> argumentSnapshot;
    std::shared_ptr<Buffer> meshArguments;
    VkBuffer argumentBuffer = VK_NULL_HANDLE;
    VkDeviceSize argumentOffset = 0;
    bool rewritten = false;
    if (!meshIndirect && !inputs.restartTable.empty()) meshArguments = directMeshArguments(context, draw, inputs.restartTable);
    if (continued) {
        record.pipeline->Continue(recording, state, record.masked);
    } else {
        for (const auto& proxy : record.proxies) {
            proxy->RecordAttachmentProxyLoad(commands, VK_IMAGE_LAYOUT_GENERAL);
            countBarrier(2);
        }
        if (start.barrier) {
            const VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
            context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 1, &before, 0, nullptr, 0, nullptr);
            countBarrier(1);
        }
        APS5_LOG_CHARS_OUT_DEBUG("Upload barrier recorded");
        if (inputs.vertexInputBehindGpuWrite) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
            countBarrier(1);
        }
        const auto pixelLdsSubgroup = PixelFunctionLdsSubgroup(shaders);
        if (pixelLdsSubgroup != 0u) PreparePixelFunctionLds(context, commands, pixelLdsSubgroup);
        if (captured) {
            recordCaptureBarrier(context, commands);
            countBarrier(1);
            meshArguments = state.tessellationCapture->header;
            argumentBuffer = meshArguments->Handle();
        } else if (meshIndirect) {
            meshArguments = recordMeshArguments(context, commands, recorder, true, state, draw, *record.indirect, countBarrier, inputs.restartTable);
            argumentBuffer = meshArguments->Handle();
        } else if (gpuIndirect) {
            rewritten = recordIndirectArguments(context, commands, recorder, true, *record.indirect, scratch, argumentSnapshot, argumentBuffer, argumentOffset, countBarrier);
        }
        if (!record.reordered.empty()) {
            std::vector<std::shared_ptr<void>> kept;
            RecordSampleOrder(context, recording, record.reorderPass, record.reordered, true, kept);
            for (auto& object : kept) recorder->Keep(std::move(object));
        }
        for (const auto& write : record.staged) RecordStagedClear(context, commands, *write.temporary);
        APS5_LOG_OUT_DEBUG("Beginning pipeline renderExtent=%ux%u", state.renderExtent.width, state.renderExtent.height);
        record.pipeline->Begin(recording, *record.framebuffer, state.renderExtent, state, record.masked);
    }
    APS5_LOG_CHARS_OUT_DEBUG("Pipeline Begin OK");
    if (drawBindings != nullptr) {
        const auto set = drawBindings->allocation.set;
        const auto fragmentSet = drawBindings->fragmentAllocation.set;
        if (set != VK_NULL_HANDLE) context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, record.pipeline->Layout(), 0, 1, &set, 0, nullptr);
        if (fragmentSet != VK_NULL_HANDLE) context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, record.pipeline->Layout(), ShaderRecompiler::RuntimeAbi::FragmentDescriptorSet, 1, &fragmentSet, 0, nullptr);
    } else {
        resources.BindDrawUse(commands, record.pipeline->Layout(), drawUse.get());
    }
    if (std::ranges::any_of(shaders, [](const CompiledShader& shader) { return shader.program != nullptr && shader.program->functionLdsBuffer; })) BindPixelFunctionLds(context, commands, record.pipeline->Layout());
    if (UsesMeshWindow(shaders)) BindMeshWindow(context, commands, record.pipeline->Layout());
    APS5_LOG_CHARS_OUT_DEBUG("Resources bound");
    std::array<std::byte, PipelinePushConstantBytes> movedPush;
    const std::array<std::byte, PipelinePushConstantBytes>* pushBytes = record.pushBytes;
    VkShaderStageFlags pushStages = record.pushStages;
    if (ShaderResources::MovedPushPatches(record.moved)) {
        movedPush = pushBytes != nullptr ? *pushBytes : AssemblePushConstants(shaders);
        if (pushBytes == nullptr) resources.PatchPushConstants(movedPush);
        if (resources.PatchMovedPushConstants(movedPush, record.moved)) {
            if (pushBytes == nullptr) pushStages = PushConstantStages(shaders);
            pushBytes = &movedPush;
        }
    }
    pushDrawConstants(*record.pipeline, commands, state, draw, shaders, resources, pushBytes, pushStages, meshArguments != nullptr ? meshArguments->DeviceAddress() : 0, !inputs.restartTable.empty(), inputs.meshLayerStride);
    APS5_LOG_CHARS_OUT_DEBUG("Push constants recorded");
    static const bool timeEachDraw = std::getenv("APS5_PROFILE_GPU_DRAWS") != nullptr;
    auto eachDrawTiming = Recorder::NoTiming;
    if (timeEachDraw) {
        std::uint64_t key = 0;
        for (const auto& shader : shaders) {
            if (shader.program != nullptr && (key == 0 || shader.stage == ShaderRecompiler::ShaderStage::Fragment)) key = shader.program->variantId;
        }
        eachDrawTiming = recorder->BeginInPassGpuTiming(Recorder::DrawTimingKey(key));
    }
    noteDrawFaultToken(context, *recorder, commands, shaders, draw, static_cast<std::uint32_t>(state.stages.mesh.has_value() ? AgcDriver::FaultStage::mesh : AgcDriver::FaultStage::graphics), record.indirect != nullptr ? record.indirect->args : nullptr);
    if (record.stencilReplay.empty()) recordDrawCommands(context, commands, state, draw, inputs, record.indirect, argumentBuffer, argumentOffset, trace, subdraw);
    else {
        recordStencilReplay(context, recording, state, draw, inputs, record.stencilReplay, shaders, resources, pushBytes, pushStages);
        for (const auto& [reference, pipeline] : record.stencilReplay) recorder->Keep(pipeline);
    }
    if (record.pipeline->SplitsFaces()) {
        record.pipeline->ContinueBackFaces(recording, state, record.masked);
        recordDrawCommands(context, commands, state, draw, inputs, record.indirect, argumentBuffer, argumentOffset, trace, subdraw);
    }
    if (record.pipeline->SplitsSecondChannel()) {
        record.pipeline->ContinueSecondChannel(recording, state, record.masked);
        recordDrawCommands(context, commands, state, draw, inputs, record.indirect, argumentBuffer, argumentOffset, trace, subdraw);
    }
    recorder->EndGpuTiming(eachDrawTiming);
    if (meshArguments != nullptr) recorder->Keep(meshArguments);
    if (argumentSnapshot != nullptr) recorder->Keep(argumentSnapshot);
    if (state.tessellationCapture != nullptr) {
        recorder->Keep(state.tessellationCapture->header);
        recorder->Keep(state.tessellationCapture->items);
    }
    if (args != nullptr) CountIndirectDraw(record.indirect->path, record.indirect->readMs, rewritten);
    auto checkRecords = indirectRecordCheck(record.indirect);
    APS5_LOG_CHARS_OUT_DEBUG("Draw recorded");
    std::function<void(CommandState&)> storeProxies;
    if (!continued && (!record.proxies.empty() || !record.reordered.empty() || record.depthLocations)) {
        storeProxies = [proxies = record.proxies, reordered = record.reordered, reorderPass = record.reorderPass, depthLocations = record.depthLocations, context, recorder](CommandState& passRecording) {
            const auto passCommands = passRecording.Commands();
            for (const auto& proxy : proxies) {
                proxy->RecordAttachmentProxyStore(passCommands, VK_IMAGE_LAYOUT_GENERAL);
                Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
            }
            if (!reordered.empty()) {
                std::vector<std::shared_ptr<void>> kept;
                RecordSampleOrder(context, passRecording, reorderPass, reordered, false, kept);
                for (auto& object : kept) recorder->Keep(std::move(object));
            }
            if (depthLocations) {
                RecordDepthSampleLocations(context, passCommands, *depthLocations);
                Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
                recorder->Keep(depthLocations->keep);
            }
        };
    }
    recorder->LeaveRenderPassOpen(passKey, drawTiming, fenced, passAccess, std::move(storeProxies));
    timer.phase(PhaseRecord);
    keepRecordedDraw(*recorder, record.resources, std::move(record.pipeline), std::move(record.framebuffer), inputs, std::move(record.targets), std::move(record.targetSlices), state.scissor, std::move(scratch), std::move(checkRecords), record.listed, record.completion, outcome, drawUse, record.spaceKeep != nullptr && *record.spaceKeep != nullptr ? record.spaceKeep->get() : nullptr);
    timer.phase(PhaseKeep);
    if (!record.staged.empty()) {
        const auto merged = recorder->Commands();
        for (auto& write : record.staged) {
            RecordStagedMerge(context, merged, *write.temporary, write.target, write.mask, write.samples, write.area);
            recorder->Keep(write.temporary);
        }
    }
    if (record.waited) {
        Recorder::CountSync(3);
        const auto ownBefore = timer.profile ? Recorder::ThreadWaitedMs() : 0.0;
        recorder->Sync();
        if (timer.profile) ownWaitedMs += Recorder::ThreadWaitedMs() - ownBefore;
        timer.phase(PhaseSync);
    }
}

}
}
}
}
}
}
