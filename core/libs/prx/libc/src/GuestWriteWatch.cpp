#include "prx/libc/include/GuestWriteWatch.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/semaphore.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <unistd.h>
#endif

#if (defined(__linux__) && defined(PAGEMAP_SCAN) && defined(UFFD_FEATURE_WP_ASYNC)) || defined(__APPLE__)
#define APS5_GUEST_WRITE_WATCH 1
#endif

#if defined(__linux__) && defined(PAGEMAP_SCAN) && defined(UFFD_FEATURE_WP_ASYNC)
#define APS5_LINUX_READ_TRAPS 1
#endif

namespace GuestWriteWatch {
namespace {

#if defined(__linux__) && defined(PAGEMAP_SCAN) && defined(UFFD_FEATURE_WP_ASYNC)
constexpr std::uintptr_t PageBytes = 4096;

class Watch {
public:
    static Watch& Get() {
        static auto* watch = new Watch;
        return *watch;
    }

    bool Available() const {
        return _pagemap >= 0;
    }

    void Register(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available() || end <= begin) return;
        std::unique_lock lock(_lock);
        remove(_ranges, begin, end);
        remove(_fresh, begin, end);
        cut(begin, end);
        uffdio_register registration{};
        registration.range.start = begin;
        registration.range.len = end - begin;
        registration.mode = UFFDIO_REGISTER_MODE_WP;
        if (ioctl(_uffd, UFFDIO_REGISTER, &registration) != 0) {
            static bool reported = false;
            if (!reported) {
                reported = true;
                std::fprintf(stderr, "[memory] write watch: cannot register 0x%llx+0x%llx (%s); the range stays unwatched\n", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin), std::strerror(errno));
            }
            return;
        }
        insert(_ranges, begin, end);
        insert(_fresh, begin, end);
    }

    bool Unregister(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available() || end <= begin) return false;
        std::unique_lock lock(_lock);
        const bool watched = remove(_ranges, begin, end);
        remove(_fresh, begin, end);
        cut(begin, end);
        return watched;
    }

    bool Alias(std::uintptr_t first, std::uintptr_t second, std::uintptr_t bytes) {
        if (!Available() || bytes == 0 || first == second || (first | second | bytes) % PageBytes != 0) return false;
        std::unique_lock lock(_lock);
        if (!covers(first, first + bytes) || !covers(second, second + bytes)) return false;
        link(first, first + bytes, second);
        link(second, second + bytes, first);
        return true;
    }

    bool Covers(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available()) return false;
        std::shared_lock lock(_lock);
        return covers(begin, end);
    }

    bool Overlaps(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available()) return false;
        std::shared_lock lock(_lock);
        const auto next = _ranges.upper_bound(begin);
        return (next != _ranges.begin() && std::prev(next)->second > begin) || (next != _ranges.end() && next->first < end);
    }

    bool Collect(std::uintptr_t begin, std::uintptr_t end, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        if (!Available()) return false;
        begin &= ~(PageBytes - 1);
        end = (end + PageBytes - 1) & ~(PageBytes - 1);
        if (end <= begin) return true;
        const auto plan = aliasPlan(begin, end);
        Fanout fanout{&plan.own, written, context};
        if (!takeFresh(begin, end, true, &Fanout::Report, &fanout) || !scanWritten(begin, end, &Fanout::Report, &fanout)) return false;
        for (const auto& alias : plan.aliases) {
            Fanout through{&alias.translations, written, context};
            if (!takeFresh(alias.begin, alias.end, false, &Fanout::Report, &through) || !scanWritten(alias.begin, alias.end, &Fanout::Report, &through)) return false;
        }
        return true;
    }

    bool CollectFresh(std::uintptr_t begin, std::uintptr_t end, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        if (!Available()) return false;
        begin &= ~(PageBytes - 1);
        end = (end + PageBytes - 1) & ~(PageBytes - 1);
        if (end <= begin) return true;
        const auto plan = aliasPlan(begin, end);
        if (plan.aliases.empty()) {
            std::shared_lock lock(_lock);
            auto it = _fresh.upper_bound(begin);
            if (it != _fresh.begin()) --it;
            while (it != _fresh.end() && it->first < end && it->second <= begin) ++it;
            if (it == _fresh.end() || it->first >= end) return true;
        }
        Fanout fanout{&plan.own, written, context};
        if (!takeFresh(begin, end, false, &Fanout::Report, &fanout)) return false;
        for (const auto& alias : plan.aliases) {
            Fanout through{&alias.translations, written, context};
            if (!takeFresh(alias.begin, alias.end, false, &Fanout::Report, &through)) return false;
        }
        return true;
    }

    bool CollectArmed(std::uintptr_t begin, std::uintptr_t end, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        if (!Available()) return false;
        begin &= ~(PageBytes - 1);
        end = (end + PageBytes - 1) & ~(PageBytes - 1);
        if (end <= begin) return true;
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> armed;
        AliasPlan plan;
        std::vector<std::vector<std::pair<std::uintptr_t, std::uintptr_t>>> aliasArmed;
        {
            std::shared_lock lock(_lock);
            if (!covers(begin, end)) return false;
            armed = armedParts(begin, end);
            plan = aliasPlanLocked(begin, end);
            for (const auto& alias : plan.aliases) aliasArmed.push_back(armedParts(alias.begin, alias.end));
        }
        Fanout fanout{&plan.own, written, context};
        for (const auto& [from, to] : armed) {
            if (!scanWritten(from, to, &Fanout::Report, &fanout)) return false;
        }
        for (std::size_t index = 0; index < plan.aliases.size(); ++index) {
            Fanout through{&plan.aliases[index].translations, written, context};
            for (const auto& [from, to] : aliasArmed[index]) {
                if (!scanWritten(from, to, &Fanout::Report, &through)) return false;
            }
        }
        return true;
    }

private:
    struct Translation {
        std::uintptr_t begin;
        std::uintptr_t end;
        std::uintptr_t partner;
    };

    struct AliasRange {
        std::uintptr_t begin;
        std::uintptr_t end;
        std::vector<Translation> translations;
    };

    struct AliasPlan {
        std::vector<Translation> own;
        std::vector<AliasRange> aliases;
    };

    struct Fanout {
        const std::vector<Translation>* translations;
        void (*written)(void*, std::uintptr_t, std::uintptr_t);
        void* context;

        static void Report(void* self, std::uintptr_t begin, std::uintptr_t end) {
            auto& fanout = *static_cast<Fanout*>(self);
            fanout.written(fanout.context, begin, end);
            for (const auto& [from, to, partner] : *fanout.translations) {
                const auto left = std::max(from, begin);
                const auto right = std::min(to, end);
                if (left < right) fanout.written(fanout.context, partner + (left - from), partner + (right - from));
            }
        }
    };

    struct Link {
        std::uintptr_t end;
        std::uintptr_t partner;
    };

    AliasPlan aliasPlan(std::uintptr_t begin, std::uintptr_t end) {
        std::shared_lock lock(_lock);
        return aliasPlanLocked(begin, end);
    }

    AliasPlan aliasPlanLocked(std::uintptr_t begin, std::uintptr_t end) const {
        AliasPlan plan;
        plan.own = translations(begin, end);
        for (const auto& [from, to, partner] : plan.own) {
            const auto stop = partner + (to - from);
            plan.aliases.push_back({partner, stop, translations(partner, stop)});
        }
        return plan;
    }

    std::vector<Translation> translations(std::uintptr_t begin, std::uintptr_t end) const {
        std::vector<Translation> found;
        if (_links.empty()) return found;
        for (auto it = _links.lower_bound(begin > _linkBytes ? begin - _linkBytes : 0); it != _links.end() && it->first < end; ++it) {
            const auto from = std::max(it->first, begin);
            const auto to = std::min(it->second.end, end);
            if (from < to) found.push_back({from, to, it->second.partner + (from - it->first)});
        }
        return found;
    }

    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> armedParts(std::uintptr_t begin, std::uintptr_t end) const {
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> armed;
        auto cursor = begin;
        auto it = _fresh.upper_bound(begin);
        if (it != _fresh.begin()) --it;
        for (; it != _fresh.end() && it->first < end; ++it) {
            if (it->second <= cursor) continue;
            if (it->first > cursor) armed.emplace_back(cursor, it->first);
            cursor = std::max(cursor, it->second);
        }
        if (cursor < end) armed.emplace_back(cursor, end);
        return armed;
    }

    void link(std::uintptr_t begin, std::uintptr_t end, std::uintptr_t partner) {
        _links.emplace(begin, Link{end, partner});
        _linkBytes = std::max(_linkBytes, end - begin);
    }

    void cut(std::uintptr_t begin, std::uintptr_t end) {
        std::vector<std::pair<std::uintptr_t, Link>> kept;
        for (auto it = _links.begin(); it != _links.end();) {
            const auto from = it->first;
            const auto bytes = it->second.end - from;
            const auto partner = it->second.partner;
            if (!(from < end && begin < from + bytes) && !(partner < end && begin < partner + bytes)) {
                ++it;
                continue;
            }
            it = _links.erase(it);
            std::vector<std::pair<std::uintptr_t, std::uintptr_t>> pieces{{0, bytes}};
            for (const auto base : {from, partner}) {
                const auto low = std::min(bytes, begin > base ? begin - base : 0);
                const auto high = std::min(bytes, end > base ? end - base : 0);
                if (low >= high) continue;
                std::vector<std::pair<std::uintptr_t, std::uintptr_t>> remaining;
                for (const auto& [left, right] : pieces) {
                    if (left < low) remaining.emplace_back(left, std::min(right, low));
                    if (right > high) remaining.emplace_back(std::max(left, high), right);
                }
                pieces = std::move(remaining);
            }
            for (const auto& [left, right] : pieces) kept.emplace_back(from + left, Link{from + right, partner + left});
        }
        for (const auto& [from, piece] : kept) _links.emplace(from, piece);
    }

    bool takeFresh(std::uintptr_t begin, std::uintptr_t end, bool whole, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> fresh;
        {
            std::unique_lock lock(_lock);
            if (whole && !covers(begin, end)) return false;
            auto it = _fresh.upper_bound(begin);
            if (it != _fresh.begin()) --it;
            for (; it != _fresh.end() && it->first < end; ++it) {
                const auto from = std::max(it->first, begin);
                const auto to = std::min(it->second, end);
                if (from < to) fresh.emplace_back(from, to);
            }
            for (const auto& [from, to] : fresh) remove(_fresh, from, to);
        }
        for (const auto& [from, to] : fresh) {
            written(context, from, to);
            if (protect(from, to)) continue;
            std::unique_lock lock(_lock);
            for (const auto& [left, right] : fresh) {
                if (!covers(left, right)) continue;
                remove(_fresh, left, right);
                insert(_fresh, left, right);
            }
            if (whole) written(context, begin, end);
            else for (const auto& [left, right] : fresh) written(context, left, right);
            return false;
        }
        return true;
    }

    bool scanWritten(std::uintptr_t begin, std::uintptr_t end, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) const {
        std::array<page_region, 256> regions;
        auto cursor = begin;
        while (cursor < end) {
            pm_scan_arg scan{};
            scan.size = sizeof(scan);
            scan.flags = PM_SCAN_WP_MATCHING | PM_SCAN_CHECK_WPASYNC;
            scan.start = cursor;
            scan.end = end;
            scan.vec = reinterpret_cast<std::uintptr_t>(regions.data());
            scan.vec_len = regions.size();
            scan.category_mask = PAGE_IS_WRITTEN;
            scan.return_mask = PAGE_IS_WRITTEN;
            const auto count = ioctl(_pagemap, PAGEMAP_SCAN, &scan);
            if (count < 0) {
                written(context, cursor, end);
                return false;
            }
            for (long i = 0; i < count; ++i) written(context, regions[i].start, regions[i].end);
            if (scan.walk_end <= cursor || scan.walk_end >= end) break;
            cursor = scan.walk_end;
        }
        return true;
    }

private:
    Watch() {
        if (std::getenv("APS5_NO_WRITE_WATCH") == nullptr) open();
    }

    void open() {
        _uffd = static_cast<int>(syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK));
        if (_uffd < 0 && errno == EPERM) _uffd = static_cast<int>(syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY));
        if (_uffd < 0) return unavailable("userfaultfd");
        uffdio_api api{};
        api.api = UFFD_API;
        api.features = UFFD_FEATURE_WP_ASYNC | UFFD_FEATURE_WP_UNPOPULATED;
        if (ioctl(_uffd, UFFDIO_API, &api) != 0 || (api.features & UFFD_FEATURE_WP_ASYNC) == 0 || (api.features & UFFD_FEATURE_WP_UNPOPULATED) == 0) return unavailable("asynchronous userfaultfd write protection");
        const int pagemap = ::open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
        if (pagemap < 0) return unavailable("/proc/self/pagemap");
        if (!probe(pagemap)) {
            close(pagemap);
            return unavailable("PAGEMAP_SCAN");
        }
        _pagemap = pagemap;
    }

    void unavailable(const char* what) {
        const int error = errno;
        if (_uffd >= 0) close(_uffd);
        _uffd = -1;
        std::fprintf(stderr, "[memory] write watch unavailable: %s failed (%s); guest memory is compared instead\n", what, std::strerror(error));
    }

    bool protect(std::uintptr_t begin, std::uintptr_t end) const {
        uffdio_writeprotect protection{};
        protection.range.start = begin;
        protection.range.len = end - begin;
        protection.mode = UFFDIO_WRITEPROTECT_MODE_WP;
        return ioctl(_uffd, UFFDIO_WRITEPROTECT, &protection) == 0;
    }

    bool probe(int pagemap) {
        constexpr std::uintptr_t probeBytes = 4 * PageBytes;
        void* pages = mmap(nullptr, probeBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (pages == MAP_FAILED) return false;
        const auto address = reinterpret_cast<std::uintptr_t>(pages);
        auto* bytes = static_cast<volatile char*>(pages);
        bytes[0] = 1;
        uffdio_register registration{};
        registration.range.start = address;
        registration.range.len = probeBytes;
        registration.mode = UFFDIO_REGISTER_MODE_WP;
        bool working = ioctl(_uffd, UFFDIO_REGISTER, &registration) == 0 && protect(address, address + probeBytes);
        std::array<page_region, 4> regions{};
        const auto scan = [&]() -> long {
            pm_scan_arg arguments{};
            arguments.size = sizeof(arguments);
            arguments.flags = PM_SCAN_WP_MATCHING | PM_SCAN_CHECK_WPASYNC;
            arguments.start = address;
            arguments.end = address + probeBytes;
            arguments.vec = reinterpret_cast<std::uintptr_t>(regions.data());
            arguments.vec_len = regions.size();
            arguments.category_mask = PAGE_IS_WRITTEN;
            arguments.return_mask = PAGE_IS_WRITTEN;
            const auto count = ioctl(pagemap, PAGEMAP_SCAN, &arguments);
            if (count < 0) return -1;
            long pagesWritten = 0;
            for (long i = 0; i < count; ++i) pagesWritten += static_cast<long>((regions[i].end - regions[i].start) / PageBytes);
            return pagesWritten;
        };
        working = working && scan() == 0;
        if (working) {
            bytes[0] = 2;
            bytes[2 * PageBytes] = 1;
            working = scan() == 2 && scan() == 0;
        }
        munmap(pages, probeBytes);
        return working;
    }

    bool covers(std::uintptr_t begin, std::uintptr_t end) const {
        auto next = _ranges.upper_bound(begin);
        if (next == _ranges.begin()) return false;
        return std::prev(next)->second >= end;
    }

    static void insert(std::map<std::uintptr_t, std::uintptr_t>& ranges, std::uintptr_t begin, std::uintptr_t end) {
        auto next = ranges.upper_bound(begin);
        if (next != ranges.begin() && std::prev(next)->second == begin) {
            begin = std::prev(next)->first;
            ranges.erase(std::prev(next));
        }
        if (next != ranges.end() && next->first == end) {
            end = next->second;
            ranges.erase(next);
        }
        ranges.emplace(begin, end);
    }

    static bool remove(std::map<std::uintptr_t, std::uintptr_t>& ranges, std::uintptr_t begin, std::uintptr_t end) {
        bool removed = false;
        auto it = ranges.upper_bound(begin);
        if (it != ranges.begin()) --it;
        while (it != ranges.end() && it->first < end) {
            const auto rangeBegin = it->first;
            const auto rangeEnd = it->second;
            if (rangeEnd <= begin) {
                ++it;
                continue;
            }
            removed = true;
            it = ranges.erase(it);
            if (rangeBegin < begin) ranges.emplace(rangeBegin, begin);
            if (rangeEnd > end) ranges.emplace(end, rangeEnd);
        }
        return removed;
    }

    int _uffd = -1;
    int _pagemap = -1;
    std::shared_mutex _lock;
    std::map<std::uintptr_t, std::uintptr_t> _ranges;
    std::map<std::uintptr_t, std::uintptr_t> _fresh;
    std::multimap<std::uintptr_t, Link> _links;
    std::uintptr_t _linkBytes = 0;
};

const bool g_opened = (Watch::Get(), true);

class TrapLock {
public:
    void lock() {
        while (_held.test_and_set(std::memory_order_acquire)) sched_yield();
    }

    void unlock() {
        _held.clear(std::memory_order_release);
    }

private:
    std::atomic_flag _held = ATOMIC_FLAG_INIT;
};

class ReadTraps {
public:
    static ReadTraps& Get() {
        static auto* traps = new ReadTraps;
        return *traps;
    }

    void Registered(std::uintptr_t begin, std::uintptr_t end) {
        if (end <= begin) return;
        const std::lock_guard lock(_lock);
        for (auto it = _traps.begin(); it != _traps.end();) it = it->owner == 0 && it->first < pageEnd(end) && pageStart(begin) < it->last ? _traps.erase(it) : std::next(it);
        eraseProtections(begin, end);
        cut(_unlearned, begin, end);
        _unlearned.emplace(begin, end);
    }

    void Forget(std::uintptr_t begin, std::uintptr_t end) {
        if (end <= begin) return;
        const std::lock_guard lock(_lock);
        std::vector<Trap> dropped;
        for (auto it = _traps.begin(); it != _traps.end();) {
            if (it->owner == 0 && it->first < pageEnd(end) && pageStart(begin) < it->last) {
                dropped.push_back(std::move(*it));
                it = _traps.erase(it);
            } else {
                ++it;
            }
        }
        for (const auto& trap : dropped) restore(trap);
        eraseProtections(begin, end);
        cut(_unlearned, begin, end);
    }

    bool SetResolver(void (*resolver)(std::uintptr_t, std::uintptr_t)) {
        if (resolver == nullptr || !Watch::Get().Available()) return false;
        std::call_once(_started, [this] { start(); });
        if (!_running.load(std::memory_order_acquire)) return false;
        _resolver.store(resolver, std::memory_order_release);
        return true;
    }

    void SetBypass(bool (*bypass)()) {
        _bypass.store(bypass, std::memory_order_release);
    }

    void Trapped(std::uintptr_t begin, std::uintptr_t end, void (*trapped)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        if (end <= begin || !_trapping.load(std::memory_order_acquire)) return;
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> runs;
        {
            const std::lock_guard lock(_lock);
            for (auto page = pageStart(begin); page < pageEnd(end); page += PageBytes) {
                if (!closed(page)) continue;
                if (!runs.empty() && runs.back().second == page) runs.back().second = page + PageBytes;
                else runs.emplace_back(page, page + PageBytes);
            }
        }
        for (const auto& [runBegin, runEnd] : runs) trapped(context, runBegin, runEnd);
    }

    bool TrapReads(std::uintptr_t begin, std::uintptr_t end) {
        if (end <= begin || _resolver.load(std::memory_order_acquire) == nullptr) return false;
        const auto first = pageStart(begin);
        const auto last = pageEnd(end);
        if (!Watch::Get().Covers(first, last)) return false;
        learn(first, last);
        Trap trap{begin, end, first, last, 0, 0, std::vector<std::uint8_t>((last - first) / PageBytes, 0)};
        const std::lock_guard lock(_lock);
        if (std::any_of(_traps.begin(), _traps.end(), [&](const Trap& held) { return held.owner == 0 && held.begin == begin && held.end == end; })) return true;
        for (const auto& [hostBegin, hostEnd] : _hostWrites)
            if (hostBegin < last && first < hostEnd) return false;
        if (!protectionIs(first, last, PROT_READ | PROT_WRITE)) return false;
        if (std::any_of(_traps.begin(), _traps.end(), [&](const Trap& held) { return held.owner != 0 && held.first < last && first < held.last; })) return false;
        if (::mprotect(reinterpret_cast<void*>(first), last - first, PROT_NONE) != 0) return false;
        _traps.push_back(std::move(trap));
        _trapping.store(true, std::memory_order_release);
        return true;
    }

    void ReleaseReads(std::uintptr_t begin, std::uintptr_t end) {
        if (end <= begin) return;
        const std::lock_guard lock(_lock);
        const auto found = std::find_if(_traps.begin(), _traps.end(), [&](const Trap& trap) { return trap.owner == 0 && trap.begin == begin && trap.end == end; });
        if (found == _traps.end()) return;
        const auto released = std::move(*found);
        _traps.erase(found);
        restore(released);
    }

    bool HandleFault(std::uintptr_t address) {
        if (!_trapping.load(std::memory_order_acquire)) return false;
        const KeepErrno keep;
        const auto page = pageStart(address);
        const auto self = CurrentThread();
        {
            const std::lock_guard lock(_lock);
            if (!closed(page)) return retryOpen(page);
            const auto bypass = _bypass.load(std::memory_order_acquire);
            if (self == _resolverThread.load(std::memory_order_acquire) || ownedBy(page, self) || (bypass != nullptr && bypass())) {
                for (auto& trap : _traps)
                    if (trap.first <= page && page < trap.last) trap.open[(page - trap.first) / PageBytes] = 1;
                ++_generation;
                return ::mprotect(reinterpret_cast<void*>(page), PageBytes, PROT_READ | PROT_WRITE) == 0;
            }
        }
        Slot* slot = nullptr;
        while (slot == nullptr) {
            for (auto& candidate : _slots) {
                unsigned expected = Free;
                if (candidate.state.compare_exchange_strong(expected, Claimed, std::memory_order_acq_rel)) {
                    slot = &candidate;
                    break;
                }
            }
            if (slot == nullptr) sched_yield();
        }
        slot->page = page;
        slot->state.store(Queued, std::memory_order_release);
        sem_post(&_wake);
        while (sem_wait(&slot->done) != 0) {}
        slot->state.store(Free, std::memory_order_release);
        return true;
    }

    int Protect(std::uintptr_t begin, std::uintptr_t end, int protection) {
        return withoutTraps(begin, end, [&] {
            if (unlearnedOverlaps(begin, end)) ++_protects;
            if (::mprotect(reinterpret_cast<void*>(begin), end - begin, protection) != 0) return errno;
            reprotect(pageStart(begin), pageEnd(end), protection);
            return 0;
        });
    }

    void BeginHostWrite(std::uintptr_t begin, std::uintptr_t end) {
        if (end <= begin) return;
        withoutTraps(begin, end, [&] {
            _hostWrites.emplace_back(pageStart(begin), pageEnd(end));
            return 0;
        });
    }

    void EndHostWrite(std::uintptr_t begin, std::uintptr_t end) {
        if (end <= begin) return;
        const std::lock_guard lock(_lock);
        const auto found = std::find(_hostWrites.begin(), _hostWrites.end(), std::pair{pageStart(begin), pageEnd(end)});
        if (found != _hostWrites.end()) _hostWrites.erase(found);
    }

private:
    static constexpr std::size_t ResolveSlots = 32;
    static constexpr unsigned UntrappedRetries = 16;
    static constexpr unsigned PageRetries = 1024;

    enum SlotState : unsigned { Free, Claimed, Queued, Running, Done };

    struct Slot {
        std::atomic<unsigned> state{Free};
        std::uintptr_t page = 0;
        sem_t done{};
    };

    struct Trap {
        std::uintptr_t begin;
        std::uintptr_t end;
        std::uintptr_t first;
        std::uintptr_t last;
        pid_t owner;
        std::uint64_t batch;
        std::vector<std::uint8_t> open;
    };

    struct Protection {
        std::uintptr_t end;
        int protection;
    };

    struct Span {
        std::uintptr_t begin;
        std::uintptr_t end;
        int protection;
    };

    struct KeepErrno {
        int saved = errno;
        ~KeepErrno() { errno = saved; }
    };

    static pid_t CurrentThread() {
        return static_cast<pid_t>(::syscall(SYS_gettid));
    }

    static std::uintptr_t pageStart(std::uintptr_t address) {
        return address & ~(PageBytes - 1);
    }

    static std::uintptr_t pageEnd(std::uintptr_t address) {
        return (address + PageBytes - 1) & ~(PageBytes - 1);
    }

    static std::vector<Span> mappedProtections(std::uintptr_t begin, std::uintptr_t end) {
        std::vector<Span> spans;
        const int file = ::open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
        if (file < 0) return spans;
        std::string text;
        std::array<char, 65536> buffer{};
        for (;;) {
            const auto count = ::read(file, buffer.data(), buffer.size());
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) break;
            text.append(buffer.data(), static_cast<std::size_t>(count));
        }
        ::close(file);
        std::size_t at = 0;
        while (at < text.size()) {
            const auto lineEnd = std::min(text.find('\n', at), text.size());
            const char* cursor = text.c_str() + at;
            char* parsed = nullptr;
            const auto first = static_cast<std::uintptr_t>(std::strtoull(cursor, &parsed, 16));
            if (parsed != nullptr && *parsed == '-') {
                const auto last = static_cast<std::uintptr_t>(std::strtoull(parsed + 1, &parsed, 16));
                if (parsed != nullptr && *parsed == ' ' && first < end && begin < last && static_cast<std::size_t>(parsed - text.c_str()) + 4 < lineEnd) {
                    const int protection = (parsed[1] == 'r' ? PROT_READ : 0) | (parsed[2] == 'w' ? PROT_WRITE : 0) | (parsed[3] == 'x' ? PROT_EXEC : 0);
                    spans.push_back({std::max(first, begin), std::min(last, end), protection});
                }
            }
            at = lineEnd + 1;
        }
        return spans;
    }

    void learn(std::uintptr_t first, std::uintptr_t last) {
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> pending;
        std::uint64_t protects = 0;
        {
            const std::lock_guard lock(_lock);
            auto it = _unlearned.upper_bound(first);
            if (it != _unlearned.begin()) --it;
            for (; it != _unlearned.end() && it->first < last; ++it)
                if (it->second > first) pending.emplace_back(*it);
            protects = _protects;
        }
        for (const auto& [begin, end] : pending) {
            const auto spans = mappedProtections(begin, end);
            const std::lock_guard lock(_lock);
            const auto found = _unlearned.find(begin);
            if (found == _unlearned.end() || found->second != end || _protects != protects) continue;
            _unlearned.erase(found);
            for (const auto& span : spans) _protections.emplace(span.begin, Protection{span.end, span.protection});
        }
    }

    bool unlearnedOverlaps(std::uintptr_t begin, std::uintptr_t end) const {
        const auto next = _unlearned.upper_bound(begin);
        return (next != _unlearned.begin() && std::prev(next)->second > begin) || (next != _unlearned.end() && next->first < end);
    }

    static void cut(std::map<std::uintptr_t, std::uintptr_t>& ranges, std::uintptr_t begin, std::uintptr_t end) {
        auto it = ranges.upper_bound(begin);
        if (it != ranges.begin()) --it;
        while (it != ranges.end() && it->first < end) {
            const auto rangeBegin = it->first;
            const auto rangeEnd = it->second;
            if (rangeEnd <= begin) {
                ++it;
                continue;
            }
            it = ranges.erase(it);
            if (rangeBegin < begin) ranges.emplace(rangeBegin, begin);
            if (rangeEnd > end) it = ranges.emplace(end, rangeEnd).first;
        }
    }

    void start() {
        if (sem_init(&_wake, 0, 0) != 0) return;
        for (auto& slot : _slots)
            if (sem_init(&slot.done, 0, 0) != 0) return;
        struct sigaction action{};
        action.sa_sigaction = &ReadTraps::fault;
        action.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&action.sa_mask);
        pthread_t thread{};
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
        const bool created = pthread_create(&thread, &attributes, [](void* traps) -> void* {
            static_cast<ReadTraps*>(traps)->resolveLoop();
            return nullptr;
        }, this) == 0;
        pthread_attr_destroy(&attributes);
        if (!created) return;
        while (_resolverThread.load(std::memory_order_acquire) == 0) sched_yield();
        if (sigaction(SIGSEGV, &action, &_previousSegv) != 0 || sigaction(SIGBUS, &action, &_previousBus) != 0) return;
        _running.store(true, std::memory_order_release);
    }

    static void fault(int signal, siginfo_t* info, void* context) {
        auto& traps = Get();
        if (info->si_code > 0 && traps.HandleFault(reinterpret_cast<std::uintptr_t>(info->si_addr))) return;
        const auto& previous = signal == SIGBUS ? traps._previousBus : traps._previousSegv;
        if ((previous.sa_flags & SA_SIGINFO) != 0 && previous.sa_sigaction != nullptr) {
            previous.sa_sigaction(signal, info, context);
            return;
        }
        if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN && previous.sa_handler != nullptr) {
            previous.sa_handler(signal);
            return;
        }
        ::signal(signal, SIG_DFL);
        if (info->si_code <= 0) ::raise(signal);
    }

    void resolveLoop() {
        pthread_setname_np(pthread_self(), "read traps");
        _resolverThread.store(CurrentThread(), std::memory_order_release);
        for (;;) {
            while (sem_wait(&_wake) != 0) {}
            for (auto& slot : _slots) {
                unsigned expected = Queued;
                if (!slot.state.compare_exchange_strong(expected, Running, std::memory_order_acq_rel)) continue;
                const auto page = slot.page;
                waitForResolving(page, page + PageBytes);
                resolve(page, page + PageBytes);
                slot.state.store(Done, std::memory_order_release);
                sem_post(&slot.done);
            }
        }
    }

    void resolve(std::uintptr_t begin, std::uintptr_t end) {
        const auto self = CurrentThread();
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> taken;
        std::uint64_t batch = 0;
        {
            const std::lock_guard lock(_lock);
            batch = ++_batches;
            auto first = pageStart(begin);
            auto last = pageEnd(end);
            for (bool grew = true; grew;) {
                grew = false;
                for (auto& trap : _traps) {
                    if (trap.owner != 0 || trap.last <= first || last <= trap.first) continue;
                    trap.owner = self;
                    trap.batch = batch;
                    taken.emplace_back(trap.begin, trap.end);
                    first = std::min(first, trap.first);
                    last = std::max(last, trap.last);
                    grew = true;
                }
            }
        }
        if (taken.empty()) return;
        const auto resolver = _resolver.load(std::memory_order_acquire);
        for (const auto& [first, last] : taken) resolver(first, last);
        const std::lock_guard lock(_lock);
        std::vector<Trap> resolved;
        for (auto it = _traps.begin(); it != _traps.end();) {
            if (it->batch == batch) {
                resolved.push_back(std::move(*it));
                it = _traps.erase(it);
            } else {
                ++it;
            }
        }
        for (const auto& trap : resolved) restore(trap);
    }

    void waitForResolving(std::uintptr_t begin, std::uintptr_t end) {
        const auto self = CurrentThread();
        const auto first = pageStart(begin);
        const auto last = pageEnd(end);
        for (;;) {
            {
                const std::lock_guard lock(_lock);
                if (std::none_of(_traps.begin(), _traps.end(), [&](const Trap& trap) { return trap.owner != 0 && trap.owner != self && trap.first < last && first < trap.last; })) return;
            }
            sched_yield();
        }
    }

    template <typename TFunction>
    int withoutTraps(std::uintptr_t begin, std::uintptr_t end, TFunction function) {
        const auto first = pageStart(begin);
        const auto last = pageEnd(end);
        const auto self = CurrentThread();
        for (;;) {
            if (_trapping.load(std::memory_order_acquire)) {
                waitForResolving(first, last);
                resolve(first, last);
            }
            const std::lock_guard lock(_lock);
            if (std::any_of(_traps.begin(), _traps.end(), [&](const Trap& trap) { return trap.owner != self && trap.first < last && first < trap.last; })) continue;
            return function();
        }
    }

    bool closed(std::uintptr_t page) const {
        return std::any_of(_traps.begin(), _traps.end(), [&](const Trap& trap) { return trap.first <= page && page < trap.last && trap.open[(page - trap.first) / PageBytes] == 0; });
    }

    bool ownedBy(std::uintptr_t page, pid_t thread) const {
        return std::any_of(_traps.begin(), _traps.end(), [&](const Trap& trap) { return trap.owner == thread && trap.first <= page && page < trap.last; });
    }

    bool retryOpen(std::uintptr_t page) {
        if (!protectionIs(page, page + PageBytes, PROT_READ | PROT_WRITE)) return false;
        thread_local std::uintptr_t retryPage = 0;
        thread_local std::uint64_t retryGeneration = 0;
        thread_local unsigned retries = 0;
        thread_local unsigned pageRetries = 0;
        if (retryPage != page) {
            retryPage = page;
            pageRetries = 0;
            retryGeneration = _generation;
            retries = 0;
        } else if (retryGeneration != _generation) {
            retryGeneration = _generation;
            retries = 0;
        }
        return ++pageRetries <= PageRetries && ++retries <= UntrappedRetries;
    }

    void restore(const Trap& removed) {
        std::uintptr_t runStart = 0;
        std::uintptr_t runEnd = 0;
        const auto flush = [&] {
            if (runEnd > runStart) ::mprotect(reinterpret_cast<void*>(runStart), runEnd - runStart, PROT_READ | PROT_WRITE);
            runStart = runEnd = 0;
        };
        for (auto page = removed.first; page < removed.last; page += PageBytes) {
            const bool reopen = removed.open[(page - removed.first) / PageBytes] == 0 && !closed(page);
            if (!reopen) {
                flush();
                continue;
            }
            if (runEnd != page) {
                flush();
                runStart = page;
            }
            runEnd = page + PageBytes;
        }
        flush();
        ++_generation;
    }

    bool protectionIs(std::uintptr_t begin, std::uintptr_t end, int protection) const {
        auto it = _protections.upper_bound(begin);
        if (it == _protections.begin()) return false;
        --it;
        for (auto cursor = begin; cursor < end; ++it) {
            if (it == _protections.end() || it->first > cursor || it->second.end <= cursor || it->second.protection != protection) return false;
            cursor = it->second.end;
        }
        return true;
    }

    void eraseProtections(std::uintptr_t begin, std::uintptr_t end) {
        auto it = _protections.upper_bound(begin);
        if (it != _protections.begin()) --it;
        while (it != _protections.end() && it->first < end) {
            const auto spanBegin = it->first;
            const auto span = it->second;
            if (span.end <= begin) {
                ++it;
                continue;
            }
            it = _protections.erase(it);
            if (spanBegin < begin) _protections.emplace(spanBegin, Protection{begin, span.protection});
            if (span.end > end) it = _protections.emplace(end, Protection{span.end, span.protection}).first;
        }
    }

    void reprotect(std::uintptr_t begin, std::uintptr_t end, int protection) {
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> known;
        auto it = _protections.upper_bound(begin);
        if (it != _protections.begin()) --it;
        for (; it != _protections.end() && it->first < end; ++it)
            if (it->second.end > begin) known.emplace_back(std::max(it->first, begin), std::min(it->second.end, end));
        eraseProtections(begin, end);
        for (const auto& [first, last] : known) _protections.emplace(first, Protection{last, protection});
    }

    TrapLock _lock;
    std::vector<Trap> _traps;
    std::map<std::uintptr_t, Protection> _protections;
    std::map<std::uintptr_t, std::uintptr_t> _unlearned;
    std::uint64_t _protects = 0;
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> _hostWrites;
    std::uint64_t _generation = 0;
    std::uint64_t _batches = 0;
    std::atomic<bool> _trapping{false};
    std::atomic<void (*)(std::uintptr_t, std::uintptr_t)> _resolver{nullptr};
    std::atomic<bool (*)()> _bypass{nullptr};
    std::once_flag _started;
    std::atomic<bool> _running{false};
    std::atomic<pid_t> _resolverThread{0};
    sem_t _wake{};
    std::array<Slot, ResolveSlots> _slots;
    struct sigaction _previousBus {};
    struct sigaction _previousSegv {};
};

const bool g_trapsOpened = (ReadTraps::Get(), true);
#elif defined(__APPLE__)
class Watch {
public:
    static Watch& Get() {
        static auto* watch = new Watch;
        return *watch;
    }

    bool Available() const {
        return _available;
    }

    void Register(std::uintptr_t begin, std::uintptr_t end) {
        if (!_available || end <= begin) return;
        begin = pageStart(begin);
        end = pageEnd(end);
        if (end <= begin) return;
        std::vector<std::uint8_t> pages((end - begin) / _page, Written | Fresh);
        std::vector<bool> queried(pages.size(), false);
        const Lock lock(_lock);
        for (auto cursor = begin; cursor < end;) {
            mach_vm_address_t address = cursor;
            mach_vm_size_t size = 0;
            vm_region_basic_info_data_64_t info{};
            mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
            mach_port_t object = MACH_PORT_NULL;
            if (mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS || address >= end) break;
            const auto first = std::max<std::uintptr_t>(cursor, address);
            const auto last = std::min<std::uintptr_t>(end, address + size);
            const std::uint8_t protection = ((info.protection & VM_PROT_READ) ? PROT_READ : 0) | ((info.protection & VM_PROT_WRITE) ? PROT_WRITE : 0) | ((info.protection & VM_PROT_EXECUTE) ? PROT_EXEC : 0);
            for (auto page = first; page < last; page += _page) {
                pages[(page - begin) / _page] = Written | Fresh | protection;
                queried[(page - begin) / _page] = true;
            }
            cursor = last;
        }
        forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
            auto& entry = pages[(page - begin) / _page];
            const std::uint8_t recorded = state & ProtectionMask;
            const bool armed = (state & Protected) != 0 && (entry & ProtectionMask) == (recorded & ~PROT_WRITE);
            if (armed || (state & ReadTrapped) != 0 || !queried[(page - begin) / _page]) entry = static_cast<std::uint8_t>(Written | Fresh | recorded);
            else state = static_cast<std::uint8_t>(state & ~Protected);
        });
        dropTraps(begin, end);
        erase(begin, end);
        _ranges.emplace(begin, Range{end, std::move(pages)});
    }

    bool Unregister(std::uintptr_t begin, std::uintptr_t end) {
        if (!_available || end <= begin) return false;
        begin = pageStart(begin);
        end = pageEnd(end);
        if (end <= begin) return false;
        const Lock lock(_lock);
        dropTraps(begin, end);
        return erase(begin, end);
    }

    bool Covers(std::uintptr_t begin, std::uintptr_t end) {
        if (!_available) return false;
        const Lock lock(_lock);
        return covers(begin, end);
    }

    bool Overlaps(std::uintptr_t begin, std::uintptr_t end) {
        if (!_available) return false;
        const Lock lock(_lock);
        const auto next = _ranges.upper_bound(begin);
        return (next != _ranges.begin() && std::prev(next)->second.end > begin) || (next != _ranges.end() && next->first < end);
    }

    bool HandleFault(std::uintptr_t address) {
        return _available && record(address);
    }

    enum class Take { All, Armed, Fresh };

    bool Collect(std::uintptr_t begin, std::uintptr_t end, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context, Take take = Take::All) {
        if (!_available) return false;
        begin = pageStart(begin);
        end = pageEnd(end);
        if (end <= begin) return true;
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> runs;
        {
            const Lock lock(_lock);
            if (take != Take::Fresh && !covers(begin, end)) return false;
            forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
                if ((state & Written) == 0) return;
                const bool fresh = (state & Fresh) != 0;
                if ((take == Take::Armed && fresh) || (take == Take::Fresh && !fresh)) return;
                state &= static_cast<std::uint8_t>(~Fresh);
                if (!runs.empty() && runs.back().second == page) runs.back().second = page + _page;
                else runs.emplace_back(page, page + _page);
                for (const auto& [first, last] : _hostWrites)
                    if (page >= first && page < last) return;
                state &= static_cast<std::uint8_t>(~Written);
                if ((state & PROT_WRITE) != 0) state |= Protected;
            });
            for (const auto& [first, last] : runs) protect(first, last);
        }
        for (const auto& [first, last] : runs) written(context, first, last);
        return true;
    }

    void ProtectionChanged(std::uintptr_t begin, std::uintptr_t end, int protection) {
        if (!_available || end <= begin) return;
        begin = pageStart(begin);
        end = pageEnd(end);
        if (end <= begin) return;
        withoutTraps(begin, end, false, [&] {
            forEachPage(begin, end, [&](std::uintptr_t, std::uint8_t& state) {
                state = static_cast<std::uint8_t>(Written | (state & Fresh) | (protection & ProtectionMask));
            });
        });
    }

    int Protect(std::uintptr_t begin, std::uintptr_t end, int protection) {
        if (!_available) return ::mprotect(reinterpret_cast<void*>(begin), end - begin, protection) == 0 ? 0 : errno;
        const auto first = pageStart(begin);
        const auto last = pageEnd(end);
        return withoutTraps(first, last, true, [&] {
            if (::mprotect(reinterpret_cast<void*>(begin), end - begin, protection) != 0) return errno;
            if (first < last) {
                forEachPage(first, last, [&](std::uintptr_t, std::uint8_t& state) {
                    state = static_cast<std::uint8_t>(Written | (state & Fresh) | (protection & ProtectionMask));
                });
            }
            return 0;
        });
    }

    void BeginHostWrite(std::uintptr_t begin, std::uintptr_t end) {
        if (!_available || end <= begin) return;
        begin = pageStart(begin);
        end = pageEnd(end);
        if (end <= begin) return;
        withoutTraps(begin, end, true, [&] {
            _hostWrites.emplace_back(begin, end);
            forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
                if ((state & Protected) != 0) ::mprotect(reinterpret_cast<void*>(page), _page, state & ProtectionMask);
                state = static_cast<std::uint8_t>((state & (ProtectionMask | Fresh)) | Written);
            });
        });
    }

    void EndHostWrite(std::uintptr_t begin, std::uintptr_t end) {
        if (!_available || end <= begin) return;
        begin = pageStart(begin);
        end = pageEnd(end);
        if (end <= begin) return;
        const Lock lock(_lock);
        const auto found = std::find(_hostWrites.begin(), _hostWrites.end(), std::pair{begin, end});
        if (found != _hostWrites.end()) _hostWrites.erase(found);
    }

    int Protection(std::uintptr_t address, std::uintptr_t limit, std::uintptr_t* runEnd) {
        const auto page = pageStart(address);
        const Lock lock(_lock);
        auto it = _ranges.upper_bound(page);
        if (_available && it != _ranges.begin() && page < std::prev(it)->second.end) {
            --it;
            const auto& pages = it->second.pages;
            const std::uint8_t protection = pages[(page - it->first) / _page] & ProtectionMask;
            auto end = page + _page;
            while (end < it->second.end && end < limit && (pages[(end - it->first) / _page] & ProtectionMask) == protection) end += _page;
            *runEnd = std::min(end, limit);
            return protection;
        }
        *runEnd = it != _ranges.end() ? std::min(it->first, limit) : limit;
        return -1;
    }

    bool SetReadTrapResolver(void (*resolver)(std::uintptr_t, std::uintptr_t)) {
        if (!_available || resolver == nullptr) return false;
        std::call_once(_resolverStarted, [this] { startResolver(); });
        if (!_resolverRunning.load(std::memory_order_acquire)) return false;
        _resolver.store(resolver, std::memory_order_release);
        return true;
    }

    void SetReadTrapBypass(bool (*bypass)()) {
        _bypass.store(bypass, std::memory_order_release);
    }

    bool TrapReads(std::uintptr_t begin, std::uintptr_t end) {
        if (!_available || end <= begin || _resolver.load(std::memory_order_acquire) == nullptr) return false;
        const auto first = pageStart(begin);
        const auto last = pageEnd(end);
        const Lock lock(_lock);
        if (!covers(first, last)) return false;
        const auto [same, other] = _traps.equal_range(begin);
        if (std::any_of(same, other, [&](const auto& trap) { return trap.second == end; })) return true;
        for (const auto& [hostBegin, hostEnd] : _hostWrites)
            if (hostBegin < last && first < hostEnd) return false;
        if (::mprotect(reinterpret_cast<void*>(first), last - first, PROT_NONE) != 0) return false;
        _traps.emplace(begin, end);
        forEachPage(first, last, [](std::uintptr_t, std::uint8_t& state) { state |= ReadTrapped; });
        _trapping.store(true, std::memory_order_release);
        return true;
    }

    void ReadTraps(std::uintptr_t begin, std::uintptr_t end, void (*trapped)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        if (!_available || end <= begin || !_trapping.load(std::memory_order_acquire)) return;
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> runs;
        {
            const Lock lock(_lock);
            forEachPage(pageStart(begin), pageEnd(end), [&](std::uintptr_t page, std::uint8_t& state) {
                if ((state & ReadTrapped) == 0) return;
                if (!runs.empty() && runs.back().second == page) runs.back().second = page + _page;
                else runs.emplace_back(page, page + _page);
            });
        }
        for (const auto& [runBegin, runEnd] : runs) trapped(context, runBegin, runEnd);
    }

    void ReleaseReads(std::uintptr_t begin, std::uintptr_t end) {
        if (!_available || end <= begin) return;
        const Lock lock(_lock);
        const auto [first, last] = _traps.equal_range(begin);
        const auto found = std::find_if(first, last, [&](const auto& trap) { return trap.second == end; });
        if (found == last) return;
        _traps.erase(found);
        untrap(begin, end);
    }

private:
    static constexpr std::uint8_t ProtectionMask = PROT_READ | PROT_WRITE | PROT_EXEC;
    static constexpr std::uint8_t Written = 0x10;
    static constexpr std::uint8_t Protected = 0x20;
    static constexpr std::uint8_t Fresh = 0x40;
    static constexpr std::uintptr_t FaultAheadBlock = 65536;
    static constexpr std::uint8_t ReadTrapped = 0x80;
    static constexpr std::size_t ResolveSlots = 32;
    static constexpr std::size_t ResolvingCapacity = 4096;
    static constexpr std::size_t RetrySlots = 64;
    static constexpr unsigned UntrappedRetries = 16;
    static constexpr unsigned GenerationRetries = RetrySlots * UntrappedRetries;

    enum SlotState : unsigned { Free, Claimed, Queued, Running, Done };
    enum class Owner { Any, Self, Others };

    struct Slot {
        std::atomic<unsigned> state{Free};
        std::uintptr_t page = 0;
        semaphore_t done = MACH_PORT_NULL;
    };

    struct Resolving {
        std::uintptr_t begin;
        std::uintptr_t end;
        pthread_t owner;
    };

    struct Retry {
        std::uintptr_t page = 0;
        pthread_t thread = nullptr;
        std::uint64_t generation = 0;
        std::uint64_t used = 0;
        unsigned count = 0;
    };

    struct Range {
        std::uintptr_t end;
        std::vector<std::uint8_t> pages;
    };

    struct Mutex {
        Mutex() {
            pthread_mutexattr_t attributes;
            pthread_mutexattr_init(&attributes);
            pthread_mutexattr_setpolicy_np(&attributes, PTHREAD_MUTEX_POLICY_FIRSTFIT_NP);
            pthread_mutex_init(&mutex, &attributes);
            pthread_mutexattr_destroy(&attributes);
            pthread_cond_init(&idle, nullptr);
        }
        pthread_mutex_t mutex;
        pthread_cond_t idle;
        std::atomic<unsigned> faulting{0};
    };

    struct Lock {
        explicit Lock(Mutex& mutex) : _mutex(mutex) {
            pthread_mutex_lock(&_mutex.mutex);
            while (_mutex.faulting.load(std::memory_order_acquire) != 0) pthread_cond_wait(&_mutex.idle, &_mutex.mutex);
        }
        ~Lock() {
            pthread_mutex_unlock(&_mutex.mutex);
        }
        Mutex& _mutex;
    };

    struct FaultLock {
        explicit FaultLock(Mutex& mutex) : _mutex(mutex) {
            _mutex.faulting.fetch_add(1, std::memory_order_acq_rel);
            pthread_mutex_lock(&_mutex.mutex);
        }
        ~FaultLock() {
            if (_mutex.faulting.fetch_sub(1, std::memory_order_acq_rel) == 1) pthread_cond_broadcast(&_mutex.idle);
            pthread_mutex_unlock(&_mutex.mutex);
        }
        Mutex& _mutex;
    };

    Watch() {
        if (const char* ahead = std::getenv("APS5_WRITE_WATCH_FAULT_AHEAD")) _faultAhead = static_cast<std::size_t>(std::strtoul(ahead, nullptr, 0));
        if (std::getenv("APS5_NO_WRITE_WATCH") != nullptr) return;
        struct sigaction action {};
        action.sa_sigaction = &Watch::fault;
        action.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGBUS, &action, &_previousBus) != 0 || sigaction(SIGSEGV, &action, &_previousSegv) != 0) {
            std::fprintf(stderr, "[memory] write watch: cannot install the fault handler (%s); guest buffers are fully re-read every use\n", std::strerror(errno));
            return;
        }
        _available = true;
    }

    static bool isWriteFault(const siginfo_t* info, void* context) {
        if (info->si_code <= 0 || info->si_code >= SI_USER || context == nullptr) return false;
        const auto state = static_cast<ucontext_t*>(context)->uc_mcontext;
        if (state == nullptr) return false;
#if defined(__x86_64__)
        constexpr unsigned PageFault = 14;
        constexpr unsigned WriteAccess = 1u << 1;
        return state->__es.__trapno == PageFault && (state->__es.__err & WriteAccess) != 0;
#elif defined(__aarch64__)
        constexpr unsigned DataAbort = 0x24;
        constexpr unsigned ExceptionClassShift = 26;
        constexpr unsigned WriteAccess = 1u << 6;
        return (state->__es.__esr >> ExceptionClassShift) == DataAbort && (state->__es.__esr & WriteAccess) != 0;
#else
        return false;
#endif
    }

    static void fault(int signal, siginfo_t* info, void* context) {
        auto& watch = Get();
        const bool write = isWriteFault(info, context);
        if (info->si_code > 0 && info->si_code < SI_USER && watch.resolveTrap(reinterpret_cast<std::uintptr_t>(info->si_addr), write)) return;
        if (write && watch.record(reinterpret_cast<std::uintptr_t>(info->si_addr))) return;
        const auto& previous = signal == SIGBUS ? watch._previousBus : watch._previousSegv;
        if ((previous.sa_flags & SA_SIGINFO) != 0 && previous.sa_sigaction != nullptr) {
            previous.sa_sigaction(signal, info, context);
            return;
        }
        if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN && previous.sa_handler != nullptr) {
            previous.sa_handler(signal);
            return;
        }
        ::signal(signal, SIG_DFL);
        if (previous.sa_handler == SIG_DFL) ::raise(signal);
    }

    bool record(std::uintptr_t address) {
        const auto page = pageStart(address);
        const FaultLock lock(_lock);
        auto it = _ranges.upper_bound(page);
        if (it == _ranges.begin()) return false;
        --it;
        if (page >= it->second.end) return false;
        auto& pages = it->second.pages;
        const auto index = static_cast<std::size_t>((page - it->first) / _page);
        auto& state = pages[index];
        if ((state & Protected) != 0) {
            const auto protection = state & ProtectionMask;
            const auto writtenOpen = [&](std::size_t at) { return (pages[at] & (Written | Protected)) == Written && (pages[at] & ProtectionMask) == protection; };
            std::size_t count = 1;
            if (_faultAhead > 1 && index >= 2 && writtenOpen(index - 1) && writtenOpen(index - 2)) {
                const auto blockPages = static_cast<std::size_t>(((page | (FaultAheadBlock - 1)) + 1 - page) / _page);
                const auto limit = std::min({_faultAhead, blockPages, pages.size() - index});
                while (count < limit && (pages[index + count] & (Protected | ReadTrapped)) == Protected && (pages[index + count] & ProtectionMask) == protection) ++count;
            }
            if (::mprotect(reinterpret_cast<void*>(page), count * _page, protection) != 0) return false;
            for (std::size_t at = index; at < index + count; ++at) pages[at] = static_cast<std::uint8_t>((pages[at] & (ProtectionMask | Fresh)) | Written);
            return true;
        }
        if ((state & PROT_WRITE) == 0) return false;
        std::uint8_t value = 0;
        mach_vm_size_t copied = 0;
        return mach_vm_read_overwrite(mach_task_self(), address, sizeof(value), reinterpret_cast<mach_vm_address_t>(&value), &copied) == KERN_SUCCESS && copied == sizeof(value);
    }

    bool resolveTrap(std::uintptr_t address, bool write) {
        if (!_trapping.load(std::memory_order_acquire)) return false;
        const auto page = pageStart(address);
        {
            const FaultLock lock(_lock);
            auto* state = pageState(page);
            if (state == nullptr) return false;
            if ((*state & ReadTrapped) == 0) return retryUntrapped(page, *state, write);
            const bool own = resolvingCovers(page, page + _page, Owner::Self) || pthread_equal(pthread_self(), _resolverThread);
            const auto bypass = _bypass.load(std::memory_order_acquire);
            if (own || (bypass != nullptr && bypass()) || (!trapCovers(page, page + _page) && !resolvingCovers(page, page + _page, Owner::Any))) {
                if (own) deferCovering(page);
                *state = static_cast<std::uint8_t>(*state & ~ReadTrapped);
                ++_untrapGeneration;
                return ::mprotect(reinterpret_cast<void*>(page), _page, untrappedProtection(*state)) == 0;
            }
        }
        Slot* slot = nullptr;
        while (slot == nullptr) {
            for (auto& candidate : _slots) {
                unsigned expected = Free;
                if (candidate.state.compare_exchange_strong(expected, Claimed, std::memory_order_acq_rel)) {
                    slot = &candidate;
                    break;
                }
            }
            if (slot == nullptr) sched_yield();
        }
        slot->page = page;
        slot->state.store(Queued, std::memory_order_release);
        semaphore_signal(_wake);
        while (semaphore_wait(slot->done) == KERN_ABORTED) {}
        slot->state.store(Free, std::memory_order_release);
        return true;
    }

    void startResolver() {
        _resolving.reserve(ResolvingCapacity);
        _deferred.reserve(ResolvingCapacity);
        if (semaphore_create(mach_task_self(), &_wake, SYNC_POLICY_FIFO, 0) != KERN_SUCCESS) return;
        for (auto& slot : _slots)
            if (semaphore_create(mach_task_self(), &slot.done, SYNC_POLICY_FIFO, 0) != KERN_SUCCESS) return;
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
        const bool created = pthread_create(&_resolverThread, &attributes, [](void* watch) -> void* {
            static_cast<Watch*>(watch)->resolveLoop();
            return nullptr;
        }, this) == 0;
        pthread_attr_destroy(&attributes);
        _resolverRunning.store(created, std::memory_order_release);
    }

    void resolveLoop() {
        pthread_setname_np("write watch read traps");
        for (;;) {
            if (semaphore_wait(_wake) != KERN_SUCCESS) continue;
            for (auto& slot : _slots) {
                unsigned expected = Queued;
                if (!slot.state.compare_exchange_strong(expected, Running, std::memory_order_acq_rel)) continue;
                const auto page = slot.page;
                waitForResolving(page, page + _page);
                resolve(page, page + _page);
                {
                    const Lock lock(_lock);
                    untrap(page, page + _page);
                }
                slot.state.store(Done, std::memory_order_release);
                semaphore_signal(slot.done);
            }
        }
    }

    void resolve(std::uintptr_t begin, std::uintptr_t end) {
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> taken;
        const auto self = pthread_self();
        {
            const Lock lock(_lock);
            taken = takeTraps(begin, end);
            for (const auto& [first, last] : taken) _resolving.push_back({first, last, self});
        }
        if (taken.empty()) return;
        const auto resolver = _resolver.load(std::memory_order_acquire);
        for (const auto& [first, last] : taken) resolver(first, last);
        for (;;) {
            std::pair<std::uintptr_t, std::uintptr_t> deferred;
            {
                const Lock lock(_lock);
                const auto found = std::find_if(_deferred.begin(), _deferred.end(), [&](const Resolving& entry) { return pthread_equal(entry.owner, self); });
                if (found == _deferred.end()) break;
                deferred = {found->begin, found->end};
                _deferred.erase(found);
                auto [first, last] = _traps.equal_range(deferred.first);
                while (first != last && first->second != deferred.second) ++first;
                if (first == last) {
                    const auto entry = std::find_if(_resolving.begin(), _resolving.end(), [&](const Resolving& resolving) { return resolving.begin == deferred.first && resolving.end == deferred.second && pthread_equal(resolving.owner, self); });
                    if (entry != _resolving.end()) _resolving.erase(entry);
                    continue;
                }
                _traps.erase(first);
            }
            taken.push_back(deferred);
            resolver(deferred.first, deferred.second);
        }
        const Lock lock(_lock);
        for (const auto& [first, last] : taken) {
            const auto found = std::find_if(_resolving.begin(), _resolving.end(), [&](const Resolving& entry) { return entry.begin == first && entry.end == last && pthread_equal(entry.owner, self); });
            if (found != _resolving.end()) _resolving.erase(found);
        }
        for (const auto& [first, last] : taken) untrap(first, last);
    }

    void deferCovering(std::uintptr_t page) {
        const auto self = pthread_self();
        for (auto it = _traps.begin(); it != _traps.end() && it->first < page + _page; ++it) {
            if (it->second <= page || _deferred.size() == _deferred.capacity() || _resolving.size() == _resolving.capacity()) continue;
            if (std::any_of(_deferred.begin(), _deferred.end(), [&](const Resolving& entry) { return entry.begin == it->first && entry.end == it->second; })) continue;
            _resolving.push_back({it->first, it->second, self});
            _deferred.push_back({it->first, it->second, self});
        }
    }

    void waitForResolving(std::uintptr_t begin, std::uintptr_t end) {
        for (;;) {
            {
                const Lock lock(_lock);
                if (!resolvingCovers(begin, end, Owner::Others)) return;
            }
            sched_yield();
        }
    }

    bool resolvingCovers(std::uintptr_t begin, std::uintptr_t end, Owner owner) const {
        const auto self = pthread_self();
        return std::any_of(_resolving.begin(), _resolving.end(), [&](const Resolving& entry) {
            if (pageStart(entry.begin) >= end || begin >= pageEnd(entry.end)) return false;
            return owner == Owner::Any || (pthread_equal(entry.owner, self) != 0) == (owner == Owner::Self);
        });
    }

    bool retryUntrapped(std::uintptr_t page, std::uint8_t state, bool write) {
        if ((untrappedProtection(state) & (write ? PROT_WRITE : PROT_READ)) == 0) return false;
        if (_retryGeneration != _untrapGeneration) {
            _retryGeneration = _untrapGeneration;
            _generationRetries = 0;
        }
        if (++_generationRetries > GenerationRetries) return false;
        const auto thread = pthread_self();
        auto found = std::find_if(_retries.begin(), _retries.end(), [&](const Retry& retry) { return retry.used != 0 && retry.page == page && pthread_equal(retry.thread, thread); });
        if (found == _retries.end()) found = std::min_element(_retries.begin(), _retries.end(), [](const Retry& left, const Retry& right) { return left.used < right.used; });
        if (found->used == 0 || found->page != page || !pthread_equal(found->thread, thread) || found->generation != _untrapGeneration) *found = {page, thread, _untrapGeneration, 0, 0};
        found->used = ++_retryUses;
        return ++found->count <= UntrappedRetries;
    }

    template <typename TFunction>
    std::invoke_result_t<TFunction> withoutTraps(std::uintptr_t begin, std::uintptr_t end, bool restore, TFunction function) {
        for (;;) {
            if (_trapping.load(std::memory_order_acquire)) {
                waitForResolving(begin, end);
                resolve(begin, end);
            }
            const Lock lock(_lock);
            if (trapCovers(pageStart(begin), pageEnd(end)) || resolvingCovers(pageStart(begin), pageEnd(end), Owner::Others)) continue;
            if (restore) untrap(begin, end);
            else forEachPage(pageStart(begin), pageEnd(end), [](std::uintptr_t, std::uint8_t& state) { state = static_cast<std::uint8_t>(state & ~ReadTrapped); });
            return function();
        }
    }

    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> takeTraps(std::uintptr_t begin, std::uintptr_t end) {
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> taken;
        const auto first = pageStart(begin);
        const auto last = pageEnd(end);
        for (auto it = _traps.begin(); it != _traps.end();) {
            if (it->first >= last || it->second <= first) {
                ++it;
                continue;
            }
            taken.emplace_back(*it);
            it = _traps.erase(it);
        }
        return taken;
    }

    void dropTraps(std::uintptr_t begin, std::uintptr_t end) {
        for (const auto& [first, last] : takeTraps(begin, end)) untrap(first, last);
        untrap(begin, end);
    }

    bool trapCovers(std::uintptr_t begin, std::uintptr_t end) const {
        return std::any_of(_traps.begin(), _traps.end(), [&](const auto& trap) { return trap.first < end && begin < trap.second; });
    }

    void untrap(std::uintptr_t begin, std::uintptr_t end) {
        const auto first = pageStart(begin);
        const auto last = pageEnd(end);
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> remaining;
        for (const auto& trap : _traps)
            if (trap.first < last && first < trap.second) remaining.push_back(trap);
        forEachPage(first, last, [&](std::uintptr_t page, std::uint8_t& state) {
            if ((state & ReadTrapped) == 0) return;
            if (std::any_of(remaining.begin(), remaining.end(), [&](const auto& trap) { return trap.first < page + _page && page < trap.second; })) return;
            if (resolvingCovers(page, page + _page, Owner::Others)) return;
            state = static_cast<std::uint8_t>(state & ~ReadTrapped);
            ::mprotect(reinterpret_cast<void*>(page), _page, untrappedProtection(state));
            ++_untrapGeneration;
        });
    }

    static int untrappedProtection(std::uint8_t state) {
        return (state & Protected) != 0 ? (state & ProtectionMask & ~PROT_WRITE) : (state & ProtectionMask);
    }

    std::uint8_t* pageState(std::uintptr_t page) {
        auto it = _ranges.upper_bound(page);
        if (it == _ranges.begin()) return nullptr;
        --it;
        if (page >= it->second.end) return nullptr;
        return &it->second.pages[(page - it->first) / _page];
    }

    std::uintptr_t pageStart(std::uintptr_t address) const {
        return address & ~(_page - 1);
    }

    std::uintptr_t pageEnd(std::uintptr_t address) const {
        return (address + _page - 1) & ~(_page - 1);
    }

    template <typename TFunction>
    void forEachPage(std::uintptr_t begin, std::uintptr_t end, TFunction function) {
        auto it = _ranges.upper_bound(begin);
        if (it != _ranges.begin()) --it;
        for (; it != _ranges.end() && it->first < end; ++it) {
            const auto first = std::max(begin, it->first);
            const auto last = std::min(end, it->second.end);
            for (auto page = first; page < last; page += _page) function(page, it->second.pages[(page - it->first) / _page]);
        }
    }

    void protect(std::uintptr_t begin, std::uintptr_t end) {
        std::uintptr_t runStart = 0;
        std::uintptr_t previous = begin;
        int runProtection = -1;
        const auto flush = [&](std::uintptr_t runEnd) {
            if (runProtection >= 0) ::mprotect(reinterpret_cast<void*>(runStart), runEnd - runStart, runProtection);
            runProtection = -1;
        };
        forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
            const int wanted = (state & ReadTrapped) != 0 ? PROT_NONE : (state & Protected) != 0 ? (state & ProtectionMask & ~PROT_WRITE) : -1;
            if (wanted != runProtection || page != previous) {
                flush(previous);
                runStart = page;
                runProtection = wanted;
            }
            previous = page + _page;
        });
        flush(previous);
    }

    bool covers(std::uintptr_t begin, std::uintptr_t end) const {
        auto it = _ranges.upper_bound(begin);
        if (it == _ranges.begin()) return false;
        --it;
        auto cursor = begin;
        for (; it != _ranges.end() && it->first <= cursor; ++it) {
            if (it->second.end > cursor) cursor = it->second.end;
            if (cursor >= end) return true;
        }
        return false;
    }

    bool erase(std::uintptr_t begin, std::uintptr_t end) {
        bool removed = false;
        auto it = _ranges.upper_bound(begin);
        if (it != _ranges.begin()) --it;
        while (it != _ranges.end() && it->first < end) {
            const auto rangeBegin = it->first;
            const auto rangeEnd = it->second.end;
            if (rangeEnd <= begin) {
                ++it;
                continue;
            }
            removed = true;
            auto pages = std::move(it->second.pages);
            it = _ranges.erase(it);
            const auto cutBegin = std::max(begin, rangeBegin);
            const auto cutEnd = std::min(end, rangeEnd);
            for (auto page = cutBegin; page < cutEnd; page += _page) {
                const auto state = pages[(page - rangeBegin) / _page];
                if ((state & (Protected | ReadTrapped)) != 0) ::mprotect(reinterpret_cast<void*>(page), _page, state & ProtectionMask);
            }
            if (rangeBegin < cutBegin) _ranges.emplace(rangeBegin, Range{cutBegin, std::vector<std::uint8_t>(pages.begin(), pages.begin() + (cutBegin - rangeBegin) / _page)});
            if (cutEnd < rangeEnd) it = _ranges.emplace(cutEnd, Range{rangeEnd, std::vector<std::uint8_t>(pages.begin() + (cutEnd - rangeBegin) / _page, pages.end())}).first;
        }
        return removed;
    }

    const std::uintptr_t _page = static_cast<std::uintptr_t>(::getpagesize());
    std::size_t _faultAhead = 32;
    bool _available = false;
    Mutex _lock;
    std::map<std::uintptr_t, Range> _ranges;
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> _hostWrites;
    std::multimap<std::uintptr_t, std::uintptr_t> _traps;
    std::vector<Resolving> _resolving;
    std::vector<Resolving> _deferred;
    std::array<Retry, RetrySlots> _retries{};
    std::uint64_t _retryUses = 0;
    std::uint64_t _retryGeneration = 0;
    unsigned _generationRetries = 0;
    std::uint64_t _untrapGeneration = 0;
    std::atomic<bool> _trapping{false};
    std::atomic<void (*)(std::uintptr_t, std::uintptr_t)> _resolver{nullptr};
    std::atomic<bool (*)()> _bypass{nullptr};
    std::once_flag _resolverStarted;
    std::atomic<bool> _resolverRunning{false};
    pthread_t _resolverThread{};
    semaphore_t _wake = MACH_PORT_NULL;
    std::array<Slot, ResolveSlots> _slots;
    struct sigaction _previousBus {};
    struct sigaction _previousSegv {};
};

const bool g_opened = (Watch::Get(), true);
#endif

}

bool GuestWriteWatchAvailable_nid_postfix() {
#ifdef APS5_GUEST_WRITE_WATCH
    return Watch::Get().Available();
#else
    return false;
#endif
}

void GuestWriteWatchRegister_nid_postfix(const void* pointer, std::size_t bytes) {
#ifdef APS5_GUEST_WRITE_WATCH
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    Watch::Get().Register(begin, begin + bytes);
#if defined(APS5_LINUX_READ_TRAPS)
    if (Watch::Get().Covers(begin, begin + bytes)) ReadTraps::Get().Registered(begin, begin + bytes);
#endif
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
#endif
}

bool GuestWriteWatchUnregister_nid_postfix(const void* pointer, std::size_t bytes) {
#ifdef APS5_GUEST_WRITE_WATCH
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
#if defined(APS5_LINUX_READ_TRAPS)
    ReadTraps::Get().Forget(begin, begin + bytes);
#endif
    return Watch::Get().Unregister(begin, begin + bytes);
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
    return false;
#endif
}

bool GuestWriteWatchAlias_nid_postfix(const void* first, const void* second, std::size_t bytes) {
#if defined(__linux__) && defined(PAGEMAP_SCAN) && defined(UFFD_FEATURE_WP_ASYNC)
    return Watch::Get().Alias(reinterpret_cast<std::uintptr_t>(first), reinterpret_cast<std::uintptr_t>(second), bytes);
#else
    static_cast<void>(first);
    static_cast<void>(second);
    static_cast<void>(bytes);
    return false;
#endif
}

bool GuestWriteWatchCovers_nid_postfix(std::uintptr_t address, std::size_t bytes) {
#ifdef APS5_GUEST_WRITE_WATCH
    return bytes != 0 && address + bytes > address && Watch::Get().Covers(address, address + bytes);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    return false;
#endif
}

bool GuestWriteWatchOverlaps_nid_postfix(std::uintptr_t address, std::size_t bytes) {
#ifdef APS5_GUEST_WRITE_WATCH
    return bytes != 0 && address + bytes > address && Watch::Get().Overlaps(address, address + bytes);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    return false;
#endif
}

bool GuestWriteWatchHandleFault_nid_postfix(std::uintptr_t address) {
#if defined(__APPLE__)
    return Watch::Get().HandleFault(address);
#elif defined(APS5_LINUX_READ_TRAPS)
    return ReadTraps::Get().HandleFault(address);
#else
    static_cast<void>(address);
    return false;
#endif
}

bool GuestWriteWatchCollect_nid_postfix(std::uintptr_t address, std::size_t bytes, void (*written)(void* context, std::uintptr_t begin, std::uintptr_t end), void* context) {
#ifdef APS5_GUEST_WRITE_WATCH
    if (bytes == 0 || address + bytes < address) return false;
    return Watch::Get().Collect(address, address + bytes, written, context);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    static_cast<void>(written);
    static_cast<void>(context);
    return false;
#endif
}

void GuestWriteWatchBeginHostWrite_nid_postfix(const void* pointer, std::size_t bytes) {
#if defined(__APPLE__)
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    Watch::Get().BeginHostWrite(begin, begin + bytes);
#elif defined(APS5_LINUX_READ_TRAPS)
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    ReadTraps::Get().BeginHostWrite(begin, begin + bytes);
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
#endif
}

void GuestWriteWatchEndHostWrite_nid_postfix(const void* pointer, std::size_t bytes) {
#if defined(__APPLE__)
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    Watch::Get().EndHostWrite(begin, begin + bytes);
#elif defined(APS5_LINUX_READ_TRAPS)
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    ReadTraps::Get().EndHostWrite(begin, begin + bytes);
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
#endif
}

void GuestWriteWatchProtectionChanged_nid_postfix(const void* pointer, std::size_t bytes, int protection) {
#if defined(__APPLE__)
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    Watch::Get().ProtectionChanged(begin, begin + bytes, protection);
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
    static_cast<void>(protection);
#endif
}

int GuestWriteWatchProtect_nid_postfix(void* pointer, std::size_t bytes, int protection) {
#if defined(__APPLE__)
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    return Watch::Get().Protect(begin, begin + bytes, protection);
#elif defined(__linux__)
#if defined(APS5_LINUX_READ_TRAPS)
    if (Watch::Get().Available()) {
        const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
        return ReadTraps::Get().Protect(begin, begin + bytes, protection);
    }
#endif
    return ::mprotect(pointer, bytes, protection) == 0 ? 0 : errno;
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
    static_cast<void>(protection);
    return ENOSYS;
#endif
}

int GuestWriteWatchProtection_nid_postfix(std::uintptr_t address, std::uintptr_t limit, std::uintptr_t* runEnd) {
#if defined(__APPLE__)
    return Watch::Get().Protection(address, limit, runEnd);
#else
    static_cast<void>(address);
    *runEnd = limit;
    return -1;
#endif
}

bool GuestWriteWatchCollectFresh_nid_postfix(std::uintptr_t address, std::size_t bytes, void (*written)(void* context, std::uintptr_t begin, std::uintptr_t end), void* context) {
#if defined(__linux__) && defined(PAGEMAP_SCAN) && defined(UFFD_FEATURE_WP_ASYNC)
    if (bytes == 0 || address + bytes < address) return false;
    return Watch::Get().CollectFresh(address, address + bytes, written, context);
#elif defined(__APPLE__)
    if (bytes == 0 || address + bytes < address) return false;
    return Watch::Get().Collect(address, address + bytes, written, context, Watch::Take::Fresh);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    static_cast<void>(written);
    static_cast<void>(context);
    return false;
#endif
}

bool GuestWriteWatchCollectArmed_nid_postfix(std::uintptr_t address, std::size_t bytes, void (*written)(void* context, std::uintptr_t begin, std::uintptr_t end), void* context) {
#if defined(__linux__) && defined(PAGEMAP_SCAN) && defined(UFFD_FEATURE_WP_ASYNC)
    if (bytes == 0 || address + bytes < address) return false;
    return Watch::Get().CollectArmed(address, address + bytes, written, context);
#elif defined(__APPLE__)
    if (bytes == 0 || address + bytes < address) return false;
    return Watch::Get().Collect(address, address + bytes, written, context, Watch::Take::Armed);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    static_cast<void>(written);
    static_cast<void>(context);
    return false;
#endif
}

bool GuestWriteWatchSetReadTrapResolver_nid_postfix(void (*resolve)(std::uintptr_t begin, std::uintptr_t end)) {
#if defined(__APPLE__)
    return Watch::Get().SetReadTrapResolver(resolve);
#elif defined(APS5_LINUX_READ_TRAPS)
    return ReadTraps::Get().SetResolver(resolve);
#else
    static_cast<void>(resolve);
    return false;
#endif
}

void GuestWriteWatchSetReadTrapBypass_nid_postfix(bool (*bypass)()) {
#if defined(__APPLE__)
    Watch::Get().SetReadTrapBypass(bypass);
#elif defined(APS5_LINUX_READ_TRAPS)
    ReadTraps::Get().SetBypass(bypass);
#else
    static_cast<void>(bypass);
#endif
}

bool GuestWriteWatchTrapReads_nid_postfix(std::uintptr_t address, std::size_t bytes) {
#if defined(__APPLE__)
    return bytes != 0 && address + bytes > address && Watch::Get().TrapReads(address, address + bytes);
#elif defined(APS5_LINUX_READ_TRAPS)
    return bytes != 0 && address + bytes > address && ReadTraps::Get().TrapReads(address, address + bytes);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    return false;
#endif
}

void GuestWriteWatchReadTraps_nid_postfix(std::uintptr_t address, std::size_t bytes, void (*trapped)(void* context, std::uintptr_t begin, std::uintptr_t end), void* context) {
#if defined(__APPLE__)
    if (bytes != 0 && address + bytes > address) Watch::Get().ReadTraps(address, address + bytes, trapped, context);
#elif defined(APS5_LINUX_READ_TRAPS)
    if (bytes != 0 && address + bytes > address) ReadTraps::Get().Trapped(address, address + bytes, trapped, context);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
    static_cast<void>(trapped);
    static_cast<void>(context);
#endif
}

void GuestWriteWatchReleaseReads_nid_postfix(std::uintptr_t address, std::size_t bytes) {
#if defined(__APPLE__)
    if (bytes != 0 && address + bytes > address) Watch::Get().ReleaseReads(address, address + bytes);
#elif defined(APS5_LINUX_READ_TRAPS)
    if (bytes != 0 && address + bytes > address) ReadTraps::Get().ReleaseReads(address, address + bytes);
#else
    static_cast<void>(address);
    static_cast<void>(bytes);
#endif
}

}
