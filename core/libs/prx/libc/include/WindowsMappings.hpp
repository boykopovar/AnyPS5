#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSMAPPINGS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_WINDOWSMAPPINGS_HPP

#ifdef _WIN32
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstddef>
#include <cstdint>
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

    void* Reserve(void* address, std::size_t bytes) {
        return allocate(GetCurrentProcess(), address, bytes, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
    }

    std::vector<std::pair<std::uintptr_t, std::size_t>> Commit(void* address, std::size_t bytes, DWORD protection, std::size_t granule, bool watched) {
        std::vector<std::pair<std::uintptr_t, std::size_t>> created;
        std::lock_guard lock(mutex);
        collectableRanges.clear();
        const auto end = reinterpret_cast<std::uintptr_t>(address) + bytes;
        for (auto cursor = reinterpret_cast<std::uintptr_t>(address); cursor < end;) {
            const auto memory = query(cursor);
            const auto stop = std::min(end, reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize);
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
                const DWORD flags = MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER | (watched ? MEM_WRITE_WATCH : 0);
                if (!allocate(GetCurrentProcess(), reinterpret_cast<void*>(cursor), size, flags, protection, nullptr, 0)) fail("replace guest placeholder with private memory");
                if (!created.empty() && created.back().first + created.back().second == cursor) created.back().second += size;
                else created.emplace_back(cursor, size);
                cursor += size;
            } else {
                if (memory.State != MEM_COMMIT) throw std::runtime_error("guest memory is not committed");
                const auto mapped = views.find(cursor & ~(pageBytes - 1));
                if (mapped != views.end()) {
                    mapped->second.protection = protection;
                    mapped->second.armed = 0;
                    invalidate(*mapped->second.page);
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

    void MapZero(void* address, std::size_t bytes) {
        std::lock_guard lock(mutex);
        if (zeroSection == nullptr && (zeroSection = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READONLY, 0, static_cast<DWORD>(pageBytes), nullptr)) == nullptr) fail("create the read-only zero guest page");
        const auto start = reinterpret_cast<std::uintptr_t>(address);
        reset(start, bytes);
        for (auto cursor = start; cursor < start + bytes; cursor += pageBytes) {
            split(cursor, pageBytes);
            if (!map(zeroSection, GetCurrentProcess(), reinterpret_cast<void*>(cursor), 0, pageBytes, MEM_REPLACE_PLACEHOLDER, PAGE_READONLY, nullptr, 0)) fail("map the read-only zero guest page");
        }
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
        const auto slotOf = [&](std::size_t done) -> const std::shared_ptr<Physical>& {
            auto& slot = physical[std::make_pair(reinterpret_cast<std::uintptr_t>(section), offset + done)];
            if (!slot) slot = std::make_shared<Physical>();
            return slot;
        };
        const auto pageOf = [](Physical& slot) {
            auto shared = slot.page.lock();
            if (!shared) {
                shared = std::make_shared<SharedPage>();
                slot.page = shared;
            }
            return shared;
        };
        const auto single = [&](std::size_t done) {
            const auto& slot = *slotOf(done);
            const auto shared = slot.page.lock();
            return resident && !slot.stored && (!shared || shared->aliases.empty());
        };
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
                residentRuns.emplace(base, Run{base + runBytes, {}, static_cast<std::uint32_t>(runBytes >> 12)});
                for (std::size_t at = 0; at < runBytes; at += pageBytes) {
                    const auto slot = slotOf(done + at);
                    const auto shared = pageOf(*slot);
                    if (slot->saved) {
                        std::memcpy(reinterpret_cast<void*>(base + at), slot->saved->bytes + slot->savedOffset, pageBytes);
                        slot->saved.reset();
                    }
                    shared->aliases.push_back(base + at);
                    views.emplace(base + at, View{shared, protection, {}, 0, owned, offset + done + at, 0, 0, true, slot});
                    invalidate(*shared);
                }
                remember(freshRanges, base, base + runBytes);
                DWORD previous;
                if (protection != PAGE_READWRITE && !VirtualProtect(reinterpret_cast<void*>(base), runBytes, protection, &previous)) fail("protect resident guest pages");
                done += runBytes;
                continue;
            }
            const auto slot = slotOf(done);
            const auto shared = pageOf(*slot);
            const auto aliases = shared->aliases;
            for (const auto alias : aliases) {
                if (views.at(alias).resident) demote(alias);
            }
            const auto viewBytes = std::min(pageBytes, sectionBytes - done);
            split(base, pageBytes);
            mapShared(base, section, offset + done, protection, viewProtection, viewBytes);
            slot->stored = true;
            shared->aliases.push_back(base);
            views.emplace(base, View{shared, protection, {}, 0, owned, offset + done, 0, 0, false, slot, hostPages(viewBytes)});
            remember(freshRanges, base, base + pageBytes);
            invalidate(*shared);
            done += pageBytes;
        }
    }

    void SetProtection(std::uintptr_t address, std::size_t bytes, DWORD protection) {
        std::lock_guard lock(mutex);
        collectableRanges.clear();
        for (auto it = views.lower_bound(address); it != views.end() && it->first < address + bytes; ++it) {
            it->second.protection = protection;
            it->second.armed = 0;
            invalidate(*it->second.page);
        }
    }

    void Pin(std::uintptr_t address, std::size_t bytes) {
        std::lock_guard lock(mutex);
        for (auto it = views.lower_bound(address & ~(pageBytes - 1)); it != views.end() && it->first < address + bytes; ++it) {
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
            auto& page = *it->second.page;
            if (page.pins != 0) --page.pins;
            invalidate(page);
        }
    }

    bool HandleWrite(std::uintptr_t address) {
        std::lock_guard lock(mutex);
        const auto base = address & ~(pageBytes - 1);
        const auto found = views.find(base);
        if (found == views.end() || !writable(found->second.protection)) return false;
        auto& view = found->second;
        const auto written = subpagesOf(base, address, address + 1);
        if (view.resident && (view.armed & written) == 0) {
            const auto memory = query(address);
            return memory.State == MEM_COMMIT && writable(memory.Protect & 0xffu);
        }
        if (view.resident) {
            resumeResidentWrites(base);
            return true;
        }
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
        const bool blank = std::none_of(first, last, [](const auto& entry) { return entry.second->stored || !entry.second->page.expired(); });
        physical.erase(first, last);
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
            if (store) invalidate(*view.page, written);
            else if (view.resident) forgetClean(it->first, it->first + pageBytes);
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
            if (store) invalidate(*it->second.page, subpagesOf(it->first, address, end));
            else if (it->second.resident) forgetClean(it->first, it->first + pageBytes);
        }
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
            if (found != views.end() && found->second.resident) demote(address + done);
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
                const auto memory = query(cursor);
                if (memory.State != MEM_COMMIT || memory.Type != MEM_PRIVATE) return false;
                const auto stop = std::min(end, reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize);
                if (capacity == *count) return true;
                const auto touched = (((stop + 4095) & ~static_cast<std::uintptr_t>(4095)) - (cursor & ~static_cast<std::uintptr_t>(4095))) / 4096;
                ULONG_PTR available = std::min<ULONG_PTR>(capacity - *count, touched);
                DWORD granularity = 0;
                if (GetWriteWatch(clear ? WRITE_WATCH_FLAG_RESET : 0, reinterpret_cast<void*>(cursor), stop - cursor, pages + *count, &available, &granularity) != 0) fail("collect private guest writes");
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
                if (found->second.protection == PAGE_NOACCESS || (found->second.resident && !watchResident)) return false;
                cursor = std::min(end, base + pageBytes);
                continue;
            }
            const auto memory = query(cursor);
            if (memory.State != MEM_COMMIT || memory.Type != MEM_PRIVATE) return false;
            cursor = std::min(end, reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize);
        }
        remember(collectableRanges, address, end);
        return true;
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
        std::shared_ptr<Saved> saved;
        std::size_t savedOffset = 0;
    };
    struct View {
        std::shared_ptr<SharedPage> page;
        DWORD protection;
        std::array<std::uint64_t, subpages> seen;
        std::uint8_t armed;
        std::shared_ptr<Section> section;
        std::uint64_t offset;
        std::uint32_t hostWrites;
        std::uint8_t collected = 0;
        bool resident = false;
        std::shared_ptr<Physical> slot;
        std::size_t viewBytes = pageBytes;
        std::uint32_t hostImports = 0;
    };
    struct Run {
        std::uintptr_t end;
        std::array<std::uint64_t, 8> collected;
        std::uint32_t fresh;
        bool writeObserved = false;
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
        const auto first = from & ~std::uintptr_t{4095};
        const bool armed = clear && run.fresh == 0 && !run.writeObserved && collection != Collection::Fresh && run.end - start >= 0x40000 && armResident(first, stop);
        if (run.fresh == 0 && collection != Collection::Fresh) {
            ULONG_PTR reported = (stop - first + 4095) / 4096;
            DWORD granularity = 0;
            if (GetWriteWatch(clear ? WRITE_WATCH_FLAG_RESET : 0, reinterpret_cast<void*>(first), stop - first, pages + *count, &reported, &granularity) != 0) fail("collect resident guest writes");
            for (ULONG_PTR index = 0; index < reported; ++index) pages[*count + index] = reinterpret_cast<void*>(std::max(from, reinterpret_cast<std::uintptr_t>(pages[*count + index])));
            *count += reported;
            if (armed) rememberClean(from, stop);
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

    void resumeResidentWrites(std::uintptr_t address) {
        const auto run = std::prev(residentRuns.upper_bound(address));
        run->second.writeObserved = true;
        forgetClean(run->first, run->second.end);
        auto first = views.lower_bound(run->first);
        while (first != views.end() && first->first < run->second.end) {
            const auto begin = first->first;
            auto end = begin;
            const auto protection = first->second.protection;
            auto next = first;
            bool armed = false;
            do {
                armed = armed || next->second.armed != 0;
                end = next->first + pageBytes;
                ++next;
            } while (next != views.end() && next->first == end && end < run->second.end && next->second.protection == protection);
            if (armed) {
                DWORD previous;
                if (!VirtualProtect(reinterpret_cast<void*>(begin), end - begin, protection, &previous)) fail("resume resident guest writes");
            }
            for (; first != next; ++first) first->second.armed = 0;
        }
    }

    bool armResident(std::uintptr_t from, std::uintptr_t stop) {
        const auto first = views.lower_bound(from & ~(pageBytes - 1));
        if (first == views.end() || !writable(first->second.protection)) return false;
        const auto protection = first->second.protection;
        for (auto it = first; it != views.end() && it->first < stop; ++it) {
            if (!it->second.resident || it->second.protection != protection || it->second.hostWrites != 0 || it->second.hostImports != 0 || it->second.page->pins != 0) return false;
        }
        DWORD previous;
        const auto readOnly = protection == PAGE_EXECUTE_READWRITE ? PAGE_EXECUTE_READ : PAGE_READONLY;
        if (!VirtualProtect(reinterpret_cast<void*>(from), stop - from, readOnly, &previous)) fail("arm resident memory write tracking");
        for (auto it = first; it != views.end() && it->first < stop; ++it) it->second.armed |= subpagesOf(it->first, from, stop);
        return true;
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

    void mapShared(std::uintptr_t base, HANDLE section, std::uint64_t offset, DWORD protection, DWORD viewProtection = PAGE_EXECUTE_READWRITE, std::size_t sectionBytes = pageBytes) {
        void* page = reinterpret_cast<void*>(base);
        const auto viewBytes = hostPages(sectionBytes);
        if (viewBytes != pageBytes) split(base, viewBytes);
        if (!map(section, GetCurrentProcess(), page, offset, sectionBytes, MEM_REPLACE_PLACEHOLDER, viewProtection, nullptr, 0)) fail("map shared guest page");
        if (query(base).State == MEM_RESERVE && !VirtualAlloc(page, viewBytes, MEM_COMMIT, PAGE_EXECUTE_READWRITE)) fail("commit shared guest page");
        DWORD previous;
        if (!VirtualProtect(page, viewBytes, protection, &previous)) fail("protect shared guest page");
        if (viewBytes == pageBytes) return;
        void* zeros = reinterpret_cast<void*>(base + viewBytes);
        if (!allocate(GetCurrentProcess(), zeros, pageBytes - viewBytes, MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0)) fail("replace guest placeholder past the end of a file with zero pages");
        if (!VirtualProtect(zeros, pageBytes - viewBytes, protection, &previous)) fail("protect guest pages past the end of a file");
    }

    static bool blank(std::uintptr_t page) {
        const auto* words = reinterpret_cast<const std::uint64_t*>(page);
        for (std::size_t i = 0; i < pageBytes / sizeof(std::uint64_t); ++i) {
            if (words[i] != 0) return false;
        }
        return true;
    }

    void demote(std::uintptr_t address) {
        const auto run = std::prev(residentRuns.upper_bound(address));
        const auto start = run->first;
        const auto stop = run->second.end;
        DWORD previous;
        if (!VirtualProtect(reinterpret_cast<void*>(start), stop - start, PAGE_READONLY, &previous)) fail("fence resident guest pages");
        const auto section = views.at(start).section;
        auto window = open(section->handle, views.at(start).offset, stop - start);
        if (query(reinterpret_cast<std::uintptr_t>(window.bytes)).State == MEM_RESERVE && !VirtualAlloc(window.bytes, stop - start, MEM_COMMIT, PAGE_READWRITE)) fail("commit direct memory pages");
        std::memcpy(window.bytes, reinterpret_cast<const void*>(start), stop - start);
        close(window);
        if (!VirtualFree(reinterpret_cast<void*>(start), stop - start, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) fail("release resident guest pages");
        for (auto page = start; page < stop; page += pageBytes) {
            auto& view = views.at(page);
            split(page, pageBytes);
            mapShared(page, view.section->handle, view.offset, view.protection);
            view.slot->stored = true;
            view.resident = false;
            view.seen.fill(0);
            view.armed = 0;
            view.collected = 0;
            for (std::size_t sub = 0; sub < subpages; ++sub) {
                if (runCollected(run->second, ((page - start) >> 12) + sub)) view.collected |= static_cast<std::uint8_t>(1u << sub);
            }
            invalidate(*view.page);
        }
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
        std::shared_ptr<Saved> saved;
        for (auto page = start; page < stop; page += pageBytes) {
            const auto found = views.find(page);
            auto& view = found->second;
            if (!blank(page)) {
                if (!saved) saved = savedMemory(stop - start);
                saved->Commit(page - start);
                std::memcpy(saved->bytes + (page - start), reinterpret_cast<const void*>(page), pageBytes);
                view.slot->saved = saved;
                view.slot->savedOffset = page - start;
            }
            std::erase(view.page->aliases, page);
            views.erase(found);
        }
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
        if (VirtualQuery(reinterpret_cast<void*>(address), &memory, sizeof(memory)) != sizeof(memory)) fail("query guest memory");
        return memory;
    }

    static void split(std::uintptr_t address, std::size_t bytes) {
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
    }

    void reset(std::uintptr_t address, std::size_t bytes) {
        collectableRanges.clear();
        const auto end = address + bytes;
        forgetClean(address, end);
        breakRun(address);
        breakRun(end);
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
                const auto found = views.find(cursor);
                if (found != views.end()) {
                    std::erase(found->second.page->aliases, cursor);
                    views.erase(found);
                    forget(freshRanges, cursor, cursor + pageBytes);
                }
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
    std::vector<std::shared_ptr<Saved>> savedPool;
    std::size_t savedPoolBytes = 0;
    std::map<std::uintptr_t, std::size_t> splitAliases;
    std::size_t granularity = 0x10000;
    bool watchResident = false;
    std::mutex mutex;
    HANDLE zeroSection = nullptr;
    AllocateFunction allocate = nullptr;
    MapFunction map = nullptr;
    UnmapFunction unmap = nullptr;
    CompareFunction compare = nullptr;
};

}
#endif

#endif
