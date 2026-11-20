#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/SrtWalker.hpp"
#include "ThreadOwned.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace AgcDriver {
namespace {
ShaderMemory::KnownValueCounts ShaderMemory::KnownValues() {
    auto& totals = CaptureTotals();
    return {totals.wordsKnown.load(std::memory_order_relaxed), totals.wordsKnownVerified.load(std::memory_order_relaxed), totals.wordsKnownMismatches.load(std::memory_order_relaxed)};
}

void ShaderMemory::CountHandleMemo(bool hit) {
    if (!CaptureProfiled()) return;
    (hit ? CaptureTotals().handleHits : CaptureTotals().handleMisses).fetch_add(1, std::memory_order_relaxed);
}

ShaderMemory::VouchCounts ShaderMemory::Vouches() {
    auto& totals = CaptureTotals();
    return {totals.vouchCalls.load(std::memory_order_relaxed), totals.vouchedBlocks.load(std::memory_order_relaxed), totals.refusedBlocks.load(std::memory_order_relaxed), totals.vouchedWords.load(std::memory_order_relaxed)};
}

ShaderMemory::SupplyCounts ShaderMemory::Supplies() {
    auto& totals = CaptureTotals();
    return {totals.suppliedShared.load(std::memory_order_relaxed), totals.suppliedCopied.load(std::memory_order_relaxed), totals.suppliedSpilled.load(std::memory_order_relaxed)};
}

std::vector<std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> ShaderMemory::SuppliedBlocks() const {
    std::vector<std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> blocks;
    blocks.reserve(supplied.size() + spilled.size());
    for (const auto& entry : supplied) blocks.push_back(entry.words);
    blocks.insert(blocks.end(), spilled.begin(), spilled.end());
    return blocks;
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture> ShaderMemory::Capture(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::SourceHandle* handle) {
    return captureWaitingOnFailure(request, handle, nullptr);
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture> ShaderMemory::Capture(const ShaderRecompiler::PreparedShaderInvocation& invocation) {
    return captureWaitingOnFailure(invocation.Request(), nullptr, &invocation);
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture> ShaderMemory::captureWaitingOnFailure(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::SourceHandle* handle, const ShaderRecompiler::PreparedShaderInvocation* invocation) {
    branchReadsRefused = 0;
    try {
        return capture(request, handle, invocation);
    } catch (const std::exception&) {
        if (branchReadsRefused == 0u) throw;
    }
    if (CaptureProfiled()) ++CaptureTotals().branchCapturesRedone;
    struct WaitBranches {
        bool& wait;
        explicit WaitBranches(bool& flag) : wait(flag) { wait = true; }
        ~WaitBranches() { wait = false; }
        WaitBranches(const WaitBranches&) = delete;
        WaitBranches& operator=(const WaitBranches&) = delete;
    } waiting(waitBranches);
    return capture(request, handle, invocation);
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture> ShaderMemory::capture(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::SourceHandle* handle, const ShaderRecompiler::PreparedShaderInvocation* invocation) {
    // The capture's word and page reads (through `read`) are attributed to it ([hooksync], [guestmem]).
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Capture);
    const bool profile = CaptureProfiled();
    auto& totals = CaptureTotals();
    // The hook's GPU waits made anywhere inside the capture.
    const auto waitedBefore = profile ? WaitedMs() : 0.0;
    const auto started = std::chrono::steady_clock::now();
    ShaderRecompiler::SrtRuntime runtime;
    runtime.userData = request.context.userData;
    runtime.shaderBase = request.shader.codeAddress;
    runtime.userContext = this;
    runtime.readMemory = &read;
    runtime.readSpecializationMemory = &read;
    runtime.accessible = &accessible;
    runtime.pendingWrite = &pending;
    runtime.readWords = &readWords;
    runtime.stampWrites = &stampWrites;
    runtime.unchangedSince = &unchangedSince;
    runtime.supplyWords = &supplyWords;
    runtime.descriptorScope = &descriptorScope;
    runtime.normalizeImageView = &normalizeImageView;
    runtime.imageViewGeneration = &imageViewGeneration;
    runtime.snapshotCurrent = &snapshotCurrent;
    auto capture = invocation != nullptr ? invocation->Capture(runtime) : handle != nullptr ? ShaderRecompiler::CaptureResources(request, runtime, *handle) : ShaderRecompiler::CaptureResources(request, runtime);
    if (profile) {
        totals.captureNanoseconds += NanosecondsSince(started);
        totals.resolveNanoseconds += capture->sourceNanoseconds;
        totals.hookWaitNanoseconds += static_cast<std::uint64_t>((WaitedMs() - waitedBefore) * 1e6);
        std::uint64_t initialBytes = 0;
        for (const auto& [address, bytes] : initial) initialBytes += bytes.size();
        ++totals.captures;
        static std::atomic<std::chrono::steady_clock::rep> nextReport{0};
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto due = nextReport.load(std::memory_order_relaxed);
        if (now >= due && nextReport.compare_exchange_strong(due, now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::seconds(10)).count(), std::memory_order_relaxed)) {
            const auto captures = static_cast<double>(totals.captures.load());
            const auto captureNs = totals.captureNanoseconds.load();
            const auto resolveNs = totals.resolveNanoseconds.load();
            const auto pageNs = totals.pageReadNanoseconds.load();
            const auto wordNs = totals.wordReadNanoseconds.load();
            const auto specializationNs = ShaderRecompiler::ResourceMaterializer::SpecializationNanoseconds();
            const auto hookNs = totals.hookWaitNanoseconds.load();
            const auto materializeNs = ShaderRecompiler::ResourceMaterializer::MaterializeNanoseconds();
            const auto evaluateNs = ShaderRecompiler::ResourceMaterializer::EvaluateNanoseconds();
            const auto walkNs = captureNs > resolveNs + pageNs + wordNs + specializationNs ? captureNs - resolveNs - pageNs - wordNs - specializationNs : 0;
            AgcDriver::ProfilePrint_nid_no_patch( "[capture] %llu captures: %llu word reads, %llu pages fetched (%llu read word by word over pending GPU writes: %llu words waited %.2f s, %llu read raw on evidence, %llu verified with %llu mismatches, %llu served from known values (%llu verified, %llu mismatches), %llu hook reads not observed: no GPU wait, %llu branch condition reads over pending GPU writes left the branch unpruned instead of waiting, %llu captures redone waiting after one failed), capture (plan lookup + materialize) %.1f s, %llu KiB registered code this capture; sub-phases (s, us per capture): source resolve %.2f/%.1f, walk %.2f/%.1f, specialization (every materialize) %.2f/%.1f, page reads %.2f/%.1f, word reads %.2f/%.1f, hook GPU waits inside %.2f/%.1f, materialize %.2f/%.1f (SRT evaluate %.2f/%.1f); source handle memo %llu hits, %llu resolves\n", static_cast<unsigned long long>(totals.captures.load()), static_cast<unsigned long long>(totals.reads.load()), static_cast<unsigned long long>(totals.pages.load()), static_cast<unsigned long long>(totals.pagesWordwise.load()), static_cast<unsigned long long>(totals.wordWaits.load()), totals.wordWaitNanoseconds.load() / 1e9, static_cast<unsigned long long>(totals.wordsRaw.load()), static_cast<unsigned long long>(totals.wordsVerified.load()), static_cast<unsigned long long>(totals.wordMismatches.load()), static_cast<unsigned long long>(totals.wordsKnown.load()), static_cast<unsigned long long>(totals.wordsKnownVerified.load()), static_cast<unsigned long long>(totals.wordsKnownMismatches.load()), static_cast<unsigned long long>(totals.observationsSkipped.load()), static_cast<unsigned long long>(totals.branchReadsRefused.load()), static_cast<unsigned long long>(totals.branchCapturesRedone.load()), captureNs / 1e9, static_cast<unsigned long long>(initialBytes / 1024), resolveNs / 1e9, resolveNs / captures / 1e3, walkNs / 1e9, walkNs / captures / 1e3, specializationNs / 1e9, specializationNs / captures / 1e3, pageNs / 1e9, pageNs / captures / 1e3, wordNs / 1e9, wordNs / captures / 1e3, hookNs / 1e9, hookNs / captures / 1e3, materializeNs / 1e9, materializeNs / captures / 1e3, evaluateNs / 1e9, evaluateNs / captures / 1e3, static_cast<unsigned long long>(totals.handleHits.load()), static_cast<unsigned long long>(totals.handleMisses.load()));
        }
    }
    return capture;
}

std::vector<ShaderRecompiler::MemoryRegion> ShaderMemory::collect(bool recentOnly) const {
    struct Cursor {
        const ShaderRecompiler::SrtGuestWords* words;
        std::size_t run;
        std::size_t at;
    };
    std::vector<Cursor> cursors;
    std::size_t runs = 0;
    for (const auto& entry : supplied) {
        if ((recentOnly && !entry.recent) || entry.words->runs.empty()) continue;
        cursors.push_back({entry.words.get(), 0u, 0u});
        runs += entry.words->runs.size() + entry.words->words.size() / PageWords;
    }
    std::vector<ShaderRecompiler::MemoryRegion> result;
    result.reserve((recentOnly ? 0u : initial.size()) + pages.size() + runs);
    auto next = recentOnly ? initial.end() : initial.begin();
    const bool heap = cursors.size() > 1u;
    const auto later = [](const Cursor& left, const Cursor& right) {
        return left.words->runs[left.run].first > right.words->runs[right.run].first;
    };
    if (heap) std::make_heap(cursors.begin(), cursors.end(), later);
    const auto emitBefore = [&](std::uint64_t address) {
        while (true) {
            Cursor* lowest = heap && !cursors.empty() ? &cursors.front() : nullptr;
            if (!heap) {
                for (auto& cursor : cursors) {
                    if (cursor.run < cursor.words->runs.size() && (lowest == nullptr || cursor.words->runs[cursor.run].first < lowest->words->runs[lowest->run].first)) lowest = &cursor;
                }
            }
            const auto run = lowest != nullptr ? lowest->words->runs[lowest->run].first : std::numeric_limits<std::uint64_t>::max();
            if (next != initial.end() && next->first < address && next->first < run) {
                result.push_back({next->first, next->second});
                ++next;
            } else if (lowest != nullptr && run < address) {
                if (heap) {
                    std::pop_heap(cursors.begin(), cursors.end(), later);
                    lowest = &cursors.back();
                }
                const auto count = lowest->words->runs[lowest->run].second;
                const auto words = std::span(lowest->words->words).subspan(lowest->at, count);
                for (std::size_t done = 0; done < count;) {
                    const auto at = run + done * sizeof(std::uint32_t);
                    const auto piece = std::min<std::size_t>(count - done, (PageBytes - static_cast<std::size_t>(at % PageBytes)) / sizeof(std::uint32_t));
                    result.push_back({at, std::as_bytes(words.subspan(done, piece))});
                    done += piece;
                }
                lowest->at += count;
                ++lowest->run;
                if (heap) {
                    if (lowest->run == lowest->words->runs.size()) cursors.pop_back();
                    else std::push_heap(cursors.begin(), cursors.end(), later);
                }
            } else {
                return;
            }
        }
    };
    for (const auto& [base, pagePointer] : pages) {
        const auto& page = *pagePointer;
        const auto& marks = recentOnly ? page.recent : page.read;
        if (marks.None()) continue;
        marks.ForEachRun([&](std::size_t first, std::size_t end) {
            const auto address = base + first * sizeof(std::uint32_t);
            emitBefore(address);
            result.push_back({address, std::as_bytes(std::span(page.words).subspan(first, end - first))});
        });
    }
    emitBefore(std::numeric_limits<std::uint64_t>::max());
    return result;
}

std::vector<ShaderRecompiler::MemoryRegion> ShaderMemory::Regions() const {
    return collect(false);
}

std::vector<ShaderRecompiler::MemoryRegion> ShaderMemory::TakeRecentRegions() {
    auto result = collect(true);
    for (auto& [base, page] : pages) page->recent.Reset();
    for (auto& entry : supplied) entry.recent = false;
    return result;
}

void AppendSharedWords(std::vector<SharedWords>& shared, std::size_t region, std::size_t first, std::span<const std::byte> bytes, std::span<const std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> blocks) {
    const auto address = reinterpret_cast<std::uintptr_t>(bytes.data());
    const auto count = bytes.size() / sizeof(std::uint32_t);
    if (count == 0) return;
    for (const auto& block : blocks) {
        if (block == nullptr || block->words.empty()) continue;
        const auto begin = reinterpret_cast<std::uintptr_t>(block->words.data());
        const auto end = begin + block->words.size() * sizeof(std::uint32_t);
        if (address < begin || address >= end || end - address < count * sizeof(std::uint32_t) || (address - begin) % sizeof(std::uint32_t) != 0) continue;
        const auto offset = static_cast<std::size_t>((address - begin) / sizeof(std::uint32_t));
        if (!shared.empty()) {
            auto& last = shared.back();
            if (last.first + last.count == first && last.offset + last.count == offset && last.region + last.regions == region && !last.block.owner_before(block) && !block.owner_before(last.block)) {
                last.count += count;
                ++last.regions;
                return;
            }
        }
        shared.push_back({first, count, offset, block, region, 1u});
        return;
    }
}

bool SameCapturedWords(std::span<const std::uint32_t> left, std::span<const SharedWords> leftShared, std::span<const std::uint32_t> right, std::span<const SharedWords> rightShared, std::size_t* compared) {
    if (compared != nullptr) *compared = 0;
    if (left.size() != right.size()) return false;
    std::size_t checked = 0;
    const auto equal = [&](std::size_t from, std::size_t to) {
        checked += to - from;
        return std::equal(left.begin() + static_cast<std::ptrdiff_t>(from), left.begin() + static_cast<std::ptrdiff_t>(to), right.begin() + static_cast<std::ptrdiff_t>(from));
    };
    std::size_t at = 0;
    std::size_t next = 0;
    bool same = true;
    for (const auto& run : leftShared) {
        while (next < rightShared.size() && rightShared[next].first < run.first) ++next;
        if (next == rightShared.size()) break;
        const auto& other = rightShared[next];
        const bool placed = other.first == run.first && other.count == run.count && other.offset == run.offset && run.first >= at && run.first <= left.size() && run.count <= left.size() - run.first;
        if (!placed || run.block.expired() || run.block.owner_before(other.block) || other.block.owner_before(run.block)) continue;
        if (!equal(at, run.first)) {
            same = false;
            break;
        }
        at = run.first + run.count;
    }
    same = same && equal(at, left.size());
    if (compared != nullptr) *compared = checked;
    return same;
}

std::size_t VouchSharedRegions(std::span<const SharedWords> shared, std::size_t regions, ShaderMemory::PendingWriteQuery pending, std::vector<VouchedRegions>& vouched, std::span<const std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> restampable, bool (*restamp)(const ShaderRecompiler::SrtGuestWords&, ShaderMemory::PendingWriteQuery)) {
    vouched.clear();
    if (shared.empty() || regions == 0) return 0;
    thread_local std::vector<std::pair<const ShaderRecompiler::SrtGuestWords*, bool>>* blocksSlot = nullptr;
    auto& blocks = ShaderRecompiler::ThreadOwned(blocksSlot);
    blocks.clear();
    auto& totals = CaptureTotals();
    std::size_t words = 0;
    for (const auto& entry : shared) {
        if (entry.regions == 0 || entry.region > regions || entry.regions > regions - entry.region || (!vouched.empty() && entry.region < vouched.back().end)) continue;
        const auto block = entry.block.lock();
        if (block == nullptr || block->low >= block->high) continue;
        auto known = std::find_if(blocks.begin(), blocks.end(), [&](const auto& seen) { return seen.first == block.get(); });
        if (known == blocks.end()) {
            const bool recoverable = restamp != nullptr && std::ranges::find(restampable, block) != restampable.end();
            const bool good = ShaderMemory::Vouched(pending, block->low, block->high - block->low, ShaderRecompiler::LoadStamp(*block), recoverable) || (recoverable && restamp(*block, pending));
            (good ? totals.vouchedBlocks : totals.refusedBlocks).fetch_add(1, std::memory_order_relaxed);
            known = blocks.insert(blocks.end(), {block.get(), good});
        }
        if (!known->second) continue;
        if (!vouched.empty() && vouched.back().end == entry.region) {
            auto& last = vouched.back();
            last.end += entry.regions;
            last.words += entry.count;
            last.low = std::min(last.low, block->low);
            last.high = std::max(last.high, block->high);
        } else {
            vouched.push_back({entry.region, entry.region + entry.regions, entry.count, block->low, block->high});
        }
        words += entry.count;
    }
    blocks.clear();
    totals.vouchedWords.fetch_add(words, std::memory_order_relaxed);
    return words;
}

DataWordPositionCounts DataWordPositions(std::span<const std::pair<std::uint64_t, std::uint64_t>> runs, std::span<const std::pair<std::uint32_t, std::uint64_t>> leaves, std::span<const std::uint64_t> otherReads, std::span<const std::uint32_t> words, std::span<const std::uint32_t> flattenedSrt, std::vector<std::uint32_t>& positions, std::vector<std::uint32_t>& slots) {
    DataWordPositionCounts counts;
    positions.clear();
    slots.clear();
    std::vector<std::size_t> prefix(runs.size() + 1, 0);
    for (std::size_t i = 0; i < runs.size(); ++i) prefix[i + 1] = prefix[i] + static_cast<std::size_t>((runs[i].second - runs[i].first) / sizeof(std::uint32_t));
    std::vector<std::pair<std::uint32_t, std::uint32_t>> found;
    for (const auto& [slot, address] : leaves) {
        if ((address & 3u) != 0) {
            ++counts.unmapped;
            continue;
        }
        if (std::binary_search(otherReads.begin(), otherReads.end(), address)) {
            ++counts.aliased;
            continue;
        }
        auto run = std::upper_bound(runs.begin(), runs.end(), address, [](std::uint64_t value, const std::pair<std::uint64_t, std::uint64_t>& candidate) { return value < candidate.first; });
        if (run == runs.begin() || address + sizeof(std::uint32_t) > (run - 1)->second) {
            ++counts.unmapped;
            continue;
        }
        --run;
        const auto position = prefix[static_cast<std::size_t>(run - runs.begin())] + static_cast<std::size_t>((address - run->first) / sizeof(std::uint32_t));
        if (position >= words.size() || slot >= flattenedSrt.size() || words[position] != flattenedSrt[slot]) {
            ++counts.mismatched;
            continue;
        }
        found.emplace_back(static_cast<std::uint32_t>(position), slot);
    }
    std::sort(found.begin(), found.end());
    positions.reserve(found.size());
    slots.reserve(found.size());
    for (const auto& [position, slot] : found) {
        positions.push_back(position);
        slots.push_back(slot);
    }
    return counts;
}

}
}
