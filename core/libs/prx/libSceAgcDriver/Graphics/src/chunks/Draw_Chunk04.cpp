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
DrawInputCopy CopyZeroPaddedDrawInput(const Context& context, std::uint64_t address, std::size_t bytes, std::size_t validBytes, std::size_t stride) {
    Require(validBytes <= bytes, "the valid bytes of a vertex fetch exceed the fetch");
    auto readable = validBytes;
    if (stride != 0 && !GuestMemory::Accessible(reinterpret_cast<const void*>(address), validBytes)) {
        const auto ranges = GuestMemory::CommittedRanges(address, validBytes);
        const std::size_t prefix = !ranges.empty() && ranges.front().first == address ? static_cast<std::size_t>(ranges.front().second - address) : 0;
        if (validBytes - prefix < stride) readable = prefix;
    }
    if (readable != 0) GuestMemory::FlushGpuWrites(address, readable);
    DrawInputCopy copy;
    auto* recorder = context.recorder;
    const bool keep = recorder != nullptr && readable != 0 && readable <= std::numeric_limits<std::uint32_t>::max();
    if (keep) {
        copy.registryGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        copy.generation = GuestMemory::CollectWrites(address, readable);
        std::uint32_t keptReadable = 0;
        if (copy.generation != 0) copy.buffer = recorder->ReusableDrawSnapshot(address, bytes, Recorder::SnapshotUse::PaddedVertex, &keptReadable);
        if (copy.buffer != nullptr && keptReadable == readable) {
            copy.reused = true;
            return copy;
        }
        copy.buffer = nullptr;
    }
    copy.buffer = std::make_shared<Buffer>(context, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto span = copy.buffer->Bytes();
    GuestMemory::Read(address, span.subspan(0, readable), 1);
    std::fill(span.begin() + static_cast<std::ptrdiff_t>(readable), span.end(), std::byte{0});
    if (keep) recorder->KeepDrawSnapshot(address, bytes, copy.generation, copy.registryGeneration, copy.buffer, Recorder::SnapshotUse::PaddedVertex, static_cast<std::uint32_t>(readable));
    return copy;
}

void KeepDrawInput(Recorder* recorder, std::uint64_t address, const DrawInputCopy& copy, Recorder::SnapshotUse use, std::uint32_t derived, std::uint32_t lowest) {
    if (recorder == nullptr || copy.reused || copy.buffer == nullptr) return;
    if (copy.generation == 0) {
        if (!copy.owned.words.empty() && !RuntimeSwitches::Set(RuntimeSwitch::NoOwnedDrawInputs)) recorder->KeepOwnedDrawInput(address, copy.buffer->Bytes().size(), copy.owned, copy.buffer, use, derived, copy.from, lowest);
        return;
    }
    recorder->KeepDrawSnapshot(address, copy.buffer->Bytes().size(), copy.generation, copy.registryGeneration, copy.buffer, use, derived, copy.from, lowest);
}

const HostImport* InPlaceDrawInput(const Context& context, std::uint64_t address, std::size_t bytes, std::size_t alignment, bool* behindGpuWrite, std::size_t fetched, std::uint64_t reads) {
    auto* recorder = context.recorder;
    static const bool copyBehindGpuWrite = std::getenv("APS5_NO_BEHIND_INPUTS_IN_PLACE") != nullptr || std::getenv("APS5_NO_DRAW_INPUTS_IN_PLACE") != nullptr;
    const bool small = std::min(fetched, bytes) <= context.drawInputsInPlaceBelow && reads <= InPlaceDrawInputMaxReads;
    if (recorder == nullptr || bytes == 0 || (!context.drawInputsInPlace && !small && (behindGpuWrite == nullptr || copyBehindGpuWrite))) return nullptr;
    bool behind = false;
    if (behindGpuWrite == nullptr) {
        GuestMemory::FlushGpuWrites(address, bytes);
        if (recorder->PendingWriteOverlaps(address, bytes)) return nullptr;
    } else {
        StorageTexture::FlushPending(address, bytes, nullptr, "in-place vertex input");
        behind = recorder->PendingWriteOverlaps(address, bytes);
        if (!behind && !context.drawInputsInPlace && !small) return nullptr;
    }
    if (recorder->PendingLabelIn(address, bytes) || recorder->QueuedStoreOverlaps(address, bytes)) return nullptr;
    const auto overlaps = [&](const auto& writer) { return writer->WritesOverlap(address, bytes); };
    if (context.copiedWriters != nullptr && std::any_of(context.copiedWriters->begin(), context.copiedWriters->end(), overlaps)) return nullptr;
    const auto drawWriters = DrawCopiedWriters();
    if (std::any_of(drawWriters->begin(), drawWriters->end(), overlaps)) return nullptr;
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr || HostImportOffset(*import, address) % alignment != 0) return nullptr;
    recorder->NotePendingRead(address, bytes, Recorder::ReadKind::DrawInput);
    if (behindGpuWrite != nullptr && behind) *behindGpuWrite = true;
    return import;
}

InPlaceIndices InPlaceIndexRange(Recorder& recorder, std::uint64_t address, std::size_t bytes, std::uint32_t indexSize, bool skipRestart) {
    const std::span<const std::byte> range(reinterpret_cast<const std::byte*>(address), bytes);
    InPlaceIndices indices;
    const auto scan = [&] {
        const auto bounds = DrawIndexRange(range, indexSize, skipRestart);
        if (!bounds) return;
        indices.highest = bounds->highest;
        indices.lowest = bounds->lowest;
    };
    if (bytes < Recorder::KeptIndexRangeIndices * indexSize) {
        scan();
        return indices;
    }
    const auto use = indexSize == 2 ? (skipRestart ? Recorder::SnapshotUse::Index16Restart : Recorder::SnapshotUse::Index16) : (skipRestart ? Recorder::SnapshotUse::Index32Restart : Recorder::SnapshotUse::Index32);
    const auto registryGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    const auto generation = GuestMemory::CollectWrites(address, bytes);
    if (generation != 0 && recorder.KnownHighestIndex(address, bytes, use, indices.highest, &indices.lowest)) {
        indices.reused = true;
        return indices;
    }
    scan();
    recorder.KeepHighestIndex(address, bytes, use, generation, registryGeneration, indices.highest, indices.lowest);
    return indices;
}

namespace {

struct DrawInputs {
    bool nothing = false;
    std::uint64_t indexBytes = 0;
    std::shared_ptr<Buffer> indices;
    VkBuffer indexHandle = VK_NULL_HANDLE;
    VkDeviceSize indexOffset = 0;
    std::uint32_t maxIndex = 0;
    std::uint32_t minIndex = 0;
    std::optional<std::uint32_t> indexCount;
    std::uint32_t listRestartPrimitive = 0;
    VertexInputLayout vertexInput;
    std::shared_ptr<const VertexInputLayout> retainedVertexInput;
    std::vector<std::shared_ptr<Buffer>> vertexBuffers;
    std::vector<VkBuffer> vertexHandles;
    std::vector<VkDeviceSize> vertexOffsets;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> inPlaceReads;
    bool vertexInputBehindGpuWrite = false;
    std::uint32_t fragmentOutputs = 0;
    VkPipelineStageFlags shaderStages = 0;
    std::uint32_t meshGroups = 0;
    std::uint32_t meshLayers = 1;
    std::uint32_t meshLayerStride = 0;
    std::vector<std::uint32_t> restartTable;
    std::uint32_t expandedIndexCount = 0;
    const VertexInputLayout& VertexLayout() const { return retainedVertexInput != nullptr ? *retainedVertexInput : vertexInput; }
};

bool StripPatches(const State& state, std::span<const CompiledShader> shaders) {
    return state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP && BarycentricPatches(state, shaders);
}

std::shared_ptr<Buffer> zeroVertexBuffer(const Context& context, std::size_t bytes) {
    auto zero = std::make_shared<Buffer>(context, bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    std::fill(zero->Bytes().begin(), zero->Bytes().end(), std::byte{0});
    return zero;
}

struct DrawPlan {
    bool nothing = false;
    std::uint64_t indexBytes = 0;
    std::uint32_t fragmentOutputs = 0;
    VkPipelineStageFlags shaderStages = 0;
    std::uint32_t meshGroups = 0;
    std::uint32_t meshLayers = 1;
    std::uint32_t meshLayerStride = 0;
    VertexInputLayout vertexInput;
    std::shared_ptr<const VertexInputLayout> retainedVertexInput;
    bool candidate = false;
    bool keyed = false;
    ResourceCache::Key contentKey;
    const VertexInputLayout& VertexLayout() const { return retainedVertexInput != nullptr ? *retainedVertexInput : vertexInput; }
};

bool EarlyDrawKeys() {
    static const bool enabled = std::getenv("APS5_NO_EARLY_DRAW_KEY") == nullptr;
    return enabled;
}

bool KeyForDevice(const std::vector<std::uint32_t>& key, VkDevice device) {
    const auto handle = reinterpret_cast<std::uint64_t>(device);
    return key.size() >= 3 && key[0] == 0xffffffffu && key[1] == static_cast<std::uint32_t>(handle) && key[2] == static_cast<std::uint32_t>(handle >> 32u);
}

DrawPlan planDraw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, const DrawRecipe* recipe, DrawOutcome& outcome, DrawTimer& timer, const std::vector<std::uint32_t>* earlyKey = nullptr) {
    DrawPlan plan;
    APS5_LOG_OUT_DEBUG("Draw indices=%u instances=%u indexSize=%u flags=%u indexAddress=0x%llx", draw.indexCount, draw.instanceCount, draw.indexSize, draw.flags, static_cast<unsigned long long>(draw.indexAddress));
    APS5_LOG_OUT_DEBUG("State colorTarget=%u render=%ux%u colorAddress=0x%llx colorBytes=%llu colorExtent=%ux%u", state.hasColorTarget ? 1u : 0u, state.renderExtent.width, state.renderExtent.height, static_cast<unsigned long long>(state.color.address), static_cast<unsigned long long>(state.color.bytes), state.color.extent.width, state.color.extent.height);
    APS5_LOG_OUT_DEBUG("Viewport x=%f y=%f w=%f h=%f minDepth=%f maxDepth=%f", state.viewport.x, state.viewport.y, state.viewport.width, state.viewport.height, state.viewport.minDepth, state.viewport.maxDepth);
    APS5_LOG_OUT_DEBUG("Scissor x=%d y=%d w=%u h=%u topology=%u cullMode=0x%x frontFace=%u", state.scissor.offset.x, state.scissor.offset.y, state.scissor.extent.width, state.scissor.extent.height, static_cast<unsigned>(state.topology), static_cast<unsigned>(state.cullMode), static_cast<unsigned>(state.frontFace));
    const auto* args = draw.indirect ? &*draw.indirect : nullptr;
    Require(draw.indexed ? draw.flags == 0 : (draw.flags & ~0x20u) == 0, "draw modifiers are unsupported");
    if (draw.indexed) {
        Require(draw.indexSize == 2 || draw.indexSize == 4, "only uint16 and uint32 index buffers are supported");
    } else {
        Require(draw.indexAddress == 0 && draw.indexSize == 0, "auto draw must not reference an index buffer");
        if (args == nullptr) {
            if (draw.indexCount == 0 || draw.instanceCount == 0) {
                plan.nothing = true;
                return plan;
            }
            Require(draw.firstVertex <= std::numeric_limits<std::uint32_t>::max() - (draw.indexCount - 1u), "auto draw vertex range overflow");
            Require(draw.firstInstance <= std::numeric_limits<std::uint32_t>::max() - (draw.instanceCount - 1u), "auto draw instance range overflow");
        }
    }
    if (args == nullptr) {
        if (draw.indexCount == 0 || draw.instanceCount == 0) {
            plan.nothing = true;
            return plan;
        }
    } else Require((!state.stages.mesh || draw.indexed) && !state.stages.tessellation && !state.quadList, "indirect draw on a non-vertex path must be resolved by the driver");
    plan.indexBytes = static_cast<std::uint64_t>(draw.indexCount) * draw.indexSize;
    const auto indexBytes = plan.indexBytes;
    APS5_LOG_OUT_DEBUG("Index buffer bytes=%llu", static_cast<unsigned long long>(indexBytes));
    Require(indexBytes <= std::numeric_limits<std::size_t>::max(), "index buffer size overflow");
    Require(!draw.indexed || !state.hasColorTarget || draw.indexAddress + indexBytes <= state.color.MemoryAddress() || state.color.MemoryAddress() + state.color.bytes <= draw.indexAddress, "index buffer aliases the render target");
    APS5_LOG_CHARS_OUT_DEBUG("ValidateShaders");
    if (recipe != nullptr) {
        plan.fragmentOutputs = recipe->fragmentOutputs;
        plan.shaderStages = recipe->shaderStages;
    } else {
        plan.fragmentOutputs = CachedFragmentOutputs(context, shaders, state, outcome.validateMemoized, outcome.validateHit);
        plan.shaderStages = PipelineStages(shaders);
    }
    APS5_LOG_CHARS_OUT_DEBUG("ValidateShaders OK");
    APS5_LOG_OUT_DEBUG("PipelineStages=0x%x", static_cast<unsigned>(plan.shaderStages));
    if (state.stages.mesh) {
        APS5_LOG_CHARS_OUT_DEBUG("Mesh path");
        Require(context.meshShader, "device does not support mesh shaders");
        const auto& mesh = *state.stages.mesh;
        const auto inputSize = mesh.inputPrimitive == 1 ? 1u : mesh.inputPrimitive == 2 ? 2u : 3u;
        Require(mesh.primitivesPerGroup != 0, "mesh pipeline groups no primitive (zero primitives per group)");
        Require(((state.stages.registerValue >> 19u) & 3u) == 0 || !draw.indexed, "indexed fast-launch draws are unsupported");
        if (args == nullptr && draw.indexCount < inputSize) {
            plan.nothing = true;
            return plan;
        }
        if (args == nullptr) {
            const auto step = mesh.inputPrimitive == 5 || mesh.inputPrimitive == 6 ? 1u : inputSize;
            const auto primitives = (draw.indexCount - inputSize) / step + 1u;
            const auto groups = (primitives - 1u) / mesh.primitivesPerGroup + 1u;
            const auto grid = MeshGridFor(context, groups, draw.instanceCount);
            Require(grid.has_value(), "mesh draw exceeds workgroup count limits");
            APS5_LOG_OUT_DEBUG("Mesh primitives=%u groups=%u as %u x %u", primitives, groups, grid->groups, grid->layers);
            plan.meshGroups = grid->groups;
            plan.meshLayers = grid->layers;
            plan.meshLayerStride = grid->layerStride;
        }
    }
    if (state.stages.tessellation) Require(draw.indexCount % state.stages.tessellation->inputControlPoints == 0, "incomplete tessellation patch");
    ValidateViewport(context, state.viewport);
    for (const auto& viewport : state.extraViewports) ValidateViewport(context, viewport);
    ValidateDepthBounds(context, state);
    ValidateSampleMask(context, state, shaders);
    timer.phase(PhaseValidate);
    const auto layoutStart = timer.profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto& attributes = shaders.front().program->vertexAttributes;
    if (recipe != nullptr) {
        plan.vertexInput = recipe->vertexInput;
    } else if (!attributes.empty()) {
        if (context.vertexInputs != nullptr) plan.retainedVertexInput = context.vertexInputs->TryGet(shaders.front().program->variantId, attributes);
        if (plan.retainedVertexInput == nullptr) plan.vertexInput = BuildVertexInputLayout(context, attributes);
    }
    timer.charge(PhaseVertex, layoutStart);
    static const bool noDrawResourceCache = std::getenv("APS5_NO_DRAW_RESOURCE_CACHE") != nullptr;
    static const bool noTextureCache = std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    static const bool trimKey = std::getenv("APS5_NO_DRAW_KEY_TRIM") == nullptr;
    plan.candidate = recipe == nullptr && !noDrawResourceCache && !noTextureCache && std::all_of(shaders.begin(), shaders.end(), [](const CompiledShader& shader) { return shader.program != nullptr && shader.program->PipelineVariantId() != 0; });
    plan.keyed = plan.candidate && !ShaderResources::NeverReusable(shaders);
    if (plan.keyed) {
        const auto keyStart = timer.profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        if (earlyKey != nullptr && KeyForDevice(*earlyKey, context.device)) {
            plan.contentKey = *earlyKey;
            static const bool verify = std::getenv("APS5_VERIFY_EARLY_DRAW_KEY") != nullptr;
            if (verify) Require(plan.contentKey == DrawResourceKey(context, shaders, state.color, static_cast<std::uint32_t>(state.colors.size()), draw.indexAddress, indexBytes, !trimKey), "the draw resource key made before the commit differs from the commit's");
        } else {
            plan.contentKey = DrawResourceKey(context, shaders, state.color, static_cast<std::uint32_t>(state.colors.size()), draw.indexAddress, indexBytes, !trimKey);
        }
        timer.charge(PhaseLookup, keyStart);
    }
    return plan;
}

bool VerifyDrawPlan() {
    static const bool verify = std::getenv("APS5_VERIFY_DRAW_PLAN") != nullptr;
    return verify;
}

void requireDrawPlanRepeats(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, const DrawPlan& plan) {
    DrawOutcome scratch;
    DrawTimer scratchTimer(false);
    const auto again = planDraw(context, state, draw, shaders, nullptr, scratch, scratchTimer);
    const auto sameBinding = [](const VkVertexInputBindingDescription& a, const VkVertexInputBindingDescription& b) { return a.binding == b.binding && a.stride == b.stride && a.inputRate == b.inputRate; };
    const auto sameAttribute = [](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b) { return a.location == b.location && a.binding == b.binding && a.format == b.format && a.offset == b.offset; };
    const auto& layout = plan.VertexLayout();
    const auto& repeated = again.VertexLayout();
    Require(again.nothing == plan.nothing && again.indexBytes == plan.indexBytes && again.fragmentOutputs == plan.fragmentOutputs && again.shaderStages == plan.shaderStages && again.meshGroups == plan.meshGroups && again.meshLayers == plan.meshLayers && again.meshLayerStride == plan.meshLayerStride && again.candidate == plan.candidate && again.keyed == plan.keyed && again.contentKey == plan.contentKey && std::ranges::equal(repeated.bindings, layout.bindings, sameBinding) && std::ranges::equal(repeated.attributes, layout.attributes, sameAttribute), "APS5_VERIFY_DRAW_PLAN: the plan built after the draw's record-time work differs from the plan the draw used");
}

DrawInputs prepareDrawInputs(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, const DrawPlan& drawPlan, DrawTimer& timer, PerformanceTimer& timing) {
    DrawInputs inputs;
    const auto reads = draw.indirect ? std::numeric_limits<std::uint64_t>::max() : static_cast<std::uint64_t>(draw.indexCount) * draw.instanceCount;
    inputs.indexBytes = drawPlan.indexBytes;
    inputs.fragmentOutputs = drawPlan.fragmentOutputs;
    inputs.shaderStages = drawPlan.shaderStages;
    inputs.meshGroups = drawPlan.meshGroups;
    inputs.meshLayers = drawPlan.meshLayers;
    inputs.meshLayerStride = drawPlan.meshLayerStride;
    inputs.vertexInput = drawPlan.vertexInput;
    inputs.retainedVertexInput = drawPlan.retainedVertexInput;
    const auto* args = draw.indirect ? &*draw.indirect : nullptr;
    const auto indexBytes = inputs.indexBytes;
    if (draw.indexed) GuestMemory::CheckRange(reinterpret_cast<const void*>(draw.indexAddress), static_cast<std::size_t>(indexBytes), draw.indexSize);
    APS5_LOG_CHARS_OUT_DEBUG("Index buffer range OK");
    if (draw.indexed) RequireNoMultisampleTarget(draw.indexAddress, static_cast<std::size_t>(indexBytes), "an index buffer read");
    if (state.pixelKill && !state.pixelEarlyZ && std::ranges::any_of(state.colors, [](const ColorTarget& color) { return color.partialWriteMask != 0; })) {
        const auto* recorder = Recorder::Active();
        Require(recorder == nullptr || !recorder->CountingSamples(), "a partial write of a packed color target merges inside the pixel shader after early depth and stencil tests, which a shader that kills pixels under late tests cannot use while occlusion queries count samples: the early tests would count the pixels it kills");
    }
    timer.phase(PhaseValidate);
    timing.Mark("validate");
    inputs.maxIndex = draw.indexed ? 0u : draw.firstVertex + draw.indexCount - 1u;
    inputs.minIndex = draw.indexed ? 0u : draw.firstVertex;
    if (state.quadList || StripPatches(state, shaders)) {
        Require(!draw.indirect, state.quadList ? "indirect quad-list draw is unsupported" : "an indirect triangle strip through emulated barycentric patches is unsupported");
        Require(state.quadList || !draw.indexed || !state.primitiveRestart, "an indexed triangle strip with primitive restart through emulated barycentric patches is unsupported");
        const std::uint32_t primitives = state.quadList ? draw.indexCount / 4u : draw.indexCount >= 3u ? draw.indexCount - 2u : 0u;
        if (primitives == 0u || draw.instanceCount == 0u) {
            inputs.nothing = true;
            return inputs;
        }
        const std::uint32_t corners = state.quadList ? 6u : 3u;
        Require(primitives <= std::numeric_limits<std::uint32_t>::max() / corners, "expanded index count overflow");
        const std::uint32_t outputIndexCount = primitives * corners;
        const std::size_t expandedIndexBytes = static_cast<std::size_t>(outputIndexCount) * sizeof(std::uint32_t);
        auto indexCopy = std::make_shared<Buffer>(context, expandedIndexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        auto* outIndices = reinterpret_cast<std::uint32_t*>(indexCopy->Bytes().data());
        std::uint32_t maxIdx = 0;
        const bool lastProvoking = state.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT;
        const auto expand = [&](const auto& source) {
            for (std::uint32_t primitive = 0; primitive < primitives; ++primitive) {
                auto* out = outIndices + static_cast<std::size_t>(primitive) * corners;
                if (state.quadList) {
                    const auto i0 = source(primitive * 4u + 0u);
                    const auto i1 = source(primitive * 4u + 1u);
                    const auto i2 = source(primitive * 4u + 2u);
                    const auto i3 = source(primitive * 4u + 3u);
                    if (lastProvoking) {
                        out[0] = i0;
                        out[1] = i1;
                        out[2] = i3;
                        out[3] = i1;
                        out[4] = i2;
                        out[5] = i3;
                    } else {
                        out[0] = i0;
                        out[1] = i1;
                        out[2] = i2;
                        out[3] = i0;
                        out[4] = i2;
                        out[5] = i3;
                    }
                } else {
                    const std::uint32_t odd = primitive & 1u;
                    if (lastProvoking) {
                        out[0] = source(primitive + odd);
                        out[1] = source(primitive + 1u - odd);
                        out[2] = source(primitive + 2u);
                    } else {
                        out[0] = source(primitive);
                        out[1] = source(primitive + 1u + odd);
                        out[2] = source(primitive + 2u - odd);
                    }
                }
                for (std::uint32_t corner = 0; corner < corners; ++corner) maxIdx = std::max(maxIdx, out[corner]);
            }
        };
        if (draw.indexed) {
            GuestMemory::FlushGpuWrites(draw.indexAddress, static_cast<std::size_t>(indexBytes));
            GuestMemory::CheckRange(reinterpret_cast<const void*>(draw.indexAddress), static_cast<std::size_t>(indexBytes), draw.indexSize);
            const auto readIndex = [&](std::uint32_t idx) -> std::uint32_t {
                if (draw.indexSize == 2) {
                    std::uint16_t v = 0;
                    GuestMemory::Read(draw.indexAddress + static_cast<std::uint64_t>(idx) * 2u, std::as_writable_bytes(std::span(&v, 1)), 2);
                    return v;
                } else {
                    std::uint32_t v = 0;
                    GuestMemory::Read(draw.indexAddress + static_cast<std::uint64_t>(idx) * 4u, std::as_writable_bytes(std::span(&v, 1)), 4);
                    return v;
                }
            };
            expand(readIndex);
            Require(draw.firstVertex <= std::numeric_limits<std::uint32_t>::max() - maxIdx, "indexed draw vertex range overflow");
        } else {
            const std::uint32_t lastVertex = state.quadList ? primitives * 4u - 1u : primitives + 1u;
            Require(draw.firstVertex <= std::numeric_limits<std::uint32_t>::max() - lastVertex, "auto draw vertex range overflow");
            expand([](std::uint32_t idx) { return idx; });
        }
        inputs.maxIndex = draw.indexed ? maxIdx : maxIdx + draw.firstVertex;
        Require(inputs.maxIndex <= context.limits.maxDrawIndexedIndexValue, "index exceeds the device's indexed draw limit");
        inputs.indexHandle = indexCopy->Handle();
        inputs.indexOffset = 0;
        inputs.expandedIndexCount = outputIndexCount;
        inputs.indices = std::move(indexCopy);
    } else if (draw.indexed && state.primitiveRestart && !context.primitiveListRestart && !state.stages.mesh && (state.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)) {
        if (indexBytes == 0) {
            inputs.nothing = true;
            return inputs;
        }
        GuestMemory::FlushGpuWrites(draw.indexAddress, static_cast<std::size_t>(indexBytes));
        auto compacted = std::make_shared<Buffer>(context, static_cast<std::size_t>(indexBytes), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        const auto bytes = compacted->Bytes().first(static_cast<std::size_t>(indexBytes));
        GuestMemory::Read(draw.indexAddress, bytes, draw.indexSize);
        const auto primitiveSize = state.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST ? 1u : state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST ? 2u : 3u;
        const auto count = draw.indirect ? draw.indexCount : CompactListRestart(bytes, draw.indexSize, primitiveSize);
        const auto kept = bytes.first(static_cast<std::size_t>(count) * draw.indexSize);
        const bool restartLeft = draw.indirect.has_value();
        auto bounds = DrawIndexRange(kept, draw.indexSize, restartLeft);
        constexpr std::uint32_t ZeroFetchSlack = 0x10000u;
        if (const auto zeroFrom = bounds && !shaders.front().program->vertexAttributes.empty() ? ZeroFetchVertex(shaders.front().program->vertexAttributes) : std::nullopt) {
            const auto limit = *zeroFrom > draw.firstVertex ? *zeroFrom - draw.firstVertex : 0u;
            if (bounds->highest >= limit && bounds->highest - limit >= ZeroFetchSlack) bounds = ClampDrawIndices(kept, draw.indexSize, restartLeft, limit);
        }
        if (!bounds) {
            inputs.nothing = true;
            return inputs;
        }
        Require(bounds->highest <= context.limits.maxDrawIndexedIndexValue, "index exceeds the device's indexed draw limit");
        inputs.maxIndex = bounds->highest;
        inputs.minIndex = bounds->lowest;
        inputs.indexCount = count;
        inputs.listRestartPrimitive = primitiveSize;
        inputs.indices = std::move(compacted);
        inputs.indexHandle = inputs.indices->Handle();
        inputs.indexOffset = 0;
    } else if (draw.indexed) {
        const bool listTopology = state.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        const bool skipRestart = state.primitiveRestart && (!listTopology || context.primitiveListRestart || state.stages.mesh);
        const auto use = draw.indexSize == 2 ? (skipRestart ? Recorder::SnapshotUse::Index16Restart : Recorder::SnapshotUse::Index16) : (skipRestart ? Recorder::SnapshotUse::Index32Restart : Recorder::SnapshotUse::Index32);
        const bool restartTable = skipRestart && state.stages.mesh && (state.stages.mesh->inputPrimitive == 5 || state.stages.mesh->inputPrimitive == 6);
        const auto* import = InPlaceDrawInput(context, draw.indexAddress, static_cast<std::size_t>(indexBytes), draw.indexSize, nullptr, static_cast<std::size_t>(indexBytes), reads);
        DrawInputCopy copy;
        if (import == nullptr) copy = CopyDrawInput(context, context.recorder, draw.indexAddress, static_cast<std::size_t>(indexBytes), draw.indexSize, use, 0, !BindsIndexCopy(state));
        else GuestMemory::CheckRange(reinterpret_cast<const void*>(draw.indexAddress), static_cast<std::size_t>(indexBytes), draw.indexSize);
        std::optional<std::uint32_t> highest;
        std::uint32_t lowest = 0;
        if (import != nullptr) {
            const auto range = InPlaceIndexRange(*context.recorder, draw.indexAddress, static_cast<std::size_t>(indexBytes), draw.indexSize, skipRestart);
            highest = range.highest;
            lowest = range.lowest;
        } else if (!copy.reused) {
            if (const auto bounds = DrawIndexRange(copy.buffer->Bytes().first(static_cast<std::size_t>(indexBytes)), draw.indexSize, skipRestart)) {
                highest = bounds->highest;
                lowest = bounds->lowest;
            }
            KeepDrawInput(context.recorder, draw.indexAddress, copy, use, skipRestart ? (highest ? *highest + 1u : 0u) : highest.value_or(0u), lowest);
        } else {
            lowest = copy.lowest;
            if (!skipRestart) highest = copy.derived;
            else if (copy.derived != 0) highest = copy.derived - 1u;
        }
        if (!highest) {
            inputs.nothing = true;
            return inputs;
        }
        constexpr std::uint32_t ZeroFetchSlack = 0x10000u;
        if (const auto zeroFrom = state.stages.mesh || shaders.front().program->vertexAttributes.empty() ? std::nullopt : ZeroFetchVertex(shaders.front().program->vertexAttributes)) {
            const auto limit = *zeroFrom > draw.firstVertex ? *zeroFrom - draw.firstVertex : 0u;
            if (*highest >= limit && *highest - limit >= ZeroFetchSlack) {
                GuestMemory::FlushGpuWrites(draw.indexAddress, static_cast<std::size_t>(indexBytes));
                auto clamped = std::make_shared<Buffer>(context, static_cast<std::size_t>(indexBytes), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
                const auto bytes = clamped->Bytes().first(static_cast<std::size_t>(indexBytes));
                GuestMemory::Read(draw.indexAddress, bytes, draw.indexSize);
                const auto clampedBounds = ClampDrawIndices(bytes, draw.indexSize, skipRestart, limit);
                import = nullptr;
                copy = DrawInputCopy{};
                copy.buffer = std::move(clamped);
                if (!clampedBounds) {
                    inputs.nothing = true;
                    return inputs;
                }
                highest = clampedBounds->highest;
                lowest = clampedBounds->lowest;
            }
        }
        Require(*highest <= context.limits.maxDrawIndexedIndexValue, "index exceeds the device's indexed draw limit");
        inputs.maxIndex = *highest;
        inputs.minIndex = lowest;
        if (restartTable) {
            const auto indexSpan = import != nullptr ? std::span<const std::byte>(reinterpret_cast<const std::byte*>(draw.indexAddress), static_cast<std::size_t>(indexBytes)) : copy.buffer->Bytes().first(static_cast<std::size_t>(indexBytes));
            inputs.restartTable = MeshRestartTable(indexSpan, draw.indexSize);
        }
        if (import != nullptr) {
            inputs.indexHandle = import->buffer;
            inputs.indexOffset = HostImportOffset(*import, draw.indexAddress);
            inputs.inPlaceReads.emplace_back(draw.indexAddress, draw.indexAddress + indexBytes);
        } else {
            inputs.indices = std::move(copy.buffer);
            inputs.indexHandle = inputs.indices->Handle();
        }
    }
    APS5_LOG_CHARS_OUT_DEBUG("Index validation OK");
    timing.Mark("index_upload");
    const auto& attributes = shaders.front().program->vertexAttributes;
    inputs.vertexOffsets.assign(attributes.size(), 0);
    inputs.vertexHandles.reserve(attributes.size());
    if (draw.indexed) {
        Require(draw.firstVertex <= std::numeric_limits<std::uint32_t>::max() - inputs.maxIndex, "indexed draw vertex range overflow");
        inputs.maxIndex += draw.firstVertex;
        inputs.minIndex += draw.firstVertex;
    }
    const VertexPlanLease planLease;
    auto& fetches = planLease->fetches;
    auto& fetchValid = planLease->fetchValid;
    auto& fetchOf = planLease->fetchOf;
    auto& zeroed = planLease->zeroed;
    fetches.reserve(attributes.size());
    fetchValid.reserve(attributes.size());
    fetchOf.assign(attributes.size(), 0);
    zeroed.resize(attributes.size());
    const auto& layout = inputs.VertexLayout();
    for (std::size_t i = 0; i < attributes.size(); ++i) {
        const auto& attribute = attributes[i];
        const auto described = std::find_if(layout.attributes.begin(), layout.attributes.end(), [&](const auto& entry) { return entry.location == attribute.location; });
        Require(described != layout.attributes.end(), "vertex attribute missing from its input layout");
        const auto narrow = DecodeVertexFormat(attribute);
        const std::size_t widened = described->format == narrow.format ? 0u : VertexReadBytes(attribute, described->format) - narrow.bytes;
        if (NullVertexDescriptor(attribute) || VertexFetchOutOfRange(attribute)) {
            zeroed[i] = zeroVertexBuffer(context, narrow.bytes + widened);
            continue;
        }
        const auto bytes = (args != nullptr ? VertexBufferExtent(attribute) : VertexBufferReadSize(attribute, inputs.maxIndex, draw.instanceCount, draw.firstInstance)) + widened;
        const auto& fields = attribute.resource.fields;
        const auto address = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
        const auto firstByte = args == nullptr ? VertexBufferReadOffset(attribute, draw.firstVertex, draw.firstInstance) : 0;
        Require(!state.hasColorTarget || address + bytes <= state.color.MemoryAddress() || state.color.MemoryAddress() + state.color.bytes <= address + firstByte, "vertex buffer aliases the render target");
        const auto stride = (fields[1] >> 16u) & 0x3fffu;
        fetchOf[i] = fetches.size();
        fetches.push_back({address, address + bytes, stride, attribute.fetchIndex, DecodeVertexFormat(attribute).alignment});
        const auto recordBytes = static_cast<std::uint64_t>(fields[2]) * stride;
        fetchValid.push_back(args == nullptr && stride != 0 ? static_cast<std::size_t>(std::min(recordBytes, static_cast<std::uint64_t>(bytes))) : bytes);
    }
    const auto soloFetches = SoloZeroPaddedFetchIndices(fetches, fetchValid);
    auto& isSolo = planLease->isSolo;
    auto& plannedFetches = planLease->plannedFetches;
    auto& plannedOf = planLease->plannedOf;
    isSolo.assign(fetches.size(), false);
    for (const auto f : soloFetches) isSolo[f] = true;
    plannedFetches.reserve(fetches.size());
    plannedOf.assign(fetches.size(), 0);
    for (std::size_t f = 0; f < fetches.size(); ++f) {
        if (isSolo[f]) continue;
        plannedOf[f] = plannedFetches.size();
        plannedFetches.push_back(fetches[f]);
    }
    const auto plan = PlanVertexCopies(plannedFetches);
    const auto rangeFrom = args == nullptr && !state.stages.mesh ? VertexCopyStarts(plan, plannedFetches, inputs.minIndex, draw.firstInstance) : std::vector<std::size_t>(plan.copies.size(), 0);
    auto& rangeHandles = planLease->rangeHandles;
    auto& rangeOffsets = planLease->rangeOffsets;
    rangeHandles.reserve(plan.copies.size());
    rangeOffsets.reserve(plan.copies.size());
    for (std::size_t range = 0; range < plan.copies.size(); ++range) {
        const auto [begin, end] = plan.copies[range];
        const auto bytes = static_cast<std::size_t>(end - begin);
        GuestMemory::CheckRange(reinterpret_cast<const void*>(begin), bytes, 1);
        RequireNoMultisampleTarget(begin, bytes, "a vertex buffer read");
        if (const auto* import = InPlaceDrawInput(context, begin, bytes, plan.alignments[range], &inputs.vertexInputBehindGpuWrite, bytes - std::min(rangeFrom[range], bytes), reads)) {
            rangeHandles.push_back(import->buffer);
            rangeOffsets.push_back(HostImportOffset(*import, begin));
            inputs.inPlaceReads.emplace_back(begin, end);
            continue;
        }
        auto copy = CopyDrawInput(context, context.recorder, begin, bytes, 1, Recorder::SnapshotUse::Vertex, rangeFrom[range]);
        KeepDrawInput(context.recorder, begin, copy, Recorder::SnapshotUse::Vertex, 0);
        rangeHandles.push_back(copy.buffer->Handle());
        rangeOffsets.push_back(0);
        inputs.vertexBuffers.push_back(std::move(copy.buffer));
    }
    std::vector<VkBuffer> soloHandles(fetches.size(), VK_NULL_HANDLE);
    for (const auto f : soloFetches) {
        const auto begin = fetches[f].begin;
        const auto bytes = static_cast<std::size_t>(fetches[f].end - begin);
        auto copy = CopyZeroPaddedDrawInput(context, begin, bytes, fetchValid[f], fetches[f].stride);
        soloHandles[f] = copy.buffer->Handle();
        inputs.vertexBuffers.push_back(std::move(copy.buffer));
    }
    for (std::size_t i = 0; i < attributes.size(); ++i) {
        if (zeroed[i] != nullptr) {
            inputs.vertexHandles.push_back(zeroed[i]->Handle());
            inputs.vertexOffsets[i] = 0;
            inputs.vertexBuffers.push_back(std::move(zeroed[i]));
            continue;
        }
        const auto f = fetchOf[i];
        if (isSolo[f]) {
            inputs.vertexHandles.push_back(soloHandles[f]);
            inputs.vertexOffsets[i] = 0;
            continue;
        }
        const auto j = plannedOf[f];
        inputs.vertexHandles.push_back(rangeHandles[plan.copyOf[j]]);
        inputs.vertexOffsets[i] = rangeOffsets[plan.copyOf[j]] + plan.offsets[j];
    }
    timer.phase(PhaseVertex);
    timing.Mark("vertex_upload");
    return inputs;
}

}
}
}
}
}
}
