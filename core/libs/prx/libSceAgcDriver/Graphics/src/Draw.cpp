#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
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
#include <set>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

std::uint32_t GuestFormatFor(VkFormat format, std::uint32_t elementBytes) {
    if (const auto guest = FindGuestColorTargetFormat(format, elementBytes)) return *guest;
    throw std::runtime_error("AGC graphics: no guest texture format matches the color buffer format " + std::to_string(static_cast<int>(format)));
}

// The color buffer as a single-mip 2D surface descriptor (tile mode SW_64KB_R_X).
GuestTextureResource SurfaceForTarget(const ColorTarget& color) {
    Require(color.tileMode == ColorTileMode::RenderTarget || color.tileMode == ColorTileMode::Standard4KB, "only 4 KiB standard and 64 KiB tiled color targets are resident");
    const bool chain = color.mipCount > 1;
    GuestTextureResource surface{};
    surface.baseAddress = chain ? color.surfaceAddress : color.address;
    surface.width = chain ? color.surfaceExtent.width : color.extent.width;
    surface.height = chain ? color.surfaceExtent.height : color.extent.height;
    surface.depthOrLastArray = color.depth - 1u;
    surface.baseArray = 0;
    surface.mipCount = color.mipCount;
    surface.baseLevel = 0;
    surface.lastLevel = color.mipCount - 1;
    surface.tileMode = ColorTextureTileMode(color.tileMode);
    surface.dimension = color.depth > 1 ? TextureDimension::k3D : TextureDimension::k2D;
    surface.format = GuestFormatFor(color.format, color.elementBytes);
    surface.dstSelX = 4;
    surface.dstSelY = 5;
    surface.dstSelZ = 6;
    surface.dstSelW = 7;
    surface.dccAddress = color.dccAddress;
    surface.dccAlphaOnMsb = color.dccAlphaOnMsb;
    return surface;
}

}

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

bool clearToTexel(StorageTexture& image, const std::array<std::byte, 16>& texel, std::uint32_t elementBytes, const char*& refusal) {
    std::array<std::byte, 16> repeated{};
    for (std::size_t offset = 0; offset + elementBytes <= repeated.size(); offset += elementBytes) std::memcpy(repeated.data() + offset, texel.data(), elementBytes);
    std::array<std::uint32_t, 4> pattern{};
    std::memcpy(pattern.data(), repeated.data(), repeated.size());
    return image.FillClear(std::span<const std::uint32_t, 4>(pattern), StorageTexture::WholeImage, refusal);
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
    MarkDccUncompressed(context, color.dccAddress, color.bytes);
}

void materializeRegisterClear(const Context& context, const ColorTarget& color, StorageTexture& resident) {
    if (color.dccAddress == 0 || resident.Descriptor().dccAddress != color.dccAddress) return;
    if (CurrentDccKeys(color.dccAddress, color.bytes) != DccKeys::ClearRegister) return;
    const auto texel = clearTexel(color, DccKeys::ClearRegister);
    const char* refusal = nullptr;
    bool cleared = clearToTexel(resident, texel, color.elementBytes, refusal);
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
        MarkDccUncompressed(context, color.dccAddress, color.bytes);
        return;
    }
    storeClearTexels(context, color, texel);
    resident.Refresh();
}

bool materializeCmaskClear(const Context& context, const ColorTarget& color, StorageTexture* resident) {
    if (color.cmaskAddress == 0) return false;
    const auto metadataBytes = static_cast<std::uint64_t>(color.cmaskBytes) * 256u;
    const auto state = CurrentDccKeys(color.cmaskAddress, metadataBytes);
    if (state == DccKeys::Uncompressed) return false;
    Require(color.dccAddress == 0, std::string("CMASK of a DCC color target that is not all expanded is not modeled (") + DccKeysName(state) + ")");
    Require(state == DccKeys::Clear0000, std::string("CMASK whose tiles are not all fast-cleared or all expanded is not modeled (") + DccKeysName(state) + ")");
    const auto texel = clearTexel(color, DccKeys::ClearRegister);
    const char* refusal = nullptr;
    if (resident == nullptr || !clearToTexel(*resident, texel, color.elementBytes, refusal)) {
        StorageTexture::FlushPending(color.address, color.bytes, nullptr, "fast-clear materialization");
        writeTexels(color, texel);
        if (resident != nullptr) resident->Refresh();
    }
    MarkDccUncompressed(context, color.cmaskAddress, metadataBytes);
    return true;
}

void imageBarrier(const Context& context, VkCommandBuffer commands, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void memoryBarrier(const Context& context, VkCommandBuffer commands, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags sourceAccess, VkAccessFlags destinationAccess) {
    const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, sourceAccess, destinationAccess};
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, sourceStage, destinationStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// The color surface as one untailed SW_64KB_R_X mip for the GPU detiler, with tightly packed linear rows.
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

// APS5_PROFILE_DRAW: per-draw phase timers in microseconds (a recorded draw's phases are far below
// the millisecond the old print rounded to), totalled over 10 s in the [draws] line.
enum DrawPhase : std::size_t { PhaseValidate, PhaseVertex, PhaseSetup, PhaseReadTarget, PhasePrepare, PhaseLookup, PhaseResources, PhasePipeline, PhaseRecord, PhaseKeep, PhaseSync, PhaseWriteBack, PhaseDescribe, PhaseCount };
constexpr std::array<const char*, PhaseCount> DrawPhaseNames{"validate", "vertex", "setup", "readTarget", "prepare", "lookup", "resources", "pipeline", "record", "keep", "sync", "writeBack", "describe"};

// Why a draw did not go into the recorder without a wait (counted in the [draws] line): its targets
// are not all resident, no recorder is active, a switch (APS5_SYNC_DRAWS, APS5_DUMP_TARGETS,
// APS5_SYNC_COMPLETION_DRAWS) forced it, or its completion work writes copied guest buffers or
// releases an address-based build's lease, which the CPU must not outrun (see `recorded` in Draw).
enum SyncReason : std::size_t { SyncNone, SyncNotResident, SyncNoRecorder, SyncDisabled, SyncCopiedWrites, SyncLease, SyncCount };
constexpr std::array<const char*, SyncCount> SyncReasonNames{"none", "non-resident target", "no recorder", "disabled", "copied writes", "lease"};
constexpr std::size_t IndirectPathCount = static_cast<std::size_t>(IndirectDrawPath::Count);
constexpr std::array<const char*, IndirectPathCount> IndirectDrawPathNames{"gpu-side", "patched SGPR not folded", "fetch offset unknown", "non-vertex path", "GE_INDX_OFFSET", "draw index", "vertex range too large", "feature gap", "pending image results", "pending label or copied write", "not imported", "disabled"};

// What became of one draw, for the totals.
struct DrawOutcome {
    // In the recorder; `waited` when the recorder was synced right after (copied writes or a lease).
    bool recorded = false;
    bool waited = false;
    bool completion = false;
    // A recorded draw began a render pass of its own, or continued the previous draw's.
    bool passBegun = false;
    bool passContinued = false;
    SyncReason reason = SyncNone;
    // Shader validation memo (see CachedFragmentOutputs).
    bool validateMemoized = false;
    bool validateHit = false;
    // A build that acquired the allocation registry lease (BDA), whose cost sits outside the
    // build's own sub-phases.
    bool addressBased = false;
    // Resident target lookups, and those that took a millisecond or more. Whether a slow one
    // re-uploaded the image or just walked its pages under contention is not visible from here
    // (StorageTexture::Version advances on every MarkDirty as well as on an upload, and Generation
    // is stamped afresh by every write-watch walk): the [texture] line's "reused" and "direct
    // uploads" counters and APS5_TRACE_UPLOAD name the actual uploads.
    std::uint64_t targetLookups = 0;
    std::uint64_t slowLookups = 0;
    double slowLookupUs = 0;
    // The lookups by whether the draw's scissor covers the whole target (an area-scoped proof
    // would change nothing for those), the pages their write-watch collects walked and their time.
    std::uint64_t fullScissorLookups = 0;
    std::uint64_t partialLookups = 0;
    std::uint64_t pagesWalked = 0;
    double lookupUs = 0;
    // How the draw's resources came about (DrawKind), for the per-kind averages.
    std::size_t kind = 0;
    // The flush hook's fence waits made inside this draw (index and vertex reads, the target
    // lookups' flushes, the build's texture lookups): nested in the draw's GpuMutex hold, so they
    // are the part of the hold that is waiting rather than working. The draw's own syncs (a
    // recorded-then-waited draw's recorder Sync, a synchronous draw's SubmitAndWait) are kept apart
    // in ownSyncUs: those wait by design and would otherwise count as nested hook waits.
    double hookWaitUs = 0;
    double ownSyncUs = 0;
};

// The resources of a draw: a recipe hit (reserved for the draw recipe step), a resource-cache
// template hit, a build, or an address-based (BDA) build.
enum DrawKind : std::size_t { KindRecipeHit, KindTemplateHit, KindBuild, KindBda, KindCount };
constexpr std::array<const char*, KindCount> DrawKindNames{"recipe hit", "template hit", "build", "BDA"};

struct DrawProfile {
    std::mutex mutex;
    std::array<double, KindCount> kindUs{};
    std::array<std::uint64_t, KindCount> kindCounts{};
    std::uint64_t fullScissorLookups = 0;
    std::uint64_t partialLookups = 0;
    std::uint64_t pagesWalked = 0;
    double lookupUs = 0;
    std::array<double, PhaseCount> totalsUs{};
    // The longest single draw's time per phase, and the longest draw: the [lock] line's 'draw'
    // hold max (tens of ms against an average well under a millisecond) needs a phase name.
    std::array<double, PhaseCount> maxUs{};
    double maxDrawUs = 0;
    double hookWaitUs = 0;
    double maxHookWaitUs = 0;
    double ownSyncUs = 0;
    // The resources build's sub-phases (ShaderResources::Timing) of draws only; dispatches report
    // theirs in the [resources] line. `other` is the rest of the build (prepareAddressBindings:
    // the registry lease and snapshots of an address-based build, the BDA table, the layout
    // lookup), and how much of it address-based builds account for.
    double bindingsUs = 0;
    double uploadUs = 0;
    double descriptorsUs = 0;
    double otherUs = 0;
    double addressOtherUs = 0;
    std::uint64_t addressBuilds = 0;
    std::uint64_t draws = 0;
    std::uint64_t recorded = 0;
    std::uint64_t waited = 0;
    // Recorded draws whose write-back (BDA fault check) runs as a completion action instead of
    // making the draw synchronous (see APS5_SYNC_COMPLETION_DRAWS).
    std::uint64_t completion = 0;
    // Render passes recorded draws began, and draws that continued the previous draw's pass.
    std::uint64_t passesBegun = 0;
    std::uint64_t passesContinued = 0;
    std::array<std::uint64_t, SyncCount> reasons{};
    // The CPU inside Graphics::Draw, split by outcome: a synchronous draw costs ten times a recorded
    // one, so one average would only show the synchronous population.
    double recordedUs = 0;
    double waitedUs = 0;
    double synchronousUs = 0;
    std::uint64_t targetLookups = 0;
    std::uint64_t slowLookups = 0;
    double slowLookupUs = 0;
    // Resource cache outcomes: hits, misses (built and inserted when reusable), entries that failed
    // Revalidate, and draws that could not use the cache (synchronous, no variant id, or disabled).
    // Which key words the misses differ in is the cache's own "[rescache] miss churn" line
    // (ResourceCache::noteMiss compares each miss with the last key of the same variants).
    std::uint64_t cacheHits = 0;
    std::uint64_t cacheMisses = 0;
    std::uint64_t cacheInvalidated = 0;
    std::uint64_t uncacheable = 0;
    // Shader validation memo (see CachedFragmentOutputs).
    std::uint64_t validateHits = 0;
    std::uint64_t validateMisses = 0;
    // Draw recipe outcomes (DrawWithRecipe): hits, and misses by DrawRecipeMiss.
    std::uint64_t recipeHits = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(DrawRecipeMiss::Count)> recipeMisses{};
    // Indirect draws by path (IndirectDrawPath), the GPU-side ones whose records were rewritten
    // with a constant, and the time of the CPU record reads (their syncs).
    std::array<std::uint64_t, IndirectPathCount> indirect{};
    std::uint64_t indirectRewritten = 0;
    double indirectReadUs = 0;
    // Draw packets that drew nothing, by DrawSkip, and their time.
    std::array<std::uint64_t, static_cast<std::size_t>(DrawSkip::Count)> skips{};
    std::array<double, static_cast<std::size_t>(DrawSkip::Count)> skipUs{};
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};
constexpr std::array<const char*, static_cast<std::size_t>(DrawSkip::Count)> DrawSkipNames{"nothing to draw", "prechecked", "thrown"};

DrawProfile& Profile() {
    static DrawProfile profile;
    return profile;
}

// Adds one draw's phases to the totals and prints the [draws] and [rescache] lines every 10 s.
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
    if (outcome.passContinued) ++profile.passesContinued;
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
    int n = std::snprintf(line, sizeof(line), "[draws] %llu draws over 10 s (%llu recorded avg %.0f us, of them %llu with completion; %llu recorded then waited avg %.0f us; %llu synchronous avg %.0f us; render passes %llu begun, %llu draws continued one; waited or synchronous because:", static_cast<unsigned long long>(profile.draws), static_cast<unsigned long long>(profile.recorded), average(profile.recordedUs, profile.recorded), static_cast<unsigned long long>(profile.completion), static_cast<unsigned long long>(profile.waited), average(profile.waitedUs, profile.waited), static_cast<unsigned long long>(synchronous), average(profile.synchronousUs, synchronous), static_cast<unsigned long long>(profile.passesBegun), static_cast<unsigned long long>(profile.passesContinued));
    const auto room = [&] { return n > 0 && static_cast<std::size_t>(n) < sizeof(line); };
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
    // The longest draw and the longest single phase of any draw (which phase a long 'draw' hold
    // was), plus the hook waits nested inside the draws.
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; longest draw %.1f ms, longest phase of any draw:", profile.maxDrawUs / 1000.0);
    for (std::size_t i = 0; i < PhaseCount && room(); ++i) {
        if (profile.maxUs[i] < 500.0) continue;
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %.1f", DrawPhaseNames[i], profile.maxUs[i] / 1000.0);
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; hook waits inside draws %.1f ms (max %.1f; the draws' own syncs, %.1f ms, not counted)", profile.hookWaitUs / 1000.0, profile.maxHookWaitUs / 1000.0, profile.ownSyncUs / 1000.0);
    std::uint64_t indirectCpu = 0;
    for (std::size_t i = 1; i < IndirectPathCount; ++i) indirectCpu += profile.indirect[i];
    if ((profile.indirect[0] != 0 || indirectCpu != 0) && room()) {
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; indirect: gpu-side %llu (%llu rewritten), cpu-side %llu (", static_cast<unsigned long long>(profile.indirect[0]), static_cast<unsigned long long>(profile.indirectRewritten), static_cast<unsigned long long>(indirectCpu));
        for (std::size_t i = 1; i < IndirectPathCount && room(); ++i) {
            if (profile.indirect[i] != 0) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %llu", IndirectDrawPathNames[i], static_cast<unsigned long long>(profile.indirect[i]));
        }
        if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "), argument reads %.1f ms", profile.indirectReadUs / 1000.0);
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; skipped packets:");
    for (std::size_t i = 0; i < profile.skips.size() && room(); ++i) {
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %llu in %.1f ms", DrawSkipNames[i], static_cast<unsigned long long>(profile.skips[i]), profile.skipUs[i] / 1000.0);
    }
    if (room()) n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), "; recipe hits %llu, misses by reason:", static_cast<unsigned long long>(profile.recipeHits));
    for (std::size_t i = 1; i < profile.recipeMisses.size() && room(); ++i) {
        n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s %llu", DrawRecipeMissName(static_cast<DrawRecipeMiss>(i)), static_cast<unsigned long long>(profile.recipeMisses[i]));
    }
    std::fprintf(stderr, "%s\n", line);
    std::fprintf(stderr, "[rescache] draws: %llu hits, %llu misses, %llu invalidated, %llu uncacheable; validation memo %llu hits / %llu misses (which key words the misses differ in: the miss churn line)\n", static_cast<unsigned long long>(profile.cacheHits), static_cast<unsigned long long>(profile.cacheMisses), static_cast<unsigned long long>(profile.cacheInvalidated), static_cast<unsigned long long>(profile.uncacheable), static_cast<unsigned long long>(profile.validateHits), static_cast<unsigned long long>(profile.validateMisses));
    profile.totalsUs.fill(0);
    profile.maxUs.fill(0);
    profile.maxDrawUs = profile.hookWaitUs = profile.maxHookWaitUs = profile.ownSyncUs = 0;
    profile.bindingsUs = profile.uploadUs = profile.descriptorsUs = profile.otherUs = profile.addressOtherUs = 0;
    profile.addressBuilds = 0;
    profile.draws = profile.recorded = profile.waited = profile.completion = 0;
    profile.passesBegun = profile.passesContinued = 0;
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

void countCache(std::uint64_t DrawProfile::*counter) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    ++(stats.*counter);
}

// ValidateShaders decodes every SPIR-V instruction of every stage on every draw. Its outcome depends
// only on the stages' compiled variants (a variant id names identical SPIR-V and binding layout),
// their push constant placement and vertex attribute shapes, and the state fields it checks, so the
// fragment output set is remembered by exactly those. Nothing here is keyed by pointer: the results
// a draw hands in live in a per-draw vector. A stage without a variant id is validated as before,
// except the rect-list control and evaluation stages, which are generated from the vertex and
// fragment results the key already names (the pipeline key treats them the same way). `memoized`
// says whether the memo applied, `hit` whether it answered. APS5_NO_VALIDATE_CACHE=1 validates
// every draw.
bool ValidationKey(const Context& context, std::span<const CompiledShader> shaders, const State& state, std::vector<std::uint64_t>& key) {
    using Stage = ShaderRecompiler::ShaderStage;
    static const bool disabled = std::getenv("APS5_NO_VALIDATE_CACHE") != nullptr;
    const auto add = [&](auto value) { key.push_back(static_cast<std::uint64_t>(value)); };
    bool keyed = !disabled;
    if (keyed) {
        key.reserve(40 + shaders.size() * 12);
        add(shaders.size());
        for (const auto& shader : shaders) {
            Require(shader.program != nullptr, "missing compiled shader");
            const auto& program = *shader.program;
            const bool generated = state.rectList && (shader.stage == Stage::TessellationControl || shader.stage == Stage::TessellationEvaluation);
            if (!generated && program.variantId == 0) {
                keyed = false;
                break;
            }
            add(shader.stage);
            add(generated ? std::uint64_t{0} : program.variantId);
            add(shader.pushConstantOffset);
            add(program.pushConstants.size());
            add(program.bdaAbiVersion);
            add(program.vertexAttributes.size());
            for (const auto& attribute : program.vertexAttributes) {
                add(attribute.location);
                add(attribute.components);
                add(attribute.fetchIndex);
                // The data format bits of the V# decide the attribute's signature.
                add((attribute.resource.fields[3] >> 12u) & 0x7fu);
            }
        }
    }
    if (keyed) {
        add(state.stages.path);
        add(state.rectList);
        add(state.topology);
        add(state.cullMode);
        add(state.blends.size());
        add(context.subgroup.subgroupSize);
        add(context.subgroup.supportedStages);
        add(context.subgroup.supportedOperations);
        add(context.fragmentShaderBarycentric);
        add(state.stages.mesh.has_value());
        if (state.stages.mesh) {
            const auto& mesh = *state.stages.mesh;
            add(mesh.inputPrimitive);
            add(mesh.primitivesPerGroup);
            add(mesh.verticesPerGroup);
            add(mesh.maxVertices);
            add(mesh.maxPrimitives);
            add(mesh.threadsPerGroup);
            add(mesh.ldsSizeDwords);
            add(mesh.provokingVertex);
        }
        add(state.stages.tessellation.has_value());
        if (state.stages.tessellation) {
            const auto& tessellation = *state.stages.tessellation;
            add(tessellation.inputControlPoints);
            add(tessellation.outputControlPoints);
            add(tessellation.domain);
            add(tessellation.partitioning);
            add(tessellation.outputTopology);
        }
    }
    return keyed;
}

std::mutex& validationMutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<std::vector<std::uint64_t>, std::string>& validationFailures() {
    static std::map<std::vector<std::uint64_t>, std::string> failures;
    return failures;
}

std::set<std::uint32_t> CachedFragmentOutputs(const Context& context, std::span<const CompiledShader> shaders, const State& state, bool& memoized, bool& hit) {
    memoized = false;
    hit = false;
    std::vector<std::uint64_t> key;
    const bool keyed = ValidationKey(context, shaders, state, key);
    static std::map<std::vector<std::uint64_t>, std::set<std::uint32_t>> memo;
    if (keyed) {
        memoized = true;
        std::lock_guard lock(validationMutex());
        if (const auto found = memo.find(key); found != memo.end()) {
            hit = true;
            return found->second;
        }
    }
    std::set<std::uint32_t> outputs;
    try {
        outputs = ValidateShaders(shaders, state, context.subgroup, context.fragmentShaderBarycentric, context.descriptorIndexing, context.imageInt64Atomics, context.geometryShader, context.sampleRateShading);
    } catch (const std::exception& error) {
        if (keyed) {
            std::lock_guard lock(validationMutex());
            auto& failures = validationFailures();
            if (failures.size() >= 1024) failures.clear();
            failures.emplace(std::move(key), error.what());
        }
        throw;
    }
    if (keyed) {
        std::lock_guard lock(validationMutex());
        // A handful of configurations recur; a runaway key space is dropped wholesale.
        if (memo.size() >= 1024) memo.clear();
        memo.emplace(std::move(key), outputs);
    }
    return outputs;
}

// The resource cache key of a recorded draw: a marker no compute key starts with (those begin with
// the stage), the device handle (the cache is process-wide and the driver replaces the headless
// device with the windowed one while workers may still use the old one: an entry's descriptor set
// and pooled buffers belong to the device that built it) and every stage's content key
// (ResourceCache::noteMiss walks this layout to attribute misses, so it changes together with it).
// With `ranges`, also the render target and the index buffer the build's alias checks compared the
// guest buffers against; without it the checks are repeated for a hit (CheckBufferAliases), since
// neither range is part of the descriptor set (the target is attached, the index buffer copied per
// draw), so a target or index ring that moves per frame does not miss on every draw. That guards a
// workload with such rings; in the profiled menu stage every draw was DRAW_INDEX_AUTO (index range
// 0) onto one fixed target, so its misses come from the stages' descriptor words themselves.
bool MovableBuffers() {
    static const bool enabled = std::getenv("APS5_NO_MOVED_BUFFER_TEMPLATES") == nullptr;
    return enabled;
}

ResourceCache::Key DrawResourceKey(const Context& context, std::span<const CompiledShader> shaders, const ColorTarget& target, std::uint64_t indexAddress, std::uint64_t indexBytes, bool ranges) {
    ResourceCache::Key key{0xffffffffu};
    const auto append64 = [&](std::uint64_t value) {
        key.push_back(static_cast<std::uint32_t>(value));
        key.push_back(static_cast<std::uint32_t>(value >> 32u));
    };
    append64(reinterpret_cast<std::uint64_t>(context.device));
    key.push_back(static_cast<std::uint32_t>(shaders.size()));
    for (const auto& shader : shaders) {
        const auto part = ShaderResources::ContentKey(shader, true, MovableBuffers());
        key.push_back(static_cast<std::uint32_t>(part.size()));
        key.insert(key.end(), part.begin(), part.end());
    }
    if (ranges) {
        append64(target.address);
        append64(target.bytes);
        append64(indexAddress);
        append64(indexBytes);
    }
    return key;
}

// The alias checks the build makes for every guest buffer descriptor (ShaderResources::
// addGuestBuffer), repeated for a cached build whose own checks compared the buffers against
// another draw's render target and index buffer. The same conditions and messages, so a draw that
// would have failed its build fails its hit.
void CheckBufferAliases(std::span<const CompiledShader> shaders, const ColorTarget& target, std::uint64_t indexAddress, std::uint64_t indexBytes) {
    const auto overlap = [](std::uint64_t first, std::uint64_t firstSize, std::uint64_t second, std::uint64_t secondSize) { return first < second + secondSize && second < first + firstSize; };
    for (const auto& shader : shaders) {
        for (const auto& binding : shader.program->bindings) {
            if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
            const auto& words = binding.guestDescriptor;
            for (std::size_t offset = 0; offset + 4 <= words.size(); offset += 4) {
                const ShaderRecompiler::ShaderBufferResource descriptor{{words[offset], words[offset + 1], words[offset + 2], words[offset + 3]}};
                const auto address = descriptor.Base48();
                const auto size = descriptor.GetSize();
                if (size == 0 || address == 0) continue;
                const auto element = offset / 4;
                const bool written = element >= binding.bufferWritten.size() || binding.bufferWritten[element];
                Require(!overlap(address, size, target.address, target.bytes), "shader buffer aliases the render target");
                Require(!written || !overlap(address, size, indexAddress, indexBytes), "writable shader buffer aliases the index buffer");
            }
        }
    }
}

}

std::array<std::uint32_t, 4> MeshIndexBufferDescriptor(const Pm4::DrawParameters& draw) {
    constexpr std::uint32_t RawWord3 = 0x31016facu;
    if (!draw.indexed) return {0u, 0u, 0u, RawWord3};
    const auto address = draw.indexAddress;
    const auto bytes = (static_cast<std::uint64_t>(draw.indexCount) * draw.indexSize + 3u) & ~std::uint64_t{3};
    Require(address != 0 && bytes != 0 && bytes <= 0xffffffffu && (address >> 48u) == 0, "invalid mesh index buffer range");
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) & 0xffffu, static_cast<std::uint32_t>(bytes), RawWord3};
}

MeshArgumentRules MeshArgumentRulesFor(const Context& context, const ShaderRecompiler::MeshConfiguration& mesh, std::uint32_t indexCount) {
    const auto inputSize = mesh.inputPrimitive == 1 ? 1u : mesh.inputPrimitive == 2 ? 2u : 3u;
    const auto step = mesh.inputPrimitive == 5 || mesh.inputPrimitive == 6 ? 1u : inputSize;
    Require(mesh.primitivesPerGroup != 0, "mesh draw contains no complete primitive");
    return {indexCount, inputSize, step, mesh.primitivesPerGroup, context.meshLimits.maxMeshWorkGroupCount[0], context.meshLimits.maxMeshWorkGroupCount[1], context.meshLimits.maxMeshWorkGroupTotalCount};
}

MeshArguments ResolveMeshArguments(const Pm4::DrawArguments& record, const MeshArgumentRules& rules) {
    const auto first = record.firstVertexOrIndex;
    const auto effective = first < rules.indexCount ? std::min(record.count, rules.indexCount - first) : 0u;
    std::uint32_t groups = 0;
    if (effective >= rules.inputSize && record.instances != 0) {
        groups = (effective - rules.inputSize) / rules.step / rules.primitivesPerGroup + 1u;
        if (groups > rules.maxGroups || record.instances > rules.maxInstances || groups > rules.maxTotal / record.instances) groups = 0;
    }
    return {groups, groups != 0 ? record.instances : 0u, groups != 0 ? 1u : 0u, effective, first};
}

std::shared_ptr<std::vector<std::shared_ptr<ShaderResources>>> DrawCopiedWriters() {
    // Never destroyed: a completion action of a batch still in flight at static teardown may erase from it.
    static auto* const writers = new std::shared_ptr<std::vector<std::shared_ptr<ShaderResources>>>(std::make_shared<std::vector<std::shared_ptr<ShaderResources>>>());
    return *writers;
}

const char* IndirectDrawPathName(IndirectDrawPath path) {
    return IndirectDrawPathNames[static_cast<std::size_t>(path)];
}

void CountIndirectDraw(IndirectDrawPath path, double readMs, bool rewritten) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    ++stats.indirect[static_cast<std::size_t>(path)];
    if (rewritten) ++stats.indirectRewritten;
    stats.indirectReadUs += readMs * 1000.0;
}

void CountDrawSkip(DrawSkip kind, double us) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& stats = Profile();
    std::lock_guard lock(stats.mutex);
    ++stats.skips[static_cast<std::size_t>(kind)];
    stats.skipUs[static_cast<std::size_t>(kind)] += us;
}


bool DrawRecipes() {
    static const bool noDrawRecipe = std::getenv("APS5_NO_DRAW_RECIPE") != nullptr;
    return !noDrawRecipe;
}

const char* DrawRecipeMissName(DrawRecipeMiss miss) {
    constexpr std::array<const char*, static_cast<std::size_t>(DrawRecipeMiss::Count)> names{"none", "not recordable", "target gone", "template gone", "objects gone", "proof"};
    return names[static_cast<std::size_t>(miss)];
}

namespace {

// APS5_PROFILE_DRAW: the per-phase timers of one draw (DrawPhase), shared by Draw's parts and
// DrawWithRecipe.
struct DrawTimer {
    bool profile;
    std::chrono::steady_clock::time_point phaseStart;
    std::array<double, PhaseCount> us{};
    explicit DrawTimer(bool profile) : profile(profile), phaseStart(std::chrono::steady_clock::now()) {}
    void phase(DrawPhase which) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        us[which] += std::chrono::duration<double, std::micro>(now - phaseStart).count();
        phaseStart = now;
    }
};

// One line per draw with the phases that took time (APS5_TRACE_DRAWS), and the 10 s totals.
// `waitedBefore` is the thread's GPU waits at the draw's start and `ownWaitedMs` the draw's own
// syncs (see DrawOutcome::hookWaitUs).
void reportDrawEnd(const State& state, const DrawTimer& timer, const ShaderResources::BuildTiming* built, DrawOutcome& outcome, double waitedBefore, double ownWaitedMs, const char* suffix) {
    if (!timer.profile) return;
    static const bool traceDraws = std::getenv("APS5_TRACE_DRAWS") != nullptr;
    outcome.hookWaitUs = std::max(0.0, Recorder::ThreadWaitedMs() - waitedBefore - ownWaitedMs) * 1000.0;
    outcome.ownSyncUs = ownWaitedMs * 1000.0;
    if (traceDraws) {
        char line[512];
        int n = std::snprintf(line, sizeof(line), "[draw] %ux%u %zu targets%s:", state.renderExtent.width, state.renderExtent.height, state.colors.size(), suffix);
        for (std::size_t i = 0; i < PhaseCount && n > 0 && static_cast<std::size_t>(n) < sizeof(line); ++i) {
            if (timer.us[i] <= 0) continue;
            n += std::snprintf(line + n, sizeof(line) - static_cast<std::size_t>(n), " %s=%.0fus", DrawPhaseNames[i], timer.us[i]);
        }
        std::fprintf(stderr, "%s\n", line);
    }
    reportDraw(timer.us, built, outcome);
}

}

DrawInputCopy CopyDrawInput(const Context& context, Recorder* recorder, std::uint64_t address, std::size_t bytes, std::size_t alignment, Recorder::SnapshotUse use) {
    Require(use != Recorder::SnapshotUse::Storage, "a draw input is a vertex or index buffer");
    DrawInputCopy copy;
    if (recorder != nullptr && bytes != 0) {
        GuestMemory::FlushGpuWrites(address, bytes);
        copy.registryGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        copy.generation = GuestMemory::CollectWrites(address, bytes);
        if (copy.generation != 0) copy.buffer = recorder->ReusableDrawSnapshot(address, bytes, use, &copy.derived);
        if (copy.buffer != nullptr) {
            copy.reused = true;
            return copy;
        }
    }
    copy.buffer = std::make_shared<Buffer>(context, bytes, use == Recorder::SnapshotUse::Vertex ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    GuestMemory::Read(address, copy.buffer->Bytes(), alignment);
    return copy;
}

void KeepDrawInput(Recorder* recorder, std::uint64_t address, const DrawInputCopy& copy, Recorder::SnapshotUse use, std::uint32_t derived) {
    if (recorder == nullptr || copy.reused || copy.generation == 0 || copy.buffer == nullptr) return;
    recorder->KeepDrawSnapshot(address, copy.buffer->Bytes().size(), copy.generation, copy.registryGeneration, copy.buffer, use, derived);
}

namespace {

// The draw's inputs before its resources (prepareDrawInputs): the validated parameters, the index
// buffer copy with its highest index, the vertex buffer copies and their layout, the fragment
// outputs and the pipeline stages.
struct DrawInputs {
    // An empty auto draw: nothing to record.
    bool nothing = false;
    std::uint64_t indexBytes = 0;
    std::shared_ptr<Buffer> indices;
    std::uint32_t maxIndex = 0;
    VertexInputLayout vertexInput;
    std::vector<std::shared_ptr<Buffer>> vertexBuffers;
    std::vector<VkBuffer> vertexHandles;
    std::vector<VkDeviceSize> vertexOffsets;
    std::set<std::uint32_t> fragmentOutputs;
    VkPipelineStageFlags shaderStages = 0;
    std::uint32_t meshGroups = 0;
};

// Draw's validation, index and vertex phases. With `recipe` the fragment outputs, the pipeline
// stages and the vertex input layout are the recipe's (derived from the same compiled stages)
// instead of computed.
DrawInputs prepareDrawInputs(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, DrawOutcome& outcome, DrawTimer& timer, const DrawRecipe* recipe) {
    DrawInputs inputs;
    APS5_LOG_OUT_DEBUG("Draw indices=%u instances=%u indexSize=%u flags=%u indexAddress=0x%llx", draw.indexCount, draw.instanceCount, draw.indexSize, draw.flags, static_cast<unsigned long long>(draw.indexAddress));
    APS5_LOG_OUT_DEBUG("State colorTarget=%u render=%ux%u colorAddress=0x%llx colorBytes=%llu colorExtent=%ux%u", state.hasColorTarget ? 1u : 0u, state.renderExtent.width, state.renderExtent.height, static_cast<unsigned long long>(state.color.address), static_cast<unsigned long long>(state.color.bytes), state.color.extent.width, state.color.extent.height);
    APS5_LOG_OUT_DEBUG("Viewport x=%f y=%f w=%f h=%f minDepth=%f maxDepth=%f", state.viewport.x, state.viewport.y, state.viewport.width, state.viewport.height, state.viewport.minDepth, state.viewport.maxDepth);
    APS5_LOG_OUT_DEBUG("Scissor x=%d y=%d w=%u h=%u topology=%u cullMode=0x%x frontFace=%u", state.scissor.offset.x, state.scissor.offset.y, state.scissor.extent.width, state.scissor.extent.height, static_cast<unsigned>(state.topology), static_cast<unsigned>(state.cullMode), static_cast<unsigned>(state.frontFace));
    // An indirect draw: the counts live in guest memory records (Pm4::DrawParameters::IndirectDraw),
    // read by the GPU from the host import or, when the GPU could not see their current bytes, by
    // the CPU (Draw's `records`). The driver resolved every non-vertex-path draw before this.
    const auto* args = draw.indirect ? &*draw.indirect : nullptr;
    Require(draw.indexed ? draw.flags == 0 : (draw.flags & ~0x20u) == 0, "draw modifiers are unsupported");
    if (draw.indexed) {
        Require(draw.indexSize == 2 || draw.indexSize == 4, "only uint16 and uint32 index buffers are supported");
    } else {
        Require(draw.indexAddress == 0 && draw.indexSize == 0, "auto draw must not reference an index buffer");
        if (args == nullptr) {
            if (draw.indexCount == 0 || draw.instanceCount == 0) {
                inputs.nothing = true;
                return inputs;
            }
            Require(draw.firstVertex <= std::numeric_limits<std::uint32_t>::max() - (draw.indexCount - 1u), "auto draw vertex range overflow");
            Require(draw.firstInstance <= std::numeric_limits<std::uint32_t>::max() - (draw.instanceCount - 1u), "auto draw instance range overflow");
        }
    }
    if (args == nullptr) Require(draw.indexCount != 0 && draw.instanceCount != 0, "zero-count indexed draws are unsupported");
    else Require((!state.stages.mesh || draw.indexed) && !state.stages.tessellation && !state.rectList, "indirect draw on a non-vertex path must be resolved by the driver");
    inputs.indexBytes = static_cast<std::uint64_t>(draw.indexCount) * draw.indexSize;
    const auto indexBytes = inputs.indexBytes;
    APS5_LOG_OUT_DEBUG("Index buffer bytes=%llu", static_cast<unsigned long long>(indexBytes));
    Require(indexBytes <= std::numeric_limits<std::size_t>::max(), "index buffer size overflow");
    if (draw.indexed) GuestMemory::CheckRange(reinterpret_cast<const void*>(draw.indexAddress), static_cast<std::size_t>(indexBytes), draw.indexSize);
    APS5_LOG_CHARS_OUT_DEBUG("Index buffer range OK");
    Require(!draw.indexed || !state.hasColorTarget || draw.indexAddress + indexBytes <= state.color.address || state.color.address + state.color.bytes <= draw.indexAddress, "index buffer aliases the render target");
    if (state.rectList) Require(draw.indexCount % 3 == 0, "incomplete rect-list primitive");
    APS5_LOG_CHARS_OUT_DEBUG("ValidateShaders");
    if (recipe != nullptr) {
        inputs.fragmentOutputs = recipe->fragmentOutputs;
        inputs.shaderStages = recipe->shaderStages;
    } else {
        inputs.fragmentOutputs = CachedFragmentOutputs(context, shaders, state, outcome.validateMemoized, outcome.validateHit);
        inputs.shaderStages = PipelineStages(shaders);
    }
    APS5_LOG_CHARS_OUT_DEBUG("ValidateShaders OK");
    APS5_LOG_OUT_DEBUG("PipelineStages=0x%x", static_cast<unsigned>(inputs.shaderStages));
    if (state.stages.mesh) {
        APS5_LOG_CHARS_OUT_DEBUG("Mesh path");
        Require(context.meshShader, "device does not support mesh shaders");
        const auto& mesh = *state.stages.mesh;
        const auto inputSize = mesh.inputPrimitive == 1 ? 1u : mesh.inputPrimitive == 2 ? 2u : 3u;
        Require(draw.indexCount >= inputSize && mesh.primitivesPerGroup != 0, "mesh draw contains no complete primitive");
        if (args == nullptr) {
            const auto step = mesh.inputPrimitive == 5 || mesh.inputPrimitive == 6 ? 1u : inputSize;
            const auto primitives = (draw.indexCount - inputSize) / step + 1u;
            inputs.meshGroups = (primitives - 1u) / mesh.primitivesPerGroup + 1u;
            APS5_LOG_OUT_DEBUG("Mesh primitives=%u groups=%u", primitives, inputs.meshGroups);
            Require(inputs.meshGroups <= context.meshLimits.maxMeshWorkGroupCount[0] && draw.instanceCount <= context.meshLimits.maxMeshWorkGroupCount[1] && static_cast<std::uint64_t>(inputs.meshGroups) * draw.instanceCount <= context.meshLimits.maxMeshWorkGroupTotalCount, "mesh draw exceeds workgroup count limits");
        }
    }
    if (state.stages.tessellation) Require(draw.indexCount % state.stages.tessellation->inputControlPoints == 0, "incomplete tessellation patch");
    // Viewport and scissor are dynamic pipeline state, so their limits are checked here per draw.
    ValidateViewport(context, state.viewport);
    ValidateDepthBounds(context, state);
    timer.phase(PhaseValidate);
    inputs.maxIndex = draw.indexed ? 0u : draw.firstVertex + draw.indexCount - 1u;
    if (draw.indexed) {
        const auto use = draw.indexSize == 2 ? Recorder::SnapshotUse::Index16 : Recorder::SnapshotUse::Index32;
        auto copy = CopyDrawInput(context, context.recorder, draw.indexAddress, static_cast<std::size_t>(indexBytes), draw.indexSize, use);
        std::uint32_t highest = copy.derived;
        if (!copy.reused) {
            highest = 0;
            const auto bytes = copy.buffer->Bytes();
            for (std::size_t offset = 0; offset < indexBytes; offset += draw.indexSize) {
                std::uint32_t index = 0;
                if (draw.indexSize == 2) {
                    std::uint16_t value = 0;
                    std::memcpy(&value, bytes.data() + offset, sizeof(value));
                    index = value;
                } else {
                    std::memcpy(&index, bytes.data() + offset, sizeof(index));
                }
                highest = std::max(highest, index);
            }
            KeepDrawInput(context.recorder, draw.indexAddress, copy, use, highest);
        }
        Require(highest <= context.limits.maxDrawIndexedIndexValue, "index exceeds the device's indexed draw limit");
        Require(!state.stages.mesh || state.stages.mesh->inputPrimitive != 5 || !state.primitiveRestart || highest != (draw.indexSize == 2 ? 0xffffu : 0xffffffffu), "primitive restart in a triangle fan geometry draw is unsupported");
        inputs.maxIndex = highest;
        inputs.indices = std::move(copy.buffer);
    }
    APS5_LOG_CHARS_OUT_DEBUG("Index validation OK");
    const auto& attributes = shaders.front().program->vertexAttributes;
    // Validates the vertex descriptors; the layout also keys and builds the pipeline.
    if (recipe != nullptr) inputs.vertexInput = recipe->vertexInput;
    else inputs.vertexInput = BuildVertexInputLayout(context, attributes);
    inputs.vertexOffsets.assign(attributes.size(), 0);
    // An indexed draw's vertex offset moves every fetch: the copy must reach the last one.
    if (draw.indexed) {
        Require(draw.firstVertex <= std::numeric_limits<std::uint32_t>::max() - inputs.maxIndex, "indexed draw vertex range overflow");
        inputs.maxIndex += draw.firstVertex;
    }
    std::vector<VertexFetch> fetches;
    fetches.reserve(attributes.size());
    for (const auto& attribute : attributes) {
        // An indirect draw's counts are unknown here: the descriptor's whole range is copied.
        const auto bytes = args != nullptr ? VertexBufferExtent(attribute) : VertexBufferReadSize(attribute, inputs.maxIndex, draw.instanceCount, draw.firstInstance);
        const auto& fields = attribute.resource.fields;
        const auto address = fields[0] | (static_cast<std::uint64_t>(fields[1] & 0xffffu) << 32u);
        Require(!state.hasColorTarget || address + bytes <= state.color.address || state.color.address + state.color.bytes <= address, "vertex buffer aliases the render target");
        fetches.push_back({address, address + bytes, (fields[1] >> 16u) & 0x3fffu, attribute.fetchIndex, DecodeVertexFormat(attribute).alignment});
    }
    const auto plan = PlanVertexCopies(fetches);
    for (const auto& [begin, end] : plan.copies) {
        const auto bytes = static_cast<std::size_t>(end - begin);
        GuestMemory::CheckRange(reinterpret_cast<const void*>(begin), bytes, 1);
        auto copy = CopyDrawInput(context, context.recorder, begin, bytes, 1, Recorder::SnapshotUse::Vertex);
        KeepDrawInput(context.recorder, begin, copy, Recorder::SnapshotUse::Vertex, 0);
        inputs.vertexBuffers.push_back(std::move(copy.buffer));
    }
    for (std::size_t i = 0; i < attributes.size(); ++i) {
        inputs.vertexHandles.push_back(inputs.vertexBuffers[plan.copyOf[i]]->Handle());
        inputs.vertexOffsets[i] = plan.offsets[i];
    }
    timer.phase(PhaseVertex);
    return inputs;
}

// The resident-target proof of one attachment (StorageTexture::Refresh: FlushPending, CollectWrites
// over the target's pages, the DCC key scan of TextureClearKeys, then UnchangedSince), with the
// [draws] target-lookup accounting. `lookup` makes (or finds) the image; DrawWithRecipe refreshes
// the stored object instead.
std::shared_ptr<StorageTexture> refreshResidentTarget(const Context& context, const State& state, const ColorTarget& color, DrawOutcome& outcome, bool profile, const std::function<std::shared_ptr<StorageTexture>()>& lookup) {
    const auto lookupStart = std::chrono::steady_clock::now();
    const auto walkedBefore = profile ? GuestMemory::ThreadCollectedBytes() : 0;
    std::shared_ptr<StorageTexture> resident;
    try {
        resident = lookup();
        if (resident != nullptr) materializeRegisterClear(context, color, *resident);
    } catch (const std::exception& error) {
        static std::mutex reportMutex;
        static std::set<std::uint64_t> reported;
        std::lock_guard lock(reportMutex);
        if (reported.insert(color.address).second) std::fprintf(stderr, "[gpu] color target 0x%llx stays non-resident: %s\n", static_cast<unsigned long long>(color.address), error.what());
    }
    if (resident != nullptr) materializeCmaskClear(context, color, resident.get());
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

ResolvedResources resolveDrawResources(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots, std::uint64_t indexBytes, bool recordable, DrawOutcome& outcome, DrawTimer& timer) {
    ResolvedResources resolved;
    APS5_LOG_CHARS_OUT_DEBUG("Creating ShaderResources");
    // A recordable draw whose stages' compiled content repeats an earlier one binds that build's
    // descriptor set when it is still valid (see ResourceCache; the dispatch path does the same).
    // Only recordable draws take part: a synchronous draw writes its resources back, which a shared
    // object must never do. Revalidate proves a hit by texture identity, which needs the texture
    // caches. Push constants still come from this draw's stages. APS5_NO_DRAW_RESOURCE_CACHE=1
    // builds every draw's resources as before.
    static const bool noDrawResourceCache = std::getenv("APS5_NO_DRAW_RESOURCE_CACHE") != nullptr;
    static const bool noTextureCache = std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    // The render target and index ranges stay out of the key (see DrawResourceKey); a hit repeats
    // the alias checks instead. Debug aid: APS5_NO_DRAW_KEY_TRIM=1 keys them as before.
    static const bool trimKey = std::getenv("APS5_NO_DRAW_KEY_TRIM") == nullptr;
    resolved.cacheable = recordable && !noDrawResourceCache && !noTextureCache && std::all_of(shaders.begin(), shaders.end(), [](const CompiledShader& shader) { return shader.program != nullptr && shader.program->variantId != 0; });
    if (resolved.cacheable) {
        resolved.contentKey = DrawResourceKey(context, shaders, state.color, draw.indexAddress, indexBytes, !trimKey);
        if (auto cached = SharedResourceCache().Find(resolved.contentKey)) {
            const bool valid = cached->Revalidate(shaders);
            auto* recorder = Recorder::Active();
            std::optional<std::vector<ShaderResources::MovedBuffer>> moved;
            if (valid && recorder != nullptr) moved = cached->MovedReadOnlyBuffers(shaders, *recorder);
            if (moved.has_value()) {
                if (trimKey) CheckBufferAliases(shaders, state.color, draw.indexAddress, indexBytes);
                resolved.resources = std::move(cached);
                resolved.moved = std::move(*moved);
                outcome.kind = KindTemplateHit;
                countCache(&DrawProfile::cacheHits);
            } else if (valid) {
                countCache(&DrawProfile::cacheMisses);
            } else {
                SharedResourceCache().Remove(resolved.contentKey);
                countCache(&DrawProfile::cacheInvalidated);
            }
        }
    } else {
        countCache(&DrawProfile::uncacheable);
    }
    timer.phase(PhaseLookup);
    if (resolved.resources == nullptr) {
        resolved.resources = std::make_shared<ShaderResources>(context, shaders, state.color, draw.indexAddress, static_cast<std::size_t>(indexBytes), snapshots);
        resolved.built = &resolved.resources->Timing();
        outcome.addressBased = resolved.resources->HoldsLease();
        outcome.kind = outcome.addressBased ? KindBda : KindBuild;
        if (resolved.cacheable) countCache(&DrawProfile::cacheMisses);
    }
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

// The draw commands of one draw: the vertex and index buffer binds, then the direct draw, the
// GPU-side indirect draw from `argumentBuffer` or the CPU-read records with the driver's rules.
void recordDrawCommands(const Context& context, VkCommandBuffer commands, const State& state, const Pm4::DrawParameters& draw, const DrawInputs& inputs, const IndirectRecord* indirect, VkBuffer argumentBuffer, VkDeviceSize argumentOffset) {
    if (context.recorder != nullptr) context.recorder->NoteSampledDraw();
    const auto* args = indirect != nullptr ? indirect->args : nullptr;
    if (state.stages.mesh && args != nullptr) {
        context.Function<PFN_vkCmdDrawMeshTasksIndirectEXT>("vkCmdDrawMeshTasksIndirectEXT")(commands, argumentBuffer, argumentOffset, 1, sizeof(VkDrawMeshTasksIndirectCommandEXT));
        return;
    }
    if (state.stages.mesh) {
        APS5_LOG_OUT_DEBUG("vkCmdDrawMeshTasksEXT groups=%u instances=%u", inputs.meshGroups, draw.instanceCount);
        context.Function<PFN_vkCmdDrawMeshTasksEXT>("vkCmdDrawMeshTasksEXT")(commands, inputs.meshGroups, draw.instanceCount, 1);
        return;
    }
    if (!inputs.vertexHandles.empty()) context.Resolved(&DeviceFunctions::cmdBindVertexBuffers, "vkCmdBindVertexBuffers")(commands, 0, static_cast<std::uint32_t>(inputs.vertexHandles.size()), inputs.vertexHandles.data(), inputs.vertexOffsets.data());
    if (draw.indexed) context.Resolved(&DeviceFunctions::cmdBindIndexBuffer, "vkCmdBindIndexBuffer")(commands, inputs.indices->Handle(), 0, draw.indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    if (args == nullptr) {
        if (draw.indexed) context.Resolved(&DeviceFunctions::cmdDrawIndexed, "vkCmdDrawIndexed")(commands, draw.indexCount, draw.instanceCount, 0, static_cast<std::int32_t>(draw.firstVertex), draw.firstInstance);
        else context.Resolved(&DeviceFunctions::cmdDraw, "vkCmdDraw")(commands, draw.indexCount, draw.instanceCount, draw.firstVertex, draw.firstInstance);
    } else if (indirect->path == IndirectDrawPath::Gpu) {
        if (args->countIndirect) {
            if (draw.indexed) context.Function<PFN_vkCmdDrawIndexedIndirectCountKHR>("vkCmdDrawIndexedIndirectCountKHR")(commands, argumentBuffer, argumentOffset, indirect->countImport->buffer, args->countAddress - indirect->countImport->base, args->count, args->stride);
            else context.Function<PFN_vkCmdDrawIndirectCountKHR>("vkCmdDrawIndirectCountKHR")(commands, argumentBuffer, argumentOffset, indirect->countImport->buffer, args->countAddress - indirect->countImport->base, args->count, args->stride);
        } else if (args->count <= 1 || context.multiDrawIndirect) {
            if (draw.indexed) context.Resolved(&DeviceFunctions::cmdDrawIndexedIndirect, "vkCmdDrawIndexedIndirect")(commands, argumentBuffer, argumentOffset, args->count, args->stride);
            else context.Resolved(&DeviceFunctions::cmdDrawIndirect, "vkCmdDrawIndirect")(commands, argumentBuffer, argumentOffset, args->count, args->stride);
        } else {
            for (std::uint32_t record = 0; record < args->count; ++record) {
                const auto offset = argumentOffset + static_cast<VkDeviceSize>(record) * args->stride;
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
                if (record.firstVertexOrIndex >= draw.indexCount) continue;
                const auto vertexOffset = args->vertexRule == Rule::InPlace ? record.vertexOffset : args->vertexConstant;
                context.Resolved(&DeviceFunctions::cmdDrawIndexed, "vkCmdDrawIndexed")(commands, std::min(record.count, draw.indexCount - record.firstVertexOrIndex), record.instances, record.firstVertexOrIndex, static_cast<std::int32_t>(vertexOffset), firstInstance);
            } else {
                const auto firstVertex = args->vertexRule == Rule::InPlace ? record.firstVertexOrIndex : args->vertexConstant;
                context.Resolved(&DeviceFunctions::cmdDraw, "vkCmdDraw")(commands, record.count, record.instances, firstVertex, firstInstance);
            }
        }
    }
}

// The GPU-side records of an indirect draw, recorded outside the render pass: the stores that
// produced them (a dispatch in place, a fill, the host) precede the indirect read, as for
// DISPATCH_INDIRECT; a Constant dimension is rewritten in a scratch copy (vkCmdDrawIndirect reads
// the record's dword as the first vertex / instance, and the constant is what the fixed-function
// fetch must start at). The scratch is kept until the batch completed. Returns whether a copy was
// rewritten; `argumentBuffer`/`argumentOffset` receive where the draw reads the records.
bool recordIndirectArguments(const Context& context, VkCommandBuffer commands, Recorder* recorder, bool recorded, const IndirectRecord& indirect, std::unique_ptr<DeviceBuffer>& scratch, VkBuffer& argumentBuffer, VkDeviceSize& argumentOffset, const std::function<void(std::uint32_t)>& countBarrier) {
    using Rule = Pm4::DrawParameters::IndirectDraw::Rule;
    const auto* args = indirect.args;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT);
    countBarrier(1);
    if (recorded) {
        // Read in place from the import when the batch runs (a synchronous draw waits for its own).
        recorder->NotePendingRead(args->arguments, static_cast<std::size_t>(args->RangeBytes()), Recorder::ReadKind::Indirect);
        if (args->countIndirect) recorder->NotePendingRead(args->countAddress, 4, Recorder::ReadKind::Indirect);
    }
    argumentBuffer = indirect.argumentImport->buffer;
    argumentOffset = args->arguments - indirect.argumentImport->base;
    const bool rewritten = args->vertexRule == Rule::Constant || args->instanceRule == Rule::Constant;
    if (!rewritten) return false;
    const auto bytes = static_cast<std::size_t>(args->RangeBytes());
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

std::shared_ptr<Buffer> recordMeshArguments(const Context& context, VkCommandBuffer commands, Recorder* recorder, bool recorded, const State& state, const Pm4::DrawParameters& draw, const IndirectRecord& indirect, const std::function<void(std::uint32_t)>& countBarrier) {
    const auto* args = indirect.args;
    const auto rules = MeshArgumentRulesFor(context, *state.stages.mesh, draw.indexCount);
    auto arguments = std::make_shared<Buffer>(context, ShaderRecompiler::MeshArgumentBytes, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
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

// Debug aid: APS5_CHECK_INDIRECT_ARGS=1 reads the records of a GPU-side draw back on the CPU once
// its batch completed (the GPU has finished writing them by then) and prints the first 16, counting
// records whose Constant dimension holds a value the hardware would have ignored. Empty otherwise.
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

// Everything a recorded draw uses lives until its batch completes.
struct Kept {
    std::shared_ptr<ShaderResources> resources;
    std::shared_ptr<Pipeline> pipeline;
    std::shared_ptr<Framebuffer> framebuffer;
    std::shared_ptr<Buffer> indices;
    std::vector<std::shared_ptr<Buffer>> vertexBuffers;
    std::vector<std::shared_ptr<StorageTexture>> targets;
    std::unique_ptr<DeviceBuffer> scratch;
};

// The completion side of a recorded draw: the kept objects, the record check, the lease outcome,
// the GPU write notes, the copied-buffer write-back (listed in DrawCopiedWriters or run as a
// completion action) and the targets marked dirty.
void keepRecordedDraw(Recorder& recorder, const std::shared_ptr<ShaderResources>& resources, std::shared_ptr<Pipeline> pipeline, std::shared_ptr<Framebuffer> framebuffer, DrawInputs& inputs, std::vector<std::shared_ptr<StorageTexture>> targets, std::unique_ptr<DeviceBuffer> scratch, std::function<void()> checkRecords, bool listed, bool completion, const DrawOutcome& outcome) {
    auto kept = std::make_shared<Kept>();
    kept->resources = resources;
    kept->pipeline = std::move(pipeline);
    kept->framebuffer = std::move(framebuffer);
    kept->indices = std::move(inputs.indices);
    kept->vertexBuffers = std::move(inputs.vertexBuffers);
    kept->scratch = std::move(scratch);
    kept->targets = std::move(targets);
    const auto& residents = kept->targets;
    recorder.Keep(kept);
    if (checkRecords) recorder.OnComplete(std::move(checkRecords));
    // Counted for the [address-sync] line: a lease released by the completion (the pin waiter
    // finishes the recorder up to the open batch, which holds the kept resources and gets the
    // next serial), or waited for by the caller (any wait releases it at once).
    if (resources->HoldsLease()) CountLeaseOutcome(outcome.waited, outcome.waited ? 0 : recorder.Submissions() + 1);
    // Every use of a (possibly shared) resources object registers its GPU writes anew.
    resources->MarkGpuWrites(recorder);
    // The write-back (fault check, copied buffers) runs when the batch completed; a fault is
    // reported by the recorder ("deferred write-back failed") instead of thrown out of the draw.
    if (listed) {
        // As VulkanDevice::dispatch lists its copied writers: delisted before the write-back
        // (one that fails must not keep indirect dispatches on the CPU), listed after the
        // registration (a throw there leaves nothing behind).
        auto writers = DrawCopiedWriters();
        recorder.OnComplete([resources, writers] {
            writers->erase(std::remove(writers->begin(), writers->end(), resources), writers->end());
            resources->WriteBackBuffers();
        });
        writers->push_back(resources);
    } else if (completion) {
        recorder.OnComplete([resources] { resources->WriteBackBuffers(); });
    }
    for (const auto& resident : residents) {
        if (resident != nullptr) resident->MarkDirty();
    }
}

// What recordDraw records from: the recorder, the resources, the pipeline and framebuffer, the
// resident targets with their views, the indirect records (null for a direct draw), how the
// completion work is handled (`listed`, `completion`, `waited`: see Draw) and, from a recipe, the
// push constant block (else assembled from the stages).
struct RecordedDraw {
    Recorder* recorder = nullptr;
    std::shared_ptr<ShaderResources> resources;
    std::shared_ptr<Pipeline> pipeline;
    std::shared_ptr<Framebuffer> framebuffer;
    std::span<const VkImageView> targetViews;
    std::vector<std::shared_ptr<StorageTexture>> targets;
    std::vector<std::shared_ptr<StorageTexture>> proxies;
    const IndirectRecord* indirect = nullptr;
    std::span<const ShaderResources::MovedBuffer> moved;
    bool listed = false;
    bool completion = false;
    bool waited = false;
    const std::array<std::byte, PipelinePushConstantBytes>* pushBytes = nullptr;
    VkShaderStageFlags pushStages = 0;
};

void pushDrawConstants(const Pipeline& pipeline, VkCommandBuffer commands, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, const ShaderResources& resources, const std::array<std::byte, PipelinePushConstantBytes>* bytes, VkShaderStageFlags stages, VkDeviceAddress meshArguments = 0) {
    auto block = bytes != nullptr ? *bytes : AssemblePushConstants(shaders);
    if (bytes == nullptr) {
        resources.PatchPushConstants(block);
        stages = PushConstantStages(shaders);
    }
    if (!state.stages.mesh) {
        pipeline.PushConstants(commands, stages, block);
        return;
    }
    const auto firstVertex = draw.indirect ? draw.indirect->vertexConstant : draw.firstVertex;
    const auto firstInstance = draw.indirect ? draw.indirect->instanceConstant : draw.firstInstance;
    const std::array<std::uint32_t, ShaderRecompiler::MeshDrawPushBytes / 4> words{draw.indexCount, firstVertex, firstInstance, draw.indexed ? draw.indexSize : 0u, static_cast<std::uint32_t>(meshArguments), static_cast<std::uint32_t>(meshArguments >> 32u)};
    static_assert(ShaderRecompiler::MeshDrawPushOffsetBytes + ShaderRecompiler::MeshDrawPushBytes == PipelinePushConstantBytes);
    std::memcpy(block.data() + ShaderRecompiler::MeshDrawPushOffsetBytes, words.data(), sizeof(words));
    pipeline.PushConstants(commands, stages | VK_SHADER_STAGE_MESH_BIT_EXT, block);
}

// The record of a draw whose targets are all resident (`lean`: recorded into the device's open
// batch, rendering in the general layout): one memory barrier before the pass (earlier writes to
// the attachments and the draw's inputs visible to it) and one after it, recorded by the recorder
// when the pass ends (Recorder::LeaveRenderPassOpen), so consecutive draws into the same
// attachments with nothing recorded between them share one pass; the host-write and host-read
// barriers are implicit in the submission and the fence. Then the kept objects, the completion
// work and, for a waited-for draw, the recorder sync.
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
            const auto bytes = snapshot.buffer->Bytes();
            addSample(snapshot.address, bytes.size(), snapshot.buffer->Handle(), 0, bytes.data());
        }
    }
    for (const auto& [begin, end] : resources.InPlaceReads()) {
        Require(end >= begin, "invalid capture input range");
        const auto bytes = static_cast<std::size_t>(end - begin);
        if (bytes == 0 || bytes > 512) continue;
        if (bindings != nullptr && std::any_of(bindings->snapshots.begin(), bindings->snapshots.end(), [&](const auto& snapshot) { return snapshot.address < end && begin < snapshot.address + snapshot.buffer->Bytes().size(); })) continue;
        if (resources.WritesOverlap(begin, bytes) || recorder.PendingWriteOverlaps(begin, bytes)) {
            CaptureTrace::Log("input-skip draw=%llu batch=%llu address=%llx bytes=%zu reason=gpu-writer", draw, batch, static_cast<unsigned long long>(begin), bytes);
            continue;
        }
        const auto* imported = HostImportFor(context, begin, bytes);
        Require(imported != nullptr, "capture input has no host import");
        addSample(begin, bytes, imported->buffer, begin - imported->base, reinterpret_cast<const std::byte*>(begin));
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

void recordDraw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, DrawInputs& inputs, RecordedDraw& record, DrawOutcome& outcome, DrawTimer& timer, double& ownWaitedMs) {
    auto* recorder = record.recorder;
    auto& resources = *record.resources;
    const auto* args = record.indirect != nullptr ? record.indirect->args : nullptr;
    const bool gpuIndirect = args != nullptr && record.indirect->path == IndirectDrawPath::Gpu;
    using CommandClass = Recorder::CommandClass;
    const auto countBarrier = [&](std::uint32_t count) { Recorder::CountBarriers(CommandClass::Draw, count); };
    // The pass is named by its attachment views (stable while the kept targets live, so unique
    // within the open batch) and the extent. The previous draw's pass is continued only when this
    // draw neither reads its attachments (a barrier would be owed, which no pass allows) nor
    // records anything outside a pass (an indirect draw's argument barrier and scratch copies).
    std::uint64_t passKey = 14695981039346656037ull;
    const auto mix = [&](std::uint64_t value) {
        passKey ^= value;
        passKey *= 1099511628211ull;
    };
    for (const auto view : record.targetViews) mix(reinterpret_cast<std::uint64_t>(view));
    if (state.blends.size() != state.colors.size()) {
        mix(state.blends.size());
        for (const auto& color : state.colors) mix(color.exportIndex);
    }
    mix(state.renderExtent.width);
    mix(state.renderExtent.height);
    const bool readsTarget = std::any_of(record.targets.begin(), record.targets.end(), [&](const std::shared_ptr<StorageTexture>& target) { return resources.ReadsImage(target.get()); });
    // A queued DCC key store over memory the draw writes or reads in place (unknown for an
    // address-based build), or over its GPU-side records, lands before it, as before a dispatch
    // (VulkanDevice::dispatch); the flush ends an open pass, so it comes before the decision.
    const auto touches = [&](std::uint64_t begin, std::uint64_t end) {
        const auto bytes = static_cast<std::size_t>(end - begin);
        if (resources.WritesOverlap(begin, bytes) || resources.ReadsOverlap(begin, bytes)) return true;
        if (!gpuIndirect) return false;
        return (begin < args->arguments + args->RangeBytes() && args->arguments < end) || (args->countIndirect && begin < args->countAddress + 4 && args->countAddress < end);
    };
    if (recorder->HasQueuedKeyStores() && (resources.HoldsLease() || recorder->AnyQueuedKeyStore(touches))) recorder->FlushKeyStores();
    // A queued label store over such memory likewise (Recorder::RecordStore).
    if (recorder->HasQueuedStores() && (resources.HoldsLease() || recorder->AnyQueuedStore(touches))) recorder->FlushStores();
    const auto drawBindings = resources.PrepareDrawBindings(*recorder, record.moved);
    const bool capture = CaptureInputsEnabled();
    const bool meshIndirect = state.stages.mesh && args != nullptr;
    const bool continued = !capture && !readsTarget && !gpuIndirect && !meshIndirect && recorder->ContinuesRenderPass(passKey);
    outcome.passContinued = continued;
    outcome.passBegun = !continued;
    const auto commands = continued ? recorder->CommandsInRenderPass() : recorder->Commands();
    if (capture) captureInputs(context, *recorder, commands, resources, drawBindings, record.targets.empty() || record.targets.front() == nullptr ? 0 : record.targets.front()->Descriptor().baseAddress);
    // The draw's [gputime] class range: from its first barrier to the pass's trailing barrier (a
    // continued draw lies inside its pass's range).
    const auto drawTiming = !continued ? recorder->BeginGpuTiming(CommandClass::Draw) : Recorder::NoTiming;
    if (!continued && Recorder::BarrierValidate()) {
        auto reads = resources.InPlaceReads();
        if (gpuIndirect) {
            reads.emplace_back(args->arguments, args->arguments + args->RangeBytes());
            if (args->countIndirect) reads.emplace_back(args->countAddress, args->countAddress + 4);
        }
        auto images = resources.StorageImages();
        for (const auto& target : record.targets) images.emplace_back(target->Image(), true);
        recorder->NoteAccess(CommandClass::Draw, Recorder::Access{reads, resources.GpuWrites(), images, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | inputs.shaderStages, resources.HoldsLease()});
    }
    std::unique_ptr<DeviceBuffer> scratch;
    std::shared_ptr<Buffer> meshArguments;
    VkBuffer argumentBuffer = VK_NULL_HANDLE;
    VkDeviceSize argumentOffset = 0;
    bool rewritten = false;
    if (continued) {
        record.pipeline->Continue(commands, state);
    } else {
        for (const auto& proxy : record.proxies) {
            proxy->RecordAttachmentProxyLoad(commands, VK_IMAGE_LAYOUT_GENERAL);
            countBarrier(2);
        }
        const VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
        context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | inputs.shaderStages, 0, 1, &before, 0, nullptr, 0, nullptr);
        countBarrier(1);
        APS5_LOG_CHARS_OUT_DEBUG("Upload barrier recorded");
        if (meshIndirect) {
            meshArguments = recordMeshArguments(context, commands, recorder, true, state, draw, *record.indirect, countBarrier);
            argumentBuffer = meshArguments->Handle();
        } else if (gpuIndirect) {
            rewritten = recordIndirectArguments(context, commands, recorder, true, *record.indirect, scratch, argumentBuffer, argumentOffset, countBarrier);
        }
        // Earlier recorded work (dispatches, the previous draw) wrote the images in the general
        // layout, which a lean draw renders in: no transitions.
        APS5_LOG_OUT_DEBUG("Beginning pipeline renderExtent=%ux%u", state.renderExtent.width, state.renderExtent.height);
        record.pipeline->Begin(commands, *record.framebuffer, state.renderExtent, state);
    }
    APS5_LOG_CHARS_OUT_DEBUG("Pipeline Begin OK");
    if (drawBindings != nullptr) {
        const auto set = drawBindings->allocation.set;
        context.Resolved(&DeviceFunctions::cmdBindDescriptorSets, "vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, record.pipeline->Layout(), 0, 1, &set, 0, nullptr);
    } else {
        resources.Bind(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, record.pipeline->Layout());
    }
    APS5_LOG_CHARS_OUT_DEBUG("Resources bound");
    pushDrawConstants(*record.pipeline, commands, state, draw, shaders, resources, record.pushBytes, record.pushStages, meshArguments != nullptr ? meshArguments->DeviceAddress() : 0);
    APS5_LOG_CHARS_OUT_DEBUG("Push constants recorded");
    recordDrawCommands(context, commands, state, draw, inputs, record.indirect, argumentBuffer, argumentOffset);
    NoteDepthSurfaceWrite(context, state);
    if (meshArguments != nullptr) recorder->Keep(meshArguments);
    if (args != nullptr) CountIndirectDraw(record.indirect->path, record.indirect->readMs, rewritten);
    auto checkRecords = indirectRecordCheck(record.indirect);
    APS5_LOG_CHARS_OUT_DEBUG("Draw recorded");
    // The pass stays open for the next draw of these attachments; the recorder ends it (and the
    // draw class range) before anything else is recorded. A draw that wrote memory owes the next
    // one a barrier, so its pass cannot be continued.
    std::function<void(VkCommandBuffer)> storeProxies;
    if (!continued && !record.proxies.empty()) {
        storeProxies = [proxies = record.proxies](VkCommandBuffer passCommands) {
            for (const auto& proxy : proxies) {
                proxy->RecordAttachmentProxyStore(passCommands, VK_IMAGE_LAYOUT_GENERAL);
                Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
            }
        };
    }
    recorder->LeaveRenderPassOpen(passKey, drawTiming, !resources.WritesMemory(), std::move(storeProxies));
    timer.phase(PhaseRecord);
    keepRecordedDraw(*recorder, record.resources, std::move(record.pipeline), std::move(record.framebuffer), inputs, std::move(record.targets), std::move(scratch), std::move(checkRecords), record.listed, record.completion, outcome);
    timer.phase(PhaseKeep);
    if (record.waited) {
        // The wait the dispatch path makes for such work (source 3, "address-based"): the
        // write-back runs before the next packet, so the CPU never reads copied results or
        // touches a leased allocation before they landed.
        Recorder::CountSync(3);
        const auto ownBefore = timer.profile ? Recorder::ThreadWaitedMs() : 0.0;
        recorder->Sync();
        if (timer.profile) ownWaitedMs += Recorder::ThreadWaitedMs() - ownBefore;
        timer.phase(PhaseSync);
    }
}

// The state with the outputs the pixel shader lacks masked: Vulkan leaves attachments a pixel
// shader has no output for undefined, so their writes are masked (shaders that only store to
// images or buffers keep their targets as they were). Empty when no mask must change.
std::optional<State> maskedState(const State& state, const std::set<std::uint32_t>& fragmentOutputs) {
    std::optional<State> masked;
    for (std::size_t index = 0; index < state.blends.size(); ++index) {
        if (fragmentOutputs.contains(static_cast<std::uint32_t>(index)) || state.blends[index].colorWriteMask == 0) continue;
        if (!masked.has_value()) masked = state;
        masked->blends[index].colorWriteMask = 0;
    }
    return masked;
}

// Debug aid: APS5_DUMP_TARGETS=<n> saves the first n renders of every color target as a raw file
// (u32 width, u32 height, u32 VkFormat, then tightly packed rows).
int DumpTargetLimit() {
    static const int dumpLimit = [] { const char* text = std::getenv("APS5_DUMP_TARGETS"); return text ? std::atoi(text) : 0; }();
    return dumpLimit;
}

// A draw whose targets are all resident is recorded into the device's open batch like a dispatch:
// the CPU does not wait for it, and the drain before it goes away (queue order and the barriers
// keep it behind earlier recorded work); see `recorded` in Draw for the buffers that still need a
// synchronous draw. Other draws keep their own synchronous batch. Debug aid: APS5_SYNC_DRAWS=1
// makes every draw synchronous.
bool RecordDraws() {
    static const bool recordDraws = std::getenv("APS5_SYNC_DRAWS") == nullptr;
    return recordDraws;
}

}

std::optional<std::string> KnownValidationFailure(const Context& context, std::span<const CompiledShader> shaders, const State& state) {
    std::vector<std::uint64_t> key;
    if (!ValidationKey(context, shaders, state, key)) return std::nullopt;
    std::lock_guard lock(validationMutex());
    const auto& failures = validationFailures();
    if (const auto found = failures.find(key); found != failures.end()) return found->second;
    return std::nullopt;
}

void Draw(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots, std::shared_ptr<const DrawRecipe>* recipeOut) {
    PerformanceTimer timing("Graphics.Draw");
    // APS5_PROFILE_DRAW prints the time of each phase of the draw (microseconds) and the [draws] totals.
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    DrawTimer timer(profile);
    // APS5_TRACE_DRAWS (together with APS5_PROFILE_DRAW; alone it only enables Driver.cpp's own
    // [draw] lines) additionally prints one line per draw with its phases and, for synchronous
    // draws, their inputs: per-draw string building at ~1400 draws per 10 s is itself a cost the
    // totals must not carry.
    static const bool traceDraws = std::getenv("APS5_TRACE_DRAWS") != nullptr;
    DrawOutcome outcome;
    const ShaderResources::BuildTiming* built = nullptr;
    // The thread's GPU waits so far: the difference at the report, less the draw's own syncs
    // (`ownWaitedMs`, sampled around them), is what this draw waited for through the flush hook
    // (nested in the draw's hold, see DrawOutcome::hookWaitUs).
    const auto waitedBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
    double ownWaitedMs = 0;
    const auto report = [&](const char* suffix) { reportDrawEnd(state, timer, built, outcome, waitedBefore, ownWaitedMs, suffix); };
    auto inputs = prepareDrawInputs(context, state, draw, shaders, outcome, timer, nullptr);
    if (inputs.nothing) return;
    const auto* args = draw.indirect ? &*draw.indirect : nullptr;
    const auto indexBytes = inputs.indexBytes;
    timing.Mark("validate");
    timing.Mark("index_upload");
    // One binding per color attachment. Tiled targets are detiled and retiled on the GPU in video memory;
    // only changed guest blocks are stored back.
    struct TargetBinding {
        ColorTarget color;
        bool gpuTiling = false;
        // Resident target: the surface's cached storage image is attached directly and marked dirty
        // afterwards, so nothing is copied in or out per draw.
        std::shared_ptr<StorageTexture> resident;
        bool proxied = false;
        // Linear pixels converted on the CPU, for targets the GPU detiler does not handle.
        std::unique_ptr<Buffer> transfer;
        // Guest bytes in host memory, and their device-local copies for the detiler.
        std::unique_ptr<Buffer> tiled;
        std::unique_ptr<DeviceBuffer> tiledDevice;
        std::unique_ptr<DeviceBuffer> linearDevice;
        std::vector<std::byte> original;
        TileMipLayout mip{};
        std::unique_ptr<RenderTarget> target;
        // APS5_DUMP_TARGETS: the rendered linear pixels, read back for target_<address>_<n>.raw.
        std::unique_ptr<Buffer> dump;
    };
    const int dumpLimit = DumpTargetLimit();
    static std::mutex dumpMutex;
    static std::map<std::uint64_t, int> dumped;
    std::vector<TargetBinding> targets(state.colors.size());
    std::vector<VkImageView> targetViews;
    for (std::size_t index = 0; index < state.colors.size(); ++index) {
        auto& binding = targets[index];
        binding.color = state.colors[index];
        const auto& color = binding.color;
        binding.gpuTiling = color.tileMode == ColorTileMode::RenderTarget && context.detiler != nullptr;
        APS5_LOG_OUT_DEBUG("Creating color target %zu address=0x%llx bytes=%llu extent=%ux%u", index, static_cast<unsigned long long>(color.address), static_cast<unsigned long long>(color.bytes), color.extent.width, color.extent.height);
        const ColorTargetLayout colorLayout(color.extent.width, color.extent.height, color.tileMode, color.elementBytes);
        constexpr VkBufferUsageFlags copies = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        timer.phase(PhaseSetup);
        // Debug aid: APS5_NO_RESIDENT_TARGETS=1 copies every target in and out again.
        static const bool residentTargets = std::getenv("APS5_NO_RESIDENT_TARGETS") == nullptr;
        if ((binding.gpuTiling || (color.tileMode == ColorTileMode::Standard4KB && context.detiler != nullptr)) && residentTargets) {
            // The lookup refreshes the image on every draw (StorageTexture::Refresh: FlushPending,
            // CollectWrites over the target's pages, the DCC key scan of TextureClearKeys, then
            // UnchangedSince). The page walk is skipped while the worker's collect epoch lasts
            // (GuestMemory::BumpCollectEpoch: ordering points of the queue, or every packet under
            // APS5_PACKET_EPOCH=1); the key scan runs on every lookup regardless. Lookups of a
            // millisecond or more are counted apart; the [texture] line says how many uploaded.
            binding.resident = refreshResidentTarget(context, state, color, outcome, profile, [&] {
                auto resident = CachedStorageSurface(context, SurfaceForTarget(color));
                Require(resident->Attachable(), "storage format cannot be a color attachment");
                Require(color.mipCount > 1 || color.depth > 1 || resident->GuestBytes() == colorLayout.Bytes(), "resident image layout differs from the color layout");
                return resident;
            });
        }
        if (binding.resident != nullptr) {
            timer.phase(PhaseReadTarget);
            binding.proxied = AttachmentProxyFormat(context, color.format) != VK_FORMAT_UNDEFINED;
            targetViews.push_back(binding.proxied ? binding.resident->AttachmentProxyView() : binding.resident->AttachmentView(color.format, color.mip, color.depthSlice));
            continue;
        }
        materializeCmaskClear(context, color, nullptr);
        Require(!color.mipTail, "rendering into a packed mip tail needs the resident image of its surface");
        Require(color.depth == 1, "rendering into a 3D color target needs the resident image of its surface");
        if (binding.gpuTiling) {
            binding.mip = ColorTargetMip(color, colorLayout);
            binding.tiled = std::make_unique<Buffer>(context, colorLayout.Bytes(), copies);
            binding.tiledDevice = std::make_unique<DeviceBuffer>(context, colorLayout.Bytes(), copies | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            binding.linearDevice = std::make_unique<DeviceBuffer>(context, colorLayout.LinearBytes(), copies | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            binding.original.resize(colorLayout.Bytes());
            GuestMemory::Read(color.address, binding.original, colorLayout.Alignment());
            std::memcpy(binding.tiled->Bytes().data(), binding.original.data(), binding.original.size());
        } else {
            binding.transfer = std::make_unique<Buffer>(context, colorLayout.LinearBytes(), copies);
            ReadColorTarget(color, binding.transfer->Bytes());
        }
        // A fast-cleared DCC target holds its clear value wherever the draw does not write.
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
    if (state.depth) targetViews.push_back(DepthSurfaceView(context, *state.depth));
    timer.phase(PhasePrepare);
    const bool recordDraws = RecordDraws();
    auto* recorder = Recorder::Active();
    const bool recordable = recordDraws && recorder != nullptr && dumpLimit == 0 && std::all_of(targets.begin(), targets.end(), [](const TargetBinding& binding) { return binding.resident != nullptr; });
    auto resolved = resolveDrawResources(context, state, draw, shaders, snapshots, indexBytes, recordable, outcome, timer);
    auto& resources = resolved.resources;
    const auto& contentKey = resolved.contentKey;
    const bool cacheable = resolved.cacheable;
    built = resolved.built;
    // The memory-state decision of an indirect draw, made here after the resource build (which may
    // retire imports and wait for recorded work) and before anything is recorded, exactly as
    // VulkanDevice::dispatch decides for DISPATCH_INDIRECT: the GPU reads the records in place from
    // the host import unless storage-image results are pending over them (flushed, then synced), a
    // label or a copied buffer's write-back still has to land on them, or they are not imported; in
    // those cases (and with APS5_NO_GPU_INDIRECT_DRAW=1) the CPU reads them through the flush hook
    // and records plain draws with the same per-dimension rules. The import pointer stays valid:
    // refreshImports runs only under GuestMemory::GpuMutex, held here, and a retired import's
    // buffer is kept while the recorder is busy.
    IndirectRecord indirect;
    indirect.args = args;
    std::vector<Pm4::DrawArguments> records;
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
            const auto overlaps = [&](const auto& writer) { return writer->WritesOverlap(address, bytes); };
            if (context.copiedWriters != nullptr && std::any_of(context.copiedWriters->begin(), context.copiedWriters->end(), overlaps)) return IndirectDrawPath::PendingLabelOrCopy;
            if (std::any_of(DrawCopiedWriters()->begin(), DrawCopiedWriters()->end(), overlaps)) return IndirectDrawPath::PendingLabelOrCopy;
            if ((import = HostImportFor(context, address, bytes)) == nullptr) return IndirectDrawPath::NotImported;
            return IndirectDrawPath::Gpu;
        };
        const auto rangeBytes = args->RangeBytes();
        if (rangeBytes == 0) {
            report(" indirect draw without records");
            return;
        }
        Require(rangeBytes <= std::numeric_limits<std::size_t>::max(), "indirect draw record range overflow");
        if (!gpuIndirectDraws) indirect.path = IndirectDrawPath::Disabled;
        else indirect.path = decide(args->arguments, static_cast<std::size_t>(rangeBytes), indirect.argumentImport);
        if (indirect.path == IndirectDrawPath::Gpu && args->countIndirect) {
            Require(context.drawIndirectCount, "indirect draw count without VK_KHR_draw_indirect_count must be resolved by the driver");
            indirect.path = decide(args->countAddress, 4, indirect.countImport);
        }
        if (indirect.path != IndirectDrawPath::Gpu) {
            // The reads wait for the producing batch through the flush hook, as the driver's
            // resolve did for every indirect dispatch before its GPU path.
            const auto readStart = std::chrono::steady_clock::now();
            const auto count = std::min(args->countIndirect ? Pm4::ReadDrawCount(*args) : args->count, args->count);
            records.reserve(count);
            for (std::uint32_t record = 0; record < count; ++record) records.push_back(Pm4::ReadDrawArguments(*args, record));
            indirect.readMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count();
            indirect.records = records;
            if (std::none_of(records.begin(), records.end(), [](const Pm4::DrawArguments& record) { return record.count != 0 && record.instances != 0; })) {
                CountIndirectDraw(indirect.path, indirect.readMs);
                report(" indirect draw with empty records");
                return;
            }
        }
    }
    // A draw whose only completion work is a BDA fault check (the rect-list stages carry a fault
    // buffer, so every rect-list draw needed one) is recorded too, with the check as a completion
    // action like a dispatch's write-back: its own batch would first wait for everything recorded
    // before it. A draw that writes a copied buffer (at the menu stage: 0x48-byte constant buffers
    // inside an import but not at its storage offset alignment, so GuestBufferMemory copies them
    // and every guest buffer counts as writable) is recorded and then waited for at once: the
    // write-back runs before the next packet, without the separate command batch and fence a
    // synchronous draw takes. The copied write-back is listed in DrawCopiedWriters, which lets such
    // draws run without the wait; both consumers of the device's list in VulkanDevice.cpp
    // (DispatchIndirect's argument check and the fill HLE's range check, see Draw.hpp) consult this
    // list too. A draw holding an address-based build's lease (which pins guest allocations until
    // the write-back releases it) runs free like the dispatch path's: the completion releases the
    // lease, and a guest thread that needs a leased allocation syncs the recorder through the
    // registry's pin waiter (see SyncLeaseWork); APS5_SYNC_LEASE_DISPATCH=1 waits for such draws
    // at once as before. Debug aids:
    // APS5_SYNC_COMPLETION_DRAWS=1 keeps every draw with completion work synchronous;
    // APS5_NO_RECORDER_SYNC_DRAWS=1 gives the waited-for draws their own batch as before;
    // APS5_NO_RECORD_COPIED_DRAWS=1 waits for copied-write draws at once instead of listing them.
    static const bool syncCompletionDraws = std::getenv("APS5_SYNC_COMPLETION_DRAWS") != nullptr;
    static const bool recorderSyncDraws = std::getenv("APS5_NO_RECORDER_SYNC_DRAWS") == nullptr;
    static const bool recordCopiedDraws = std::getenv("APS5_NO_RECORD_COPIED_DRAWS") == nullptr;
    const bool completion = resources->NeedsCompletion();
    const bool copiedWrites = completion && resources->HasCopiedWrites();
    // A lease only forces the wait when deferred release is off.
    const bool lease = resources->HoldsLease() && SyncLeaseWork();
    outcome.completion = completion;
    outcome.recorded = recordable;
    // Whether the copied write-back is listed in DrawCopiedWriters instead of waited for.
    bool listed = false;
    if (!recordable) {
        outcome.reason = !recordDraws || dumpLimit != 0 ? SyncDisabled : recorder == nullptr ? SyncNoRecorder : SyncNotResident;
    } else if (completion && (syncCompletionDraws || copiedWrites || lease)) {
        outcome.reason = syncCompletionDraws ? SyncDisabled : lease ? SyncLease : SyncCopiedWrites;
        if (syncCompletionDraws) outcome.recorded = false;
        else if (copiedWrites && !lease && recordCopiedDraws) listed = true;
        else if (recorderSyncDraws) outcome.waited = true;
        else outcome.recorded = false;
    }
    const bool recorded = outcome.recorded;
    // A build this recorded draw can share with later identical ones goes into the cache (a cache
    // hit is reusable by construction, so `recorded` holds for it; one with completion work is
    // never reusable).
    if (cacheable && built != nullptr && recorded && resources->Reusable()) SharedResourceCache().Insert(contentKey, resources);
    // A recorded draw renders into its resident targets in the general layout (recordDraw). Debug
    // aid: APS5_DRAW_TRANSITIONS=1 keeps the per-draw upload and download barriers, the layout
    // transitions of every target and one pass per draw, as before.
    static const bool drawTransitions = std::getenv("APS5_DRAW_TRANSITIONS") != nullptr;
    const bool lean = recorded && !drawTransitions;
    if (!lean && !resolved.moved.empty()) {
        resolved.resources = std::make_shared<ShaderResources>(context, shaders, state.color, draw.indexAddress, static_cast<std::size_t>(indexBytes), snapshots);
        resolved.moved.clear();
    }
    APS5_LOG_CHARS_OUT_DEBUG("Creating Pipeline");
    // The state is copied only when a mask must change.
    auto masked = maskedState(state, inputs.fragmentOutputs);
    if (std::any_of(targets.begin(), targets.end(), [](const TargetBinding& binding) { return binding.proxied; })) recipeOut = nullptr;
    for (std::size_t index = 0; index < targets.size(); ++index) {
        if (!targets[index].proxied) continue;
        if (!masked.has_value()) masked = state;
        masked->colors[index].format = AttachmentProxyFormat(context, state.colors[index].format);
        masked->blends.at(state.colors[index].exportIndex).colorWriteMask &= VK_COLOR_COMPONENT_R_BIT;
    }
    const State& pipelineState = masked.has_value() ? *masked : state;
    auto pipeline = CachedPipeline(context, pipelineState, inputs.vertexInput, *resources, shaders, lean ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // Resident targets' views are stable while their storage image lives, so the framebuffer is
    // reused with the pipeline; a per-draw RenderTarget gets a framebuffer of its own.
    std::vector<std::shared_ptr<StorageTexture>> owners;
    owners.reserve(targets.size());
    for (const auto& binding : targets) owners.push_back(binding.resident);
    auto framebuffer = pipeline->AcquireFramebuffer(targetViews, owners, state.renderExtent);
    timer.phase(PhasePipeline);
    APS5_LOG_CHARS_OUT_DEBUG("Pipeline created");
    if (lean) {
        RecordedDraw record;
        record.recorder = recorder;
        record.resources = resources;
        record.moved = resolved.moved;
        record.pipeline = pipeline;
        record.framebuffer = framebuffer;
        record.targetViews = targetViews;
        record.targets = owners;
        for (const auto& binding : targets) {
            if (binding.proxied) record.proxies.push_back(binding.resident);
        }
        record.indirect = args != nullptr ? &indirect : nullptr;
        record.listed = listed;
        record.completion = completion;
        record.waited = outcome.waited;
        recordDraw(context, state, draw, shaders, inputs, record, outcome, timer, ownWaitedMs);
        // The recipe for the caller's draw-cache entry (design_cpu_final M8, rule R3 for draws):
        // only a template the resource cache serves under this content key (reusable: no lease,
        // no copied writes, every direct region import- or mirror-served) of a direct draw, so a
        // hit's proof is the template's ProveCurrent and nothing needs completion work.
        if (recipeOut != nullptr && DrawRecipes() && cacheable && !outcome.waited && args == nullptr && resources->Reusable() && !state.depth && resolved.moved.empty()) {
            auto recipe = std::make_shared<DrawRecipe>();
            recipe->device = context.device;
            recipe->templateRef = resources;
            recipe->key = contentKey;
            recipe->pipeline = pipeline;
            recipe->framebuffer = framebuffer;
            for (const auto& owner : owners) recipe->targets.emplace_back(owner);
            recipe->targetViews = targetViews;
            std::uint64_t passKey = 14695981039346656037ull;
            for (const auto view : targetViews) passKey = (passKey ^ reinterpret_cast<std::uint64_t>(view)) * 1099511628211ull;
            passKey = (passKey ^ state.renderExtent.width) * 1099511628211ull;
            passKey = (passKey ^ state.renderExtent.height) * 1099511628211ull;
            recipe->passKey = passKey;
            recipe->vertexInput = inputs.vertexInput;
            recipe->pushStages = PushConstantStages(shaders);
            if (recipe->pushStages != 0) {
                recipe->pushBytes = AssemblePushConstants(shaders);
                resources->PatchPushConstants(recipe->pushBytes);
            }
            recipe->masked = masked;
            recipe->fragmentOutputs = inputs.fragmentOutputs;
            recipe->shaderStages = inputs.shaderStages;
            *recipeOut = std::move(recipe);
        }
        timing.Mark("draw_and_resource_release");
        report(outcome.waited ? (outcome.reason == SyncLease ? " recorded then waited (lease)" : " recorded then waited (copied writes)") : completion ? " recorded with completion" : " recorded");
        return;
    }
    APS5_LOG_CHARS_OUT_DEBUG("Creating CommandBatch");
    std::optional<CommandBatch> batch;
    if (!recorded) {
        // The color-target detiles below take their descriptor sets from a fresh batch.
        if (context.detiler != nullptr) context.detiler->BeginBatch();
        batch.emplace(context);
    }
    using CommandClass = Recorder::CommandClass;
    const auto countBarrier = [&](std::uint32_t count = 1) { if (recorded) Recorder::CountBarriers(CommandClass::Draw, count); };
    const bool gpuIndirect = args != nullptr && indirect.path == IndirectDrawPath::Gpu;
    // A queued DCC key store over memory the draw writes or reads in place (unknown for an
    // address-based build), or over its GPU-side records, lands before it, as before a dispatch
    // (VulkanDevice::dispatch).
    const auto touches = [&](std::uint64_t begin, std::uint64_t end) {
        const auto bytes = static_cast<std::size_t>(end - begin);
        if (resources->WritesOverlap(begin, bytes) || resources->ReadsOverlap(begin, bytes)) return true;
        if (!gpuIndirect) return false;
        return (begin < args->arguments + args->RangeBytes() && args->arguments < end) || (args->countIndirect && begin < args->countAddress + 4 && args->countAddress < end);
    };
    if (recorded && recorder->HasQueuedKeyStores() && (resources->HoldsLease() || recorder->AnyQueuedKeyStore(touches))) recorder->FlushKeyStores();
    // A queued label store over such memory likewise (Recorder::RecordStore).
    if (recorded && recorder->HasQueuedStores() && (resources->HoldsLease() || recorder->AnyQueuedStore(touches))) recorder->FlushStores();
    const auto commands = recorded ? recorder->Commands() : batch->Handle();
    APS5_LOG_CHARS_OUT_DEBUG("CommandBatch created");
    // The draw's [gputime] class range: from its first barrier to the download barrier.
    const auto drawTiming = recorded ? recorder->BeginGpuTiming(CommandClass::Draw) : Recorder::NoTiming;
    if (recorded && Recorder::BarrierValidate()) {
        auto reads = resources->InPlaceReads();
        if (gpuIndirect) {
            reads.emplace_back(args->arguments, args->arguments + args->RangeBytes());
            if (args->countIndirect) reads.emplace_back(args->countAddress, args->countAddress + 4);
        }
        auto images = resources->StorageImages();
        for (const auto& binding : targets) {
            if (binding.resident != nullptr) images.emplace_back(binding.resident->Image(), true);
        }
        recorder->NoteAccess(CommandClass::Draw, Recorder::Access{reads, resources->GpuWrites(), images, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | inputs.shaderStages, resources->HoldsLease()});
    }
    std::unique_ptr<DeviceBuffer> scratch;
    VkBuffer argumentBuffer = VK_NULL_HANDLE;
    VkDeviceSize argumentOffset = 0;
    bool rewritten = false;
    VkMemoryBarrier upload{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    upload.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    upload.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | inputs.shaderStages, 0, 1, &upload, 0, nullptr, 0, nullptr);
    countBarrier();
    if (state.depth) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        countBarrier();
    }
    APS5_LOG_CHARS_OUT_DEBUG("Upload barrier recorded");
    std::shared_ptr<Buffer> meshArguments;
    if (state.stages.mesh && args != nullptr) {
        meshArguments = recordMeshArguments(context, commands, recorder, recorded, state, draw, indirect, countBarrier);
        argumentBuffer = meshArguments->Handle();
    } else if (gpuIndirect) {
        rewritten = recordIndirectArguments(context, commands, recorder, recorded, indirect, scratch, argumentBuffer, argumentOffset, countBarrier);
    }
    for (auto& binding : targets) {
        if (binding.proxied) {
            binding.resident->RecordAttachmentProxyLoad(commands, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            countBarrier(2);
            continue;
        }
        if (binding.resident != nullptr) {
            imageBarrier(context, commands, binding.resident->Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            countBarrier();
            continue;
        }
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {binding.color.extent.width, binding.color.extent.height, 1};
        const VkBuffer linear = binding.gpuTiling ? binding.linearDevice->Handle() : binding.transfer->Handle();
        countBarrier(binding.gpuTiling ? 4 : 2);
        if (binding.gpuTiling) {
            CopyBuffer(context, commands, binding.tiled->Handle(), 0, binding.tiledDevice->Handle(), 0, binding.original.size());
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            context.detiler->Dispatch(commands, TextureTileMode::kR64KBX, binding.color.elementBytes, binding.tiledDevice->Handle(), 0, binding.linearDevice->Handle(), 0, binding.mip);
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        }
        imageBarrier(context, commands, binding.target->Image(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Resolved(&DeviceFunctions::cmdCopyBufferToImage, "vkCmdCopyBufferToImage")(commands, linear, binding.target->Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        imageBarrier(context, commands, binding.target->Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }
    APS5_LOG_OUT_DEBUG("Beginning pipeline renderExtent=%ux%u", state.renderExtent.width, state.renderExtent.height);
    pipeline->Begin(commands, *framebuffer, state.renderExtent, state);
    APS5_LOG_CHARS_OUT_DEBUG("Pipeline Begin OK");
    resources->Bind(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout());
    APS5_LOG_CHARS_OUT_DEBUG("Resources bound");
    pushDrawConstants(*pipeline, commands, state, draw, shaders, *resources, nullptr, 0, meshArguments != nullptr ? meshArguments->DeviceAddress() : 0);
    APS5_LOG_CHARS_OUT_DEBUG("Push constants recorded");
    recordDrawCommands(context, commands, state, draw, inputs, args != nullptr ? &indirect : nullptr, argumentBuffer, argumentOffset);
    NoteDepthSurfaceWrite(context, state);
    if (meshArguments != nullptr && recorded) recorder->Keep(meshArguments);
    if (args != nullptr) CountIndirectDraw(indirect.path, indirect.readMs, rewritten);
    auto checkRecords = indirectRecordCheck(args != nullptr ? &indirect : nullptr);
    APS5_LOG_CHARS_OUT_DEBUG("Draw recorded");
    context.Resolved(&DeviceFunctions::cmdEndRenderPass, "vkCmdEndRenderPass")(commands);
    APS5_LOG_CHARS_OUT_DEBUG("Render pass ended");
    for (auto& binding : targets) {
        if (binding.proxied) {
            binding.resident->RecordAttachmentProxyStore(commands, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            countBarrier(2);
            continue;
        }
        if (binding.resident != nullptr) {
            imageBarrier(context, commands, binding.resident->Image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
            countBarrier();
            continue;
        }
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {binding.color.extent.width, binding.color.extent.height, 1};
        countBarrier(binding.gpuTiling ? 4 : 2);
        imageBarrier(context, commands, binding.target->Image(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        const VkBuffer linear = binding.gpuTiling ? binding.linearDevice->Handle() : binding.transfer->Handle();
        VkBufferMemoryBarrier reuse{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        reuse.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        reuse.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        reuse.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        reuse.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        reuse.buffer = linear;
        reuse.size = VK_WHOLE_SIZE;
        context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &reuse, 0, nullptr);
        context.Resolved(&DeviceFunctions::cmdCopyImageToBuffer, "vkCmdCopyImageToBuffer")(commands, binding.target->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear, 1, &copy);
        if (binding.gpuTiling) {
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
            if (dumpLimit > 0) {
                std::lock_guard lock(dumpMutex);
                if (dumped[binding.color.address] < dumpLimit) {
                    binding.dump = std::make_unique<Buffer>(context, binding.linearDevice->Size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                    CopyBuffer(context, commands, binding.linearDevice->Handle(), 0, binding.dump->Handle(), 0, binding.linearDevice->Size());
                }
            }
            context.detiler->Dispatch(commands, TextureTileMode::kR64KBX, binding.color.elementBytes, binding.linearDevice->Handle(), 0, binding.tiledDevice->Handle(), 0, binding.mip, true);
            memoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            CopyBuffer(context, commands, binding.tiledDevice->Handle(), 0, binding.tiled->Handle(), 0, binding.original.size());
        }
    }
    VkMemoryBarrier download{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    download.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    download.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT | inputs.shaderStages, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &download, 0, nullptr, 0, nullptr);
    countBarrier();
    if (recorded) recorder->EndGpuTiming(drawTiming);
    APS5_LOG_CHARS_OUT_DEBUG("Download barrier recorded");
    APS5_LOG_CHARS_OUT_DEBUG("SubmitAndWait");
    timer.phase(PhaseRecord);
    if (recorded) {
        keepRecordedDraw(*recorder, resources, pipeline, framebuffer, inputs, owners, std::move(scratch), std::move(checkRecords), listed, completion, outcome);
        timer.phase(PhaseKeep);
        if (outcome.waited) {
            // The wait the dispatch path makes for such work (source 3, "address-based"): the
            // write-back runs before the next packet, so the CPU never reads copied results or
            // touches a leased allocation before they landed.
            Recorder::CountSync(3);
            const auto ownBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
            recorder->Sync();
            if (profile) ownWaitedMs += Recorder::ThreadWaitedMs() - ownBefore;
            timer.phase(PhaseSync);
        }
        report(outcome.waited ? (outcome.reason == SyncLease ? " recorded then waited (lease)" : " recorded then waited (copied writes)") : completion ? " recorded with completion" : " recorded");
        return;
    }
    {
        // The recorder sync SubmitAndWait makes is the draw's own wait, not a nested hook wait.
        const auto ownBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
        batch->SubmitAndWait();
        if (profile) ownWaitedMs += Recorder::ThreadWaitedMs() - ownBefore;
    }
    if (checkRecords) checkRecords();
    timer.phase(PhaseSync);
    APS5_LOG_CHARS_OUT_DEBUG("SubmitAndWait OK");
    for (const auto& binding : targets) GuestMemory::CheckRange(reinterpret_cast<const void*>(binding.color.address), binding.color.bytes, 256, true);
    for (auto& binding : targets) {
        if (!binding.dump) continue;
        std::lock_guard lock(dumpMutex);
        char name[64];
        std::snprintf(name, sizeof(name), "target_%llx_%d.raw", static_cast<unsigned long long>(binding.color.address), dumped[binding.color.address]++);
        if (std::FILE* file = std::fopen(name, "wb")) {
            const std::uint32_t header[3] = {binding.color.extent.width, binding.color.extent.height, static_cast<std::uint32_t>(binding.color.format)};
            std::fwrite(header, sizeof(header), 1, file);
            std::fwrite(binding.dump->Bytes().data(), 1, binding.dump->Bytes().size(), file);
            std::fclose(file);
        }
    }
    APS5_LOG_CHARS_OUT_DEBUG("Shader resources WriteBack");
    resources->WriteBack();
    APS5_LOG_CHARS_OUT_DEBUG("Shader resources WriteBack OK");
    static const bool skipTargetWrite = std::getenv("APS5_NO_TARGET_WRITEBACK") != nullptr;
    for (const auto& binding : targets) {
        if (skipTargetWrite) break;
        if (binding.resident != nullptr) {
            // The results stay on the GPU until something reads the target's memory.
            binding.resident->MarkDirty();
            continue;
        }
        if (binding.gpuTiling) GuestMemory::WriteChanged(binding.color.address, binding.tiled->Bytes(), binding.original);
        else WriteColorTarget(binding.color, binding.transfer->Bytes());
        // The stored texels are the whole target now, so later reads must see them rather than a fast clear.
        MarkDccUncompressed(binding.color.dccAddress, ColorTargetLayout(binding.color.extent.width, binding.color.extent.height, binding.color.tileMode, binding.color.elementBytes).Bytes());
    }
    timer.phase(PhaseWriteBack);
    if (profile && traceDraws) {
        // A synchronous draw's inputs and target sample are described as a rendering debug aid;
        // the ~0.5 ms that takes (Describe samples every bound range and reads its DCC keys) is
        // shown as its own phase and kept out of the plain profile.
        std::size_t nonzero = 0;
        std::size_t sampled = 0;
        if (!targets.empty() && targets.front().resident == nullptr) {
            const auto bytes = targets.front().gpuTiling ? targets.front().tiled->Bytes() : targets.front().transfer->Bytes();
            sampled = bytes.size() / 64;
            for (std::size_t i = 0; i < bytes.size(); i += 64) nonzero += bytes[i] != std::byte{0};
        }
        std::fprintf(stderr, "[draw]   inputs:%s\n", resources->Describe().c_str());
        if (targets.size() > 1) {
            std::string list;
            for (const auto& binding : targets) {
                char entry[64];
                std::snprintf(entry, sizeof(entry), " 0x%llx(%d,%ux%u)", static_cast<unsigned long long>(binding.color.address), static_cast<int>(binding.color.format), binding.color.extent.width, binding.color.extent.height);
                list += entry;
            }
            std::fprintf(stderr, "[draw]   targets:%s\n", list.c_str());
        }
        timer.phase(PhaseDescribe);
        char suffix[160];
        std::snprintf(suffix, sizeof(suffix), " synchronous (%s), first 0x%llx format %d (%zu of %zu sampled bytes nonzero)", SyncReasonNames[outcome.reason], static_cast<unsigned long long>(state.color.address), static_cast<int>(state.color.format), nonzero, sampled);
        report(suffix);
    } else {
        report(" synchronous");
    }
    APS5_LOG_CHARS_OUT_DEBUG("Draw finished");
}

DrawRecipeOutcome DrawWithRecipe(const Context& context, const State& state, const Pm4::DrawParameters& draw, std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots, const DrawRecipe& recipe) {
    static_cast<void>(snapshots);
    PerformanceTimer timing("Graphics.DrawWithRecipe");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    DrawTimer timer(profile);
    DrawRecipeOutcome result;
    DrawOutcome outcome;
    outcome.kind = KindRecipeHit;
    const auto waitedBefore = profile ? Recorder::ThreadWaitedMs() : 0.0;
    double ownWaitedMs = 0;
    const auto miss = [&](DrawRecipeMiss reason) {
        result.miss = reason;
        if (profile) {
            auto& stats = Profile();
            std::lock_guard lock(stats.mutex);
            ++stats.recipeMisses[static_cast<std::size_t>(reason)];
        }
        return result;
    };
    Require(!draw.indirect, "a draw recipe covers direct draws only");
    auto* recorder = Recorder::Active();
    if (!RecordDraws() || recorder == nullptr || DumpTargetLimit() != 0) return miss(DrawRecipeMiss::NotRecordable);
    Require(recipe.targets.size() == state.colors.size() && recipe.targetViews.size() == state.colors.size(), "draw recipe targets do not match the state");
    auto inputs = prepareDrawInputs(context, state, draw, shaders, outcome, timer, &recipe);
    if (inputs.nothing) {
        result.recorded = true;
        return result;
    }
    // The resident-target proof on the stored images (design_cpu_final 3.7): while an image is
    // still the storage cache's for its surface (StorageImageCached: the cache entry, removed under
    // the cache mutex before an eviction's flush, and the LRU touch a lookup would make), it is
    // what the lookup would return, and Refresh brings it up to date exactly as the lookup's does
    // (FlushPending, the whole-surface collect and UnchangedSince, the key scan). An image gone
    // from the cache is a miss: the ordinary path looks up anew.
    std::vector<std::shared_ptr<StorageTexture>> targets;
    targets.reserve(recipe.targets.size());
    for (std::size_t index = 0; index < recipe.targets.size(); ++index) {
        timer.phase(PhaseSetup);
        auto stored = recipe.targets[index].lock();
        if (stored == nullptr || !StorageImageCached(context, stored.get()) || !StorageImageServesKeys(*stored, state.colors[index].dccAddress)) return miss(DrawRecipeMiss::TargetGone);
        auto resident = refreshResidentTarget(context, state, state.colors[index], outcome, profile, [&] {
            stored->Refresh();
            return stored;
        });
        timer.phase(PhaseReadTarget);
        if (resident == nullptr) return miss(DrawRecipeMiss::TargetGone);
        targets.push_back(std::move(resident));
    }
    timer.phase(PhasePrepare);
    // The template's proof (rules R6/R7): ProveCurrent, T1 included, the alias checks the trimmed
    // key leaves to a hit repeated; a failure removes the template from the cache (the batch keeps
    // it) and the caller rebuilds.
    auto resources = recipe.templateRef.lock();
    if (resources == nullptr) return miss(DrawRecipeMiss::TemplateGone);
    Require(resources->Reusable(), "draw recipe over a non-reusable template");
    const auto proofStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const bool proved = resources->ProveCurrent(shaders, &result.proof);
    if (profile) result.proofUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - proofStart).count();
    if (!proved) {
        SharedResourceCache().Remove(recipe.key, resources.get());
        countCache(&DrawProfile::cacheInvalidated);
        recorder->Keep(std::move(resources));
        timer.phase(PhaseLookup);
        return miss(DrawRecipeMiss::Proof);
    }
    CheckBufferAliases(shaders, state.color, draw.indexAddress, inputs.indexBytes);
    SharedResourceCache().Touch(recipe.key);
    countCache(&DrawProfile::cacheHits);
    timer.phase(PhaseLookup);
    outcome.completion = false;
    outcome.recorded = true;
    // The pipeline is the recipe's while the pipeline store still holds it (its device's teardown
    // or its eviction bound drops it: a miss). The framebuffer is the recipe's while the pipeline's
    // list holds it: the views are the stored targets' (stable while they live, and they are the
    // same objects), so AcquireFramebuffer only runs when the list evicted it.
    auto pipeline = recipe.pipeline.lock();
    if (pipeline == nullptr) return miss(DrawRecipeMiss::ObjectsGone);
    auto framebuffer = recipe.framebuffer.lock();
    if (framebuffer == nullptr) framebuffer = pipeline->AcquireFramebuffer(recipe.targetViews, targets, state.renderExtent);
    timer.phase(PhasePipeline);
    const auto recordStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    RecordedDraw record;
    record.recorder = recorder;
    record.resources = std::move(resources);
    record.pipeline = std::move(pipeline);
    record.framebuffer = std::move(framebuffer);
    record.targetViews = recipe.targetViews;
    record.targets = std::move(targets);
    record.pushBytes = &recipe.pushBytes;
    record.pushStages = recipe.pushStages;
    recordDraw(context, state, draw, shaders, inputs, record, outcome, timer, ownWaitedMs);
    if (profile) {
        result.recordUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - recordStart).count();
        auto& stats = Profile();
        std::lock_guard lock(stats.mutex);
        ++stats.recipeHits;
    }
    timing.Mark("draw_and_resource_release");
    reportDrawEnd(state, timer, nullptr, outcome, waitedBefore, ownWaitedMs, " recorded from recipe");
    result.recorded = true;
    return result;
}

namespace {

std::shared_ptr<StorageTexture> metadataPassResident(const Context& context, const ColorTarget& color) {
    if ((color.tileMode != ColorTileMode::RenderTarget && color.tileMode != ColorTileMode::Standard4KB) || context.detiler == nullptr) return nullptr;
    std::shared_ptr<StorageTexture> resident;
    try {
        resident = CachedStorageSurface(context, SurfaceForTarget(color));
    } catch (const std::exception&) {
        return nullptr;
    }
    return resident != nullptr && resident->GuestBytes() == color.bytes ? resident : nullptr;
}

}

void RunColorMetadataPass(const Context& context, const ColorMetadataPass& pass) {
    for (const auto& color : pass.targets) {
        if (color.cmaskAddress != 0) {
            const auto resident = metadataPassResident(context, color);
            if (materializeCmaskClear(context, color, resident.get()) && resident != nullptr) resident->MarkDirty();
        }
        if (color.dccAddress == 0) continue;
        auto keys = CurrentDccKeys(color.dccAddress, color.bytes);
        if (keys == DccKeys::Uncompressed) continue;
        Require(IsDccClear(keys), std::string("CB metadata pass over DCC keys that are ") + DccKeysName(keys) + " (per-block metadata is not modeled)");
        const auto texel = clearTexel(color, keys);
        const auto resident = metadataPassResident(context, color);
        if (resident != nullptr) {
            const char* refusal = nullptr;
            const bool current = keys == DccKeys::ClearRegister ? clearToTexel(*resident, texel, color.elementBytes, refusal) : StorageTexture::FindPending(color.address, color.bytes) == resident || resident->UploadedKeys() == keys;
            if (current) {
                resident->MarkDirty();
                MarkDccUncompressed(context, color.dccAddress, color.bytes);
                continue;
            }
        }
        storeClearTexels(context, color, texel);
    }
}

}
