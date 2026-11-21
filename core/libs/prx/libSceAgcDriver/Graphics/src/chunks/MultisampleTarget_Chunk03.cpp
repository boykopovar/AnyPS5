#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include <shared_mutex>
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/FirstSampleCopy.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuardSpans.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/PrimedSharedMutex.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
void MaterializeMultisampleTargetClear(const Context& context, const ColorTarget& target) {
    const DccPublication publication(target);
    TargetsLock lock;
    acquire(context, target);
}

std::vector<std::byte> ReadMultisampleTargetSamples_nid_no_patch(const Context& context, const ColorTarget& target) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    return snapshotTarget(context, target)->ReadSamples();
}

MultisampleColorSnapshot ReadMultisampleTargetColorSnapshot_nid_no_patch(const Context& context, const ColorTarget& target) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    return snapshotTarget(context, target)->ColorSnapshot();
}

VkImageView MultisampleTargetStorageView(const Context& context, const GuestTextureResource& resource, bool uintStorage) {
    TargetsLock lock;
    discardRetiredTargets();
    const auto format = ResolveTextureFormat(resource.format);
    const MultisampleTarget* other = nullptr;
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        if (target->context.device != context.device || target->target.surfaceAddress != resource.baseAddress) continue;
        const auto& rendered = target->target;
        if (rendered.eqaaFragments != 0) throw std::runtime_error(eqaaReadRefusal("storage image access to", rendered, resource));
        if (sameStorageForm(context, rendered.format, format) && rendered.extent.width == resource.width && rendered.extent.height == resource.height && rendered.samples == resource.samples && rendered.mip == 0) {
            if (!holdHostOnly(*target)) continue;
            return target->Storage(StorageFormatForGuest(context, resource.format), uintStorage);
        }
        other = target.get();
    }
    if (other == nullptr) {
        Require(Recorder::Active() != nullptr, "a multisampled color target is only written by recorded dispatches");
        return createFilled(context, resource, format, "writing", true).Storage(StorageFormatForGuest(context, resource.format), uintStorage);
    }
    char text[320];
    std::snprintf(text, sizeof(text), "AGC graphics: storage image access to multisampled color target 0x%llx (%ux%u, %u samples, vk format %d) as a %ux%u texture of %u samples and guest format %u (vk %d) is not implemented", static_cast<unsigned long long>(resource.baseAddress), other->target.extent.width, other->target.extent.height, other->target.samples, static_cast<int>(other->target.format), resource.width, resource.height, resource.samples, resource.format, static_cast<int>(format));
    throw std::runtime_error(text);
}

bool MultisampleTargetStorable(const Context& context, const ColorTarget& target) {
    TargetsLock lock;
    return std::any_of(targets().begin(), targets().end(), [&](const auto& candidate) {
        return candidate->active->load(std::memory_order_acquire) && candidate->context.device == context.device && sameTarget(candidate->target, target) && candidate->Storable();
    });
}

void ResolveMultisampleTarget(const Context& context, const ColorTarget& source, StorageTexture& destination, VkImageView destinationView) {
    const DccPublication publication(source);
    TargetsLock lock;
    auto& target = acquire(context, source);
    target.Keep();
    const std::array<VkImageView, 2> views{target.View(source.format), destinationView};
    auto* averaging = source.packing == ShaderRecompiler::ColorExportPacking::Float11_11_10 ? &target.PackedResolve() : nullptr;
    auto framebuffer = averaging != nullptr ? nullptr : std::make_shared<Framebuffer>(context, target.ResolvePass(source.format), views, source.extent);
    std::shared_ptr<void> averaged;
    recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);
        if (averaging != nullptr) {
            averaged = averaging->RecordInto(commands, recorder, views[0], VK_IMAGE_LAYOUT_GENERAL, destinationView);
        } else {
            VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            begin.renderPass = target.ResolvePass(source.format);
            begin.framebuffer = framebuffer->Handle();
            begin.renderArea = {{0, 0}, source.extent};
            context.Resolved(&DeviceFunctions::cmdBeginRenderPass, "vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
            context.Resolved(&DeviceFunctions::cmdEndRenderPass, "vkCmdEndRenderPass")(commands);
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
        if (recorder != nullptr) {
            Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
            if (framebuffer != nullptr) recorder->Keep(framebuffer);
        }
    });
    destination.MarkDirty();
}

bool MultisampledColorFillAbsorbs(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    if (bytes == 0) return false;
    TargetsLock lock;
    bool absorbed = false;
    for (const auto& target : targets()) {
        VkClearColorValue clear{};
        if (!target->active->load(std::memory_order_acquire) || target->target.address != address || MultisampledColorBytes(target->target) != bytes || !MultisampledFillClearColor(target->target, pattern, clear)) continue;
        absorbed = holdHostOnly(*target) || absorbed;
    }
    return absorbed;
}

void NoteColorMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    TargetsLock lock;
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        target->NoteFill(address, bytes, pattern);
        if (target->htileBytes != 0 && address < target->htileAddress + target->htileBytes && target->htileAddress < address + bytes) {
            if (pattern == 0u && address <= target->htileAddress && address + bytes >= target->htileAddress + target->htileBytes) {
                target->htileRefusal.clear();
                target->NoteFill(target->target.address, static_cast<std::size_t>(MultisampledColorBytes(target->target)), 0u);
            } else {
                char text[224];
                std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx with 0x%08x over the HTILE 0x%llx+0x%llx of a sampled multisampled depth-layout texture is not modeled", static_cast<unsigned long long>(address), bytes, pattern, static_cast<unsigned long long>(target->htileAddress), static_cast<unsigned long long>(target->htileBytes));
                target->htileRefusal = text;
            }
        }
        if (target->target.dccAddress != 0) {
            target->NoteMetadataFill(address, bytes, pattern);
            continue;
        }
        const auto before = target->cmask.Keys();
        target->cmask.NoteFill(address, bytes, pattern);
        if (before == DccKeys::Clear0000 || target->cmask.Keys() != DccKeys::Clear0000) continue;
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1) < 8) std::fprintf(stderr, "[gpu] multisampled color target 0x%llx fast-cleared through its CMASK 0x%llx (fill 0x%llx+0x%zx)\n", static_cast<unsigned long long>(target->target.surfaceAddress), static_cast<unsigned long long>(target->target.cmaskAddress), static_cast<unsigned long long>(address), bytes);
    }
}

std::shared_ptr<Texture> MultisampleTargetFmaskTexture_nid_no_patch(const Context& context, const GuestTextureResource& resource, VkComponentMapping components) {
    TargetsLock lock;
    discardRetiredTargets();
    std::shared_ptr<MultisampleTarget> owner;
    for (const auto& candidate : targets()) {
        if (!candidate->active->load(std::memory_order_acquire) || candidate->context.device != context.device) continue;
        const auto& target = candidate->target;
        if (target.fmaskAddress != resource.baseAddress && (resource.fmaskCmaskAddress == 0u || target.cmaskAddress != resource.fmaskCmaskAddress)) continue;
        Require(owner == nullptr, "FMASK descriptor refers to more than one resident target");
        owner = candidate;
    }
    return owner != nullptr ? owner->FmaskSampled(resource, components) : nullptr;
}

void ClearMultisampleTargets(VkDevice device) {
    TargetsLock lock;
    std::erase_if(targets(), [&](const auto& target) { return target->context.device == device; });
    std::unique_lock guardLock(guardedMutex());
    const auto removed = std::erase_if(guarded(), [&](const auto& range) { return range.device == device; });
    if (removed != 0) {
        guardedGeneration().fetch_add(1, std::memory_order_release);
        moveGuardSpans();
    }
    anyGuarded().store(!guarded().empty(), std::memory_order_release);
    UpdateResidentImageUnmapObserver_nid_no_patch();
}

bool MultisampleDccUnwatchLocksGpu(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    if (!anyGuarded().load(std::memory_order_acquire)) return false;
    std::shared_lock lock(guardedMutex());
    for (const auto& [begin, end] : ranges) {
        if (end <= begin) continue;
        for (const auto& range : guarded()) {
            if (begin < range.address + range.bytes && range.address < end && range.active->load(std::memory_order_acquire) && !range.dccOwner.expired()) return true;
        }
    }
    return false;
}

void SaveMultisampleDccBeforeUnwatch(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address || !anyGuarded().load(std::memory_order_acquire)) return;
    std::vector<std::shared_ptr<MultisampleTarget>> owners;
    {
        std::shared_lock lock(guardedMutex());
        for (const auto& range : guarded()) {
            if (address >= range.address + range.bytes || range.address >= address + bytes || !range.active->load(std::memory_order_acquire)) continue;
            if (auto owner = range.dccOwner.lock(); owner && std::find(owners.begin(), owners.end(), owner) == owners.end()) owners.push_back(std::move(owner));
        }
    }
    if (owners.empty()) return;
    RequireGpuLockBeforeImports("the multisample color unwatch hook");
    std::lock_guard gpu(GuestMemory::GpuMutex());
    for (const auto& owner : owners) owner->SaveDccBeforeUnwatch(address, bytes);
}

void SetMultisampleDccPublishHookForTest(void (*hook)()) {
    dccPublishHookForTest.store(hook, std::memory_order_release);
}

void SetMultisampleDccCopyHookForTest(void (*hook)()) {
    dccCopyHookForTest.store(hook, std::memory_order_release);
}

void ClaimMultisampleTargetMemory(std::uint64_t address, std::uint64_t bytes) {
    if (bytes == 0 || !anyGuarded().load(std::memory_order_acquire) || !mayOverlapGuarded(address, bytes)) return;
    std::shared_lock lock(guardedMutex());
    claimOverlapping(address, bytes, nullptr);
}

bool MultisampleColorTargetOverlaps(std::uint64_t address, std::uint64_t bytes) {
    if (bytes == 0 || !anyGuarded().load(std::memory_order_acquire)) return false;
    std::shared_lock lock(guardedMutex());
    return std::ranges::any_of(guarded(), [&](const auto& range) { return !range.fmask && address < range.address + range.bytes && range.address < address + bytes; });
}

void ClaimMultisampleTargetStore(std::uint64_t address, std::uint64_t bytes) {
    if (bytes == 0 || !anyGuarded().load(std::memory_order_acquire)) return;
    std::shared_lock lock(guardedMutex());
    for (const auto& range : guarded()) {
        if (range.fmask || address >= range.address + range.bytes || range.address >= address + bytes || range.use->claimed.exchange(true, std::memory_order_acq_rel)) continue;
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1) < 8) std::fprintf(stderr, "[gpu] a single-sample storage image store to 0x%llx+0x%llx claims the memory of multisampled color target 0x%llx+0x%llx (%u samples)\n", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), static_cast<unsigned long long>(range.address), static_cast<unsigned long long>(range.bytes), range.samples);
    }
}

void GuardOneFragmentFmask(const Context& context, const ColorTarget& target) {
    const auto bits = static_cast<std::uint64_t>(target.extent.width) * target.extent.height * target.coverageSamples;
    const auto bytes = ColorTileModeIsGcn(target.tileMode) ? target.gcnFmaskBytes : std::max<std::uint64_t>((bits / 8u + 0xffffu) & ~0xffffull, 0x10000u);
    std::unique_lock lock(guardedMutex());
    const bool known = std::ranges::any_of(guarded(), [&](const auto& range) { return range.fmask && range.device == context.device && range.address == target.fmaskAddress && range.bytes == bytes; });
    if (!known) {
        guarded().push_back({context.device, target.fmaskAddress, bytes, target.coverageSamples, std::make_shared<std::atomic<bool>>(true), nullptr, std::make_shared<TargetUse>(), true});
        guardedGeneration().fetch_add(1, std::memory_order_release);
        moveGuardSpans();
    }
    anyGuarded().store(true, std::memory_order_release);
    lock.unlock();
    InstallResidentImageReadGuard_nid_no_patch();
}

bool HoldsMultisampleTargetsLock() {
    return targetsMutexHeld;
}

std::uint64_t MultisampleTargetRangeGeneration() {
    std::shared_lock lock(guardedMutex());
    return guardedGeneration().load(std::memory_order_acquire);
}

std::uint64_t MultisampleClaimSerial() {
    return guardedGeneration().load(std::memory_order_acquire) + claimsReleased().load(std::memory_order_acquire);
}

std::optional<std::vector<ShaderRecompiler::BdaAbi::Range>> ExcludeMultisampleTargetRanges(
    VkDevice device, std::span<const ShaderRecompiler::BdaAbi::Range> ranges, std::uint64_t& generation) {
    if (ranges.empty()) {
        generation = MultisampleTargetRangeGeneration();
        return std::nullopt;
    }
    if (!anyGuarded().load(std::memory_order_acquire)) {
        generation = MultisampleTargetRangeGeneration();
        return std::nullopt;
    }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> excluded;
    {
        std::shared_lock lock(guardedMutex());
        generation = guardedGeneration().load(std::memory_order_relaxed);
        for (const auto& range : guarded()) {
            if (range.device != device || !range.active->load(std::memory_order_acquire)) continue;
            excluded.emplace_back(range.address, range.address + range.bytes);
        }
    }
    if (excluded.empty()) return std::nullopt;
    std::sort(excluded.begin(), excluded.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    merged.reserve(excluded.size());
    for (const auto& range : excluded) {
        if (!merged.empty() && range.first <= merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
        else merged.push_back(range);
    }
    std::vector<ShaderRecompiler::BdaAbi::Range> filtered;
    filtered.reserve(ranges.size() + merged.size());
    bool changed = false;
    for (const auto& range : ranges) {
        auto cursor = range.begin;
        for (const auto& [begin, end] : merged) {
            if (end <= cursor) continue;
            if (begin >= range.end) break;
            const auto cutBegin = std::max(cursor, begin);
            const auto cutEnd = std::min(range.end, end);
            if (cutBegin > cursor) {
                auto part = range;
                part.begin = cursor;
                part.end = cutBegin;
                part.deviceAddress += cursor - range.begin;
                filtered.push_back(part);
            }
            if (cutBegin < cutEnd) {
                changed = true;
                cursor = cutEnd;
            }
            if (cursor == range.end) break;
        }
        if (cursor < range.end) {
            auto part = range;
            part.begin = cursor;
            part.deviceAddress += cursor - range.begin;
            filtered.push_back(part);
        }
    }
    return changed ? std::optional(std::move(filtered)) : std::nullopt;
}

void RequireNoMultisampleTarget(std::uint64_t address, std::size_t bytes, const char* access, DepthReadCause cause) {
    RequireNoResidentDepthRead_nid_no_patch(address, bytes, std::nullopt, true, cause);
    RequireNoMultisampleColorTarget(address, bytes, access);
}

void NoteGuardSpans(GuardSpanEvent event) {
    static const bool stats = std::getenv("APS5_PROFILE_DRAW") != nullptr || VerifyGuardSpans();
    if (!stats) return;
    struct Tally {
        std::array<std::uint64_t, static_cast<std::size_t>(GuardSpanEvent::Count)> counts{};
        std::uint32_t events = 0;
    };
    struct TallyStorage {};
    auto& tally = HostThreadLocal<Tally, TallyStorage>();
    ++tally.counts[static_cast<std::size_t>(event)];
    if ((++tally.events & 0x3ffu) != 0 && event != GuardSpanEvent::Refill && event != GuardSpanEvent::Missed) return;
    static std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(GuardSpanEvent::Count)> counts{};
    static std::atomic<std::int64_t> reportedAt{0};
    for (std::size_t index = 0; index < counts.size(); ++index) {
        if (tally.counts[index] != 0) counts[index].fetch_add(tally.counts[index], std::memory_order_relaxed);
        tally.counts[index] = 0;
    }
    const auto tenSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 10;
    if (reportedAt.exchange(tenSeconds, std::memory_order_relaxed) == tenSeconds) return;
    std::array<unsigned long long, static_cast<std::size_t>(GuardSpanEvent::Count)> taken{};
    for (std::size_t index = 0; index < taken.size(); ++index) taken[index] = counts[index].exchange(0, std::memory_order_relaxed);
    AgcDriver::ProfilePrint_nid_no_patch("[guard-spans] 10 s: %llu clear without the lock, %llu refills, %llu overlaps; verify %llu checked, %llu missed\n", taken[0], taken[1], taken[2], taken[3], taken[4]);
}

bool VerifyGuardSpans() {
    static const bool verify = std::getenv("APS5_VERIFY_GUARD_SPANS") != nullptr;
    return verify;
}

void ReportGuardSpanMiss(const char* list, std::uint64_t address, std::uint64_t bytes) {
    static std::atomic<unsigned> reports{0};
    if (reports.fetch_add(1, std::memory_order_relaxed) < 20) std::fprintf(stderr, "[guard-spans] verify: the %s copy answered no overlap for 0x%llx+0x%llx, but a listed guard overlaps it\n", list, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
    NoteGuardSpans(GuardSpanEvent::Missed);
}

void RequireNoMultisampleColorTarget(std::uint64_t address, std::size_t bytes, const char* access) {
    if (bytes == 0 || !anyGuarded().load(std::memory_order_acquire) || !mayOverlapGuarded(address, bytes)) return;
    const auto overlaps = [&](const GuardedRange& range) { return address < range.address + range.bytes && range.address < address + bytes; };
    const auto refuse = [&](const GuardedRange& range) {
        char text[320];
        if (range.fmask) {
            std::snprintf(text, sizeof(text), "AGC graphics: %s of 0x%llx+0x%zx overlaps the FMASK 0x%llx+0x%llx of a one-fragment color target of %u samples drawn on the host, which does not write it: guest access to it is not implemented", access, static_cast<unsigned long long>(address), bytes, static_cast<unsigned long long>(range.address), static_cast<unsigned long long>(range.bytes), range.samples);
            throw std::runtime_error(text);
        }
        std::snprintf(text, sizeof(text), "AGC graphics: %s of 0x%llx+0x%zx overlaps %s 0x%llx+0x%llx (%u samples), whose samples are only on the host: guest access to it is not implemented", access, static_cast<unsigned long long>(address), bytes, range.plane, static_cast<unsigned long long>(range.address), static_cast<unsigned long long>(range.bytes), range.samples);
        throw std::runtime_error(text);
    };
    const auto canRetire = [&](const GuardedRange& range) {
        if (!range.use->hostOnly.load(std::memory_order_acquire) || range.use->claimed.load(std::memory_order_acquire)) return true;
        if (range.metadataValid) return false;
        const auto used = range.use->writes.load(std::memory_order_acquire);
        const auto begin = std::max(address, range.address);
        const auto overlap = static_cast<std::size_t>(std::min(address + bytes, range.address + range.bytes) - begin);
        return used != 0 && GuestMemory::Watched(begin, overlap) && !GuestMemory::UnchangedSinceCollected(begin, overlap, used);
    };
    {
        std::shared_lock lock(guardedMutex());
        bool retirable = false;
        for (const auto& range : guarded()) {
            if (!overlaps(range)) continue;
            if (!canRetire(range)) refuse(range);
            retirable = true;
        }
        if (!retirable) return;
    }
    std::unique_lock lock(guardedMutex());
    std::vector<std::shared_ptr<std::atomic<bool>>> retired;
    for (const auto& range : guarded()) {
        if (!overlaps(range)) continue;
        if (!canRetire(range)) refuse(range);
        retired.push_back(range.active);
    }
    for (const auto& active : retired) active->store(false, std::memory_order_release);
    if (std::erase_if(guarded(), [&](const auto& range) { return std::find(retired.begin(), retired.end(), range.active) != retired.end(); }) != 0) guardedGeneration().fetch_add(1, std::memory_order_release);
    if (!retired.empty()) moveGuardSpans();
    anyGuarded().store(!guarded().empty(), std::memory_order_release);
}

void RetireMultisampleColorTarget(const std::shared_ptr<std::atomic<bool>>& active) {
    std::unique_lock lock(guardedMutex());
    active->store(false, std::memory_order_release);
    if (std::erase_if(guarded(), [&](const auto& range) { return range.active == active; }) != 0u) guardedGeneration().fetch_add(1, std::memory_order_release);
    anyGuarded().store(!guarded().empty(), std::memory_order_release);
}

void RetireMultisampleColorTargetsOnFmaskWrite(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || !anyGuarded().load(std::memory_order_acquire)) return;
    std::unique_lock lock(guardedMutex());
    bool changed = false;
    std::vector<std::shared_ptr<std::atomic<bool>>> retired;
    for (const auto& range : guarded()) {
        if (!range.fmask && range.plane != fmaskPlane) continue;
        if (!range.use->hostOnly.load(std::memory_order_acquire) || range.use->claimed.load(std::memory_order_acquire)) continue;
        if (address > range.address || range.address + range.bytes > address + bytes) continue;
        if (std::find(retired.begin(), retired.end(), range.active) == retired.end()) retired.push_back(range.active);
    }
    if (retired.empty()) return;
    for (const auto& active : retired) active->store(false, std::memory_order_release);
    std::erase_if(guarded(), [&](const auto& range) {
        if (std::find(retired.begin(), retired.end(), range.active) == retired.end()) return false;
        if (range.metadataValid) range.metadataValid->store(false, std::memory_order_release);
        return true;
    });
    guardedGeneration().fetch_add(1, std::memory_order_release);
    anyGuarded().store(!guarded().empty(), std::memory_order_release);
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> HostOnlySampleRanges(std::uint64_t address, std::size_t bytes) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    AppendHostWrittenDepthRanges(address, bytes, ranges);
    if (bytes != 0 && anyGuarded().load(std::memory_order_acquire)) {
        std::shared_lock lock(guardedMutex());
        for (const auto& range : guarded()) {
            if (address < range.address + range.bytes && range.address < address + bytes && range.use->hostOnly.load(std::memory_order_acquire)) ranges.emplace_back(range.address, range.bytes);
        }
    }
    std::ranges::sort(ranges);
    return ranges;
}

std::optional<MultisampleColorAlias> MultisampleColorTargetOver(std::uint64_t address, std::size_t bytes) {
    const auto aliases = MultisampleColorTargetsOver(address, bytes);
    if (aliases.empty()) return std::nullopt;
    return aliases.front();
}

std::vector<MultisampleColorAlias> MultisampleColorTargetsOver(std::uint64_t address, std::size_t bytes) {
    std::vector<MultisampleColorAlias> aliases;
    if (bytes == 0 || !anyGuarded().load(std::memory_order_acquire) || !mayOverlapGuarded(address, bytes)) return aliases;
    TargetsLock lock;
    std::shared_lock guardLock(guardedMutex());
    for (const auto& range : guarded()) {
        if (address >= range.address + range.bytes || range.address >= address + bytes) continue;
        if (std::any_of(aliases.begin(), aliases.end(), [&](const MultisampleColorAlias& alias) { return alias.active == range.active; })) continue;
        const auto target = std::find_if(targets().begin(), targets().end(), [&](const auto& candidate) { return candidate->active == range.active; });
        if (target != targets().end()) {
            aliases.push_back((*target)->Alias());
            continue;
        }
        MultisampleColorAlias alias{range.address, range.bytes, VK_FORMAT_UNDEFINED, range.samples};
        alias.active = range.active;
        aliases.push_back(alias);
    }
    return aliases;
}

}
}
}
}
