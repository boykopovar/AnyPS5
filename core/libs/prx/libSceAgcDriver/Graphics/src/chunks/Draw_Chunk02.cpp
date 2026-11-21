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
std::array<std::byte, 16> clearTexel(const ColorTarget& color, DccKeys keys) {
    std::array<std::byte, 16> texel{};
    const auto elementBytes = static_cast<std::size_t>(color.elementBytes);
    Require(elementBytes != 0 && elementBytes <= texel.size(), "unexpected color element size");
    if (keys == DccKeys::ClearRegister) {
        Require(elementBytes <= sizeof(color.clearWords), "the DCC register clear of a texel over 64 bits is not modeled");
        std::memcpy(texel.data(), color.clearWords.data(), elementBytes);
    } else {
        Require(FillDccClear(color.format, keys, color.dccAlphaOnMsb, std::span(texel.data(), elementBytes)), "the target's format has no encoding of its DCC clear code");
    }
    return texel;
}

bool clearToTexel(StorageTexture& image, const std::array<std::byte, 16>& texel, std::uint32_t elementBytes, const char*& refusal, bool allowRegisterClear = false) {
    std::array<std::byte, 16> repeated{};
    for (std::size_t offset = 0; offset + elementBytes <= repeated.size(); offset += elementBytes) std::memcpy(repeated.data() + offset, texel.data(), elementBytes);
    std::array<std::uint32_t, 4> pattern{};
    std::memcpy(pattern.data(), repeated.data(), repeated.size());
    return image.FillClear(std::span<const std::uint32_t, 4>(pattern), StorageTexture::WholeImage, refusal, allowRegisterClear);
}

void writeTexels(const ColorTarget& color, const std::array<std::byte, 16>& texel) {
    const auto elementBytes = static_cast<std::size_t>(color.elementBytes);
    std::vector<std::byte> texels(color.bytes);
    for (std::size_t offset = 0; offset + elementBytes <= texels.size(); offset += elementBytes) std::memcpy(texels.data() + offset, texel.data(), elementBytes);
    GuestMemory::Write(color.address, texels);
}

void storeClearTexels(const Context& context, const ColorTarget& color, const std::array<std::byte, 16>& texel) {
    StorageTexture::FlushPending(color.address, color.bytes, nullptr, "fast-clear materialization");
    const auto keys = ReadDccKeys(color.dccAddress, color.bytes);
    if (!IsDccClear(keys)) return;
    writeTexels(color, keys == DccKeys::ClearRegister ? texel : clearTexel(color, keys));
    MarkDccUncompressed(context, color.dccAddress, color.bytes, colorKeyCount(color, color.bytes));
}

void storeSingleTexels(const Context& context, const ColorTarget& color) {
    Require(!ColorTileModeIsLinear(color.tileMode), "comp-to-single DCC keys of a linear color target");
    const ColorTargetLayout surface(color.extent.width, color.extent.height, color.tileMode, color.elementBytes, color.pipeBankXor);
    Require(surface.Bytes() == color.bytes, "comp-to-single DCC keys over a color target whose layout differs from its surface");
    StorageTexture::FlushPending(color.address, color.bytes, nullptr, "comp-to-single expansion");
    std::vector<std::byte> texels(color.bytes);
    GuestMemory::Read(color.address, texels, surface.Alignment());
    constexpr std::size_t BlockBytes = 256;
    for (std::uint32_t y = 0; y < color.extent.height; ++y) {
        for (std::uint32_t x = 0; x < color.extent.width; ++x) {
            const auto offset = surface.Offset(x, y);
            const auto first = offset / BlockBytes * BlockBytes;
            if (offset != first) std::memcpy(texels.data() + offset, texels.data() + first, color.elementBytes);
        }
    }
    GuestMemory::Write(color.address, texels, surface.Alignment());
    MarkDccUncompressed(context, color.dccAddress, color.bytes, colorKeyCount(color, color.bytes));
}

void materializeRegisterClear(const Context& context, const ColorTarget& color, StorageTexture& resident) {
    if (color.dccAddress == 0 || resident.Descriptor().dccAddress != color.dccAddress) return;
    if (ProvedCurrentDccKeys(color.dccAddress, color.bytes, resident.TargetKeyProof()) != DccKeys::ClearRegister) return;
    const auto texel = clearTexel(color, DccKeys::ClearRegister);
    const char* refusal = nullptr;
    const bool pendingKeysKnown = KnownPendingDccKeys(color.dccAddress, color.bytes) == DccKeys::ClearRegister;
    bool cleared = clearToTexel(resident, texel, color.elementBytes, refusal, pendingKeysKnown);
    const auto keyBytes = static_cast<std::size_t>(color.bytes / 256);
    if (!cleared && keyBytes != 0) {
        if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->PendingWriteOverlaps(color.dccAddress, keyBytes)) {
            Recorder::CountSync(2);
            recorder->SyncThrough(color.dccAddress, keyBytes);
            if (CurrentDccKeys(color.dccAddress, color.bytes) != DccKeys::ClearRegister) return;
            cleared = clearToTexel(resident, texel, color.elementBytes, refusal);
        }
    }
    if (cleared) {
        MarkDccUncompressed(context, color.dccAddress, color.bytes, colorKeyCount(color, color.bytes));
        return;
    }
    storeClearTexels(context, color, texel);
    resident.Refresh();
}

enum class CmaskState { Expanded, Cleared, Partial };

struct CmaskProof {
    std::size_t bytes = 0;
    std::uint64_t generation = 0;
};

AgcDriver::Mutex& cmaskProofMutex() {
    static AgcDriver::Mutex mutex;
    return mutex;
}

std::unordered_map<std::uint64_t, CmaskProof>& cmaskProofs() {
    static std::unordered_map<std::uint64_t, CmaskProof> proofs;
    return proofs;
}

bool cmaskProved(std::uint64_t address, std::size_t bytes) {
    if (GuestMemory::CollectWrites(address, bytes) == 0) return false;
    std::lock_guard lock(cmaskProofMutex());
    const auto found = cmaskProofs().find(address);
    return found != cmaskProofs().end() && found->second.bytes == bytes && !GuestMemory::StoredOver(address, bytes, found->second.generation);
}

void proveCmask(std::uint64_t address, std::size_t bytes, std::uint64_t generation) {
    std::lock_guard lock(cmaskProofMutex());
    if (generation == 0) cmaskProofs().erase(address);
    else cmaskProofs()[address] = {bytes, generation};
}

void requireCmaskCodes(std::uint32_t codes) {
    Require((codes & 1u) == 0, "a CMASK fast clear of a DCC color target, whose clear belongs in its DCC keys, was recorded");
}

CmaskState readCmask(const ColorTarget& color, const CmaskLayout& layout, std::vector<std::uint32_t>& cleared) {
    const auto address = color.cmaskAddress;
    const auto bytes = layout.Bytes();
    const bool locked = GuestMemory::GpuMutex().HeldByThisThread();
    auto* recorder = Recorder::Active();
    if (locked && recorder != nullptr && recorder->PendingWriteOverlaps(address, bytes)) {
        Recorder::CountSync(2);
        recorder->SyncThrough(address, bytes);
    }
    if (AnyShadowedOverlaps(address, bytes)) {
        Require(locked, "a CMASK under a unit shadow must be read under the device lock");
        PublishShadow(address, bytes, PublishScope::Whole, PublishReason::Keys);
        if (recorder != nullptr) {
            Recorder::CountSync(2);
            recorder->Sync();
        }
    }
    std::vector<std::byte> mask(bytes);
    GuestMemory::Read(address, mask, CmaskLayout::Alignment);
    const auto uniform = [&](std::byte value) { return std::all_of(mask.begin(), mask.end(), [&](std::byte entry) { return entry == value; }); };
    if (color.coverageSamples > color.samples) {
        for (std::uint32_t tileY = 0; tileY < layout.TilesY(); ++tileY) {
            for (std::uint32_t tileX = 0; tileX < layout.TilesX(); ++tileX) {
                const auto nibble = layout.Nibble(tileX, tileY);
                const auto code = (std::to_integer<std::uint32_t>(mask[nibble / 2u]) >> ((nibble % 2u) * 4u)) & 0xfu;
                if (code != 0u && (code >> 2u) != 3u) {
                    char text[128];
                    std::snprintf(text, sizeof(text), "AGC graphics: CMASK code 0x%x of a one-fragment color target is not implemented", code);
                    throw std::runtime_error(text);
                }
            }
        }
    }
    if (uniform(std::byte{0xff})) return CmaskState::Expanded;
    if (uniform(std::byte{0})) return CmaskState::Cleared;
    for (std::uint32_t tileY = 0; tileY < layout.TilesY(); ++tileY) {
        for (std::uint32_t tileX = 0; tileX < layout.TilesX(); ++tileX) {
            const auto nibble = layout.Nibble(tileX, tileY);
            const auto code = (std::to_integer<std::uint32_t>(mask[nibble / 2u]) >> ((nibble % 2u) * 4u)) & 0xfu;
            if (code == 0u) cleared.push_back(tileY * layout.TilesX() + tileX);
        }
    }
    if (cleared.empty()) return CmaskState::Expanded;
    if (cleared.size() == static_cast<std::size_t>(layout.TilesX()) * layout.TilesY()) return CmaskState::Cleared;
    return CmaskState::Partial;
}

void storeCmaskClearTexels(const ColorTarget& color, const CmaskLayout& layout, const std::vector<std::uint32_t>* tiles, const std::array<std::byte, 16>& texel) {
    StorageTexture::FlushPending(color.address, color.bytes, nullptr, "fast-clear materialization");
    const ColorTargetLayout surface(color.extent.width, color.extent.height, color.tileMode, color.elementBytes);
    Require(surface.Bytes() == color.bytes, "CMASK fast clear over a color target whose layout differs from its surface");
    std::vector<std::byte> texels(surface.Bytes());
    GuestMemory::Read(color.address, texels, surface.Alignment());
    const auto fill = [&](std::uint32_t tile) {
        const auto x0 = (tile % layout.TilesX()) * 8u;
        const auto y0 = (tile / layout.TilesX()) * 8u;
        for (auto y = y0; y < std::min(y0 + 8u, color.extent.height); ++y) {
            for (auto x = x0; x < std::min(x0 + 8u, color.extent.width); ++x) std::memcpy(texels.data() + surface.Offset(x, y), texel.data(), color.elementBytes);
        }
    };
    if (tiles != nullptr) {
        for (const auto tile : *tiles) fill(tile);
    } else {
        for (std::uint32_t tile = 0; tile < layout.TilesX() * layout.TilesY(); ++tile) fill(tile);
    }
    GuestMemory::Write(color.address, texels, surface.Alignment());
}

void recordCmaskClear(const Context& context, Recorder& recorder, const HostImport& import, const ColorTarget& color, const CmaskLayout& layout, const std::shared_ptr<StorageTexture>& resident, VkImageView view) {
    const auto address = color.cmaskAddress;
    const auto bytes = layout.Bytes();
    StorageTexture::FlushPending(address, bytes, nullptr, "fast-clear materialization");
    if (AnyShadowedOverlaps(address, bytes)) PublishShadow(address, bytes, PublishScope::Whole, PublishReason::Keys);
    recorder.FlushKeyStoresOverlapping(address, bytes);
    const auto commands = recorder.Commands();
    if (resident != nullptr && !resident->Cached()) recorder.KeepBytes(resident.get(), static_cast<std::size_t>(resident->BudgetBytes()));
    recorder.Keep(resident);
    context.detiler->DispatchCmaskClear(commands, import.buffer, HostImportOffset(import, address), bytes, view, color.extent.width, color.extent.height, color.elementBytes, color.clearWords, color.dccAddress == 0);
    recorder.NotePendingWrite(address, bytes);
    proveCmask(address, bytes, GuestMemory::MarkWritten(address, bytes));
}

bool materializeCmaskClear(const Context& context, const ColorTarget& color, const std::shared_ptr<StorageTexture>& resident) {
    if (color.cmaskAddress == 0) return false;
    if (context.detiler != nullptr) requireCmaskCodes(context.detiler->CmaskErrors());
    const CmaskLayout layout(color.extent.width, color.extent.height);
    const auto bytes = layout.Bytes();
    Require(color.cmaskBytes == bytes, "CMASK size differs from the color target's CMASK layout");
    if (cmaskProved(color.cmaskAddress, bytes)) return false;
    if (color.coverageSamples > color.samples) {
        std::vector<std::uint32_t> codes;
        static_cast<void>(readCmask(color, layout, codes));
    }
    auto* recorder = Recorder::Active();
    if (resident != nullptr && context.detiler != nullptr && recorder != nullptr && GuestMemory::GpuMutex().HeldByThisThread()) {
        const auto* import = HostImportFor(context, color.cmaskAddress, bytes);
        const auto view = import != nullptr ? resident->ElementView() : VK_NULL_HANDLE;
        if (view != VK_NULL_HANDLE) {
            recordCmaskClear(context, *recorder, *import, color, layout, resident, view);
            return true;
        }
    }
    const auto collected = GuestMemory::CollectWrites(color.cmaskAddress, bytes);
    std::vector<std::uint32_t> cleared;
    const auto state = readCmask(color, layout, cleared);
    if (state == CmaskState::Expanded) {
        proveCmask(color.cmaskAddress, bytes, collected);
        return false;
    }
    Require(color.dccAddress == 0, "CMASK fast clear of a DCC color target, whose clear belongs in its DCC keys");
    const auto texel = clearTexel(color, DccKeys::ClearRegister);
    const char* refusal = nullptr;
    const bool filled = state == CmaskState::Cleared && color.mipCount == 1 && resident != nullptr && clearToTexel(*resident, texel, color.elementBytes, refusal);
    if (!filled) {
        storeCmaskClearTexels(color, layout, state == CmaskState::Cleared ? nullptr : &cleared, texel);
        if (resident != nullptr) resident->Refresh();
    }
    const std::vector<std::byte> expanded(bytes, std::byte{0xff});
    GuestMemory::Write(color.cmaskAddress, expanded, CmaskLayout::Alignment);
    proveCmask(color.cmaskAddress, bytes, GuestMemory::CollectWrites(color.cmaskAddress, bytes));
    return filled;
}

void imageBarrier(const Context& context, VkCommandBuffer commands, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess, VkImageSubresourceRange subresources = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = subresources;
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void memoryBarrier(const Context& context, VkCommandBuffer commands, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess) {
    const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, sourceAccess, destinationAccess};
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, sourceStage, destinationStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

TileMipLayout ColorTargetMip(const ColorTarget& color, const ColorTargetLayout& layout) {
    TileMipLayout mip{};
    mip.width = color.extent.width;
    mip.height = color.extent.height;
    mip.blocksPerRow = layout.BlocksPerRow();
    mip.pitchBytes = color.extent.width * color.elementBytes;
    mip.tiledSize = layout.Bytes();
    mip.linearSize = layout.LinearBytes();
    return mip;
}

enum DrawPhase : std::size_t { PhaseValidate, PhaseVertex, PhaseSetup, PhaseReadTarget, PhasePrepare, PhaseLookup, PhaseResources, PhasePipeline, PhaseRecord, PhaseKeep, PhaseSync, PhaseWriteBack, PhaseDescribe, PhaseCount };
constexpr std::array<const char*, PhaseCount> DrawPhaseNames{"validate", "vertex", "setup", "readTarget", "prepare", "lookup", "resources", "pipeline", "record", "keep", "sync", "writeBack", "describe"};

enum SyncReason : std::size_t { SyncNone, SyncNotResident, SyncNoRecorder, SyncDisabled, SyncCopiedWrites, SyncLease, SyncCount };
constexpr std::array<const char*, SyncCount> SyncReasonNames{"none", "non-resident target", "no recorder", "disabled", "copied writes", "lease"};
constexpr std::size_t IndirectPathCount = static_cast<std::size_t>(IndirectDrawPath::Count);
constexpr std::array<const char*, IndirectPathCount> IndirectDrawPathNames{"gpu-side", "patched SGPR not folded", "fetch offset unknown", "non-vertex path", "GE_INDX_OFFSET", "draw index", "vertex range too large", "feature gap", "pending image results", "pending label or copied write", "not imported", "disabled", "index range not fully readable", "index limit unknown", "records settled"};

struct DrawOutcome {
    bool recorded = false;
    bool waited = false;
    bool completion = false;
    bool passBegun = false;
    bool passContinued = false;
    PassBreak passBreak = PassBreak::Continued;
    bool keyStoresFlushed = false;
    bool storesFlushed = false;
    bool addressDraw = false;
    SyncReason reason = SyncNone;
    bool validateMemoized = false;
    bool validateHit = false;
    bool addressBased = false;
    std::uint64_t targetLookups = 0;
    std::uint64_t slowLookups = 0;
    double slowLookupUs = 0;
    std::uint64_t fullScissorLookups = 0;
    std::uint64_t partialLookups = 0;
    std::uint64_t pagesWalked = 0;
    double lookupUs = 0;
    std::size_t kind = 0;
    double hookWaitUs = 0;
    double ownSyncUs = 0;
};

enum DrawKind : std::size_t { KindRecipeHit, KindTemplateHit, KindBuild, KindBda, KindCount };
constexpr std::array<const char*, KindCount> DrawKindNames{"recipe hit", "template hit", "build", "BDA"};

struct DrawProfile {
    AgcDriver::Mutex mutex;
    std::array<double, KindCount> kindUs{};
    std::array<std::uint64_t, KindCount> kindCounts{};
    std::uint64_t fullScissorLookups = 0;
    std::uint64_t partialLookups = 0;
    std::uint64_t pagesWalked = 0;
    double lookupUs = 0;
    std::array<double, PhaseCount> totalsUs{};
    std::array<double, PhaseCount> maxUs{};
    double maxDrawUs = 0;
    double hookWaitUs = 0;
    double maxHookWaitUs = 0;
    double ownSyncUs = 0;
    double bindingsUs = 0;
    double uploadUs = 0;
    double descriptorsUs = 0;
    double otherUs = 0;
    double addressOtherUs = 0;
    std::uint64_t addressBuilds = 0;
    std::uint64_t draws = 0;
    std::uint64_t recorded = 0;
    std::uint64_t waited = 0;
    std::uint64_t completion = 0;
    std::uint64_t passesBegun = 0;
    std::uint64_t passesContinued = 0;
    std::array<DrawPassCounts, 2> passTotals{};
    std::array<std::uint64_t, PassBreakCount> passBreaks{};
    std::array<std::uint64_t, PassBreakCount> reportedPassBreaks{};
    std::array<std::uint64_t, SyncCount> reasons{};
    double recordedUs = 0;
    double waitedUs = 0;
    double synchronousUs = 0;
    std::uint64_t targetLookups = 0;
    std::uint64_t slowLookups = 0;
    double slowLookupUs = 0;
    std::uint64_t cacheHits = 0;
    std::uint64_t cacheMisses = 0;
    std::uint64_t cacheInvalidated = 0;
    std::uint64_t uncacheable = 0;
    std::uint64_t validateHits = 0;
    std::uint64_t validateMisses = 0;
    std::uint64_t recipeHits = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(DrawRecipeMiss::Count)> recipeMisses{};
    std::array<std::uint64_t, IndirectPathCount> indirect{};
    std::uint64_t indirectRewritten = 0;
    std::uint64_t indirectEmpty = 0;
    double indirectReadUs = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(DrawSkip::Count)> skips{};
    std::array<double, static_cast<std::size_t>(DrawSkip::Count)> skipUs{};
    AddressDrawReuseCounts addressSeen{};
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};
constexpr std::array<const char*, static_cast<std::size_t>(DrawSkip::Count)> DrawSkipNames{"nothing to draw", "prechecked", "thrown"};
constexpr std::array<const char*, static_cast<std::size_t>(AddressDrawReuse::Count)> AddressDrawReuseNames{"reused", "built", "not read-only", "not space-only", "snapshot outside the space (copied)", "partial overlap", "snapshot changed", "moved GuestBuffers element", "patched data word", "space or epoch change", "in use", "revalidation"};

std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(AddressDrawReuse::Count)>& addressDrawReuses() {
    static std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(AddressDrawReuse::Count)> counts{};
    return counts;
}

void countAddressDrawReuse(AddressDrawReuse outcome) {
    addressDrawReuses()[static_cast<std::size_t>(outcome)].fetch_add(1, std::memory_order_relaxed);
}

std::atomic<std::uint64_t>& addressDrawReuseVerifications() {
    static std::atomic<std::uint64_t> verifications{0};
    return verifications;
}

void completeSnapshotWrites(std::span<const GuestMemorySnapshot> snapshots) {
    auto* recorder = Recorder::Active();
    if (recorder == nullptr || Recorder::InCompletion()) return;
    for (const auto& snapshot : snapshots) {
        if (snapshot.bytes.empty() || !recorder->PendingWriteOverlaps(snapshot.address, snapshot.bytes.size())) continue;
        Recorder::CountSync(4);
        recorder->SyncThrough(snapshot.address, snapshot.bytes.size());
    }
}

DrawProfile& Profile() {
    static DrawProfile profile;
    return profile;
}

void reportDraw(const std::array<double, PhaseCount>& us, const ShaderResources::BuildTiming* built, const DrawOutcome& outcome) {
    auto& profile = Profile();
    std::lock_guard lock(profile.mutex);
    double drawUs = 0;
    for (std::size_t i = 0; i < PhaseCount; ++i) {
        profile.totalsUs[i] += us[i];
        profile.maxUs[i] = std::max(profile.maxUs[i], us[i]);
        drawUs += us[i];
    }
    profile.maxDrawUs = std::max(profile.maxDrawUs, drawUs);
    profile.hookWaitUs += outcome.hookWaitUs;
    profile.maxHookWaitUs = std::max(profile.maxHookWaitUs, outcome.hookWaitUs);
    profile.ownSyncUs += outcome.ownSyncUs;
    if (built != nullptr) {
        profile.bindingsUs += built->bindingsMs * 1000.0;
        profile.uploadUs += built->uploadMs * 1000.0;
        profile.descriptorsUs += built->descriptorsMs * 1000.0;
        const auto other = std::max(0.0, us[PhaseResources] - (built->bindingsMs + built->uploadMs + built->descriptorsMs) * 1000.0);
        profile.otherUs += other;
        if (outcome.addressBased) {
            profile.addressOtherUs += other;
            ++profile.addressBuilds;
        }
    }
    ++profile.draws;
    if (outcome.recorded) ++(outcome.waited ? profile.waited : profile.recorded);
    if (outcome.completion) ++profile.completion;
    if (outcome.passBegun) ++profile.passesBegun;
    if (outcome.passBegun) ++profile.passBreaks[static_cast<std::size_t>(outcome.passBreak)];
    if (outcome.passContinued) ++profile.passesContinued;
    if (outcome.recorded) {
        auto& totals = profile.passTotals[outcome.addressDraw ? 1 : 0];
        ++totals.draws;
        if (outcome.passBegun) ++totals.passesBegun;
        if (outcome.passContinued) ++totals.passesContinued;
        if (outcome.keyStoresFlushed) ++totals.keyStoreFlushes;
        if (outcome.storesFlushed) ++totals.storeFlushes;
    }
    ++profile.reasons[outcome.reason];
    (outcome.recorded ? (outcome.waited ? profile.waitedUs : profile.recordedUs) : profile.synchronousUs) += drawUs;
    profile.targetLookups += outcome.targetLookups;
    profile.slowLookups += outcome.slowLookups;
    profile.slowLookupUs += outcome.slowLookupUs;
    profile.fullScissorLookups += outcome.fullScissorLookups;
    profile.partialLookups += outcome.partialLookups;
    profile.pagesWalked += outcome.pagesWalked;
    profile.lookupUs += outcome.lookupUs;
    if (outcome.kind < KindCount) {
        ++profile.kindCounts[outcome.kind];
        profile.kindUs[outcome.kind] += drawUs;
    }
    if (outcome.validateMemoized) ++(outcome.validateHit ? profile.validateHits : profile.validateMisses);
    const auto now = std::chrono::steady_clock::now();
    if (now - profile.lastReport < std::chrono::seconds(10)) return;
    profile.lastReport = now;
    const auto synchronous = profile.draws - profile.recorded - profile.waited;
    const auto average = [](double total, std::uint64_t count) { return count != 0 ? total / static_cast<double>(count) : 0.0; };
    char line[2048];
    int n = std::snprintf(line, sizeof(line), "[draws] %llu draws over 10 s (%llu recorded avg %.0f us, of them %llu with completion; %llu recorded then waited avg %.0f us; %llu synchronous avg %.0f us; render passes %llu begun", static_cast<unsigned long long>(profile.draws), static_cast<unsigned long long>(profile.recorded), average(profile.recordedUs, profile.recorded), static_cast<unsigned long long>(profile.completion), static_cast<unsigned long long>(profile.waited), average(profile.waitedUs, profile.waited), static_cast<unsigned long long>(synchronous), average(profile.synchronousUs, synchronous), static_cast<unsigned long long>(profile.passesBegun));
    const auto room = [&] { return n > 0 && static_cast<std::size_t>(n) < sizeof(line); };
    bool listed = false;
    for (std::size_t i = 1; i < PassBreakCount && room(); ++i) {
        const auto count = profile.passBreaks[i] - profile.reportedPassBreaks[i];
        if (count == 0) continue;
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "%s%s %llu", listed ? ", " : " (", PassBreakNames[i], static_cast<unsigned long long>(count));
        listed = true;
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "%s, %llu draws continued one; waited or synchronous because:", listed ? ")" : "", static_cast<unsigned long long>(profile.passesContinued));
    for (std::size_t i = SyncNone + 1; i < SyncCount && room(); ++i) {
        if (profile.reasons[i] != 0) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %llu", SyncReasonNames[i], static_cast<unsigned long long>(profile.reasons[i]));
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "):");
    for (std::size_t i = 0; i < PhaseCount && room(); ++i) {
        if (profile.totalsUs[i] <= 0) continue;
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s=%.1fms", DrawPhaseNames[i], profile.totalsUs[i] / 1000.0);
        if (i == PhaseResources && room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " (bindings %.1f, upload %.1f, descriptors %.1f, other %.1f of which %.1f in %llu address-based builds)", profile.bindingsUs / 1000.0, profile.uploadUs / 1000.0, profile.descriptorsUs / 1000.0, profile.otherUs / 1000.0, profile.addressOtherUs / 1000.0, static_cast<unsigned long long>(profile.addressBuilds));
        if (i == PhaseReadTarget && room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " (%llu resident lookups, %llu of them >= 1 ms = %.1f; target lookups: full-scissor %llu / partial %llu / pages walked %llu / %.0f us)", static_cast<unsigned long long>(profile.targetLookups), static_cast<unsigned long long>(profile.slowLookups), profile.slowLookupUs / 1000.0, static_cast<unsigned long long>(profile.fullScissorLookups), static_cast<unsigned long long>(profile.partialLookups), static_cast<unsigned long long>(profile.pagesWalked), profile.lookupUs);
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; per kind (avg us, count):");
    for (std::size_t kind = 0; kind < KindCount && room(); ++kind) {
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %.0f (%llu)", DrawKindNames[kind], average(profile.kindUs[kind], profile.kindCounts[kind]), static_cast<unsigned long long>(profile.kindCounts[kind]));
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; longest draw %.1f ms, longest phase of any draw:", profile.maxDrawUs / 1000.0);
    for (std::size_t i = 0; i < PhaseCount && room(); ++i) {
        if (profile.maxUs[i] < 500.0) continue;
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %.1f", DrawPhaseNames[i], profile.maxUs[i] / 1000.0);
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; hook waits inside draws %.1f ms (max %.1f; the draws' own syncs, %.1f ms, not counted)", profile.hookWaitUs / 1000.0, profile.maxHookWaitUs / 1000.0, profile.ownSyncUs / 1000.0);
    std::uint64_t indirectCpu = 0;
    for (std::size_t i = 1; i < IndirectPathCount; ++i) indirectCpu += profile.indirect[i];
    if ((profile.indirect[0] != 0 || indirectCpu != 0 || profile.indirectEmpty != 0) && room()) {
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; indirect: gpu-side %llu (%llu rewritten), cpu-side %llu (", static_cast<unsigned long long>(profile.indirect[0]), static_cast<unsigned long long>(profile.indirectRewritten), static_cast<unsigned long long>(indirectCpu));
        for (std::size_t i = 1; i < IndirectPathCount && room(); ++i) {
            if (profile.indirect[i] != 0) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %llu", IndirectDrawPathNames[i], static_cast<unsigned long long>(profile.indirect[i]));
        }
        if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "), empty skipped before their stages %llu, argument reads %.1f ms", static_cast<unsigned long long>(profile.indirectEmpty), profile.indirectReadUs / 1000.0);
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; skipped packets:");
    for (std::size_t i = 0; i < profile.skips.size() && room(); ++i) {
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %llu in %.1f ms", DrawSkipNames[i], static_cast<unsigned long long>(profile.skips[i]), profile.skipUs[i] / 1000.0);
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; recipe hits %llu, misses by reason:", static_cast<unsigned long long>(profile.recipeHits));
    for (std::size_t i = 1; i < profile.recipeMisses.size() && room(); ++i) {
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %llu", DrawRecipeMissName(static_cast<DrawRecipeMiss>(i)), static_cast<unsigned long long>(profile.recipeMisses[i]));
    }
    AgcDriver::ProfilePrint_nid_no_patch("%s\n", line);
    AgcDriver::ProfilePrint_nid_no_patch("[rescache] draws: %llu hits, %llu misses, %llu invalidated, %llu uncacheable; validation memo %llu hits / %llu misses (which key words the misses differ in: the miss churn line)\n", static_cast<unsigned long long>(profile.cacheHits), static_cast<unsigned long long>(profile.cacheMisses), static_cast<unsigned long long>(profile.cacheInvalidated), static_cast<unsigned long long>(profile.uncacheable), static_cast<unsigned long long>(profile.validateHits), static_cast<unsigned long long>(profile.validateMisses));
    const auto address = AddressDrawReuseCounters();
    if (address != profile.addressSeen) {
        char reuse[512];
        const auto seen = [&](AddressDrawReuse outcome) { return static_cast<unsigned long long>(address[static_cast<std::size_t>(outcome)] - profile.addressSeen[static_cast<std::size_t>(outcome)]); };
        int at = std::snprintf(reuse, sizeof(reuse), "[rescache] address-based draws (10 s): reused %llu, built %llu; refused:", seen(AddressDrawReuse::Reused), seen(AddressDrawReuse::Built));
        for (auto outcome = static_cast<std::size_t>(AddressDrawReuse::NotReadOnly); outcome < address.size() && at > 0 && static_cast<std::size_t>(at) < sizeof(reuse); ++outcome) {
            at += std::snprintf(reuse + at, sizeof(reuse) - static_cast<std::size_t>(at), "%s %s %llu", outcome == static_cast<std::size_t>(AddressDrawReuse::NotReadOnly) ? "" : ",", AddressDrawReuseNames[outcome], seen(static_cast<AddressDrawReuse>(outcome)));
        }
        AgcDriver::ProfilePrint_nid_no_patch("%s\n", reuse);
        profile.addressSeen = address;
    }
    profile.totalsUs.fill(0);
    profile.maxUs.fill(0);
    profile.maxDrawUs = profile.hookWaitUs = profile.maxHookWaitUs = profile.ownSyncUs = 0;
    profile.bindingsUs = profile.uploadUs = profile.descriptorsUs = profile.otherUs = profile.addressOtherUs = 0;
    profile.addressBuilds = 0;
    profile.draws = profile.recorded = profile.waited = profile.completion = 0;
    profile.passesBegun = profile.passesContinued = 0;
    profile.reportedPassBreaks = profile.passBreaks;
    profile.reasons.fill(0);
    profile.recordedUs = profile.waitedUs = profile.synchronousUs = 0;
    profile.targetLookups = profile.slowLookups = 0;
    profile.slowLookupUs = 0;
    profile.fullScissorLookups = profile.partialLookups = profile.pagesWalked = 0;
    profile.lookupUs = 0;
    profile.kindUs.fill(0);
    profile.kindCounts.fill(0);
    profile.cacheHits = profile.cacheMisses = profile.cacheInvalidated = profile.uncacheable = 0;
    profile.validateHits = profile.validateMisses = 0;
    profile.recipeHits = 0;
    profile.recipeMisses.fill(0);
    profile.indirect.fill(0);
    profile.indirectRewritten = 0;
    profile.indirectReadUs = 0;
    profile.skips.fill(0);
    profile.skipUs.fill(0);
}

}
}
}
