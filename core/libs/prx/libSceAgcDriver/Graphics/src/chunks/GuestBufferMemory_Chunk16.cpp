#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/AdaptiveStaging.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "ThreadOwned.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SnapshotCopies.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ResidentMirror.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#ifdef __APPLE__
struct udmabuf_create {
    std::uint32_t memfd;
    std::uint32_t flags;
    std::uint64_t offset;
    std::uint64_t size;
};
struct udmabuf_create_item {
    std::uint32_t memfd;
    std::uint32_t pad;
    std::uint64_t offset;
    std::uint64_t size;
};
struct udmabuf_create_list {
    std::uint32_t flags;
    std::uint32_t count;
    udmabuf_create_item list[];
};
constexpr std::uint32_t UDMABUF_FLAGS_CLOEXEC = 0x01;
constexpr unsigned long UDMABUF_CREATE = 0;
constexpr unsigned long UDMABUF_CREATE_LIST = 0;
#else
#include <linux/fs.h>
#include <linux/udmabuf.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <array>
#include <vector>
#include <span>
#include <atomic>
#include <tuple>
#include <utility>
#include <functional>
#include <bit>
#include <condition_variable>
#include <deque>
#include <iterator>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_set>
#include <thread>
#include <stop_token>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
std::uint64_t GuestBufferMemory::ViewBytes(std::uint64_t bytes, std::uint32_t adjustment) {
    if (adjustment % 4 == 0) return bytes + adjustment;
    const auto dwords = adjustment / 4 + bytes / 4 + (adjustment % 4 + bytes % 4 + 3) / 4;
    Require(dwords <= std::numeric_limits<std::uint64_t>::max() / 4, "guest buffer view range overflow");
    return 4 * dwords;
}

ShaderRecompiler::BdaAbi::Range GuestBufferMemory::addressRange(const Region& region) {
    Require(region.buffer != nullptr || region.direct != nullptr || region.mirror != nullptr, "incomplete guest GPU upload");
    const auto address = region.direct != nullptr ? region.direct->address + (region.begin - region.direct->base) : region.mirror != nullptr ? mirrorAddress(*region.mirror) + (region.begin - region.mirror->base) : region.buffer->DeviceAddress();
    Require(region.end - region.begin <= std::numeric_limits<std::uint64_t>::max() - address, "GPU address range overflow");
    const auto permissions = ShaderRecompiler::BdaAbi::Read | (addressWritable(region) ? ShaderRecompiler::BdaAbi::Write : 0u);
    return {region.begin, region.direct != nullptr ? inPlaceEnd(region) : servedEnd(region), address, permissions, 0};
}

std::uint64_t GuestBufferMemory::servedEnd(const Region& region) {
    if (region.mirror != nullptr) return std::min(region.end, region.mirror->base + region.mirror->bytes);
    return region.bufferEnd != 0 ? region.bufferEnd : region.end;
}

namespace {

using AddressRange = ShaderRecompiler::BdaAbi::Range;

void AppendFrom(const AddressRange& range, std::uint64_t begin, std::uint64_t end, std::vector<AddressRange>& result) {
    if (begin >= end) return;
    auto part = range;
    part.begin = begin;
    part.end = end;
    part.deviceAddress += begin - range.begin;
    result.push_back(part);
}

void AppendDisjoint(const std::vector<AddressRange>& ranges, std::vector<AddressRange>& result) {
    std::uint64_t reached = 0;
    for (const auto& range : ranges) {
        AppendFrom(range, std::max(range.begin, reached), range.end, result);
        reached = std::max(reached, range.end);
    }
}

void AppendUncovered(const std::vector<AddressRange>& ranges, const std::vector<AddressRange>& covering, std::vector<AddressRange>& result) {
    for (const auto& range : ranges) {
        auto cursor = range.begin;
        auto cover = std::partition_point(covering.begin(), covering.end(), [&](const AddressRange& candidate) { return candidate.end <= range.begin; });
        for (; cover != covering.end() && cover->begin < range.end && cursor < range.end; ++cover) {
            AppendFrom(range, cursor, std::min(cover->begin, range.end), result);
            cursor = std::max(cursor, cover->end);
        }
        AppendFrom(range, cursor, range.end, result);
    }
}

std::vector<AddressRange> MergeByBegin(const std::vector<AddressRange>& left, const std::vector<AddressRange>& right) {
    std::vector<AddressRange> merged;
    merged.reserve(left.size() + right.size());
    std::merge(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(merged), [](const AddressRange& first, const AddressRange& second) { return first.begin < second.begin; });
    return merged;
}

}

std::vector<ShaderRecompiler::BdaAbi::Range> GuestBufferMemory::AddressRanges() const {
    Require(uploaded && !committed, "guest GPU address ranges are not available");
    std::vector<ShaderRecompiler::BdaAbi::Range> result;
    if (space != nullptr) {
        std::vector<ShaderRecompiler::BdaAbi::Range> own;
        std::vector<ShaderRecompiler::BdaAbi::Range> overlays;
        own.reserve(regions.size());
        for (const auto& region : regions) (region.overlay ? overlays : own).push_back(addressRange(region));
        std::vector<ShaderRecompiler::BdaAbi::Range> uncovered;
        uncovered.reserve(own.size());
        AppendUncovered(own, space->ranges, uncovered);
        result = MergeByBegin(space->ranges, uncovered);
        if (!overlays.empty()) {
            std::sort(overlays.begin(), overlays.end(), [](const auto& left, const auto& right) { return left.begin < right.begin; });
            std::vector<ShaderRecompiler::BdaAbi::Range> cut;
            cut.reserve(result.size() + overlays.size());
            AppendUncovered(result, overlays, cut);
            result = MergeByBegin(cut, overlays);
        }
    } else if (regionsOverlap) {
        std::vector<ShaderRecompiler::BdaAbi::Range> live;
        std::vector<ShaderRecompiler::BdaAbi::Range> copies;
        for (const auto& region : regions) (region.direct != nullptr || region.mirror != nullptr ? live : copies).push_back(addressRange(region));
        std::vector<ShaderRecompiler::BdaAbi::Range> liveParts;
        AppendDisjoint(live, liveParts);
        std::vector<ShaderRecompiler::BdaAbi::Range> copyParts;
        AppendDisjoint(copies, copyParts);
        std::vector<ShaderRecompiler::BdaAbi::Range> uncovered;
        AppendUncovered(copyParts, liveParts, uncovered);
        result = MergeByBegin(liveParts, uncovered);
    } else {
        result.reserve(regions.size());
        for (const auto& region : regions) result.push_back(addressRange(region));
    }
    if (writes.empty()) return result;
    auto sortedWrites = writes;
    std::sort(sortedWrites.begin(), sortedWrites.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> intervals;
    for (const auto& interval : sortedWrites) {
        if (!intervals.empty() && interval.first <= intervals.back().second) intervals.back().second = std::max(intervals.back().second, interval.second);
        else intervals.push_back(interval);
    }
    std::vector<ShaderRecompiler::BdaAbi::Range> merged;
    merged.reserve(result.size() + intervals.size() * 2u);
    std::size_t firstWrite = 0;
    for (const auto& range : result) {
        if ((range.permissions & ShaderRecompiler::BdaAbi::Write) != 0u) {
            merged.push_back(range);
            continue;
        }
        while (firstWrite < intervals.size() && intervals[firstWrite].second <= range.begin) ++firstWrite;
        auto cursor = range.begin;
        const auto append = [&](std::uint64_t end, bool written) {
            if (cursor == end) return;
            auto part = range;
            part.begin = cursor;
            part.end = end;
            part.deviceAddress += cursor - range.begin;
            if (written) {
                const auto* region = owner(cursor, end);
                Require(region != nullptr && region->writable && (region->mirror == nullptr || region->mirror->writable), "runtime buffer write has no writable GPU owner");
                part.permissions |= ShaderRecompiler::BdaAbi::Write;
            }
            merged.push_back(part);
            cursor = end;
        };
        for (auto index = firstWrite; index < intervals.size() && intervals[index].first < range.end; ++index) {
            append(std::max(cursor, intervals[index].first), false);
            append(std::min(range.end, intervals[index].second), true);
        }
        append(range.end, false);
    }
    return merged;
}

std::optional<std::pair<std::uint64_t, std::shared_ptr<const std::vector<ShaderRecompiler::BdaAbi::Range>>>> GuestBufferMemory::TrySnapshotAddressSpace() noexcept {
    try {
        const auto space = Spaces().current.load();
        if (space == nullptr) return std::nullopt;
        return std::make_pair(space->serial, std::make_shared<const std::vector<ShaderRecompiler::BdaAbi::Range>>(space->ranges));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<GuestBufferMemory::CachedTable> GuestBufferMemory::CachedAddressTable() const {
    if (space == nullptr) return std::nullopt;
    Require(uploaded && !committed, "guest GPU address ranges are not available");
    if (writes.empty() && regions.empty()) return CachedTable{space->serial, &space->ranges};
    if (tableRanges != nullptr) return CachedTable{tableSerial, tableRanges.get()};
    const auto& spaceRanges = space->ranges;
    const bool inWritable = regions.empty() && std::all_of(writes.begin(), writes.end(), [&](const auto& written) {
        auto at = std::partition_point(spaceRanges.begin(), spaceRanges.end(), [&](const auto& range) { return range.end <= written.first; });
        for (; at != spaceRanges.end() && at->begin < written.second; ++at) {
            if ((at->permissions & ShaderRecompiler::BdaAbi::Write) == 0u) return false;
        }
        return true;
    });
    if (inWritable) return CachedTable{space->serial, &space->ranges};
    std::vector<ShaderRecompiler::BdaAbi::Range> own;
    own.reserve(regions.size());
    for (const auto& region : regions) own.push_back(addressRange(region));
    auto key = AddressTableKey(writes, own);
    static const bool verify = std::getenv("APS5_VERIFY_BDA_TABLES") != nullptr;
    std::lock_guard lock(space->tableMutex);
    const auto found = space->tables.find(key);
    if (found != space->tables.end()) {
        space->tableRecency.splice(space->tableRecency.end(), space->tableRecency, found->second.recent);
        tableSerial = found->second.serial;
        tableRanges = found->second.ranges;
        if (verify) {
            const auto built = AddressRanges();
            Require(built.size() == tableRanges->size() && (built.empty() || std::memcmp(built.data(), tableRanges->data(), built.size() * sizeof(built.front())) == 0), "a cached BDA table differs from the ranges of the build that found it");
        }
    } else {
        tableRanges = std::make_shared<const std::vector<ShaderRecompiler::BdaAbi::Range>>(AddressRanges());
        tableSerial = Spaces().serials.fetch_add(1, std::memory_order_relaxed) + 1;
        if (space->tables.size() >= 64u) {
            space->tables.erase(*space->tableRecency.front());
            space->tableRecency.pop_front();
        }
        const auto inserted = space->tables.emplace(std::move(key), AddressSpace::Table{tableSerial, tableRanges, {}}).first;
        inserted->second.recent = space->tableRecency.insert(space->tableRecency.end(), &inserted->first);
    }
    return CachedTable{tableSerial, tableRanges.get()};
}

bool GuestBufferMemory::HasCopiedWrites() const {
    for (const auto& [begin, end] : writes) {
        const auto* found = owner(begin, end);
        if (found == nullptr) continue;
        const auto& region = *found;
        if (region.direct != nullptr) continue;
        if (region.gpuCopy && (region.copiedBack || region.deviceLocal)) continue;
        if (region.mirror != nullptr && !region.mirror->writable) continue;
        return true;
    }
    return false;
}

bool GuestBufferMemory::CopiedWritesOverlap(std::uint64_t address, std::size_t bytes) const {
    if (bytes == 0) return false;
    for (const auto& [begin, end] : writes) {
        if (!(address < end && begin < address + bytes)) continue;
        const auto* found = owner(begin, end);
        if (found == nullptr) continue;
        const auto& region = *found;
        if (region.direct != nullptr) continue;
        if (region.gpuCopy && (region.copiedBack || region.deviceLocal)) continue;
        if (region.mirror != nullptr && !region.mirror->writable) continue;
        return true;
    }
    return false;
}

void GuestBufferMemory::RunAccesses(std::vector<std::array<std::uint64_t, 3>>& out) const {
    if (HoldsLease()) out.push_back({0, std::numeric_limits<std::uint64_t>::max(), Recorder::RunAccessWritten});
    for (const auto& region : regions) out.push_back({region.begin, region.end, 0});
    for (const auto& [begin, end] : writes) out.push_back({begin, end, Recorder::RunAccessWritten});
}

void GuestBufferMemory::MarkStagedWrites() const {
    for (const auto& [begin, end] : writes) {
        const auto* found = owner(begin, end);
        if (found != nullptr && found->gpuCopy && found->direct == nullptr) GuestMemory::MarkWritten(begin, static_cast<std::size_t>(end - begin));
    }
}

void GuestBufferMemory::MarkDirectWrites() const {
    for (const auto& [begin, end] : writes) {
        const auto* found = owner(begin, end);
        if (found == nullptr) continue;
        const auto& region = *found;
        if (region.direct != nullptr || (region.gpuCopy && region.copiedBack)) {
            std::uint64_t order = 0;
            static_cast<void>(GuestMemory::MarkPossiblyWritten(begin, static_cast<std::size_t>(end - begin), order));
            directWritesGeneration = std::max(directWritesGeneration, order);
        }
    }
}

namespace {

using ChangedStore = void (*)(std::uint64_t, std::span<const std::byte>, std::span<const std::byte>);

void storeChanged(std::uint64_t address, std::span<const std::byte> current, std::span<const std::byte> original, ChangedStore store) {
    const auto writable = GuestMemory::DescribeCommitted(address, current.size(), true);
    if (writable.whole) {
        store(address, current, original);
        return;
    }
    const auto unchanged = [&](std::uint64_t from, std::uint64_t to) {
        const auto at = static_cast<std::size_t>(from - address);
        if (std::memcmp(current.data() + at, original.data() + at, static_cast<std::size_t>(to - from)) == 0) return;
        char message[192];
        std::snprintf(message, sizeof(message), "the GPU changed guest memory 0x%llx+0x%llx that is not writable (a direct memory page shared by several views; aliased writes are not implemented)", static_cast<unsigned long long>(from), static_cast<unsigned long long>(to - from));
        throw std::runtime_error(message);
    };
    auto cursor = address;
    for (const auto& [first, last] : writable.ranges) {
        if (cursor < first) unchanged(cursor, first);
        const auto at = static_cast<std::size_t>(first - address);
        const auto length = static_cast<std::size_t>(last - first);
        store(first, current.subspan(at, length), original.subspan(at, length));
        cursor = last;
    }
    if (cursor < address + current.size()) unchanged(cursor, address + current.size());
}

void storeSparse(ImageMirror& mirror, std::uint64_t address, std::span<const std::byte> current, std::span<const std::byte> original) {
    constexpr std::uint64_t block = 65536;
    const auto end = address + current.size();
    const auto forget = [&](std::uint64_t from, std::uint64_t to) {
        const auto at = static_cast<std::size_t>(from - address);
        if (from >= to || std::memcmp(current.data() + at, original.data() + at, static_cast<std::size_t>(to - from)) == 0) return;
        for (auto index = from / block - mirror.base / block; index <= (to - 1) / block - mirror.base / block; ++index) mirror.generations[static_cast<std::size_t>(index)] = 0;
    };
    auto cursor = address;
    for (const auto& [first, last] : GuestMemory::DescribeCommitted(address, current.size(), true).ranges) {
        forget(cursor, first);
        const auto at = static_cast<std::size_t>(first - address);
        const auto length = static_cast<std::size_t>(last - first);
        GuestMemory::WriteChanged(first, current.subspan(at, length), original.subspan(at, length));
        cursor = last;
    }
    forget(cursor, end);
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> mergeWrites(std::vector<std::pair<std::uint64_t, std::uint64_t>> writes) {
    std::sort(writes.begin(), writes.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    for (const auto& range : writes) {
        if (!merged.empty() && range.first < merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
        else merged.push_back(range);
    }
    return merged;
}

}

void GuestBufferMemory::releaseHeapReferences() noexcept {
    for (auto& reference : heapReferences) ReferenceStorage::Shared().Return(std::move(reference.second));
    heapReferences.clear();
    heapReadbacks.clear();
}

void GuestBufferMemory::takeHeapReferences() {
    releaseHeapReferences();
    for (const auto& [begin, end] : mergeWrites(writes)) {
        const auto* found = owner(begin, end);
        if (found == nullptr || found->mirror == nullptr || !found->mirror->heap || !found->mirror->writable) continue;
        Require(end <= found->end, "written range exceeds its heap mirror");
        auto& mirror = *found->mirror;
        const auto served = std::min(end, servedEnd(*found));
        if (begin >= served) continue;
        growResident(mirror, {{begin, served}});
        const auto size = static_cast<std::size_t>(served - begin);
        auto storage = ReferenceStorage::Shared().Take(size);
        if (mirror.resident != nullptr) {
            storage.resize(size);
            mirror.resident->Read(begin, storage);
        } else {
            const auto bytes = mirror.buffer->Bytes().subspan(static_cast<std::size_t>(begin - mirror.base), size);
            storage.assign(bytes.begin(), bytes.end());
        }
        heapReferences.emplace_back(begin, std::move(storage));
    }
}

void GuestBufferMemory::WriteBack() {
    Require(uploaded && !committed, "guest memory cannot be committed twice or before upload");
    const auto merged = mergeWrites(writes);
    for (const auto& [begin, end] : merged) {
        const auto* found = owner(begin, end);
        Require(found != nullptr, "write-back range has no GPU owner");
        const auto& region = *found;
        if (region.direct == nullptr && !(region.gpuCopy && (region.copiedBack || region.deviceLocal)) && !(region.mirror != nullptr && !region.mirror->writable)) Recorder::NoteWrittenBack(begin, static_cast<std::size_t>(end - begin));
        if (region.direct != nullptr) {
            StorageTexture::StampKeepingNewerResults(begin, static_cast<std::size_t>(end - begin), directWritesGeneration, [&] { return GuestMemory::MarkPossiblyWritten(begin, static_cast<std::size_t>(end - begin)); });
            continue;
        }
        if (region.mirror != nullptr) {
            if (!region.mirror->writable) continue;
            Require(end <= region.end, "write-back range exceeds its GPU owner");
            auto& mirror = *region.mirror;
            const auto offset = static_cast<std::size_t>(begin - mirror.base);
            const auto served = std::min(end, servedEnd(region));
            if (mirror.heap && begin >= served) continue;
            const auto length = static_cast<std::size_t>((mirror.heap ? served : end) - begin);
            if (mirror.heap) {
                const auto reference = std::find_if(heapReferences.begin(), heapReferences.end(), [&](const auto& entry) { return entry.first == begin; });
                Require(reference != heapReferences.end() && reference->second.size() == length, "heap mirror write-back has no reference");
                if (mirror.resident == nullptr) {
                    const auto current = mirror.buffer->Bytes().subspan(offset, length);
                    if (mirror.sparse) storeSparse(mirror, begin, current, reference->second);
                    else storeChanged(begin, current, reference->second, GuestMemory::WriteChanged);
                    continue;
                }
                std::vector<std::byte> current(length);
                if (const auto readback = std::find_if(heapReadbacks.begin(), heapReadbacks.end(), [&](const auto& entry) { return entry.first == begin; }); readback != heapReadbacks.end()) {
                    Require(readback->second->Bytes().size() == length, "heap mirror readback does not match its write-back");
                    std::memcpy(current.data(), readback->second->Bytes().data(), length);
                } else {
                    mirror.resident->ReadDevice(begin, current);
                }
                mirror.resident->Store(begin, current);
                if (mirror.sparse) storeSparse(mirror, begin, current, reference->second);
                else storeChanged(begin, current, reference->second, GuestMemory::WriteChanged);
                continue;
            }
            const auto current = mirror.buffer->Bytes().subspan(offset, length);
            GuestMemory::WriteChanged(begin, current, std::span<const std::byte>(mirror.shadow).subspan(offset, length));
            std::memcpy(mirror.shadow.data() + offset, current.data(), length);
            continue;
        }
        if (region.gpuCopy) {
            Require(region.buffer != nullptr && end <= region.end, "write-back range exceeds its GPU owner");
            if (region.copiedBack) {
                StorageTexture::StampKeepingNewerResults(begin, static_cast<std::size_t>(end - begin), directWritesGeneration, [&] { return GuestMemory::MarkPossiblyWritten(begin, static_cast<std::size_t>(end - begin)); });
                continue;
            }
            static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
            if (region.deviceLocal) {
                continue;
            }
            GuestMemory::Write(begin, region.buffer->Bytes().subspan(static_cast<std::size_t>(begin - region.begin), static_cast<std::size_t>(end - begin)));
            if (profile) Copies().stagingStores.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        Require(region.buffer != nullptr && region.writable && end <= region.end, "write-back range exceeds its GPU owner");
        static const bool traceBig = std::getenv("APS5_TRACE_BIGBUF") != nullptr;
        if (traceBig && end - begin >= (1u << 20u)) std::fprintf(stderr, "[bigbuf] writeback 0x%llx+0x%llx sparse=%d\n", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin), region.sparse ? 1 : 0);
        if (!region.sparse) {
            const auto first = static_cast<std::size_t>(begin - region.begin);
            const auto length = static_cast<std::size_t>(end - begin);
            storeChanged(begin, region.buffer->Bytes().subspan(first, length), std::span<const std::byte>(region.uploaded).subspan(first, length), GuestMemory::WriteChangedKeepingGuest);
            continue;
        }
        std::size_t packed = 0;
        for (const auto& [first, last] : region.backed) {
            const auto from = std::max(first, begin);
            const auto to = std::min(last, end);
            if (from < to) {
                const auto length = static_cast<std::size_t>(to - from);
                storeChanged(from, region.buffer->Bytes().subspan(static_cast<std::size_t>(from - region.begin), length), std::span<const std::byte>(region.uploaded).subspan(packed + static_cast<std::size_t>(from - first), length), GuestMemory::WriteChangedKeepingGuest);
            }
            packed += static_cast<std::size_t>(last - first);
        }
    }
    committed = true;
    lease.clear();
    space.reset();
    releaseHeapReferences();
}

}
}
}
}
}
}
}
}
}
