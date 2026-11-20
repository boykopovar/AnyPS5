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
std::shared_ptr<StorageTexture> refreshResidentTarget(const Context& context, const State& state, const ColorTarget& color, DrawOutcome& outcome, bool profile, const std::function<std::shared_ptr<StorageTexture>()>& lookup) {
    const auto lookupStart = std::chrono::steady_clock::now();
    const auto walkedBefore = profile ? GuestMemory::ThreadCollectedBytes() : 0;
    std::shared_ptr<StorageTexture> resident;
    try {
        resident = lookup();
        if (resident != nullptr) materializeRegisterClear(context, color, *resident);
    } catch (const std::exception& error) {
        resident = nullptr;
        static AgcDriver::Mutex reportMutex;
        static std::set<std::uint64_t> reported;
        std::lock_guard lock(reportMutex);
        if (reported.insert(color.address).second) std::fprintf(stderr, "[gpu] color target 0x%llx stays non-resident: %s\n", static_cast<unsigned long long>(color.address), error.what());
    }
    if (resident != nullptr) materializeCmaskClear(context, color, resident);
    if (profile) {
        const auto lookupUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - lookupStart).count();
        ++outcome.targetLookups;
        if (lookupUs >= 1000.0) {
            ++outcome.slowLookups;
            outcome.slowLookupUs += lookupUs;
        }
        outcome.lookupUs += lookupUs;
        outcome.pagesWalked += (GuestMemory::ThreadCollectedBytes() - walkedBefore) / 4096;
        const bool fullScissor = state.scissor.offset.x <= 0 && state.scissor.offset.y <= 0 && state.scissor.extent.width >= color.extent.width && state.scissor.extent.height >= color.extent.height;
        ++(fullScissor ? outcome.fullScissorLookups : outcome.partialLookups);
    }
    return resident;
}

// A draw's resources (resolveDrawResources): the resource-cache template a recordable draw's
// stages repeat, or a fresh build.
struct ResolvedResources {
    std::shared_ptr<ShaderResources> resources;
    std::vector<ShaderResources::MovedBuffer> moved;
    ResourceCache::Key contentKey;
    bool cacheable = false;
    const ShaderResources::BuildTiming* built = nullptr;
};

enum class AddressStages { None, ReadOnly, Storing };

AddressStages addressStages(std::span<const CompiledShader> shaders) {
    auto stages = AddressStages::None;
    for (const auto& shader : shaders) {
        if (shader.program == nullptr || std::none_of(shader.program->bindings.begin(), shader.program->bindings.end(), [](const ShaderRecompiler::DescriptorBinding& binding) { return binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable; })) continue;
        if (ShaderStoresThroughAddresses(shader.program->spirv)) return AddressStages::Storing;
        stages = AddressStages::ReadOnly;
    }
    return stages;
}

bool verifyAddressDrawReuse() {
    static const bool verify = std::getenv("APS5_VERIFY_ADDRESS_DRAW_REUSE") != nullptr;
    return verify;
}

bool snapshotsInTable(std::span<const std::byte> table, std::span<const GuestMemorySnapshot> snapshots) {
    ShaderRecompiler::BdaAbi::Header header{};
    if (table.size() < sizeof(header)) return snapshots.empty();
    std::memcpy(&header, table.data(), sizeof(header));
    if (header.entryBytes != sizeof(ShaderRecompiler::BdaAbi::Range) || (table.size() - sizeof(header)) / sizeof(ShaderRecompiler::BdaAbi::Range) < header.count) return false;
    std::vector<ShaderRecompiler::BdaAbi::Range> ranges(header.count);
    if (!ranges.empty()) std::memcpy(ranges.data(), table.data() + sizeof(header), ranges.size() * sizeof(ranges.front()));
    return std::ranges::all_of(snapshots, [&](const GuestMemorySnapshot& snapshot) {
        auto at = snapshot.address;
        const auto end = snapshot.address + snapshot.bytes.size();
        for (const auto& range : ranges) {
            if (at >= end || range.begin > at) break;
            if (range.end > at && (range.permissions & ShaderRecompiler::BdaAbi::Read) != 0u) at = range.end;
        }
        return at >= end;
    });
}

bool snapshotsEqualMemory(std::span<const GuestMemorySnapshot> snapshots) {
    return std::ranges::all_of(snapshots, [](const GuestMemorySnapshot& snapshot) { return snapshot.bytes.empty() || std::memcmp(reinterpret_cast<const void*>(snapshot.address), snapshot.bytes.data(), snapshot.bytes.size()) == 0; });
}

void verifyServedSpace(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots, std::uint64_t indexBytes, const ShaderResources& served) {
    const ShaderResources fresh(context, shaders, state.color, static_cast<std::uint32_t>(state.colors.size()), draw.indexAddress, static_cast<std::size_t>(indexBytes), snapshots);
    const auto servedTable = served.AddressTableBytes();
    const auto freshTable = fresh.AddressTableBytes();
    const char* what = !fresh.SpaceReuse() ? "space reuse" : !std::ranges::equal(servedTable, freshTable) ? "BDA table" : !snapshotsInTable(servedTable, snapshots) ? "snapshot ranges" : !snapshotsEqualMemory(snapshots) ? "snapshot bytes" : served.SampledTextures() != fresh.SampledTextures() ? "sampled textures" : served.StorageImages() != fresh.StorageImages() ? "storage images" : served.LayoutKey() != fresh.LayoutKey() || served.FragmentLayoutKey() != fresh.FragmentLayoutKey() ? "set layouts" : nullptr;
    addressDrawReuseVerifications().fetch_add(1, std::memory_order_relaxed);
    if (what == nullptr) return;
    char message[320];
    std::snprintf(message, sizeof(message), "APS5_VERIFY_ADDRESS_DRAW_REUSE: a served address-based draw hit disagrees with a fresh build or guest memory in its %s (BDA table %zu bytes served, %zu fresh; %zu / %zu sampled textures; %zu snapshots)", what, servedTable.size(), freshTable.size(), served.SampledTextures().size(), fresh.SampledTextures().size(), snapshots.size());
    throw std::runtime_error(message);
}

bool drawStagingEnabled() {
    static const bool disabled = std::getenv("APS5_NO_DRAW_STAGING") != nullptr;
    return !disabled;
}

ResolvedResources resolveDrawResources(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots, std::uint64_t indexBytes, bool recordable, const DrawPlan& plan, DrawOutcome& outcome, DrawTimer& timer) {
    ResolvedResources resolved;
    APS5_LOG_CHARS_OUT_DEBUG("Creating ShaderResources");
    // A recordable draw whose stages' compiled content repeats an earlier one binds that build's
    // descriptor set when it is still valid (see ResourceCache; the dispatch path does the same).
    // Only recordable draws take part: a synchronous draw writes its resources back, which a shared
    // object must never do. Revalidate proves a hit by texture identity, which needs the texture
    // caches. Push constants still come from this draw's stages. APS5_NO_DRAW_RESOURCE_CACHE=1
    // builds every draw's resources as before.
    // The render target and index ranges stay out of the key (see DrawResourceKey); a hit repeats
    // the alias checks instead. Debug aid: APS5_NO_DRAW_KEY_TRIM=1 keys them as before.
    static const bool trimKey = std::getenv("APS5_NO_DRAW_KEY_TRIM") == nullptr;
    const bool candidate = recordable && plan.candidate;
    resolved.cacheable = recordable && plan.keyed;
    const auto address = candidate && !ShaderResources::SpaceReuseDisabled() ? addressStages(shaders) : AddressStages::None;
    std::optional<AddressDrawReuse> reuse;
    if (address == AddressStages::Storing) reuse = AddressDrawReuse::NotReadOnly;
    else if (address == AddressStages::ReadOnly && !resolved.cacheable) reuse = AddressDrawReuse::NotSpaceOnly;
    if (resolved.cacheable) {
        resolved.contentKey = plan.contentKey;
        if (auto cached = SharedResourceCache().Find(resolved.contentKey)) {
            const auto releaseSpace = [](ShaderResources* resources) { static_cast<void>(resources->ReleaseSpace()); };
            std::unique_ptr<ShaderResources, decltype(releaseSpace)> heldSpace(nullptr, releaseSpace);
            const bool counted = address == AddressStages::ReadOnly && cached->SpaceReuse();
            bool spaceHeld = !cached->SpaceReuse();
            bool inUse = false;
            if (!spaceHeld) {
                if (auto* active = Recorder::Active(); active != nullptr) {
                    GuestBufferMemory::ReleaseRetainedSpaces(active->Id(), active->CompletedThrough());
                    static_cast<void>(cached->ReleaseCompletedSpace(active->Id(), active->CompletedThrough()));
                }
                if (cached->SpaceHeld()) {
                    inUse = true;
                    reuse = AddressDrawReuse::InUse;
                } else if (!cached->RetakeSpace()) {
                    reuse = AddressDrawReuse::SpaceChanged;
                } else {
                    heldSpace.reset(cached.get());
                    if (!cached->AddressTableGuardCurrent()) {
                        reuse = AddressDrawReuse::SpaceChanged;
                        static_cast<void>(cached->ReleaseSpace());
                    } else {
                        completeSnapshotWrites(snapshots);
                        const auto service = cached->SnapshotsServedInPlace(snapshots);
                        spaceHeld = service == SnapshotService::Served;
                        if (!spaceHeld) {
                            reuse = service == SnapshotService::Partial ? AddressDrawReuse::PartialOverlap : service == SnapshotService::Changed ? AddressDrawReuse::SnapshotChanged : AddressDrawReuse::SnapshotOutside;
                            static_cast<void>(cached->ReleaseSpace());
                        }
                    }
                }
            }
            const bool valid = spaceHeld && cached->Revalidate(shaders);
            if (counted && spaceHeld && !valid) reuse = AddressDrawReuse::Revalidation;
            auto* recorder = Recorder::Active();
            std::optional<std::vector<ShaderResources::MovedBuffer>> moved;
            bool patched = false;
            if (valid && recorder != nullptr) moved = cached->MovedReadOnlyBuffers(shaders, *recorder, &state.color, &patched);
            if (moved.has_value()) {
                if (trimKey) CheckBufferAliases(shaders, state.color, draw.indexAddress, indexBytes);
                if (counted && verifyAddressDrawReuse()) verifyServedSpace(context, state, draw, shaders, snapshots, indexBytes, *cached);
                static_cast<void>(heldSpace.release());
                resolved.resources = std::move(cached);
                resolved.moved = std::move(*moved);
                outcome.kind = KindTemplateHit;
                countCache(&DrawProfile::cacheHits);
                if (counted) reuse = AddressDrawReuse::Reused;
            } else if (valid || inUse) {
                if (valid && cached->SpaceReuse()) static_cast<void>(cached->ReleaseSpace());
                if (valid && counted) reuse = patched ? AddressDrawReuse::PatchedData : AddressDrawReuse::MovedBuffer;
                countCache(&DrawProfile::cacheMisses);
            } else {
                if (cached->SpaceReuse()) static_cast<void>(cached->ReleaseSpace());
                SharedResourceCache().Remove(resolved.contentKey);
                countCache(&DrawProfile::cacheInvalidated);
            }
        }
    } else {
        countCache(&DrawProfile::uncacheable);
    }
    timer.phase(PhaseLookup);
    if (resolved.resources == nullptr) {
        resolved.resources = std::make_shared<ShaderResources>(context, shaders, state.color, static_cast<std::uint32_t>(state.colors.size()), draw.indexAddress, static_cast<std::size_t>(indexBytes), snapshots, recordable && drawStagingEnabled());
        resolved.built = &resolved.resources->Timing();
        outcome.addressBased = resolved.resources->HoldsLease();
        outcome.kind = outcome.addressBased ? KindBda : KindBuild;
        if (resolved.cacheable) countCache(&DrawProfile::cacheMisses);
        if (address == AddressStages::ReadOnly && !reuse.has_value()) reuse = resolved.resources->SpaceReuse() ? AddressDrawReuse::Built : AddressDrawReuse::NotSpaceOnly;
    }
    if (reuse.has_value()) countAddressDrawReuse(*reuse);
    timer.phase(PhaseResources);
    APS5_LOG_CHARS_OUT_DEBUG("ShaderResources created");
    return resolved;
}

// A GPU-side indirect draw's records (Draw's recorded path; a recipe draw has none) as recordDraw
// takes them: the arguments, how they are read, their host imports and, on the CPU path, the
// records read.
struct IndirectRecord {
    const Pm4::DrawParameters::IndirectDraw* args = nullptr;
    IndirectDrawPath path = IndirectDrawPath::Gpu;
    const HostImport* argumentImport = nullptr;
    const HostImport* countImport = nullptr;
    std::span<const Pm4::DrawArguments> records;
    double readMs = 0;
};

void CompactIndirectListRestart(const Context& context, const Pm4::DrawParameters& draw, DrawInputs& inputs, std::vector<Pm4::DrawArguments>& records) {
    const auto bound = inputs.indexCount.value_or(draw.indexCount);
    const auto raw = inputs.indices->Bytes();
    std::uint64_t total = 0;
    for (const auto& record : records) {
        if (record.count != 0 && record.instances != 0 && record.firstVertexOrIndex < bound) total += std::min(record.count, bound - record.firstVertexOrIndex);
    }
    Require(total <= std::numeric_limits<std::uint32_t>::max(), "indirect list restart index count overflow");
    auto compacted = std::make_shared<Buffer>(context, static_cast<std::size_t>(std::max<std::uint64_t>(total, 1u) * draw.indexSize), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    const auto out = compacted->Bytes();
    std::uint32_t cursor = 0;
    for (auto& record : records) {
        if (record.count == 0 || record.instances == 0 || record.firstVertexOrIndex >= bound) {
            record.count = 0;
            continue;
        }
        const auto count = std::min(record.count, bound - record.firstVertexOrIndex);
        const auto at = out.subspan(static_cast<std::size_t>(cursor) * draw.indexSize, static_cast<std::size_t>(count) * draw.indexSize);
        std::memcpy(at.data(), raw.data() + static_cast<std::size_t>(record.firstVertexOrIndex) * draw.indexSize, at.size());
        record.count = CompactListRestart(at, draw.indexSize, inputs.listRestartPrimitive);
        record.firstVertexOrIndex = cursor;
        cursor += record.count;
    }
    inputs.indexCount = cursor;
    inputs.indices = std::move(compacted);
    inputs.indexHandle = inputs.indices->Handle();
    inputs.indexOffset = 0;
}

// The draw commands of one draw: the vertex and index buffer binds, then the direct draw, the
// GPU-side indirect draw from `argumentBuffer` or the CPU-read records with the driver's rules.
bool recordDrawCommandsImpl(const Context& context, VkCommandBuffer commands, const State& state, const Pm4::DrawParameters& draw, const DrawInputs& inputs, const IndirectRecord* indirect, VkBuffer argumentBuffer, VkDeviceSize argumentOffset, const DrawTraceContext* trace, std::uint32_t& subdraw, std::optional<std::uint32_t> pieceFirstIndex = std::nullopt) {
    bool emitted = false;
    if (context.recorder != nullptr) context.recorder->NoteSampledDraw(commands);
    const auto recorded = [&](std::uint32_t index) {
        if (trace != nullptr && context.recorder != nullptr) {
            context.recorder->NoteDrawTraceToken(trace->token);
            CaptureTrace::Log("draw-recorded token=%llu batch=%llu submission=%llu queue=%u offset=%zu target=%llx subdraw=%u", static_cast<unsigned long long>(trace->token), static_cast<unsigned long long>(context.recorder->OpenBatchSerial()), static_cast<unsigned long long>(trace->submission), trace->queue, trace->offset, static_cast<unsigned long long>(trace->target), index);
        }
    };
    const auto* args = indirect != nullptr ? indirect->args : nullptr;
    if (state.stages.mesh && (args != nullptr || state.tessellationCapture != nullptr)) {
        CountVkDraw(VkDrawKind::Mesh);
        context.Function<PFN_vkCmdDrawMeshTasksIndirectEXT>("vkCmdDrawMeshTasksIndirectEXT")(commands, argumentBuffer, argumentOffset, 1, sizeof(VkDrawMeshTasksIndirectCommandEXT));
        recorded(subdraw++);
        emitted = true;
        return true;
    }
    if (state.stages.mesh) {
        APS5_LOG_OUT_DEBUG("vkCmdDrawMeshTasksEXT groups=%u instances=%u layers=%u", inputs.meshGroups, draw.instanceCount, inputs.meshLayers);
        CountVkDraw(VkDrawKind::Mesh);
        context.Function<PFN_vkCmdDrawMeshTasksEXT>("vkCmdDrawMeshTasksEXT")(commands, inputs.meshGroups, draw.instanceCount, inputs.meshLayers);
        recorded(subdraw++);
        emitted = true;
        return true;
    }
    if (!inputs.vertexHandles.empty()) context.Resolved(&DeviceFunctions::cmdBindVertexBuffers, "vkCmdBindVertexBuffers")(commands, 0, static_cast<std::uint32_t>(inputs.vertexHandles.size()), inputs.vertexHandles.data(), inputs.vertexOffsets.data());
    if (inputs.expandedIndexCount != 0u) {
        context.Resolved(&DeviceFunctions::cmdBindIndexBuffer, "vkCmdBindIndexBuffer")(commands, inputs.indexHandle, inputs.indexOffset, VK_INDEX_TYPE_UINT32);
        CountVkDraw(VkDrawKind::Indexed);
        context.Resolved(&DeviceFunctions::cmdDrawIndexed, "vkCmdDrawIndexed")(commands, inputs.expandedIndexCount, draw.instanceCount, 0, static_cast<std::int32_t>(draw.firstVertex), draw.firstInstance);
        emitted = true;
        return true;
    }
    if (draw.indexed) context.Resolved(&DeviceFunctions::cmdBindIndexBuffer, "vkCmdBindIndexBuffer")(commands, inputs.indexHandle, inputs.indexOffset, draw.indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    if (args == nullptr) {
        if (draw.indexed) {
            CountVkDraw(VkDrawKind::Indexed);
            context.Resolved(&DeviceFunctions::cmdDrawIndexed, "vkCmdDrawIndexed")(commands, pieceFirstIndex ? draw.indexCount : inputs.indexCount.value_or(draw.indexCount), draw.instanceCount, pieceFirstIndex.value_or(0u), static_cast<std::int32_t>(draw.firstVertex), draw.firstInstance);
            recorded(subdraw++);
            emitted = true;
        } else {
            CountVkDraw(VkDrawKind::Direct);
            context.Resolved(&DeviceFunctions::cmdDraw, "vkCmdDraw")(commands, draw.indexCount, draw.instanceCount, draw.firstVertex, draw.firstInstance);
            recorded(subdraw++);
            emitted = true;
        }
    } else if (indirect->path == IndirectDrawPath::Gpu) {
        emitted = true;
        if (args->countIndirect) {
            CountVkDraw(VkDrawKind::Indirect);
            if (draw.indexed) context.Function<PFN_vkCmdDrawIndexedIndirectCountKHR>("vkCmdDrawIndexedIndirectCountKHR")(commands, argumentBuffer, argumentOffset, indirect->countImport->buffer, HostImportOffset(*indirect->countImport, args->countAddress), args->count, args->stride);
            else context.Function<PFN_vkCmdDrawIndirectCountKHR>("vkCmdDrawIndirectCountKHR")(commands, argumentBuffer, argumentOffset, indirect->countImport->buffer, HostImportOffset(*indirect->countImport, args->countAddress), args->count, args->stride);
        } else if (args->count <= 1 || context.multiDrawIndirect) {
            CountVkDraw(VkDrawKind::Indirect);
            if (draw.indexed) context.Resolved(&DeviceFunctions::cmdDrawIndexedIndirect, "vkCmdDrawIndexedIndirect")(commands, argumentBuffer, argumentOffset, args->count, args->stride);
            else context.Resolved(&DeviceFunctions::cmdDrawIndirect, "vkCmdDrawIndirect")(commands, argumentBuffer, argumentOffset, args->count, args->stride);
        } else {
            for (std::uint32_t record = 0; record < args->count; ++record) {
                const auto offset = argumentOffset + static_cast<VkDeviceSize>(record) * args->stride;
                CountVkDraw(VkDrawKind::Indirect);
                if (draw.indexed) context.Resolved(&DeviceFunctions::cmdDrawIndexedIndirect, "vkCmdDrawIndexedIndirect")(commands, argumentBuffer, offset, 1, args->stride);
                else context.Resolved(&DeviceFunctions::cmdDrawIndirect, "vkCmdDrawIndirect")(commands, argumentBuffer, offset, 1, args->stride);
            }
        }
    } else {
        // The CPU-read records with the driver's rules: an InPlace dimension takes the record's
        // value, a Constant one (the CP writes nowhere) the constant.
        using Rule = Pm4::DrawParameters::IndirectDraw::Rule;
        for (const auto& record : indirect->records) {
            if (record.count == 0 || record.instances == 0) continue;
            const auto firstInstance = args->instanceRule == Rule::InPlace ? record.firstInstance : args->instanceConstant;
            if (draw.indexed) {
                // The CP clamps the index range to INDEX_BUFFER_SIZE (the bound copy).
                const auto boundCount = inputs.indexCount.value_or(draw.indexCount);
                if (record.firstVertexOrIndex >= boundCount) continue;
                const auto vertexOffset = args->vertexRule == Rule::InPlace ? record.vertexOffset : args->vertexConstant;
                emitted = true;
                CountVkDraw(VkDrawKind::Indexed);
                context.Resolved(&DeviceFunctions::cmdDrawIndexed, "vkCmdDrawIndexed")(commands, std::min(record.count, boundCount - record.firstVertexOrIndex), record.instances, record.firstVertexOrIndex, static_cast<std::int32_t>(vertexOffset), firstInstance);
            } else {
                const auto firstVertex = args->vertexRule == Rule::InPlace ? record.firstVertexOrIndex : args->vertexConstant;
                emitted = true;
                CountVkDraw(VkDrawKind::Direct);
                context.Resolved(&DeviceFunctions::cmdDraw, "vkCmdDraw")(commands, record.count, record.instances, firstVertex, firstInstance);
            }
        }
    }
    return emitted;
}


void NoteGuestWorkIn(const Context& context, VkCommandBuffer commands) {
    static const bool reporting = LibcRunReportEnabled_nid_no_patch();
    if (!reporting) return;
    if (context.recorder != nullptr && context.recorder->NoteGuestWork(commands)) return;
    NoteGuestWorkOnCommandBuffer(commands);
}

void recordDrawCommands(const Context& context, VkCommandBuffer commands, const State& state, const Pm4::DrawParameters& draw, const DrawInputs& inputs, const IndirectRecord* indirect, VkBuffer argumentBuffer, VkDeviceSize argumentOffset, const DrawTraceContext* trace, std::uint32_t& subdraw, std::optional<std::uint32_t> pieceFirstIndex = std::nullopt) {
    if (recordDrawCommandsImpl(context, commands, state, draw, inputs, indirect, argumentBuffer, argumentOffset, trace, subdraw, pieceFirstIndex)) NoteGuestWorkIn(context, commands);
}

// The GPU-side records of an indirect draw, recorded outside the render pass: the stores that
// produced them (a dispatch in place, a fill, the host) precede the indirect read, as for
// DISPATCH_INDIRECT; a Constant dimension is rewritten in a scratch copy (vkCmdDrawIndirect reads
// the record's dword as the first vertex / instance, and the constant is what the fixed-function
// fetch must start at). The scratch is kept until the batch completed. Returns whether a copy was
// rewritten; `argumentBuffer`/`argumentOffset` receive where the draw reads the records.
bool recordIndirectArguments(const Context& context, VkCommandBuffer commands, Recorder* recorder, bool recorded, const IndirectRecord& indirect, std::unique_ptr<DeviceBuffer>& scratch, std::shared_ptr<Buffer>& snapshot, VkBuffer& argumentBuffer, VkDeviceSize& argumentOffset, const std::function<void(std::uint32_t)>& countBarrier) {
    using Rule = Pm4::DrawParameters::IndirectDraw::Rule;
    const auto* args = indirect.args;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT);
    countBarrier(1);
    const bool rewritten = args->vertexRule == Rule::Constant || args->instanceRule == Rule::Constant;
    const auto bytes = static_cast<std::size_t>(args->RangeBytes());
    const auto* source = reinterpret_cast<const std::byte*>(args->arguments);
    if (!args->countIndirect && bytes != 0 && !Recorder::SnapshotWriteOverlaps(args->arguments, bytes) && (recorder == nullptr || recorder->PendingLeaseSerial(args->arguments, bytes) == 0) && GuestMemory::Accessible(source, bytes, false)) {
        snapshot = std::make_shared<Buffer>(context, bytes, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        const auto out = snapshot->Bytes();
        std::memcpy(out.data(), source, bytes);
        for (std::uint32_t record = 0; record < args->count; ++record) {
            const auto base = static_cast<std::size_t>(record) * args->stride;
            if (args->vertexRule == Rule::Constant) std::memcpy(out.data() + base + args->VertexDwordOffset(), &args->vertexConstant, 4);
            if (args->instanceRule == Rule::Constant) std::memcpy(out.data() + base + args->InstanceDwordOffset(), &args->instanceConstant, 4);
        }
        argumentBuffer = snapshot->Handle();
        argumentOffset = 0;
        return rewritten;
    }
    if (recorded) {
        // Read in place from the import when the batch runs (a synchronous draw waits for its own).
        recorder->NotePendingRead(args->arguments, static_cast<std::size_t>(args->RangeBytes()), Recorder::ReadKind::Indirect);
        if (args->countIndirect) recorder->NotePendingRead(args->countAddress, 4, Recorder::ReadKind::Indirect);
    }
    argumentBuffer = indirect.argumentImport->buffer;
    argumentOffset = HostImportOffset(*indirect.argumentImport, args->arguments);
    if (!rewritten) return false;
    scratch = std::make_unique<DeviceBuffer>(context, bytes, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    CopyBuffer(context, commands, argumentBuffer, argumentOffset, scratch->Handle(), 0, bytes);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    countBarrier(1);
    const auto update = context.Resolved(&DeviceFunctions::cmdUpdateBuffer, "vkCmdUpdateBuffer");
    for (std::uint32_t record = 0; record < args->count; ++record) {
        const VkDeviceSize base = static_cast<VkDeviceSize>(record) * args->stride;
        if (args->vertexRule == Rule::Constant) update(commands, scratch->Handle(), base + args->VertexDwordOffset(), 4, &args->vertexConstant);
        if (args->instanceRule == Rule::Constant) update(commands, scratch->Handle(), base + args->InstanceDwordOffset(), 4, &args->instanceConstant);
    }
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
    countBarrier(1);
    argumentBuffer = scratch->Handle();
    argumentOffset = 0;
    return true;
}

void recordCaptureBarrier(const Context& context, VkCommandBuffer commands) {
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
}

std::shared_ptr<Buffer> meshArgumentBuffer(const Context& context, std::span<const std::uint32_t> restartTable) {
    const auto tableBytes = restartTable.empty() ? 0u : sizeof(std::uint32_t) * (restartTable.size() + 1u);
    auto arguments = std::make_shared<Buffer>(context, ShaderRecompiler::MeshArgumentBytes + tableBytes, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    if (!restartTable.empty()) {
        Require(restartTable.size() <= std::numeric_limits<std::uint32_t>::max(), "mesh restart table is too large");
        const auto length = static_cast<std::uint32_t>(restartTable.size());
        auto* bytes = arguments->Bytes().data() + ShaderRecompiler::MeshArgumentBytes;
        std::memcpy(bytes, &length, sizeof(length));
        std::memcpy(bytes + sizeof(length), restartTable.data(), restartTable.size_bytes());
    }
    return arguments;
}

}
}
}
}
}
}
