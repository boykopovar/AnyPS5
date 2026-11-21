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
namespace {
void rememberTargets(const State& state, std::span<const TargetBinding> targets, std::span<const VkImageView> views, const TargetProof& proof) {
    ++targetMemoBuilds;
    noteTargetMemo(TargetMemoEvent::Build);
    auto& colors = ThreadTargetMemo().colors;
    auto& depth = ThreadTargetMemo().depth;
    const bool enabled = TargetMemoEnabled() && proof.epoch != 0;
    colors.valid = enabled && !targets.empty() && std::ranges::all_of(targets, [&](const TargetBinding& binding) {
        return binding.resident != nullptr && binding.resident->Cached() && !binding.multisampled && binding.color.fmaskAddress == 0 && !colorOverlapsDepth(state, binding) && metadataProved(binding.color);
    });
    if (colors.valid) {
        colors.proof = proof;
        colors.clearOfDepth = depthFootprint(state.depth);
        colors.descriptors.assign(state.colors.begin(), state.colors.end());
        colors.attachments.resize(targets.size());
        for (std::size_t index = 0; index < targets.size(); ++index) colors.attachments[index] = {targets[index].resident, targets[index].proxied, views[index]};
    }
    depth.valid = enabled && (!state.depth || state.depth->samples == 1u);
    if (depth.valid) {
        depth.proof = proof;
        depth.descriptor = state.depth;
    }
}

bool repeatColors(const Context& context, const State& state, const TargetProof& proof, std::vector<TargetBinding>& targets, std::vector<VkImageView>& views) {
    auto& memo = ThreadTargetMemo().colors;
    thread_local std::uint32_t lookups = 0;
    if (targetMemoProfiled && (++lookups & 4095u) == 0u) reportTargetMemo();
    if (state.colors.empty()) return false;
    noteTargetMemo(TargetMemoEvent::Lookup);
    if (!memo.valid || !TargetMemoEnabled()) {
        noteTargetMemo(TargetMemoEvent::Invalid);
        return false;
    }
    if (!colorProofMatches(memo.proof, proof)) return false;
    if (memo.descriptors != state.colors) {
        noteTargetMemo(TargetMemoEvent::Descriptor);
        return false;
    }
    const auto footprint = depthFootprint(state.depth);
    const bool overlapKnown = footprint == memo.clearOfDepth && !RuntimeSwitches::Set(RuntimeSwitch::NoTargetMemoOverlapCache);
    for (std::size_t index = 0; index < targets.size(); ++index) {
        auto& binding = targets[index];
        binding.resident = memo.attachments[index].resident.lock();
        if (binding.resident == nullptr || !binding.resident->Cached()) {
            noteTargetMemo(binding.resident == nullptr ? TargetMemoEvent::ResidentGone : TargetMemoEvent::ResidentEvicted);
            return false;
        }
        binding.color = state.colors[index];
        if (VerifyTargetMemo()) Require(overlapKnown ? !colorOverlapsDepth(state, binding) : true, "APS5_VERIFY_TARGET_MEMO: a cached depth overlap differs from its computation");
        if (!overlapKnown && colorOverlapsDepth(state, binding)) {
            noteTargetMemo(TargetMemoEvent::DepthOverlap);
            return false;
        }
        binding.gpuTiling = binding.color.tileMode == ColorTileMode::RenderTarget && context.detiler != nullptr;
        binding.proxied = memo.attachments[index].proxied;
        views.push_back(memo.attachments[index].view);
    }
    noteTargetMemo(TargetMemoEvent::Hit);
    noteTargetMemo(overlapKnown ? TargetMemoEvent::OverlapCached : TargetMemoEvent::OverlapComputed);
    if (!overlapKnown) memo.clearOfDepth = footprint;
    return true;
}

VkImageView repeatDepth(const Context& context, const State& state, const TargetProof* proof) {
    const auto& memo = ThreadTargetMemo().depth;
    if (!state.depth || !memo.valid || !TargetMemoEnabled() || targetMemoOwner.load(std::memory_order_relaxed) != &ThreadTargetMemo() || memo.proof != (proof != nullptr ? *proof : currentTargetProof(context)) || !memo.descriptor) return VK_NULL_HANDLE;
    if (*memo.descriptor != *state.depth) return VK_NULL_HANDLE;
    return RepeatDepthSurfaceView(context, *state.depth, state.depthWrite, writesStencilTarget(state));
}

const char* targetObligation(const Context& context, const State& state, std::span<const TargetBinding> targets, bool repeatedDepth) {
    for (const auto& binding : targets) {
        const auto& color = binding.color;
        const auto layers = color.arrayLastSlice != 0 ? color.layers : 0u;
        if (const auto* owed = binding.resident->RefreshObligation(layers != 0 ? color.arraySlice : 0u, layers)) return owed;
        if (const auto* owed = DepthPlanesObligation(context.device, color.MemoryAddress(), color.bytes)) return owed;
        if (color.cmaskAddress == 0 || cmaskProved(color.cmaskAddress, color.cmaskBytes)) continue;
        std::vector<std::uint32_t> cleared;
        if (readCmask(color, CmaskLayout(color.extent.width, color.extent.height), cleared) != CmaskState::Expanded) return "a CMASK clear";
    }
    return repeatedDepth ? DepthTargetObligation(context, *state.depth, state.depthWrite) : nullptr;
}

bool prepareDraw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots, PreparedDraw& prepared) {
    auto*& recipeOut = prepared.recipeOut;
    auto& stencilReferences = prepared.stencilReferences;
    for (const auto& shader : shaders) {
        if (shader.program != nullptr && !shader.program->stencilReferences.empty()) {
            Require(shader.stage == ShaderRecompiler::ShaderStage::Fragment && stencilReferences.empty(), "stencil replay requires exactly one fragment stage");
            stencilReferences = shader.program->stencilReferences;
        }
    }
    if (!stencilReferences.empty()) {
        Require(!draw.indirect && !state.primitiveRestart && !state.rectList && !state.quadList && !state.stages.mesh && !state.stages.tessellation && state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, "stencil replay supports direct triangle lists without restart, mesh or tessellation");
        Require(!state.depthBiasPerFace && !ShadersWriteMemory(shaders), "stencil replay refuses face splits and shader memory writes");
        Require(std::ranges::all_of(shaders, [](const CompiledShader& shader) { return shader.program != nullptr && shader.program->stencilReplaySafe && (shader.stage == ShaderRecompiler::ShaderStage::Vertex || shader.stage == ShaderRecompiler::ShaderStage::Fragment); }), "stencil replay refuses stages with side effects, clocks, subgroup dependencies or primitive ID inputs");
        if (stencilReferences.size() == 256u) {
            static std::once_flag unbounded;
            std::call_once(unbounded, [] { APS5_LOG_CHARS_OUT("Stencil replay: a draw replays all 256 stencil reference values because its pixel shader's export is not bounded, recording 256 passes per triangle and instance"); });
        }
        for (const auto& shader : shaders) {
            if (shader.stage != ShaderRecompiler::ShaderStage::Vertex) continue;
            Require((shader.program->hostSubgroupSize == 1u || shader.program->hostSubgroupSize == context.subgroup.subgroupSize) && shader.program->spirv.size() >= 5u && shader.program->spirv[1] >= 0x00010000u && shader.program->spirv[1] < 0x00010600u, "stencil replay vertex subgroup contract differs from the physical device");
        }
        Require(static_cast<std::uint64_t>(draw.firstInstance) + draw.instanceCount <= 0x100000000ull && (draw.indexed || static_cast<std::uint64_t>(draw.firstVertex) + draw.indexCount <= 0x100000000ull), "stencil replay draw indices overflow");
        Require(state.samples == 1u && (!state.depth || HasStencil(state.depth->format)), "stencil replay requires single-sample rendering and an eight-bit stencil format");
        recipeOut = nullptr;
    }
    if (state.quadList || StripPatches(state, shaders)) recipeOut = nullptr;
    if (context.recorder != nullptr) context.recorder->BoundKeptBytes();
    auto& timing = prepared.timing.emplace("Graphics.Draw");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& timer = prepared.timer;
    timer = DrawTimer(profile);
    static const bool traceDraws = std::getenv("APS5_TRACE_DRAWS") != nullptr;
    auto& outcome = prepared.outcome;
    auto& built = prepared.built;
    const auto waitedBefore = prepared.waitedBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
    auto& ownWaitedMs = prepared.ownWaitedMs;
    const auto report = [&](const char* suffix) { reportDrawEnd(state, timer, built, outcome, waitedBefore, ownWaitedMs, suffix); };
    auto& inputs = prepared.inputs;
    const auto plan = planDraw(context, state, draw, shaders, nullptr, outcome, timer, prepared.contentKey);
    if (plan.nothing) return false;
    inputs = prepareDrawInputs(context, state, draw, shaders, plan, timer, timing);
    if (inputs.nothing) {
        if (VerifyDrawPlan()) requireDrawPlanRepeats(context, state, draw, shaders, plan);
        return false;
    }
    const auto* args = prepared.args = draw.indirect ? &*draw.indirect : nullptr;
    const auto indexBytes = inputs.indexBytes;
    const int dumpLimit = prepared.dumpLimit = DumpTargetLimit();
    auto& targets = prepared.targets;
    targets.resize(state.colors.size());
    auto& targetViews = prepared.targetViews;
    const bool verifyAttachments = prepared.passAttachments && VerifyPassAttachments() && memoAttachments(context, state, prepared, false);
    prepared.attachmentsFromMemo = prepared.passAttachments && !VerifyPassAttachments() && memoAttachments(context, state, prepared, true);
    const bool memoActive = TargetMemoEnabled();
    if (!memoActive) dropTargetMemo();
    const bool memoConsulted = memoActive && !prepared.attachmentsFromMemo;
    const bool memoServes = memoConsulted && (!verifyAttachments || VerifyTargetMemo());
    bool targetsUsed = false;
    TargetProof targetProof;
    TargetProof admissionProof;
    bool repeatedColors = false;
    if (memoConsulted) {
        static_cast<void>(ownsTargetMemo(ThreadTargetMemo()));
        targetProof = currentTargetProof(context);
        admissionProof = targetProof;
    }
    if (memoServes) {
        targetsUsed = true;
        repeatedColors = repeatColors(context, state, targetProof, targets, targetViews);
    }
    const auto prepareColors = [&] {
        if (targetsUsed) {
            targets.clear();
            targets.resize(state.colors.size());
        }
        targetsUsed = true;
        targetViews.clear();
        for (std::size_t index = 0; index < state.colors.size(); ++index) {
            auto& binding = targets[index];
            binding.color = state.colors[index];
            const auto& color = binding.color;
            binding.gpuTiling = color.tileMode == ColorTileMode::RenderTarget && context.detiler != nullptr;
            if (color.samples > 1) {
                binding.multisampled = true;
                timer.phase(PhaseSetup);
                if (color.staged) {
                    const auto packed = StagedMergeTarget(color);
                    binding.stagedTemporary = AcquireStagedTemporary(context, color.extent, color.samples);
                    PrepareMultisampleTargetForDraw_nid_no_patch(context, packed);
                    static_cast<void>(MultisampleTargetView(context, packed, binding.multisampleOwner, &binding.multisampleImage));
                    binding.stagedTarget = MultisampleTargetStorageView(context, TargetMergeSurface(packed));
                    Require(MultisampleTargetStorable(context, packed), "a partial write of a multisampled packed color target needs a multisample image of this form to allow storage");
                    targetViews.push_back(binding.stagedTemporary->View());
                    timer.phase(PhaseReadTarget);
                    continue;
                }
                PrepareMultisampleTargetForDraw_nid_no_patch(context, color);
                targetViews.push_back(MultisampleTargetView(context, color, binding.multisampleOwner, &binding.multisampleImage));
                timer.phase(PhaseReadTarget);
                continue;
            }
            APS5_LOG_OUT_DEBUG("Creating color target %zu address=0x%llx bytes=%llu extent=%ux%u", index, static_cast<unsigned long long>(color.address), static_cast<unsigned long long>(color.bytes), color.extent.width, color.extent.height);
            const ColorTargetLayout colorLayout(color.extent.width, color.extent.height, color.tileMode, color.elementBytes);
            static_cast<void>(FlushDepthPlanes(color.address, colorLayout.Bytes(), DepthPlaneScope::Whole));
            RetireDepthSurfaces(context.device, color.address, colorLayout.Bytes());
            ClaimMultisampleTargetMemory(color.address, colorLayout.Bytes());
            if (color.fmaskAddress != 0) GuardOneFragmentFmask(context, color);
            constexpr VkBufferUsageFlags copies = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            timer.phase(PhaseSetup);
            static const bool residentTargets = std::getenv("APS5_NO_RESIDENT_TARGETS") == nullptr;
            static const bool residentLinear = std::getenv("APS5_NO_RESIDENT_LINEAR") == nullptr;
            const bool linearResident = residentLinear && residentTargets && context.detiler != nullptr && color.tileMode == ColorTileMode::Linear && (ExistingHostImportFor(context, color.address, colorLayout.Bytes()) != nullptr || (GuestMemory::Watched(color.address, colorLayout.Bytes()) && HostImportFor(context, color.address, colorLayout.Bytes(), ImportRequester::ColorTarget) != nullptr));
            if ((binding.gpuTiling || ((!ColorTileModeIsLinear(color.tileMode) || linearResident) && context.detiler != nullptr)) && residentTargets) {
                binding.resident = refreshResidentTarget(context, state, color, outcome, profile, [&] {
                    if (color.arrayLastSlice != 0) {
                        Require(color.dccAddress == 0 && color.cmaskAddress == 0 && AttachmentProxyFormat(context, color.format) == VK_FORMAT_UNDEFINED, "a slice of a 2D array color target with DCC, CMASK or an attachment proxy has no resident array image");
                        const auto array = ArraySurfaceForTarget(color);
                        const auto sliceBytes = ComputeSurfaceSize(ComputeElementMipLayout(ColorTextureTileMode(color.tileMode), color.elementBytes, color.surfaceExtent.width, color.surfaceExtent.height, color.mipCount), 1);
                        const auto geometry = DescribeSurface(array);
                        for (std::uint32_t slice = 0; slice <= color.arrayLastSlice; ++slice) Require(geometry.GuestLayerOffset(slice) == slice * sliceBytes, "the array image's layer pitch differs from the color view's slice offset");
                        Require(color.surfaceAddress == color.arrayAddress + color.arraySlice * sliceBytes, "the array image's layer pitch differs from the color view's slice offset");
                        auto resident = CachedStorageLayers(context, array, color.arraySlice, color.layers);
                        Require(resident->Attachable(), "storage format cannot be a color attachment");
                        return resident;
                    }
                    const auto surface = SurfaceForTarget(color);
                    auto resident = RepeatStorageSurface(context, surface, shapeTarget(context, prepared.shape, index));
                    if (resident != nullptr && VerifyShapeMemo()) {
                        Require(CachedStorageSurface(context, surface) == resident, "APS5_VERIFY_SHAPE_MEMO: the shape memo's resident target differs from the storage cache's");
                        noteShapeMemoVerified(false);
                    }
                    if (resident == nullptr) resident = CachedStorageSurface(context, surface);
                    Require(resident->Attachable(), "storage format cannot be a color attachment");
                    Require(color.volume || color.mipCount > 1 || color.depth > 1 || resident->GuestBytes() == colorLayout.Bytes(), "resident image layout differs from the color layout");
                    return resident;
                });
            }
            if (binding.resident != nullptr) {
                timer.phase(PhaseReadTarget);
                binding.proxied = AttachmentProxyFormat(context, color.format) != VK_FORMAT_UNDEFINED;
                targetViews.push_back(binding.proxied ? binding.resident->AttachmentProxyView() : binding.resident->AttachmentView(color.format, color.mip, color.depthSlice, color.layers, color.arraySlice));
                continue;
            }
            materializeCmaskClear(context, color, nullptr);
            Require(!color.mipTail || binding.gpuTiling, "rendering into a packed mip tail requires the GPU tiled fallback");
            Require(color.depth == 1 && (!color.volume || (color.mipCount == 1 && color.tileMode == ColorTileMode::Linear)), "rendering into a 3D color target needs the resident image of its surface");
            Require(color.layers == 1, "layered rendering into a 2D array color target needs the resident image of its array");
            Require(color.arraySlice == 0 || !ColorTileModeIsXor(color.tileMode), "rendering into a slice of a 2D array color target with an XOR tile mode needs the resident image of its array");
            if (binding.gpuTiling) {
                binding.mip = color.mipTail ? ComputeColorTargetMipLayout(color.tileMode, color.elementBytes, color.surfaceExtent.width, color.surfaceExtent.height, color.mipCount, color.mip) : ColorTargetMip(color, colorLayout);
                binding.tiled = std::make_unique<Buffer>(context, colorLayout.Bytes(), copies);
                binding.tiledDevice = std::make_unique<DeviceBuffer>(context, colorLayout.Bytes(), copies | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                binding.linearDevice = std::make_unique<DeviceBuffer>(context, colorLayout.LinearBytes(), copies | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                binding.original.resize(colorLayout.Bytes());
                GuestMemory::Read(color.address, binding.original, colorLayout.Alignment());
                std::memcpy(binding.tiled->Bytes().data(), binding.original.data(), binding.original.size());
            } else {
                const auto x0 = static_cast<std::uint32_t>(std::max(state.scissor.offset.x, 0));
                const auto y0 = static_cast<std::uint32_t>(std::max(state.scissor.offset.y, 0));
                const auto x1 = std::min<std::uint32_t>(x0 + state.scissor.extent.width, color.extent.width);
                const auto y1 = std::min<std::uint32_t>(y0 + state.scissor.extent.height, color.extent.height);
                binding.banded = color.tileMode == ColorTileMode::Linear && color.dccAddress == 0 && x0 < x1 && y0 < y1 && (x1 - x0 < color.extent.width || y1 - y0 < color.extent.height);
                if (binding.banded) {
                    binding.band = {{static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0)}, {x1 - x0, y1 - y0}};
                    binding.bandOffset = colorLayout.Offset(x0, y0);
                    binding.bandPitch = static_cast<std::uint32_t>(colorLayout.Bytes() / (std::size_t{color.extent.height} * color.elementBytes));
                    const auto bandBytes = colorLayout.Offset(x1 - 1, y1 - 1) + color.elementBytes - binding.bandOffset;
                    binding.transfer = std::make_unique<Buffer>(context, bandBytes, copies);
                    binding.original.resize(bandBytes);
                    GuestMemory::Read(color.address + binding.bandOffset, binding.original, color.elementBytes);
                    std::memcpy(binding.transfer->Bytes().data(), binding.original.data(), bandBytes);
                } else {
                    binding.transfer = std::make_unique<Buffer>(context, colorLayout.LinearBytes(), copies);
                    ReadColorTarget(color, binding.transfer->Bytes());
                }
            }
            if (color.dccAddress != 0) {
                const auto keys = CurrentDccKeys(color.dccAddress, colorLayout.Bytes());
                if (IsDccClear(keys)) {
                    const auto pixels = binding.gpuTiling ? binding.tiled->Bytes() : binding.transfer->Bytes();
                    if (keys == DccKeys::ClearRegister) {
                        const auto texel = clearTexel(color, keys);
                        for (std::size_t offset = 0; offset + color.elementBytes <= pixels.size(); offset += color.elementBytes) std::memcpy(pixels.data() + offset, texel.data(), color.elementBytes);
                    } else if (!FillDccClear(color.format, keys, color.dccAlphaOnMsb, pixels)) {
                        static std::set<std::pair<std::uint64_t, int>> reported;
                        if (reported.size() < 32 && reported.insert({color.address, static_cast<int>(keys)}).second) std::fprintf(stderr, "[gpu] color target 0x%llx (VkFormat %d) has %s DCC keys; its stored texels are used\n", static_cast<unsigned long long>(color.address), static_cast<int>(color.format), DccKeysName(keys));
                        if (binding.gpuTiling) std::memcpy(pixels.data(), binding.original.data(), binding.original.size());
                        else ReadColorTarget(color, pixels);
                    }
                }
            }
            timer.phase(PhaseReadTarget);
            binding.target = std::make_unique<RenderTarget>(context, color, state.blends.at(color.exportIndex).blendEnable != 0);
            targetViews.push_back(binding.target->View());
        }
    };
    if (!repeatedColors && !prepared.attachmentsFromMemo) prepareColors();
    const auto prepareDepth = [&] {
        noteTargetMemo(TargetMemoEvent::DepthFallback);
        return DepthSurfaceViewWithStencilWriteIntent(context, *state.depth, state.depthWrite, state.stencilTest, writesStencilTarget(state), overwrittenStencil(state));
    };
    auto depthView = memoServes ? repeatDepth(context, state, repeatedColors ? &targetProof : nullptr) : VkImageView{VK_NULL_HANDLE};
    bool repeatedDepth = state.depth && depthView != VK_NULL_HANDLE;
    if (repeatedDepth) noteTargetMemo(TargetMemoEvent::DepthHit);
    if (state.depth && !repeatedDepth && !prepared.attachmentsFromMemo) depthView = prepareDepth();
    if (repeatedColors && (targetMemoOwner.load(std::memory_order_relaxed) != &ThreadTargetMemo() || !colorProofMatches(targetProof, currentTargetProof(context)) || !std::ranges::all_of(targets, [](const TargetBinding& binding) { return binding.resident->Cached(); }))) {
        noteTargetMemo(TargetMemoEvent::PostDepthInvalidated);
        repeatedColors = false;
        admissionProof = currentTargetProof(context);
        prepareColors();
        if (state.depth) depthView = prepareDepth();
        repeatedDepth = false;
    }
    if (repeatedColors) {
        for (const auto& binding : targets) {
            binding.resident->NoteProved();
            binding.resident->NoteUsed();
        }
        noteTargetMemo(TargetMemoEvent::Accept);
        if (state.depth && !repeatedDepth) noteTargetMemo(TargetMemoEvent::AcceptWithDepthMiss);
        if (!VerifyTargetMemo()) noteTargetMemo(TargetMemoEvent::AvoidedRefreshLookups, targets.size());
    }
    if ((repeatedColors || repeatedDepth) && VerifyTargetMemo()) {
        noteTargetMemo(TargetMemoEvent::VerifyChecked);
        const auto verifyProof = currentTargetProof(context);
        const auto ownProofBreaks = GuestMemory::OwnProofBreaks();
        const auto ownPendingMoves = StorageTexture::OwnPendingMoves();
        const auto ownRefreshWork = StorageTexture::OwnRefreshWork();
        const auto* owed = targetObligation(context, state, repeatedColors ? std::span<const TargetBinding>(targets) : std::span<const TargetBinding>{}, repeatedDepth);
        Require(owed == nullptr, std::string("APS5_VERIFY_TARGET_MEMO: a repeated target skipped ") + (owed != nullptr ? owed : ""));
        if (repeatedColors) {
            const auto repeatedViews = targetViews;
            std::vector<std::shared_ptr<StorageTexture>> repeatedResidents;
            for (const auto& binding : targets) repeatedResidents.push_back(binding.resident);
            prepareColors();
            Require(repeatedViews == targetViews && std::ranges::equal(repeatedResidents, targets, {}, {}, &TargetBinding::resident), "APS5_VERIFY_TARGET_MEMO: a repeated color differs from its lookup");
        }
        if (repeatedDepth) {
            const auto fullDepth = prepareDepth();
            Require(fullDepth == depthView, "APS5_VERIFY_TARGET_MEMO: a repeated depth differs from its lookup");
        }
        const auto* moved = StorageTexture::OwnRefreshWork() != ownRefreshWork ? "image content" : GuestMemory::OwnProofBreaks() != ownProofBreaks ? "proofs" : StorageTexture::OwnPendingMoves() != ownPendingMoves ? "pending" : verifyProof.MovedBefore(currentTargetProof(context));
        Require(moved == nullptr, std::string("APS5_VERIFY_TARGET_MEMO: a repeated target's lookup moved ") + (moved != nullptr ? moved : ""));
    }
    if (state.depth && !prepared.attachmentsFromMemo) targetViews.push_back(depthView);
    if (memoConsulted) {
        const bool retainedOwner = ownsTargetMemo(ThreadTargetMemo());
        const auto preparedProof = currentTargetProof(context);
        const bool unchangedDepth = (ThreadTargetMemo().depth.valid && ThreadTargetMemo().depth.proof == preparedProof &&
            (repeatedDepth || (!state.depth && !ThreadTargetMemo().depth.descriptor))) ||
            (state.depth && state.depth->samples != 1u && !ThreadTargetMemo().depth.valid);
        const bool unchanged = retainedOwner && repeatedColors && ThreadTargetMemo().colors.valid &&
            ThreadTargetMemo().colors.proof == preparedProof && unchangedDepth;
        if (admissionProof == preparedProof) {
            if (!unchanged) rememberTargets(state, targets, targetViews, preparedProof);
        } else {
            noteTargetMemo(TargetMemoEvent::Admission);
            ThreadTargetMemo().Invalidate();
        }
    }
    if (verifyAttachments) checkAttachments(state, prepared);
    timer.phase(PhasePrepare);
    const bool recordDraws = RecordDraws();
    auto* recorder = prepared.recorder = Recorder::Active();
    bool recordable = recordDraws && recorder != nullptr && dumpLimit == 0 && std::all_of(targets.begin(), targets.end(), [](const TargetBinding& binding) { return binding.resident != nullptr || binding.multisampled; });
    auto& resolved = prepared.resolved;
    resolved = resolveDrawResources(context, state, draw, shaders, snapshots, indexBytes, recordable, plan, outcome, timer);
    if (stopAfterResolve.load(std::memory_order_relaxed) != 0) {
        const auto stop = stopAfterResolve.exchange(0, std::memory_order_relaxed);
        if (stop == 1) return false;
        if (stop == 2) throw std::runtime_error("injected failure after the draw's resources were resolved");
    }
    if (state.depth && writesDepthTarget(state)) NoteDepthSurfaceWrite(context, *state.depth);
    auto& resources = resolved.resources;
    for (std::size_t index = 0; index < targets.size(); ++index) {
        auto& binding = targets[index];
        if (binding.color.partialWriteMask == 0 || binding.multisampled || binding.resident == nullptr || binding.proxied || storesIntoResident(*resources, *binding.resident)) continue;
        const auto& color = binding.color;
        binding.resident = refreshResidentTarget(context, state, color, outcome, profile, [&] { return color.arrayLastSlice != 0 ? CachedStorageLayers(context, ArraySurfaceForTarget(color), color.arraySlice, color.layers) : CachedStorageSurface(context, SurfaceForTarget(color)); });
        if (binding.resident != nullptr) targetViews[index] = binding.resident->AttachmentView(color.format, color.mip, color.depthSlice, color.layers, color.arraySlice);
    }
    if (!stencilReferences.empty() && resources->ChecksFaults()) recordable = false;
    if (!stencilReferences.empty()) {
        const auto readImages = resources->StorageImages();
        const auto inputOverlaps = [&](std::uint64_t address, std::uint64_t bytes) {
            return std::ranges::any_of(inputs.inPlaceReads, [&](const auto& range) { return address < range.second && range.first < address + bytes; });
        };
        for (const auto& target : targets) {
            const bool imageRead = target.resident != nullptr && std::ranges::any_of(readImages, [&](const auto& image) { return image.first == target.resident->Image(); });
            Require(!imageRead && !resources->ReadsImage(target.resident.get()) && !resources->ReadsOverlap(target.color.MemoryAddress(), static_cast<std::size_t>(target.color.bytes)) && !inputOverlaps(target.color.MemoryAddress(), target.color.bytes), "stencil replay refuses attachment feedback reads");
        }
        if (state.depth) {
            const auto bytes = static_cast<std::size_t>(state.depth->extent.width) * state.depth->extent.height * 4u;
            Require(!resources->ReadsOverlap(state.depth->address, bytes) && !resources->ReadsOverlap(state.depth->stencilAddress, bytes) && !inputOverlaps(state.depth->address, bytes) && !inputOverlaps(state.depth->stencilAddress, bytes), "stencil replay refuses depth or stencil feedback reads");
            for (const auto& shader : shaders) {
                for (const auto& binding : shader.program->bindings) {
                    if (binding.role != ShaderRecompiler::DescriptorRole::GuestImages && binding.role != ShaderRecompiler::DescriptorRole::ImageTable) continue;
                    for (std::size_t offset = 0; offset + 8u <= binding.guestDescriptor.size(); offset += 8u) {
                        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(offset, 8u);
                        if (words[0] == 0u && (words[1] & 0xffu) == 0u) continue;
                        const auto texture = DecodeTextureResource(words);
                        Require(!DepthSurfaceAt(context, words, texture), "stencil replay refuses sampled depth-surface feedback");
                    }
                }
            }
        }
    }
    const auto& contentKey = resolved.contentKey;
    const bool cacheable = resolved.cacheable;
    built = resolved.built;
    auto& indirect = prepared.indirect;
    indirect.args = args;
    auto& records = prepared.records;
    if (args != nullptr) {
        static const bool gpuIndirectDraws = std::getenv("APS5_NO_GPU_INDIRECT_DRAW") == nullptr;
        const auto decide = [&](std::uint64_t address, std::size_t bytes, const HostImport*& import) {
            if (StorageTexture::FlushPending(address, bytes, nullptr, "indirect draw arguments")) {
                if (recorder != nullptr) {
                    Recorder::CountSync(2);
                    recorder->Sync();
                }
                return IndirectDrawPath::PendingImage;
            }
            if (recorder != nullptr && recorder->PendingLabelIn(address, bytes)) return IndirectDrawPath::PendingLabelOrCopy;
            const auto overlaps = [&](const auto& writer) { return writer->CopiedWritesOverlap(address, bytes); };
            if (context.copiedWriters != nullptr && std::any_of(context.copiedWriters->begin(), context.copiedWriters->end(), overlaps)) return IndirectDrawPath::PendingLabelOrCopy;
            if (std::any_of(DrawCopiedWriters()->begin(), DrawCopiedWriters()->end(), overlaps)) return IndirectDrawPath::PendingLabelOrCopy;
            if ((import = HostImportFor(context, address, bytes)) == nullptr) return IndirectDrawPath::NotImported;
            static const bool settledReads = std::getenv("APS5_NO_SETTLED_INDIRECT_READ") == nullptr;
            if (settledReads && recorder != nullptr && recorder->PendingWriteSettled(address, bytes)) return IndirectDrawPath::Settled;
            return IndirectDrawPath::Gpu;
        };
        const auto rangeBytes = args->RangeBytes();
        if (rangeBytes == 0) {
            report(" indirect draw without records");
            return false;
        }
        Require(rangeBytes <= std::numeric_limits<std::size_t>::max(), "indirect draw record range overflow");
        if (!gpuIndirectDraws) indirect.path = IndirectDrawPath::Disabled;
        else if (inputs.listRestartPrimitive != 0) indirect.path = IndirectDrawPath::FeatureGap;
        else indirect.path = decide(args->arguments, static_cast<std::size_t>(rangeBytes), indirect.argumentImport);
        if (indirect.path == IndirectDrawPath::Gpu && args->countIndirect) {
            Require(context.drawIndirectCount, "indirect draw count without VK_KHR_draw_indirect_count must be resolved by the driver");
            const auto countPath = decide(args->countAddress, 4, indirect.countImport);
            if (countPath != IndirectDrawPath::Settled) indirect.path = countPath;
        }
        if (indirect.path != IndirectDrawPath::Gpu) {
            const auto readStart = std::chrono::steady_clock::now();
            FinishLeaseWrites();
            const auto count = std::min(args->countIndirect ? Pm4::ReadDrawCount(*args) : args->count, args->count);
            records.reserve(count);
            for (std::uint32_t record = 0; record < count; ++record) records.push_back(Pm4::ReadDrawArguments(*args, record));
            indirect.readMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count();
            if (inputs.listRestartPrimitive != 0) CompactIndirectListRestart(context, draw, inputs, records);
            indirect.records = records;
            if (std::none_of(records.begin(), records.end(), [](const Pm4::DrawArguments& record) { return record.count != 0 && record.instances != 0; })) {
                CountIndirectDraw(indirect.path, indirect.readMs);
                report(" indirect draw with empty records");
                return false;
            }
        }
    }
    static const bool syncCompletionDraws = std::getenv("APS5_SYNC_COMPLETION_DRAWS") != nullptr;
    static const bool recorderSyncDraws = std::getenv("APS5_NO_RECORDER_SYNC_DRAWS") == nullptr;
    static const bool recordCopiedDraws = std::getenv("APS5_NO_RECORD_COPIED_DRAWS") == nullptr;
    const bool completion = prepared.completion = resources->NeedsCompletion();
    const bool copiedWrites = completion && resources->HasCopiedWrites();
    const bool lease = resources->HoldsLease() && SyncLeaseWork();
    outcome.completion = completion;
    outcome.recorded = recordable;
    auto& listed = prepared.listed;
    if (!recordable) {
        outcome.reason = !recordDraws || dumpLimit != 0 ? SyncDisabled : recorder == nullptr ? SyncNoRecorder : SyncNotResident;
    } else if (completion && (syncCompletionDraws || copiedWrites || lease)) {
        outcome.reason = syncCompletionDraws ? SyncDisabled : lease ? SyncLease : SyncCopiedWrites;
        if (syncCompletionDraws) outcome.recorded = false;
        else if (copiedWrites && !lease && recordCopiedDraws) listed = true;
        else if (recorderSyncDraws) outcome.waited = true;
        else outcome.recorded = false;
    }
    const bool recorded = prepared.recorded = outcome.recorded;
    if (cacheable && built != nullptr && recorded && resources->Reusable()) SharedResourceCache().Insert(contentKey, resources);
    static const bool drawTransitions = std::getenv("APS5_DRAW_TRANSITIONS") != nullptr;
    const bool lean = prepared.lean = recorded && !drawTransitions;
    const bool multisampled = prepared.multisampled = std::any_of(targets.begin(), targets.end(), [](const TargetBinding& binding) { return binding.multisampled; });
    Require(lean || !multisampled, "multisampled draws are only rendered as recorded draws");
    for (const auto& binding : targets) {
        if (!binding.multisampled) continue;
        const auto& sources = resources->MultisampleSources();
        const auto reads = std::count(sources.begin(), sources.end(), binding.color.surfaceAddress);
        Require(reads == 0, "sampling a multisampled color target the draw renders to is not implemented");
    }
    for (const auto& binding : targets) {
        if (binding.color.partialWriteMask == 0 || binding.multisampled) continue;
        Require(lean && binding.resident != nullptr && !binding.proxied, "a partial write of a packed color target is merged into its resident image by a recorded draw only");
        Require(storesIntoResident(*resources, *binding.resident), "the partial write of a packed color target does not store into the target's resident image");
    }
    if (!lean && (!resolved.moved.empty() || resolved.resources->HasStagedCopies())) {
        if (resolved.resources->SpaceReuse()) static_cast<void>(resolved.resources->ReleaseSpace());
        resolved.resources = std::make_shared<ShaderResources>(context, shaders, state.color, static_cast<std::uint32_t>(state.colors.size()), draw.indexAddress, static_cast<std::size_t>(indexBytes), snapshots);
        resolved.moved.clear();
    }
    if (state.depth) RequireNoNativeDepthFeedback_nid_no_patch(context, *state.depth, resources->SampledTextures());
    APS5_LOG_CHARS_OUT_DEBUG("Creating Pipeline");
    auto& masked = prepared.masked;
    masked = maskedState(state, inputs.fragmentOutputs);
    if (std::any_of(targets.begin(), targets.end(), [](const TargetBinding& binding) { return binding.proxied; })) recipeOut = nullptr;
    for (std::size_t index = 0; index < targets.size(); ++index) {
        if (!targets[index].proxied) continue;
        if (!masked.has_value()) masked = state;
        masked->colors[index].format = AttachmentProxyFormat(context, state.colors[index].format);
        masked->blends.at(state.colors[index].exportIndex).colorWriteMask &= VK_COLOR_COMPONENT_R_BIT;
    }
    const State& pipelineState = masked.has_value() ? *masked : state;
    static const bool verifySetLayouts = std::getenv("APS5_VERIFY_SET_LAYOUTS") != nullptr;
    if (verifySetLayouts && context.descriptorCache != nullptr) {
        const auto described = DescribeSetLayouts(context, shaders);
        const auto built = resources->SetLayouts();
        Require(described.layouts.main == built.main && described.layouts.fragment == built.fragment && described.layouts.split == built.split && std::ranges::equal(described.mainKey, built.mainKey) && std::ranges::equal(described.fragmentKey, built.fragmentKey), "set layouts described from the shaders differ from the draw's");
    }
    auto& stencilReplay = prepared.stencilReplay;
    for (const auto reference : stencilReferences) {
        auto selected = pipelineState;
        selected.stencilReplayReference = reference;
        selected.stencilFront.reference = reference;
        selected.stencilBack.reference = reference;
        auto selectedPipeline = stencilReplay.empty() ? CachedPipeline(context, selected, inputs.VertexLayout(), *resources, shaders, lean ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) : stencilReplay.front().second;
        Require(!selectedPipeline->SplitsFaces() && !selectedPipeline->SplitsSecondChannel(), "stencil replay refuses pipeline face or channel splits");
        stencilReplay.emplace_back(reference, std::move(selectedPipeline));
    }
    auto& pipeline = prepared.pipeline;
    const auto attachmentLayout = lean ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (!stencilReplay.empty()) {
        pipeline = stencilReplay.front().second;
    } else {
        const bool memoized = stencilReferences.empty() && std::none_of(targets.begin(), targets.end(), [](const TargetBinding& binding) { return binding.proxied || binding.multisampled; });
        const auto layouts = resources->SetLayouts();
        std::uint64_t storeHash = 0;
        if (memoized) pipeline = shapePipeline(context, prepared.shape, inputs.VertexLayout(), layouts, shaders, attachmentLayout, inputs.fragmentOutputs);
        if (pipeline != nullptr && VerifyShapeMemo()) {
            Require(CachedPipeline(context, pipelineState, inputs.VertexLayout(), layouts, shaders, attachmentLayout) == pipeline, "APS5_VERIFY_SHAPE_MEMO: the shape memo's pipeline differs from the pipeline store's");
            noteShapeMemoVerified(true);
        }
        const bool repeated = pipeline != nullptr;
        if (!repeated && DefersPipelineBuild(shaders)) {
            pipeline = ResidentPipelineOrDeferredBuild(context, pipelineState, inputs.VertexLayout(), layouts, shaders, attachmentLayout, &storeHash);
            if (pipeline == nullptr) {
                report(" pipeline building in the background");
                return false;
            }
        }
        if (!repeated && pipeline == nullptr) pipeline = CachedPipeline(context, pipelineState, inputs.VertexLayout(), layouts, shaders, attachmentLayout, nullptr, &storeHash);
        if (memoized) rememberShape(context, prepared.shape, targets, repeated ? nullptr : &pipeline, storeHash, inputs.VertexLayout(), layouts, shaders, attachmentLayout, inputs.fragmentOutputs);
    }
    auto& owners = prepared.owners;
    owners.reserve(targets.size());
    auto& framebufferOwners = prepared.framebufferOwners;
    framebufferOwners.reserve(targets.size());
    for (const auto& binding : targets) {
        owners.push_back(binding.resident);
        framebufferOwners.push_back(binding.multisampled ? binding.multisampleOwner : binding.resident);
    }
    Require(!pipeline->ReordersSamples() || std::ranges::none_of(state.colors, [](const ColorTarget& color) { return color.staged; }), "a partial write of a multisampled packed color target on a device that puts the guest's samples in order is not implemented");
    auto& framebuffer = prepared.framebuffer;
    framebuffer = pipeline->AcquireFramebuffer(targetViews, framebufferOwners, state.renderExtent, state.renderLayers);
    auto& reordered = prepared.reordered;
    auto& reorderPass = prepared.reorderPass;
    if (pipeline->ReordersSamples()) {
        reorderPass = SampleOrderPassFor(context, pipelineState, pipeline->HostSampleOrder());
        for (const auto& step : PlanSampleOrder(context, state, pipeline->HostSampleOrder())) {
            if (step.attachment < targets.size()) reordered.push_back(MultisampleTargetOrderTarget(context, targets[step.attachment].color));
            else reordered.push_back(DepthSurfaceOrderTarget(context, *state.depth));
            Require(reordered.back().method == step.method, "a reordered attachment's surface disagrees with the planned way of putting its samples in order");
        }
    }
    prepared.depthLocations = passDepthLocations(context, pipelineState, *pipeline);
    timer.phase(PhasePipeline);
    APS5_LOG_CHARS_OUT_DEBUG("Pipeline created");
    if (VerifyDrawPlan()) requireDrawPlanRepeats(context, state, draw, shaders, plan);
    return true;
}

}
}
}
}
}
}
}
