#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlimMutex.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>

namespace AgcDriver::Graphics {

namespace {

constexpr std::uint64_t UnitBytes = 65536;
constexpr std::size_t ReasonCount = static_cast<std::size_t>(PublishReason::Count);
constexpr const char* ReasonNames[ReasonCount] = {"hook", "region", "indirect", "copy-src", "copy-dst", "fill", "fill-clear", "label", "keys", "scanout", "validate", "upload", "tail", "evict", "retire", "teardown", "address", "other"};

std::uint64_t BudgetBytes(const Context& context) {
    static const char* text = std::getenv("APS5_UNIT_SHADOW_MIB");
    if (text != nullptr) return std::strtoull(text, nullptr, 10) << 20u;
    static std::once_flag once;
    static std::uint64_t budget = 0;
    std::call_once(once, [&] {
        if (context.memoryProperties2 == nullptr || context.physical == VK_NULL_HANDLE) {
            budget = UnitShadowBudget(context.memory, nullptr);
            return;
        }
        VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
        VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
        properties.pNext = &reported;
        context.memoryProperties2(context.physical, &properties);
        budget = UnitShadowBudget(context.memory, &reported);
    });
    return budget;
}

std::uint32_t SlabUnits() {
    static const std::uint32_t units = [] {
        const char* text = std::getenv("APS5_UNIT_SHADOW_SLAB_MIB");
        const auto mib = text != nullptr ? std::strtoull(text, nullptr, 10) : 8ull;
        return static_cast<std::uint32_t>(std::clamp<std::uint64_t>(mib, 1, 1024) * 16);
    }();
    return units;
}

std::atomic<std::int64_t> tracePublishesLeft{[] {
    const char* text = std::getenv("APS5_TRACE_SHADOW");
    return text != nullptr ? static_cast<std::int64_t>(std::strtoll(text, nullptr, 10)) : std::int64_t{0};
}()};
std::atomic<std::int64_t> traceSeedsLeft{64};

bool TraceEnabled() {
    static const bool enabled = std::getenv("APS5_TRACE_SHADOW") != nullptr;
    return enabled;
}

struct Statistics {
    std::atomic<std::uint64_t> retiled{0}, retiledBytes{0}, refused{0}, refusedBytes{0}, seeds{0}, seedBytes{0};
    std::atomic<std::uint64_t> detiledShadow{0}, detiledShadowBytes{0}, detiledImport{0}, detiledImportBytes{0}, tailPublishes{0};
    std::array<std::atomic<std::uint64_t>, ReasonCount> published{}, publishedBytes{};
    std::atomic<std::uint64_t> staleDroppedCpu{0}, staleDroppedDriver{0}, skippedInCompletion{0}, evictions{0}, lostOnRetire{0}, droppedOnRetire{0}, slabLost{0}, verified{0}, mismatches{0};
};

Statistics& Stats() {
    static Statistics statistics;
    return statistics;
}

struct UnitShadow {
    Context context;
    std::uint64_t importBase = 0;
    std::uint64_t importBytes = 0;
    VkBuffer importBuffer = VK_NULL_HANDLE;
    std::uint64_t importLead = 0;
    std::uint64_t base = 0;
    std::vector<std::uint64_t> generation;
    std::vector<std::uint8_t> published;
    std::vector<std::shared_ptr<ShadowSlab>> slabs;
    std::vector<std::uint64_t> slabFailedUntil;
    std::uint32_t liveUnits = 0;

    std::uint64_t Units() const { return generation.size(); }
    std::uint64_t UnitBegin(std::uint64_t unit) const { return base + unit * UnitBytes; }
    std::uint64_t UnitOf(std::uint64_t address) const { return (address - base) / UnitBytes; }
    std::uint64_t ImportEnd() const { return importBase + importBytes; }
    std::pair<std::uint64_t, std::uint64_t> UnitRange(std::uint64_t unit) const { return {std::max(UnitBegin(unit), importBase), std::min(UnitBegin(unit + 1), ImportEnd())}; }
    bool Live(std::uint64_t unit) const { return generation[unit] != 0 && published[unit] == 0; }
    void Recount() {
        std::uint32_t live = 0;
        for (std::uint64_t unit = 0; unit < Units(); ++unit) live += Live(unit) ? 1u : 0u;
        liveUnits = live;
    }
};

struct Shadows {
    SlimMutex mutex;
    std::map<std::uint64_t, std::shared_ptr<UnitShadow>> byBase;
    std::uint64_t liveBytes = 0;
    std::uint64_t peakBytes = 0;
    std::uint32_t liveSlabs = 0;
    std::uint64_t retired = 0;
};

Shadows& Registry() {
    static Shadows shadows;
    return shadows;
}

struct LiveRanges {
    std::uint64_t serial = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
};
struct LiveRangesTag {};
alignas(64) std::atomic<std::uint64_t> liveSerial{0};
AtomicSharedPtr<const LiveRanges> publishedLive;

void liveChanged() {
    liveSerial.fetch_add(1, std::memory_order_release);
}

VkDeviceSize SlabBytes(const ShadowSlab& slab) {
    return static_cast<VkDeviceSize>(slab.units) * UnitBytes;
}

template<typename TVisit>
bool anyOverlapping(const Shadows& registry, std::uint64_t address, std::uint64_t end, TVisit&& visit) {
    if (end <= address || registry.byBase.empty()) return false;
    auto it = registry.byBase.upper_bound(address);
    if (it != registry.byBase.begin()) --it;
    for (; it != registry.byBase.end() && it->first < end; ++it) {
        const auto& shadow = it->second;
        if (address < shadow->ImportEnd() && shadow->importBase < end && visit(shadow)) return true;
    }
    return false;
}

std::vector<std::shared_ptr<UnitShadow>> overlapping(const Shadows& registry, std::uint64_t address, std::uint64_t end) {
    std::vector<std::shared_ptr<UnitShadow>> result;
    anyOverlapping(registry, address, end, [&](const std::shared_ptr<UnitShadow>& shadow) {
        result.push_back(shadow);
        return false;
    });
    return result;
}

std::shared_ptr<UnitShadow> find(const Shadows& registry, const HostImport& import) {
    const auto found = registry.byBase.find(import.base);
    if (found == registry.byBase.end()) return nullptr;
    const auto& shadow = found->second;
    return shadow->importBuffer == import.buffer && shadow->importBytes == import.bytes ? shadow : nullptr;
}

std::shared_ptr<UnitShadow> findOrCreate(Shadows& registry, const Context& context, const HostImport& import) {
    if (auto shadow = find(registry, import); shadow != nullptr && shadow->context.device == context.device) return shadow;
    if (const auto found = registry.byBase.find(import.base); found != registry.byBase.end()) {
        for (const auto& slab : found->second->slabs) {
            if (slab == nullptr) continue;
            registry.liveBytes -= SlabBytes(*slab);
            --registry.liveSlabs;
        }
        registry.byBase.erase(found);
        liveChanged();
    }
    auto shadow = std::make_shared<UnitShadow>();
    shadow->context = context;
    shadow->importBase = import.base;
    shadow->importBytes = import.bytes;
    shadow->importBuffer = import.buffer;
    shadow->importLead = import.lead;
    shadow->base = import.base & ~(UnitBytes - 1);
    const auto units = (import.base + import.bytes - shadow->base + UnitBytes - 1) / UnitBytes;
    shadow->generation.assign(static_cast<std::size_t>(units), 0);
    shadow->published.assign(static_cast<std::size_t>(units), 0);
    const auto slabs = (units + SlabUnits() - 1) / SlabUnits();
    shadow->slabs.assign(static_cast<std::size_t>(slabs), nullptr);
    shadow->slabFailedUntil.assign(static_cast<std::size_t>(slabs), 0);
    registry.byBase.emplace(import.base, shadow);
    return shadow;
}

void changedUnits(const UnitShadow& shadow, std::uint64_t first, std::uint64_t last, std::vector<std::uint8_t>& changed, std::vector<std::uint8_t>& cpu) {
    const auto count = static_cast<std::size_t>(last - first + 1);
    changed.assign(count, 1);
    cpu.assign(count, 1);
    const auto begin = std::max(shadow.UnitBegin(first), shadow.importBase);
    const auto end = std::min(shadow.UnitBegin(last + 1), shadow.ImportEnd());
    if (end <= begin) return;
    GuestMemory::ChangedBlocks(begin, static_cast<std::size_t>(end - begin), std::span(shadow.generation).subspan(static_cast<std::size_t>(first), count), changed, cpu);
}

std::shared_ptr<ShadowSlab> makeSlab(const Context& context, std::uint64_t firstUnit, std::uint32_t units) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = static_cast<VkDeviceSize>(units) * UnitBytes;
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer = VK_NULL_HANDLE;
    if (context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &buffer) != VK_SUCCESS) return nullptr;
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, buffer, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    try {
        allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (MemoryAccounting::Allocate(context, allocation, "unit shadow", &memory) != VK_SUCCESS) memory = VK_NULL_HANDLE;
        if (memory != VK_NULL_HANDLE && context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, buffer, memory, 0) != VK_SUCCESS) {
            MemoryAccounting::Free(context, memory);
            memory = VK_NULL_HANDLE;
        }
    } catch (const std::exception&) {
        memory = VK_NULL_HANDLE;
    }
    if (memory == VK_NULL_HANDLE) {
        context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr);
        return nullptr;
    }
    return std::make_shared<ShadowSlab>(context, buffer, memory, firstUnit, units);
}

struct SlabCopies {
    std::shared_ptr<ShadowSlab> slab;
    std::vector<VkBufferCopy> regions;
};

struct PublishOnCpuStorage {};

bool& publishOnCpu() {
    return HostThreadLocal<bool, PublishOnCpuStorage>();
}

std::atomic<void (*)(std::uint64_t, std::uint64_t)> publishSelectedHook{nullptr};

bool recordCopiesOnCpu(const UnitShadow& shadow, const std::vector<SlabCopies>& copies, const std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges, std::uint64_t bytes, std::uint64_t checked) {
    auto* recorder = Recorder::Active();
    if (recorder == nullptr) return false;
    const auto& context = shadow.context;
    auto host = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    struct Piece {
        std::uint64_t address;
        std::uint64_t offset;
        std::uint64_t size;
    };
    std::vector<Piece> pieces;
    const auto commands = recorder->Commands();
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::ShadowPublish);
    constexpr VkAccessFlags transferAccess = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, transferAccess);
    Recorder::CountBarriers(Recorder::CommandClass::ShadowPublish);
    const auto copyBuffer = context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
    std::uint64_t offset = 0;
    for (const auto& entry : copies) {
        std::vector<VkBufferCopy> regions;
        for (const auto& region : entry.regions) {
            regions.push_back({region.srcOffset, offset, region.size});
            pieces.push_back({shadow.importBase + region.dstOffset - shadow.importLead, offset, region.size});
            offset += region.size;
        }
        copyBuffer(commands, entry.slab->buffer, host->Handle(), static_cast<std::uint32_t>(regions.size()), regions.data());
        recorder->Keep(entry.slab);
    }
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::ShadowPublish);
    recorder->EndGpuTiming(timing, bytes);
    recorder->NotePendingWrites(ranges);
    recorder->OnComplete([host, pieces = std::move(pieces), checked] {
        host->Invalidate();
        for (const auto& piece : pieces) {
            for (std::uint64_t at = piece.address; at < piece.address + piece.size;) {
                const auto next = std::min(piece.address + piece.size, (at & ~(UnitBytes - 1)) + UnitBytes);
                const auto bytes = static_cast<std::size_t>(next - at);
                if (GuestMemory::UnchangedSinceCollected(at, bytes, checked)) GuestMemory::StoreOwnBytes(at, host->Bytes().subspan(static_cast<std::size_t>(piece.offset + (at - piece.address)), bytes));
                else Stats().staleDroppedCpu.fetch_add(1, std::memory_order_relaxed);
                at = next;
            }
        }
    });
    return true;
}

void recordCopies(const UnitShadow& shadow, const std::vector<SlabCopies>& copies, const std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges, std::uint64_t bytes) {
    const auto& context = shadow.context;
    using CommandClass = Recorder::CommandClass;
    auto* recorder = Recorder::Active();
    std::unique_ptr<CommandBatch> batch;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkAccessFlags covered = 0;
    auto timing = Recorder::NoTiming;
    if (recorder != nullptr) {
        commands = recorder->Commands(&covered);
        timing = recorder->BeginGpuTiming(CommandClass::ShadowPublish);
        if (Recorder::BarrierValidate()) recorder->NoteAccess(CommandClass::ShadowPublish, Recorder::Access{{}, ranges, {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    } else {
        batch = std::make_unique<CommandBatch>(context);
        commands = batch->Handle();
    }
    constexpr VkAccessFlags transferAccess = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    if (recorder != nullptr && (covered & transferAccess) == transferAccess && Recorder::MergeBarriers()) {
        Recorder::CountMerged(CommandClass::ShadowPublish);
    } else {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, transferAccess);
        Recorder::CountBarriers(CommandClass::ShadowPublish);
    }
    const auto copyBuffer = context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
    for (const auto& entry : copies) copyBuffer(commands, entry.slab->buffer, shadow.importBuffer, static_cast<std::uint32_t>(entry.regions.size()), entry.regions.data());
    constexpr VkAccessFlags publishedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, publishedAccess);
    Recorder::CountBarriers(CommandClass::ShadowPublish);
    if (batch) {
        batch->SubmitAndWait();
        return;
    }
    recorder->EndGpuTiming(timing, bytes);
    recorder->MarkCovered(publishedAccess);
    for (const auto& entry : copies) recorder->Keep(entry.slab);
    recorder->NoteStampedWrites(ranges);
}

void verifyPublished(const UnitShadow& shadow, const std::vector<SlabCopies>& copies) {
    const auto& context = shadow.context;
    if (auto* recorder = Recorder::Active()) {
        Recorder::CountSync(4);
        recorder->Sync();
    }
    for (const auto& entry : copies) {
        for (const auto& region : entry.regions) {
            Buffer host(context, static_cast<std::size_t>(region.size), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            CommandBatch batch(context);
            CopyBuffer(context, batch.Handle(), entry.slab->buffer, region.srcOffset, host.Handle(), 0, region.size);
            RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            batch.SubmitAndWait();
            host.Invalidate();
            const auto address = shadow.importBase + region.dstOffset - shadow.importLead;
            Stats().verified.fetch_add(1, std::memory_order_relaxed);
            if (!GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(region.size)) || std::memcmp(reinterpret_cast<const void*>(address), host.Bytes().data(), static_cast<std::size_t>(region.size)) != 0) {
                Stats().mismatches.fetch_add(1, std::memory_order_relaxed);
                static std::atomic<int> reports{0};
                if (reports.fetch_add(1) < 8) std::fprintf(stderr, "[shadow] verify: published bytes differ from the slab at 0x%llx+0x%llx\n", static_cast<unsigned long long>(address), static_cast<unsigned long long>(region.size));
            }
        }
    }
}

std::atomic<std::uint64_t> publishesInProgress{0};

class PublishInProgress {
public:
    PublishInProgress() { publishesInProgress.fetch_add(1, std::memory_order_seq_cst); }
    ~PublishInProgress() { publishesInProgress.fetch_sub(1, std::memory_order_seq_cst); }
    PublishInProgress(const PublishInProgress&) = delete;
    PublishInProgress& operator=(const PublishInProgress&) = delete;
};

std::size_t publishUnits(const std::shared_ptr<UnitShadow>& shadow, std::uint64_t first, std::uint64_t last, const std::vector<std::uint8_t>& selected, PublishReason reason) {
    const PublishInProgress inProgress;
    auto& registry = Registry();
    std::vector<SlabCopies> copies;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::uint64_t bytes = 0;
    std::size_t units = 0;
    const auto checked = GuestMemory::TrackerGeneration();
    {
        std::uint64_t walkFirst = last + 1, walkLast = first;
        {
            std::lock_guard lock(registry.mutex);
            for (auto unit = first; unit <= last; ++unit) {
                if (!selected[static_cast<std::size_t>(unit - first)] || !shadow->Live(unit)) continue;
                walkFirst = std::min(walkFirst, unit);
                walkLast = std::max(walkLast, unit);
            }
        }
        if (walkFirst > walkLast) return 0;
        const auto walkBegin = std::max(shadow->UnitBegin(walkFirst), shadow->importBase);
        const auto walkEnd = std::min(shadow->UnitBegin(walkLast + 1), shadow->ImportEnd());
        if (walkEnd > walkBegin) GuestMemory::CollectWritesUncached(walkBegin, static_cast<std::size_t>(walkEnd - walkBegin));
    }
    {
        std::lock_guard lock(registry.mutex);
        std::vector<std::uint8_t> changed, cpu;
        changedUnits(*shadow, first, last, changed, cpu);
        std::uint32_t ended = 0;
        for (auto unit = first; unit <= last; ++unit) {
            const auto k = static_cast<std::size_t>(unit - first);
            if (!selected[k] || !shadow->Live(unit)) continue;
            if (changed[k] == GuestMemory::BlockWritten) {
                shadow->generation[unit] = 0;
                ++ended;
                (cpu[k] != 0 ? Stats().staleDroppedCpu : Stats().staleDroppedDriver).fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            const auto slabIndex = static_cast<std::size_t>(unit / SlabUnits());
            const auto& slab = shadow->slabs[slabIndex];
            if (slab == nullptr) {
                shadow->generation[unit] = 0;
                ++ended;
                continue;
            }
            const auto [begin, end] = shadow->UnitRange(unit);
            if (end <= begin) continue;
            const VkBufferCopy region{(unit - slab->firstUnit) * UnitBytes + (begin - shadow->UnitBegin(unit)), begin - shadow->importBase + shadow->importLead, end - begin};
            if (!copies.empty() && copies.back().slab == slab && copies.back().regions.back().dstOffset + copies.back().regions.back().size == region.dstOffset) {
                copies.back().regions.back().size += region.size;
                ranges.back().second = end;
            } else {
                if (copies.empty() || copies.back().slab != slab) copies.push_back({slab, {}});
                copies.back().regions.push_back(region);
                ranges.emplace_back(begin, end);
            }
            shadow->published[unit] = 1;
            ++ended;
            slab->lastUse = Recorder::Presents();
            bytes += end - begin;
            ++units;
        }
        shadow->liveUnits -= ended;
        if (ended != 0) liveChanged();
    }
    if (units == 0) return 0;
    Stats().published[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
    Stats().publishedBytes[static_cast<std::size_t>(reason)].fetch_add(bytes, std::memory_order_relaxed);
    if (tracePublishesLeft.load(std::memory_order_relaxed) > 0 && tracePublishesLeft.fetch_sub(1, std::memory_order_relaxed) > 0) {
        const auto packet = GuestMemory::CurrentPacket();
        std::fprintf(stderr, "[shadow] publish %s: 0x%llx+0x%llx, %zu units, %.2f MiB, %zu slabs (packet 0x%x queue 0x%x)\n", ReasonNames[static_cast<std::size_t>(reason)], static_cast<unsigned long long>(ranges.front().first), static_cast<unsigned long long>(ranges.back().second - ranges.front().first), units, bytes / 1048576.0, copies.size(), packet.opcode, packet.queue);
    }
    if (const auto hook = publishSelectedHook.load(std::memory_order_acquire)) hook(ranges.front().first, ranges.front().second);
    if (!publishOnCpu() || !recordCopiesOnCpu(*shadow, copies, ranges, bytes, checked)) recordCopies(*shadow, copies, ranges, bytes);
    if (ShadowVerify()) verifyPublished(*shadow, copies);
    return units;
}

std::size_t publishRange(const std::shared_ptr<UnitShadow>& shadow, std::uint64_t address, std::uint64_t end, PublishScope scope, PublishReason reason) {
    const auto begin = std::max(address, shadow->importBase);
    const auto stop = std::min(end, shadow->ImportEnd());
    if (stop <= begin || shadow->liveUnits == 0) return 0;
    const auto first = shadow->UnitOf(begin);
    const auto last = shadow->UnitOf(stop - 1);
    std::vector<std::uint8_t> selected(static_cast<std::size_t>(last - first + 1), 0);
    bool any = false;
    for (auto unit = first; unit <= last; ++unit) {
        const auto [unitBegin, unitEnd] = shadow->UnitRange(unit);
        const bool whole = address <= unitBegin && end >= unitEnd;
        if (scope == PublishScope::PartialUnits && whole) continue;
        selected[static_cast<std::size_t>(unit - first)] = 1;
        any = true;
    }
    return any ? publishUnits(shadow, first, last, selected, reason) : 0;
}

bool evictOne(Shadows& registry) {
    std::shared_ptr<UnitShadow> victim;
    std::size_t victimSlab = 0;
    {
        std::lock_guard lock(registry.mutex);
        std::uint64_t oldest = ~0ull;
        for (const auto& [base, shadow] : registry.byBase) {
            for (std::size_t k = 0; k < shadow->slabs.size(); ++k) {
                const auto& slab = shadow->slabs[k];
                if (slab != nullptr && slab->pins.load(std::memory_order_relaxed) == 0 && slab->lastUse < oldest) {
                    oldest = slab->lastUse;
                    victim = shadow;
                    victimSlab = k;
                }
            }
        }
    }
    if (victim == nullptr) return false;
    const auto firstUnit = static_cast<std::uint64_t>(victimSlab) * SlabUnits();
    const auto lastUnit = std::min(firstUnit + SlabUnits(), victim->Units()) - 1;
    const auto published = publishRange(victim, victim->UnitBegin(firstUnit), victim->UnitBegin(lastUnit + 1), PublishScope::Whole, PublishReason::Evict);
    std::lock_guard lock(registry.mutex);
    auto& slab = victim->slabs[victimSlab];
    if (slab == nullptr) return true;
    if (TraceEnabled()) std::fprintf(stderr, "[shadow] evict slab %zu of import 0x%llx (%.1f MiB, %zu units published)\n", victimSlab, static_cast<unsigned long long>(victim->importBase), SlabBytes(*slab) / 1048576.0, published);
    for (auto unit = firstUnit; unit <= lastUnit; ++unit) {
        victim->generation[unit] = 0;
        victim->published[unit] = 0;
    }
    victim->Recount();
    liveChanged();
    registry.liveBytes -= SlabBytes(*slab);
    --registry.liveSlabs;
    slab.reset();
    Stats().evictions.fetch_add(1, std::memory_order_relaxed);
    return true;
}

}

}
