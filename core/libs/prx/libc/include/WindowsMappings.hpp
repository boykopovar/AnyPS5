#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSMAPPINGS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSMAPPINGS_HPP

#ifdef _WIN32
#include <windows.h>
#include <algorithm>
#include <array>
#ifdef APS5_COUNT_MAPPING_QUERIES
#include <atomic>
#endif
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <map>
#include <memory>
#include <vector>
#include <stdexcept>
#include <string>
#include <system_error>

namespace GuestArena {

class WindowsMappings {
public:
    enum class Collection { All, Armed, Fresh };

    static WindowsMappings& Get() {
        static auto* mappings = new WindowsMappings;
        return *mappings;
    }

#ifdef APS5_COUNT_MAPPING_QUERIES
    static std::uint64_t MemoryQueries() {
        return queries.load(std::memory_order_relaxed);
    }
#endif

    void* Reserve(void* address, std::size_t bytes) {
        return allocate(GetCurrentProcess(), address, bytes, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
    }

    std::vector<std::pair<std::uintptr_t, std::size_t>> Commit(void* address, std::size_t bytes, DWORD protection, std::size_t granule, bool watched) {
        if (granule == 0) throw std::invalid_argument("guest memory commit granule is zero");
        std::vector<std::pair<std::uintptr_t, std::size_t>> created;
        std::lock_guard lock(mutex);
        collectableRanges.clear();
        const DWORD flags = MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER | (watched ? MEM_WRITE_WATCH : 0);
        const auto cut = [](std::uintptr_t at, std::size_t size, const char* operation) {
            if (!VirtualFree(reinterpret_cast<void*>(at), size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) fail(operation);
        };
        const auto replace = [&](std::uintptr_t at, std::size_t size) {
            if (!allocate(GetCurrentProcess(), reinterpret_cast<void*>(at), size, flags, protection, nullptr, 0)) fail("replace guest placeholder with private memory");
            privateAllocations.emplace(at, at + size);
            if (!created.empty() && created.back().first + created.back().second == at) created.back().second += size;
            else created.emplace_back(at, size);
        };
        const auto end = reinterpret_cast<std::uintptr_t>(address) + bytes;
        for (auto cursor = reinterpret_cast<std::uintptr_t>(address); cursor < end;) {
            const auto memory = query(cursor);
            const auto regionEnd = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
            const auto stop = std::min(end, regionEnd);
            if (memory.State == MEM_RESERVE && std::min(end, cursor + granule) <= regionEnd) {
                privateAllocations.erase(privateAllocations.lower_bound(cursor), privateAllocations.lower_bound(stop));
                forgetClean(cursor, stop);
                const auto holder = reinterpret_cast<std::uintptr_t>(memory.AllocationBase);
                if (holder != cursor) cut(holder, cursor - holder, "split guest placeholder prefix");
                if (stop != regionEnd) cut(cursor, stop - cursor, "split guest placeholder suffix");
                while (cursor < stop && std::min(end, cursor + granule) <= stop) {
                    const auto size = std::min(end, cursor + granule) - cursor;
                    if (cursor + size != stop) cut(cursor, size, "split guest placeholder suffix");
                    replace(cursor, size);
                    cursor += size;
                }
                continue;
            }
            if (memory.State == MEM_RESERVE) {
                const auto limit = std::min(end, cursor + granule);
                auto placeholderEnd = stop;
                while (placeholderEnd < limit) {
                    const auto next = query(placeholderEnd);
                    if (next.State != MEM_RESERVE) break;
                    placeholderEnd = reinterpret_cast<std::uintptr_t>(next.BaseAddress) + next.RegionSize;
                }
                const auto size = std::min(limit, placeholderEnd) - cursor;
                reset(cursor, size);
                replace(cursor, size);
                cursor += size;
            } else {
                if (memory.State != MEM_COMMIT) throw std::runtime_error("guest memory is not committed");
                for (auto mapped = views.lower_bound(cursor & ~(pageBytes - 1)); mapped != views.end() && mapped->first < stop; ++mapped) {
                    mapped->second.protection = protection;
                    mapped->second.armed = 0;
                    invalidate(mapped->first, mapped->second);
                }
                DWORD previous;
                if (!VirtualProtect(reinterpret_cast<void*>(cursor), stop - cursor, protection, &previous)) fail("protect guest memory");
                cursor = stop;
            }
        }
        return created;
    }

    void Reset(void* address, std::size_t bytes) {
        std::lock_guard lock(mutex);
        reset(reinterpret_cast<std::uintptr_t>(address), bytes);
    }

    void Map(void* address, std::size_t bytes, HANDLE section, std::uint64_t offset, DWORD protection, bool watched = false, bool resident = false, DWORD viewProtection = PAGE_EXECUTE_READWRITE, std::size_t sectionBytes = SIZE_MAX, std::vector<std::pair<std::uintptr_t, std::size_t>>* createdPrivate = nullptr) {
        if (sectionBytes < bytes && (resident || sectionBytes == 0 || bytes - sectionBytes >= pageBytes)) throw std::runtime_error("shared guest mapping extends a whole page past its section");
        std::lock_guard lock(mutex);
        collectableRanges.clear();
        auto cursor = reinterpret_cast<std::uintptr_t>(address);
        if (createdPrivate != nullptr) createdPrivate->reserve(createdPrivate->size() + bytes / pageBytes);
        reset(cursor, bytes);
        if (resident) watchResident = watched;
        HANDLE duplicate = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), section, GetCurrentProcess(), &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS)) fail("keep shared guest section");
        const auto owned = std::make_shared<Section>(duplicate);
        const auto key = reinterpret_cast<std::uintptr_t>(section);
        const auto slotOf = [&](std::size_t done) -> const std::shared_ptr<Physical>& {
            auto& slot = physical[std::make_pair(key, offset + done)];
            if (!slot) slot = std::make_shared<Physical>();
            return slot;
        };
        const auto pageOf = [](Physical& slot) {
            auto shared = slot.page.lock();
            if (!shared) {
                shared = std::make_shared<SharedPage>();
                shared->comparedWhenShared = slot.comparedWhenShared;
                slot.page = shared;
            }
            return shared;
        };
        const auto single = [&](std::size_t done) {
            if (!resident || residentRun(key, offset + done) != residentRuns.end()) return false;
            const auto found = physical.find(std::make_pair(key, offset + done));
            if (found == physical.end()) return true;
            const auto shared = found->second->page.lock();
            return !found->second->stored && (!shared || shared->aliases.empty());
        };
        const bool perPage = !resident && sectionBytes == SIZE_MAX;
        if (perPage) commitSection(section, offset, bytes);
        for (std::size_t done = 0; done < bytes;) {
            const auto base = cursor + done;
            if (single(done)) {
                auto runBytes = pageBytes;
                while (done + runBytes < bytes && (base + runBytes) % chunkBytes != 0 && single(done + runBytes)) runBytes += pageBytes;
                split(base, runBytes);
                commitResident(base, runBytes);
                if (createdPrivate != nullptr) {
                    if (!createdPrivate->empty() && createdPrivate->back().first + createdPrivate->back().second == base) createdPrivate->back().second += runBytes;
                    else createdPrivate->emplace_back(base, runBytes);
                }
                const auto run = residentRuns.emplace(base, Run{base + runBytes, {}, static_cast<std::uint32_t>(runBytes >> 12), key, offset + done}).first;
                run->second.watched = watchResident;
                residentIndex.emplace(std::make_pair(key, offset + done), run);
                for (std::size_t at = 0; at < runBytes; at += pageBytes) {
                    bool compared = false;
                    if (const auto found = physical.find(std::make_pair(key, offset + done + at)); found != physical.end()) {
                        const auto& slot = *found->second;
                        if (slot.saved) std::memcpy(reinterpret_cast<void*>(base + at), slot.saved->bytes + slot.savedOffset, pageBytes);
                        compared = slot.comparedWhenShared;
                        physical.erase(found);
                    }
                    views.emplace(base + at, View{nullptr, protection, {}, 0, compared, 0, owned, offset + done + at, 0, 0, true});
                }
                forgetClean(base, base + runBytes);
                remember(freshRanges, base, base + runBytes);
                DWORD previous;
                if (protection != PAGE_READWRITE && !VirtualProtect(reinterpret_cast<void*>(base), runBytes, protection, &previous)) fail("protect resident guest pages");
                done += runBytes;
                continue;
            }
            if (sectionBytes != SIZE_MAX) {
                if (const auto run = residentRun(key, offset + done); run != residentRuns.end()) demote(run->first);
                const auto slot = slotOf(done);
                const auto shared = pageOf(*slot);
                for (const auto alias : shared->aliases) views.at(alias).aliasDemoted = false;
                const auto viewBytes = std::min(pageBytes, sectionBytes - done);
                split(base, pageBytes);
                mapShared(base, section, offset + done, protection, viewProtection, viewBytes);
                slot->stored = true;
                shared->aliases.push_back(base);
                const auto created = views.emplace(base, View{shared, protection, {}, 0, false, 0, owned, offset + done, 0, 0, false, slot, hostPages(viewBytes)}).first;
                if (shared->comparedWhenShared) created->second.unreported = allSubpages;
                remember(freshRanges, base, base + pageBytes);
                invalidate(*shared);
                done += pageBytes;
                continue;
            }
            std::vector<std::pair<std::shared_ptr<Physical>, std::shared_ptr<SharedPage>>> pages;
            pages.reserve((bytes - done) / pageBytes);
            auto runBytes = std::size_t{0};
            do {
                if (const auto run = residentRun(key, offset + done + runBytes); run != residentRuns.end()) demote(run->first);
                const auto slot = slotOf(done + runBytes);
                const auto shared = pageOf(*slot);
                for (const auto alias : shared->aliases) views.at(alias).aliasDemoted = false;
                pages.emplace_back(slot, shared);
                runBytes += pageBytes;
            } while (done + runBytes < bytes && !single(done + runBytes));
            if (!perPage) commitSection(section, offset + done, runBytes);
            split(base, runBytes);
            mapRun(base, runBytes, section, offset + done);
            DWORD previous;
            if (!VirtualProtect(reinterpret_cast<void*>(base), runBytes, protection, &previous)) fail("protect shared guest pages");
            collectableRanges.clear();
            for (std::size_t at = 0; at < runBytes; at += pageBytes) {
                auto& [slot, shared] = pages[at / pageBytes];
                slot->stored = true;
                shared->aliases.push_back(base + at);
                invalidate(*shared);
                const auto created = views.emplace(base + at, View{std::move(shared), protection, {}, 0, false, 0, owned, offset + done + at, 0, 0, false, std::move(slot)}).first;
                if (created->second.page->comparedWhenShared) created->second.unreported = allSubpages;
            }
            remember(freshRanges, base, base + runBytes);
            done += runBytes;
        }
    }

    void SetProtection(std::uintptr_t address, std::size_t bytes, DWORD protection) {
        std::lock_guard lock(mutex);
        collectableRanges.clear();
        for (auto it = views.lower_bound(address); it != views.end() && it->first < address + bytes; ++it) {
            it->second.protection = protection;
            it->second.armed = 0;
            invalidate(it->first, it->second);
        }
    }

    void Pin(std::uintptr_t address, std::size_t bytes) {
        std::lock_guard lock(mutex);
        for (auto it = views.lower_bound(address & ~(pageBytes - 1)); it != views.end() && it->first < address + bytes; ++it) {
            if (it->second.resident) {
                ++it->second.pins;
                invalidate(it->first, it->second);
                continue;
            }
            auto& page = *it->second.page;
            ++page.pins;
            for (const auto alias : page.aliases) {
                auto& view = views.at(alias);
                if (view.armed == 0) continue;
                protectSubpages(alias, view.armed, view.protection, "pin shared guest page writable", view.viewBytes);
                view.armed = 0;
            }
            invalidate(page);
        }
    }

    void Unpin(std::uintptr_t address, std::size_t bytes) {
        std::lock_guard lock(mutex);
        for (auto it = views.lower_bound(address & ~(pageBytes - 1)); it != views.end() && it->first < address + bytes; ++it) {
            auto& pins = it->second.resident ? it->second.pins : it->second.page->pins;
            if (pins != 0) --pins;
            invalidate(it->first, it->second);
        }
    }

    void CompareWhenShared(std::uintptr_t address, std::size_t bytes, bool residentOnly = false) {
        std::lock_guard lock(mutex);
        collectableRanges.clear();
        for (auto it = views.lower_bound(address & ~(pageBytes - 1)); it != views.end() && it->first < address + bytes; ++it) {
            if (residentOnly && !it->second.resident && !it->second.aliasDemoted) continue;
            if (it->second.resident) {
                it->second.comparedWhenShared = true;
                continue;
            }
            auto& page = *it->second.page;
            const bool marked = page.comparedWhenShared;
            page.comparedWhenShared = true;
            it->second.slot->comparedWhenShared = true;
            for (const auto alias : page.aliases) {
                auto& view = views.at(alias);
                if (view.resident) continue;
                if (!marked) view.unreported = allSubpages;
                if (view.armed == 0) continue;
                protectSubpages(alias, view.armed, view.protection, "stop arming a compared shared page", view.viewBytes);
                view.armed = 0;
            }
        }
    }

    void ComparedPages(std::uintptr_t address, std::size_t bytes, void** pages, std::size_t* count, bool unreported) {
        std::lock_guard lock(mutex);
        const auto capacity = *count;
        *count = 0;
        const auto end = address + bytes;
        for (auto it = views.lower_bound(address & ~(pageBytes - 1)); it != views.end() && it->first < end; ++it) {
            auto& view = it->second;
            if (view.resident || !view.page->comparedWhenShared) continue;
            const auto listed = static_cast<std::uint8_t>(subpagesOf(it->first, address, end) & (unreported ? view.unreported : allSubpages));
            for (std::size_t sub = 0; sub < subpages; ++sub) {
                const auto bit = static_cast<std::uint8_t>(1u << sub);
                if ((listed & bit) == 0) continue;
                if (*count == capacity) return;
                pages[(*count)++] = reinterpret_cast<void*>(it->first + sub * subpageBytes);
                if (unreported) view.unreported &= static_cast<std::uint8_t>(~bit);
            }
        }
    }

    bool HandleWrite(std::uintptr_t address) {
        std::lock_guard lock(mutex);
        const auto base = address & ~(pageBytes - 1);
        const auto found = views.find(base);
        if (found == views.end() || !writable(found->second.protection)) return false;
        auto& view = found->second;
        if (view.resident) {
            const auto memory = query(address);
            return memory.State == MEM_COMMIT && writable(memory.Protect & 0xffu);
        }
        const auto written = subpagesOf(base, address, address + 1);
        invalidate(*view.page, written);
        protectSubpages(base, written, view.protection, "resume shared memory write", view.viewBytes);
        view.armed &= static_cast<std::uint8_t>(~written);
        return true;
    }

    bool HandleRead(std::uintptr_t address) {
        std::lock_guard lock(mutex);
        if (!views.contains(address & ~(pageBytes - 1))) return false;
        const auto memory = query(address);
        return memory.State == MEM_COMMIT && (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
    }

    bool ResidentSpan(std::uintptr_t address, std::size_t bytes, std::uintptr_t* start, std::uintptr_t* end) {
        std::lock_guard lock(mutex);
        auto run = residentRuns.upper_bound(address);
        if (run != residentRuns.begin() && std::prev(run)->second.end > address) --run;
        if (run == residentRuns.end() || run->first >= address + bytes) return false;
        *start = run->first;
        for (; run != residentRuns.end() && run->first < address + bytes; ++run) *end = run->second.end;
        return true;
    }

    bool ForgetSection(HANDLE section) {
        std::lock_guard lock(mutex);
        const auto key = reinterpret_cast<std::uintptr_t>(section);
        const auto first = physical.lower_bound(std::make_pair(key, std::uint64_t{0}));
        const auto last = physical.lower_bound(std::make_pair(key + 1, std::uint64_t{0}));
        bool blank = std::none_of(first, last, [](const auto& entry) { return entry.second->stored || !entry.second->page.expired(); });
        physical.erase(first, last);
        for (auto run = residentIndex.lower_bound(std::make_pair(key, std::uint64_t{0})); run != residentIndex.end() && run->first.first == key; run = residentIndex.erase(run)) blank = false;
        return blank;
    }

    bool BeginHostWrite(std::uintptr_t address, std::size_t bytes, bool store = true) {
        std::lock_guard lock(mutex);
        const auto first = views.lower_bound(address & ~(pageBytes - 1));
        const auto end = address + bytes;
        for (auto it = first; it != views.end() && it->first < end; ++it) {
            if (!writable(it->second.protection)) return false;
        }
        for (auto it = first; it != views.end() && it->first < end; ++it) {
            auto& view = it->second;
            if (store) ++view.hostWrites;
            else ++view.hostImports;
            const auto written = subpagesOf(it->first, address, end);
            if (store) invalidate(it->first, view, written);
            else if (view.resident) {
                forgetClean(it->first, it->first + pageBytes);
                markImported(it->first);
            }
            const auto opened = static_cast<std::uint8_t>((store || view.resident) ? view.armed & written : 0);
            if (opened == 0) continue;
            protectSubpages(it->first, opened, view.protection, "open shared memory to a host write", view.viewBytes);
            view.armed &= static_cast<std::uint8_t>(~opened);
        }
        return true;
    }

    void EndHostWrite(std::uintptr_t address, std::size_t bytes, bool store = true) {
        std::lock_guard lock(mutex);
        const auto end = address + bytes;
        for (auto it = views.lower_bound(address & ~(pageBytes - 1)); it != views.end() && it->first < end; ++it) {
            if (store) --it->second.hostWrites;
            else --it->second.hostImports;
            if (store) invalidate(it->first, it->second, subpagesOf(it->first, address, end));
            else if (it->second.resident) forgetClean(it->first, it->first + pageBytes);
        }
    }

    static std::vector<bool> TouchedPages(std::uintptr_t start, std::size_t pages, void* const* written, std::size_t count) {
        std::vector<bool> touched(pages);
        for (std::size_t index = 0; index < count; ++index) {
            const auto page = (reinterpret_cast<std::uintptr_t>(written[index]) - start) / pageBytes;
            if (page < pages) touched[page] = true;
        }
        return touched;
    }

    void markImported(std::uintptr_t page) {
        const auto next = residentRuns.upper_bound(page);
        if (next == residentRuns.begin()) return;
        const auto run = std::prev(next);
        if (page < run->second.end) run->second.imported = true;
    }

    void* MapAlias(std::uintptr_t address, std::size_t bytes) {
        std::lock_guard lock(mutex);
        const auto refuse = [&](const char* reason) {
            char text[192];
            std::snprintf(text, sizeof(text), "read-write alias of shared guest memory 0x%llx+0x%llx: %s", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), reason);
            return std::runtime_error(text);
        };
        if (address % pageBytes != 0 || bytes % pageBytes != 0 || bytes == 0) throw refuse("the range is not made of whole shared pages");
        for (std::size_t done = 0; done < bytes; done += pageBytes) {
            const auto found = views.find(address + done);
            if (found == views.end() || !found->second.resident) continue;
            const auto run = std::prev(residentRuns.upper_bound(address + done));
            if (run->first < address || run->second.end > address + bytes) throw refuse("a resident run of direct memory extends outside the range");
            for (auto page = views.find(run->first); page != views.end() && page->first < run->second.end; ++page) {
                if (page->second.hostWrites != 0) return nullptr;
            }
        }
        for (std::size_t done = 0; done < bytes; done += pageBytes) {
            const auto found = views.find(address + done);
            if (found != views.end() && found->second.resident) demote(address + done, true);
        }
        auto view = views.find(address);
        if (view == views.end()) throw refuse("the range does not start at a shared view");
        const auto section = view->second.section;
        const auto offset = view->second.offset;
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        bool contiguous = true;
        for (std::size_t done = 0; done < bytes; done += pageBytes) {
            const auto page = views.find(address + done);
            if (page == views.end()) throw refuse("the range is not a run of shared views");
            if (page->second.viewBytes != pageBytes) throw refuse("a page extends past the end of its file");
            if (page->second.offset != offset + done || (page->second.section != section && !sameSection(page->second.section->handle, section->handle))) contiguous = false;
        }
        if (!contiguous) return mapAliasRuns(address, bytes);
        const auto lead = offset % system.dwAllocationGranularity;
        void* alias = map(section->handle, GetCurrentProcess(), nullptr, offset - lead, lead + bytes, 0, PAGE_READWRITE, nullptr, 0);
        if (alias == nullptr) {
            char text[160];
            std::snprintf(text, sizeof(text), "MapViewOfFile3 of a read-write alias of shared guest memory 0x%llx+0x%llx", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), text);
        }
        return static_cast<char*>(alias) + lead;
    }

    void UnmapAlias(void* alias) {
        if (alias == nullptr) return;
        {
            std::lock_guard lock(mutex);
            if (const auto found = splitAliases.find(reinterpret_cast<std::uintptr_t>(alias)); found != splitAliases.end()) {
                const auto error = releaseAlias(found->first, found->second);
                splitAliases.erase(found);
                if (error != ERROR_SUCCESS) throw std::system_error(static_cast<int>(error), std::system_category(), "unmap a run of a shared guest alias");
                return;
            }
        }
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        const auto base = reinterpret_cast<std::uintptr_t>(alias) & ~(static_cast<std::uintptr_t>(system.dwAllocationGranularity) - 1);
        if (!unmap(GetCurrentProcess(), reinterpret_cast<void*>(base), 0)) fail("unmap shared guest alias");
    }

    bool Protection(std::uintptr_t address, std::uint32_t* protection) {
        std::lock_guard lock(mutex);
        const auto found = views.find(address & ~(pageBytes - 1));
        if (found == views.end()) return false;
        *protection = found->second.protection;
        return true;
    }

#ifdef APS5_COUNT_MAPPING_QUERIES
    void CollectWork(std::uint64_t* systemCalls, std::uint64_t* pieces) {
        std::lock_guard lock(mutex);
        *systemCalls = collectSystemCalls;
        *pieces = collectPieces;
    }
#endif

    bool Collect(std::uintptr_t address, std::size_t bytes, void** pages, std::size_t* count, bool clear, Collection collection = Collection::All) {
        std::lock_guard lock(mutex);
        const auto capacity = *count;
        *count = 0;
        const auto end = address + bytes;
        if (collection == Collection::Fresh) {
            const auto fresh = freshRanges.upper_bound(address);
            if ((fresh == freshRanges.begin() || std::prev(fresh)->second <= address) && (fresh == freshRanges.end() || fresh->first >= end)) return true;
            for (auto it = views.lower_bound(address & ~(pageBytes - 1)); it != views.end() && it->first < end; ++it) {
                if (it->second.resident) {
                    if (!watchResident) return false;
                    continue;
                }
                if (it->second.page->comparedWhenShared) return false;
                if (it->second.protection != PAGE_NOACCESS) continue;
                for (auto at = std::max(address, it->first); at < std::min(end, it->first + pageBytes); at += 4096) {
                    if ((it->second.collected & (1u << ((at - it->first) / 4096))) == 0) return false;
                }
            }
        } else if (clear && !collectable(address, end)) return false;
        auto position = views.lower_bound(address & ~(pageBytes - 1));
        std::uintptr_t cleanStart = 0;
        std::uintptr_t cleanEnd = 0;
        const auto flushClean = [&] {
            if (cleanEnd > cleanStart) rememberClean(cleanStart, cleanEnd);
            cleanStart = cleanEnd = 0;
        };
        for (auto cursor = address; cursor < end;) {
            const auto nextClean = cleanRanges.upper_bound(cursor);
            if (nextClean != cleanRanges.begin()) {
                const auto clean = std::prev(nextClean);
                if (cursor < clean->second) {
                    cursor = std::min(end, clean->second);
                    continue;
                }
            }
            const auto base = cursor & ~(pageBytes - 1);
            const auto found = viewAt(base, position);
            if (found != views.end() && found->second.resident) {
                flushClean();
                if (!watchResident) return false;
                const auto available = capacity - *count;
                if (available == 0) return true;
                const auto run = std::prev(residentRuns.upper_bound(cursor));
                const auto stop = std::min({end, run->second.end, (cursor & ~std::uintptr_t{4095}) + available * 4096, nextClean == cleanRanges.end() ? end : nextClean->first});
                collectResident(run->first, run->second, cursor, stop, pages, count, clear, collection);
                if (*count == capacity) return true;
                cursor = stop;
            } else if (found != views.end()) {
                auto& view = found->second;
                auto stop = std::min(std::min(end, base + pageBytes), nextClean == cleanRanges.end() ? end : nextClean->first);
                if (collection != Collection::All) {
                    const auto selected = [&](std::uintptr_t at) {
                        const bool collected = (view.collected & (1u << ((at - base) / 4096))) != 0;
                        return collection == Collection::Armed ? collected : !collected;
                    };
                    while (cursor < stop && !selected(cursor)) cursor = std::min(stop, cursor + 4096);
                    if (cursor == stop) continue;
                    for (auto at = cursor + 4096; at < stop; at += 4096) {
                        if (!selected(at)) {
                            stop = at;
                            break;
                        }
                    }
                }
                if (view.protection == PAGE_NOACCESS) {
                    flushClean();
                    return false;
                }
                const bool pinned = view.page->pins != 0;
                std::uint8_t settled = 0;
                auto settledEnd = cursor;
                bool full = false;
                for (std::size_t sub = 0; sub < subpages; ++sub) {
                    const auto from = std::max(cursor, base + sub * subpageBytes);
                    const auto to = std::min(stop, base + (sub + 1) * subpageBytes);
                    if (from >= to) continue;
                    if (collection == Collection::Fresh || pinned || view.seen[sub] != view.page->generation[sub]) {
                        if (*count == capacity) {
                            full = true;
                            break;
                        }
                        pages[(*count)++] = reinterpret_cast<void*>(from);
                    }
                    settled |= static_cast<std::uint8_t>(1u << sub);
                    settledEnd = to;
                }
                if (clear) {
                    for (std::size_t sub = 0; sub < subpages; ++sub) {
                        const auto bit = static_cast<std::uint8_t>(1u << sub);
                        if ((settled & bit) == 0 || (view.collected & bit) != 0) continue;
                        view.collected |= bit;
                        forget(freshRanges, base + sub * subpageBytes, base + (sub + 1) * subpageBytes);
                    }
                }
                if (clear && !pinned && settled != 0) {
                    std::uint8_t untracked = 0;
                    for (const auto alias : view.page->aliases) {
                        auto& other = views.at(alias);
                        if (other.resident) continue;
                        const auto arming = static_cast<std::uint8_t>(settled & ~other.armed);
                        if (!writable(other.protection) || arming == 0) continue;
                        if (other.hostWrites != 0) {
                            untracked |= arming;
                            continue;
                        }
                        const DWORD protection = other.protection == PAGE_EXECUTE_READWRITE ? PAGE_EXECUTE_READ : PAGE_READONLY;
                        protectSubpages(alias, arming, protection, "arm shared memory write tracking", other.viewBytes);
                        other.armed |= arming;
                    }
                    const auto tracked = static_cast<std::uint8_t>(settled & ~untracked);
                    for (std::size_t sub = 0; sub < subpages; ++sub) {
                        if ((tracked & (1u << sub)) == 0) continue;
                        const auto from = std::max(cursor, base + sub * subpageBytes);
                        const auto to = std::min(settledEnd, base + (sub + 1) * subpageBytes);
                        if (from == base + sub * subpageBytes && to == base + (sub + 1) * subpageBytes) view.seen[sub] = view.page->generation[sub];
                        if (from != cleanEnd) {
                            flushClean();
                            cleanStart = from;
                        }
                        cleanEnd = to;
                    }
                }
                if (full) {
                    flushClean();
                    return true;
                }
                cursor = stop;
            } else {
                flushClean();
                if (collection == Collection::Fresh) {
                    const auto next = views.lower_bound(cursor);
                    cursor = next == views.end() ? end : std::min(end, next->first);
                    continue;
                }
                const auto allocationEnd = privateAllocationEnd(cursor);
                if (allocationEnd == 0) return false;
                const auto stop = std::min(end, allocationEnd);
                if (capacity == *count) return true;
                const auto touched = (((stop + 4095) & ~static_cast<std::uintptr_t>(4095)) - (cursor & ~static_cast<std::uintptr_t>(4095))) / 4096;
                ULONG_PTR available = std::min<ULONG_PTR>(capacity - *count, touched);
                DWORD granularity = 0;
#ifdef APS5_COUNT_MAPPING_QUERIES
                ++collectSystemCalls;
                collectPieces += (stop + pageBytes - 1) / pageBytes - cursor / pageBytes;
#endif
                const bool committed = !verifyDirect() || committedPrivate(cursor, stop);
                if (GetWriteWatch(clear ? WRITE_WATCH_FLAG_RESET : 0, reinterpret_cast<void*>(cursor), stop - cursor, pages + *count, &available, &granularity) != 0) {
                    const auto memory = query(cursor);
                    if (memory.State != MEM_COMMIT || memory.Type != MEM_PRIVATE) return false;
                    fail("collect private guest writes");
                }
                if (!committed) staleProof(cursor, stop);
                *count += available;
                if (*count == capacity) return true;
                cursor = stop;
            }
        }
        flushClean();
        return true;
    }

private:
    bool collectable(std::uintptr_t address, std::uintptr_t end) {
        const auto proven = collectableRanges.upper_bound(address);
        if (proven != collectableRanges.begin() && std::prev(proven)->second >= end) return true;
        auto position = views.lower_bound(address & ~(pageBytes - 1));
        for (auto cursor = address; cursor < end;) {
            const auto base = cursor & ~(pageBytes - 1);
            if (const auto found = viewAt(base, position); found != views.end()) {
                if (found->second.protection == PAGE_NOACCESS || (found->second.resident ? !watchResident : found->second.page->comparedWhenShared)) return false;
                cursor = std::min(end, base + pageBytes);
                continue;
            }
            const auto allocationEnd = privateAllocationEnd(cursor);
            if (allocationEnd == 0) return false;
            cursor = std::min(end, allocationEnd);
        }
        remember(collectableRanges, address, end);
        return true;
    }

    static bool verifyDirect() {
        static const bool verify = std::getenv("APS5_VERIFY_DIRECT_COLLECT") != nullptr;
        return verify;
    }

    static bool committedPrivate(std::uintptr_t address, std::uintptr_t end) {
        for (auto cursor = address; cursor < end;) {
            const auto memory = query(cursor);
            if (memory.State != MEM_COMMIT || memory.Type != MEM_PRIVATE) return false;
            cursor = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
        }
        return true;
    }

    [[noreturn]] static void staleProof(std::uintptr_t address, std::uintptr_t end) {
        char text[192];
        std::snprintf(text, sizeof(text), "a private collect of guest writes succeeded over 0x%llx+0x%llx, which is no longer committed private memory (APS5_VERIFY_DIRECT_COLLECT)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(end - address));
        throw std::runtime_error(text);
    }

    std::uintptr_t privateAllocationEnd(std::uintptr_t address) const {
        const auto next = privateAllocations.upper_bound(address);
        if (next == privateAllocations.begin()) return 0;
        const auto allocation = std::prev(next);
        return address < allocation->second ? allocation->second : 0;
    }

    static constexpr std::size_t pageBytes = 0x4000;
    static constexpr std::size_t subpageBytes = 0x1000;
    static_assert(subpageBytes == 4096, "Collect reports one page per subpage");
    static constexpr std::size_t subpages = pageBytes / subpageBytes;
    static constexpr std::uint8_t allSubpages = (1u << subpages) - 1;
    static constexpr std::size_t chunkBytes = 0x200000;
    struct SharedPage {
        std::array<std::uint64_t, subpages> generation = [] {
            std::array<std::uint64_t, subpages> initial{};
            initial.fill(1);
            return initial;
        }();
        std::vector<std::uintptr_t> aliases;
        std::uint32_t pins = 0;
        bool comparedWhenShared = false;
    };
    struct Section {
        HANDLE handle;
        explicit Section(HANDLE handle) : handle(handle) {}
        Section(const Section&) = delete;
        Section& operator=(const Section&) = delete;
        ~Section() { CloseHandle(handle); }
    };
    struct Saved {
        unsigned char* bytes = nullptr;
        std::size_t length;
        std::array<std::uint64_t, chunkBytes / pageBytes / 64> committed{};
        explicit Saved(std::size_t length) : length(length) {
            bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, length, MEM_RESERVE, PAGE_READWRITE));
            if (bytes == nullptr) fail("reserve unmapped direct memory");
        }
        Saved(const Saved&) = delete;
        Saved& operator=(const Saved&) = delete;
        ~Saved() { VirtualFree(bytes, 0, MEM_RELEASE); }

        void Commit(std::size_t offset) {
            const auto page = offset / pageBytes;
            const auto bit = std::uint64_t{1} << (page & 63);
            if ((committed[page >> 6] & bit) != 0) return;
            if (!VirtualAlloc(bytes + offset, pageBytes, MEM_COMMIT, PAGE_READWRITE)) fail("keep unmapped direct memory");
            committed[page >> 6] |= bit;
        }
    };
    struct Physical {
        std::weak_ptr<SharedPage> page;
        bool stored = false;
        bool comparedWhenShared = false;
        std::shared_ptr<Saved> saved;
        std::size_t savedOffset = 0;
    };
    struct View {
        std::shared_ptr<SharedPage> page;
        DWORD protection;
        std::array<std::uint64_t, subpages> seen;
        std::uint8_t armed;
        bool comparedWhenShared = false;
        std::uint32_t pins = 0;
        std::shared_ptr<Section> section;
        std::uint64_t offset;
        std::uint32_t hostWrites;
        std::uint8_t collected = 0;
        bool resident = false;
        std::shared_ptr<Physical> slot;
        std::size_t viewBytes = pageBytes;
        std::uint32_t hostImports = 0;
        bool aliasDemoted = false;
        std::uint8_t unreported = 0;
    };
    struct Run {
        std::uintptr_t end;
        std::array<std::uint64_t, 8> collected;
        std::uint32_t fresh;
        std::uintptr_t key;
        std::uint64_t offset;
        bool watched = false;
        bool reset = false;
        bool imported = false;
    };
    struct Window {
        void* view = nullptr;
        unsigned char* bytes = nullptr;
    };
    using Ranges = std::map<std::uintptr_t, std::uintptr_t>;

    static bool runCollected(const Run& run, std::size_t bit) {
        return (run.collected[bit >> 6] >> (bit & 63) & 1) != 0;
    }

    void collectResident(std::uintptr_t start, Run& run, std::uintptr_t from, std::uintptr_t stop, void** pages, std::size_t* count, bool clear, Collection collection) {
        if (clear) run.reset = true;
        const auto first = from & ~std::uintptr_t{4095};
        if (run.fresh == 0 && collection != Collection::Fresh) {
            ULONG_PTR reported = (stop - first + 4095) / 4096;
            DWORD granularity = 0;
            if (GetWriteWatch(clear ? WRITE_WATCH_FLAG_RESET : 0, reinterpret_cast<void*>(first), stop - first, pages + *count, &reported, &granularity) != 0) fail("collect resident guest writes");
            for (ULONG_PTR index = 0; index < reported; ++index) pages[*count + index] = reinterpret_cast<void*>(std::max(from, reinterpret_cast<std::uintptr_t>(pages[*count + index])));
            *count += reported;
            return;
        }
        std::vector<std::uintptr_t> written;
        if (collection != Collection::Fresh) {
            std::vector<void*> found((stop - first + 4095) / 4096);
            ULONG_PTR reported = found.size();
            DWORD granularity = 0;
            if (GetWriteWatch(clear ? WRITE_WATCH_FLAG_RESET : 0, reinterpret_cast<void*>(first), stop - first, found.data(), &reported, &granularity) != 0) fail("collect resident guest writes");
            for (ULONG_PTR index = 0; index < reported; ++index) written.push_back(reinterpret_cast<std::uintptr_t>(found[index]));
        }
        auto next = written.begin();
        std::uintptr_t resetFrom = 0;
        const auto flushReset = [&](std::uintptr_t to) {
            if (resetFrom != 0 && ResetWriteWatch(reinterpret_cast<void*>(resetFrom), to - resetFrom) != 0) fail("reset resident guest writes");
            resetFrom = 0;
        };
        for (auto at = first; at < stop; at += 4096) {
            const auto bit = (at - start) >> 12;
            const bool fresh = !runCollected(run, bit);
            bool wrote = false;
            while (next != written.end() && *next < at) ++next;
            if (next != written.end() && *next == at) wrote = true;
            const bool report = collection == Collection::Fresh ? fresh : collection == Collection::Armed ? !fresh && wrote : fresh || wrote;
            if (report) pages[(*count)++] = reinterpret_cast<void*>(std::max(at, from));
            const bool settle = clear && fresh && collection != Collection::Armed;
            if (settle) {
                run.collected[bit >> 6] |= 1ull << (bit & 63);
                --run.fresh;
                forget(freshRanges, at, at + 4096);
                if (collection == Collection::Fresh && resetFrom == 0) resetFrom = at;
            } else if (collection == Collection::Fresh) {
                flushReset(at);
            }
        }
        if (collection == Collection::Fresh) flushReset(stop);
    }

    Window open(HANDLE section, std::uint64_t offset, std::size_t bytes) {
        const auto lead = offset % granularity;
        Window window;
        window.view = map(section, GetCurrentProcess(), nullptr, offset - lead, lead + bytes, 0, PAGE_READWRITE, nullptr, 0);
        if (window.view == nullptr) fail("open a window on direct memory");
        window.bytes = static_cast<unsigned char*>(window.view) + lead;
        return window;
    }

    void close(Window& window) {
        if (window.view != nullptr && !unmap(GetCurrentProcess(), window.view, 0)) fail("close a window on direct memory");
        window = {};
    }

    void commitResident(std::uintptr_t base, std::size_t bytes) {
        const DWORD flags = MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER | (watchResident ? MEM_WRITE_WATCH : 0);
        if (!allocate(GetCurrentProcess(), reinterpret_cast<void*>(base), bytes, flags, PAGE_READWRITE, nullptr, 0)) fail("replace guest placeholder with resident direct memory");
    }

    static std::size_t hostPages(std::size_t bytes) {
        return (bytes + subpageBytes - 1) & ~(subpageBytes - 1);
    }

    void commitSection(HANDLE section, std::uint64_t offset, std::size_t bytes) {
        auto window = open(section, offset, bytes);
        const auto memory = query(reinterpret_cast<std::uintptr_t>(window.bytes));
        if ((memory.State != MEM_COMMIT || memory.RegionSize < bytes) && !VirtualAlloc(window.bytes, bytes, MEM_COMMIT, PAGE_READWRITE)) {
            const auto error = GetLastError();
            close(window);
            throw std::system_error(static_cast<int>(error), std::system_category(), "commit direct memory pages");
        }
        close(window);
    }

    void mapShared(std::uintptr_t base, HANDLE section, std::uint64_t offset, DWORD protection, DWORD viewProtection = PAGE_EXECUTE_READWRITE, std::size_t sectionBytes = pageBytes, bool committed = false) {
        void* page = reinterpret_cast<void*>(base);
        const auto viewBytes = hostPages(sectionBytes);
        if (viewBytes != pageBytes) split(base, viewBytes);
        if (!map(section, GetCurrentProcess(), page, offset, sectionBytes, MEM_REPLACE_PLACEHOLDER, viewProtection, nullptr, 0)) fail("map shared guest page");
        if (!committed && query(base).State == MEM_RESERVE && !VirtualAlloc(page, viewBytes, MEM_COMMIT, PAGE_EXECUTE_READWRITE)) fail("commit shared guest page");
        DWORD previous;
        if (!VirtualProtect(page, viewBytes, protection, &previous)) fail("protect shared guest page");
        if (viewBytes == pageBytes) return;
        void* zeros = reinterpret_cast<void*>(base + viewBytes);
        if (!allocate(GetCurrentProcess(), zeros, pageBytes - viewBytes, MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0)) fail("replace guest placeholder past the end of a file with zero pages");
        if (!VirtualProtect(zeros, pageBytes - viewBytes, protection, &previous)) fail("protect guest pages past the end of a file");
    }

    void mapRun(std::uintptr_t base, std::size_t bytes, HANDLE section, std::uint64_t offset) {
        if (!map(section, GetCurrentProcess(), reinterpret_cast<void*>(base), offset, bytes, MEM_REPLACE_PLACEHOLDER, PAGE_EXECUTE_READWRITE, nullptr, 0)) fail("map a run of shared guest pages");
    }

    void breakView(std::uintptr_t at) {
        const auto memory = query(at);
        if (memory.State == MEM_FREE || memory.Type != MEM_MAPPED) return;
        const auto start = reinterpret_cast<std::uintptr_t>(memory.AllocationBase);
        if (start == at) return;
        const auto first = views.find(start);
        if (first == views.end()) return;
        struct Part {
            std::uintptr_t base;
            std::uintptr_t end;
            DWORD protection;
            bool committed;
        };
        std::vector<Part> parts;
        auto stop = start;
        for (;;) {
            const auto part = query(stop);
            if (part.AllocationBase != memory.AllocationBase) break;
            const auto partEnd = reinterpret_cast<std::uintptr_t>(part.BaseAddress) + part.RegionSize;
            parts.push_back({stop, partEnd, part.Protect, part.State == MEM_COMMIT});
            stop = partEnd;
        }
        const auto section = first->second.section;
        const auto offset = first->second.offset;
        if (!unmap(GetCurrentProcess(), reinterpret_cast<void*>(start), MEM_PRESERVE_PLACEHOLDER)) fail("unmap a run of shared guest pages to split it");
        split(start, at - start);
        mapRun(start, at - start, section->handle, offset);
        mapRun(at, stop - at, section->handle, offset + (at - start));
        for (const auto& part : parts) {
            if (!part.committed) continue;
            for (const auto [from, to] : {std::pair{part.base, std::min(part.end, at)}, std::pair{std::max(part.base, at), part.end}}) {
                DWORD previous;
                if (from < to && !VirtualProtect(reinterpret_cast<void*>(from), to - from, part.protection, &previous)) fail("protect a split run of shared guest pages");
            }
        }
    }

    static bool blank(std::uintptr_t page) {
        const auto* words = reinterpret_cast<const std::uint64_t*>(page);
        for (std::size_t i = 0; i < pageBytes / sizeof(std::uint64_t); ++i) {
            if (words[i] != 0) return false;
        }
        return true;
    }

    std::map<std::uintptr_t, Run>::iterator residentRun(std::uintptr_t key, std::uint64_t offset) {
        const auto next = residentIndex.upper_bound(std::make_pair(key, offset));
        if (next == residentIndex.begin()) return residentRuns.end();
        const auto& [first, run] = *std::prev(next);
        return first.first == key && offset - first.second < run->second.end - run->first ? run : residentRuns.end();
    }

    std::map<std::pair<std::uintptr_t, std::uint64_t>, std::map<std::uintptr_t, Run>::iterator>::iterator indexEntry(std::map<std::uintptr_t, Run>::iterator run) {
        const auto found = residentIndex.find(std::make_pair(run->second.key, run->second.offset));
        return found != residentIndex.end() && found->second == run ? found : residentIndex.end();
    }

    void demote(std::uintptr_t address, bool alias = false) {
        const auto run = std::prev(residentRuns.upper_bound(address));
        const auto start = run->first;
        const auto stop = run->second.end;
        const auto entry = indexEntry(run);
        const bool indexed = entry != residentIndex.end();
        std::vector<std::pair<std::shared_ptr<SharedPage>, std::shared_ptr<Physical>>> created;
        created.reserve((stop - start) / pageBytes);
        std::map<std::pair<std::uintptr_t, std::uint64_t>, std::shared_ptr<Physical>> staged;
        for (auto page = start; page < stop; page += pageBytes) {
            const auto& view = views.at(page);
            auto shared = std::make_shared<SharedPage>();
            shared->aliases.push_back(page);
            shared->pins = view.pins;
            shared->comparedWhenShared = view.comparedWhenShared;
            auto slot = std::make_shared<Physical>();
            slot->page = shared;
            slot->comparedWhenShared = view.comparedWhenShared;
            if (indexed) staged.emplace(std::make_pair(run->second.key, view.offset), slot);
            created.emplace_back(std::move(shared), std::move(slot));
        }
        DWORD previous;
        if (!VirtualProtect(reinterpret_cast<void*>(start), stop - start, PAGE_READONLY, &previous)) fail("fence resident guest pages");
        const auto section = views.at(start).section;
        auto window = open(section->handle, views.at(start).offset, stop - start);
        if (query(reinterpret_cast<std::uintptr_t>(window.bytes)).State == MEM_RESERVE && !VirtualAlloc(window.bytes, stop - start, MEM_COMMIT, PAGE_READWRITE)) fail("commit direct memory pages");
        std::memcpy(window.bytes, reinterpret_cast<const void*>(start), stop - start);
        close(window);
        if (!VirtualFree(reinterpret_cast<void*>(start), stop - start, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) fail("release resident guest pages");
        physical.merge(staged);
        for (auto& [key, slot] : staged) physical.find(key)->second = std::move(slot);
        for (auto page = start; page < stop; page += pageBytes) {
            auto& view = views.at(page);
            split(page, pageBytes);
            mapShared(page, view.section->handle, view.offset, view.protection);
            auto& [shared, slot] = created[(page - start) / pageBytes];
            view.page = std::move(shared);
            view.slot = std::move(slot);
            view.slot->stored = true;
            view.resident = false;
            view.aliasDemoted = alias;
            view.unreported = view.page->comparedWhenShared ? allSubpages : std::uint8_t{0};
            view.seen.fill(0);
            view.armed = 0;
            view.collected = 0;
            for (std::size_t sub = 0; sub < subpages; ++sub) {
                if (runCollected(run->second, ((page - start) >> 12) + sub)) view.collected |= static_cast<std::uint8_t>(1u << sub);
            }
            invalidate(*view.page);
        }
        if (indexed) residentIndex.erase(entry);
        residentRuns.erase(run);
    }

    void breakRun(std::uintptr_t at) {
        const auto next = residentRuns.upper_bound(at);
        if (next == residentRuns.begin()) return;
        const auto run = std::prev(next);
        if (at > run->first && at < run->second.end) demote(at);
    }

    void evict(std::map<std::uintptr_t, Run>::iterator run) {
        const auto start = run->first;
        const auto stop = run->second.end;
        DWORD previous;
        if (!VirtualProtect(reinterpret_cast<void*>(start), stop - start, PAGE_READONLY, &previous)) fail("open resident guest pages");
        const auto entry = indexEntry(run);
        const bool indexed = entry != residentIndex.end();
        std::vector<void*> dirty;
        std::vector<bool> touched;
        bool filtered = false;
        if (indexed && run->second.watched && !run->second.reset && !run->second.imported) {
            dirty.resize((stop - start) / subpageBytes);
            ULONG_PTR reported = dirty.size();
            DWORD granularity = 0;
            if (GetWriteWatch(0, reinterpret_cast<void*>(start), stop - start, dirty.data(), &reported, &granularity) == 0) {
                touched = TouchedPages(start, (stop - start) / pageBytes, dirty.data(), reported);
                filtered = true;
            }
        }
        std::shared_ptr<Saved> saved;
        for (auto page = start; page < stop; page += pageBytes) {
            const auto found = views.find(page);
            const auto& view = found->second;
            const bool written = indexed && (!filtered || touched[(page - start) / pageBytes]) && !blank(page);
            if (written || (indexed && view.comparedWhenShared)) {
                auto& slot = physical[std::make_pair(run->second.key, view.offset)];
                if (!slot) slot = std::make_shared<Physical>();
                slot->comparedWhenShared = view.comparedWhenShared;
                if (written) {
                    if (!saved) saved = savedMemory(stop - start);
                    saved->Commit(page - start);
                    std::memcpy(saved->bytes + (page - start), reinterpret_cast<const void*>(page), pageBytes);
                    slot->saved = saved;
                    slot->savedOffset = page - start;
                }
            }
            views.erase(found);
        }
        if (indexed) residentIndex.erase(entry);
        forget(freshRanges, start, stop);
        residentRuns.erase(run);
    }

    std::shared_ptr<Saved> savedMemory(std::size_t bytes) {
        for (const auto& saved : savedPool) {
            if (saved.use_count() == 1 && saved->length == bytes) return saved;
        }
        constexpr std::size_t limit = 32u * 1024u * 1024u;
        const auto reserved = (bytes + granularity - 1) / granularity * granularity;
        for (auto it = savedPool.begin(); reserved <= limit && savedPoolBytes > limit - reserved && it != savedPool.end();) {
            if (it->use_count() != 1) {
                ++it;
                continue;
            }
            savedPoolBytes -= ((*it)->length + granularity - 1) / granularity * granularity;
            it = savedPool.erase(it);
        }
        auto saved = std::make_shared<Saved>(bytes);
        if (reserved <= limit && savedPoolBytes <= limit - reserved) {
            savedPool.push_back(saved);
            savedPoolBytes += reserved;
        }
        return saved;
    }

    static void forget(Ranges& ranges, std::uintptr_t start, std::uintptr_t end) {
        auto it = ranges.lower_bound(start);
        if (it != ranges.begin() && std::prev(it)->second > start) --it;
        while (it != ranges.end() && it->first < end) {
            const auto first = it->first;
            const auto last = it->second;
            it = ranges.erase(it);
            if (first < start) ranges.emplace(first, start);
            if (last > end) it = ranges.emplace(end, last).first;
        }
    }

    static void remember(Ranges& ranges, std::uintptr_t start, std::uintptr_t end) {
        auto it = ranges.lower_bound(start);
        if (it != ranges.begin() && std::prev(it)->second >= start) --it;
        while (it != ranges.end() && it->first <= end) {
            start = std::min(start, it->first);
            end = std::max(end, it->second);
            it = ranges.erase(it);
        }
        ranges.emplace(start, end);
    }

    void forgetClean(std::uintptr_t start, std::uintptr_t end) {
        forget(cleanRanges, start, end);
    }

    void rememberClean(std::uintptr_t start, std::uintptr_t end) {
        remember(cleanRanges, start, end);
    }

    void invalidate(SharedPage& page, std::uint8_t mask = allSubpages) {
        for (std::size_t sub = 0; sub < subpages; ++sub) {
            if ((mask & (1u << sub)) == 0) continue;
            ++page.generation[sub];
            for (const auto alias : page.aliases) forgetClean(alias + sub * subpageBytes, alias + (sub + 1) * subpageBytes);
        }
    }

    void invalidate(std::uintptr_t base, const View& view, std::uint8_t mask = allSubpages) {
        if (view.page) {
            invalidate(*view.page, mask);
            return;
        }
        for (std::size_t sub = 0; sub < subpages; ++sub) {
            if ((mask & (1u << sub)) != 0) forgetClean(base + sub * subpageBytes, base + (sub + 1) * subpageBytes);
        }
    }

    static std::uint8_t subpagesOf(std::uintptr_t base, std::uintptr_t address, std::uintptr_t end) {
        const auto from = std::max(address, base);
        const auto to = std::min(end, base + pageBytes);
        if (to <= from) return 0;
        const auto first = (from - base) / subpageBytes;
        const auto last = (to - 1 - base) / subpageBytes;
        return static_cast<std::uint8_t>(((1u << (last + 1)) - 1) & ~((1u << first) - 1));
    }

    static void protectSubpages(std::uintptr_t base, std::uint8_t mask, DWORD protection, const char* operation, std::size_t viewBytes = pageBytes) {
        for (std::size_t sub = 0; sub < subpages;) {
            if ((mask & (1u << sub)) == 0) {
                ++sub;
                continue;
            }
            auto stop = sub + 1;
            while (stop < subpages && (mask & (1u << stop)) != 0 && stop * subpageBytes != viewBytes) ++stop;
            DWORD previous;
            if (!VirtualProtect(reinterpret_cast<void*>(base + sub * subpageBytes), (stop - sub) * subpageBytes, protection, &previous)) fail(operation);
            sub = stop;
        }
    }

    bool sameSection(HANDLE first, HANDLE second) const {
        return compare != nullptr && compare(first, second);
    }

    static bool writable(DWORD protection) {
        return protection == PAGE_READWRITE || protection == PAGE_EXECUTE_READWRITE;
    }
    using AllocateFunction = PVOID (WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
    using MapFunction = PVOID (WINAPI*)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
    using UnmapFunction = BOOL (WINAPI*)(HANDLE, PVOID, ULONG);
    using CompareFunction = BOOL (WINAPI*)(HANDLE, HANDLE);

    WindowsMappings() {
        const auto module = GetModuleHandleW(L"KernelBase.dll");
        if (!module) fail("load Windows memory API");
        allocate = reinterpret_cast<AllocateFunction>(GetProcAddress(module, "VirtualAlloc2"));
        map = reinterpret_cast<MapFunction>(GetProcAddress(module, "MapViewOfFile3"));
        unmap = reinterpret_cast<UnmapFunction>(GetProcAddress(module, "UnmapViewOfFile2"));
        if (!allocate || !map || !unmap) throw std::runtime_error("Windows placeholder memory APIs are required");
        compare = reinterpret_cast<CompareFunction>(GetProcAddress(module, "CompareObjectHandles"));
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        granularity = system.dwAllocationGranularity;
    }

    [[noreturn]] static void fail(const char* operation) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
    }

    static MEMORY_BASIC_INFORMATION query(std::uintptr_t address) {
        MEMORY_BASIC_INFORMATION memory{};
#ifdef APS5_COUNT_MAPPING_QUERIES
        queries.fetch_add(1, std::memory_order_relaxed);
#endif
        if (VirtualQuery(reinterpret_cast<void*>(address), &memory, sizeof(memory)) != sizeof(memory)) fail("query guest memory");
        return memory;
    }

    static std::uintptr_t split(std::uintptr_t address, std::size_t bytes) {
        auto memory = query(address);
        memory = query(reinterpret_cast<std::uintptr_t>(memory.AllocationBase));
        if (memory.State != MEM_RESERVE) throw std::runtime_error("guest mapping requires a placeholder");
        const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
        if (address != base) {
            if (!VirtualFree(reinterpret_cast<void*>(base), address - base, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) fail("split guest placeholder prefix");
            memory = query(address);
        }
        if (memory.RegionSize < bytes) throw std::runtime_error("guest placeholder is too small");
        if (memory.RegionSize != bytes && !VirtualFree(reinterpret_cast<void*>(address), bytes, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) fail("split guest placeholder suffix");
        return address + memory.RegionSize;
    }

    void reset(std::uintptr_t address, std::size_t bytes) {
        collectableRanges.clear();
        const auto end = address + bytes;
        privateAllocations.erase(privateAllocations.lower_bound(address), privateAllocations.lower_bound(end));
        forgetClean(address, end);
        breakRun(address);
        breakRun(end);
        breakView(address);
        breakView(end);
        if (const auto run = residentRuns.find(address); run != residentRuns.end() && run->second.end == end) {
            const auto memory = query(address);
            if (reinterpret_cast<std::uintptr_t>(memory.AllocationBase) != address) throw std::runtime_error("cannot release part of a host allocation");
            if (memory.Type != MEM_PRIVATE) throw std::runtime_error("resident guest mapping is not private memory");
            auto allocationEnd = address;
            do {
                const auto part = query(allocationEnd);
                if (part.AllocationBase != memory.AllocationBase) break;
                allocationEnd = reinterpret_cast<std::uintptr_t>(part.BaseAddress) + part.RegionSize;
            } while (allocationEnd < end);
            if (allocationEnd != end || query(end).AllocationBase == memory.AllocationBase) throw std::runtime_error("guest release truncates a host allocation");
            evict(run);
            if (!VirtualFree(reinterpret_cast<void*>(address), bytes, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) fail("release private guest memory");
            return;
        }
        for (auto cursor = address; cursor < end;) {
            const auto memory = query(cursor);
            if (memory.State == MEM_RESERVE) {
                cursor = std::min(end, reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize);
                continue;
            }
            if (reinterpret_cast<std::uintptr_t>(memory.AllocationBase) != cursor) throw std::runtime_error("cannot release part of a host allocation");
            auto allocationEnd = cursor;
            do {
                const auto part = query(allocationEnd);
                if (part.AllocationBase != memory.AllocationBase) break;
                allocationEnd = reinterpret_cast<std::uintptr_t>(part.BaseAddress) + part.RegionSize;
            } while (allocationEnd < end);
            if (allocationEnd > end || query(allocationEnd).AllocationBase == memory.AllocationBase) throw std::runtime_error("guest release truncates a host allocation");
            if (memory.Type == MEM_MAPPED) {
                if (!unmap(GetCurrentProcess(), reinterpret_cast<void*>(cursor), MEM_PRESERVE_PLACEHOLDER)) fail("unmap shared guest page");
                for (auto found = views.lower_bound(cursor); found != views.end() && found->first < allocationEnd;) {
                    if (found->second.page != nullptr) std::erase(found->second.page->aliases, found->first);
                    found = views.erase(found);
                }
                forget(freshRanges, cursor, allocationEnd);
            } else if (memory.Type == MEM_PRIVATE) {
                if (const auto run = residentRuns.find(cursor); run != residentRuns.end()) evict(run);
                if (!VirtualFree(reinterpret_cast<void*>(cursor), allocationEnd - cursor, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) fail("release private guest memory");
            } else {
                throw std::runtime_error("unsupported guest mapping type");
            }
            cursor = allocationEnd;
        }
        const auto last = query(reinterpret_cast<std::uintptr_t>(query(end - 1).AllocationBase));
        const auto lastBase = reinterpret_cast<std::uintptr_t>(last.BaseAddress);
        if (lastBase + last.RegionSize > end) split(lastBase, end - lastBase);
        const auto first = query(address);
        split(address, std::min(bytes, reinterpret_cast<std::uintptr_t>(first.BaseAddress) + first.RegionSize - address));
        if (query(address).RegionSize != bytes && !VirtualFree(reinterpret_cast<void*>(address), bytes, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS)) fail("coalesce guest placeholders");
    }

    void* mapAliasRuns(std::uintptr_t address, std::size_t bytes) {
        auto* reserved = allocate(GetCurrentProcess(), nullptr, bytes, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
        if (reserved == nullptr) fail("reserve a read-write alias of shared guest memory");
        const auto base = reinterpret_cast<std::uintptr_t>(reserved);
        try {
            for (std::size_t done = 0; done < bytes;) {
                const auto& first = views.at(address + done);
                auto run = pageBytes;
                while (done + run < bytes) {
                    const auto& next = views.at(address + done + run);
                    if (next.offset != first.offset + run || (next.section != first.section && !sameSection(next.section->handle, first.section->handle))) break;
                    run += pageBytes;
                }
                split(base + done, run);
                if (!map(first.section->handle, GetCurrentProcess(), reinterpret_cast<void*>(base + done), first.offset, run, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0)) fail("map a run of a read-write alias of shared guest memory");
                done += run;
            }
        } catch (...) {
            releaseAlias(base, bytes);
            throw;
        }
        splitAliases.emplace(base, bytes);
        return reserved;
    }

    DWORD releaseAlias(std::uintptr_t base, std::size_t bytes) {
        DWORD error = ERROR_SUCCESS;
        for (auto cursor = base; cursor < base + bytes;) {
            const auto memory = query(cursor);
            const auto released = memory.State == MEM_RESERVE ? VirtualFree(memory.BaseAddress, 0, MEM_RELEASE) : unmap(GetCurrentProcess(), memory.BaseAddress, 0);
            if (!released && error == ERROR_SUCCESS) error = GetLastError();
            cursor = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
        }
        return error;
    }

    Ranges cleanRanges;
    Ranges collectableRanges;
    Ranges privateAllocations;
#ifdef APS5_COUNT_MAPPING_QUERIES
    std::uint64_t collectSystemCalls = 0;
    std::uint64_t collectPieces = 0;
#endif
    Ranges freshRanges;
    std::map<std::uintptr_t, View> views;

    using Views = std::map<std::uintptr_t, View>;

    Views::iterator viewAt(std::uintptr_t base, Views::iterator& position) {
        if (position != views.end() && position->first < base) {
            ++position;
            if (position != views.end() && position->first < base) position = views.lower_bound(base);
        }
        return position != views.end() && position->first == base ? position : views.end();
    }
    std::map<std::pair<std::uintptr_t, std::uint64_t>, std::shared_ptr<Physical>> physical;
    std::map<std::uintptr_t, Run> residentRuns;
    std::map<std::pair<std::uintptr_t, std::uint64_t>, std::map<std::uintptr_t, Run>::iterator> residentIndex;
    std::vector<std::shared_ptr<Saved>> savedPool;
    std::size_t savedPoolBytes = 0;
    std::map<std::uintptr_t, std::size_t> splitAliases;
    std::size_t granularity = 0x10000;
    bool watchResident = false;
#ifdef APS5_COUNT_MAPPING_QUERIES
    static inline std::atomic<std::uint64_t> queries{0};
#endif
    std::mutex mutex;
    AllocateFunction allocate = nullptr;
    MapFunction map = nullptr;
    UnmapFunction unmap = nullptr;
    CompareFunction compare = nullptr;
};

}
#endif

#endif
