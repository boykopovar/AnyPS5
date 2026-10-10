#include "prx/libc/include/GuestWriteWatch.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/userfaultfd.h>
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
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#if (defined(__linux__) && defined(PAGEMAP_SCAN) && defined(UFFD_FEATURE_WP_ASYNC)) || defined(__APPLE__)
#define APS5_GUEST_WRITE_WATCH 1
#endif

namespace GuestWriteWatch {
namespace {

#if defined(__linux__) && defined(PAGEMAP_SCAN) && defined(UFFD_FEATURE_WP_ASYNC)
constexpr std::uintptr_t PageBytes = 4096;

class Watch {
public:
    static Watch& Get() {
        static Watch watch;
        return watch;
    }

    bool Available() const {
        return _pagemap >= 0;
    }

    void Register(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available() || end <= begin) return;
        std::unique_lock lock(_lock);
        remove(_ranges, begin, end);
        remove(_fresh, begin, end);
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
        return watched;
    }

    bool Covers(std::uintptr_t begin, std::uintptr_t end) {
        if (!Available()) return false;
        std::shared_lock lock(_lock);
        return covers(begin, end);
    }

    bool Collect(std::uintptr_t begin, std::uintptr_t end, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        if (!Available()) return false;
        begin &= ~(PageBytes - 1);
        end = (end + PageBytes - 1) & ~(PageBytes - 1);
        if (end <= begin) return true;
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> fresh;
        {
            std::unique_lock lock(_lock);
            if (!covers(begin, end)) return false;
            auto it = _fresh.upper_bound(begin);
            if (it != _fresh.begin()) --it;
            for (; it != _fresh.end() && it->first < end; ++it) {
                const auto from = std::max(it->first, begin);
                const auto to = std::min(it->second, end);
                if (from < to) fresh.emplace_back(from, to);
            }
            if (!fresh.empty()) remove(_fresh, begin, end);
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
            written(context, begin, end);
            return false;
        }
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
};

const bool g_opened = (Watch::Get(), true);
#elif defined(__APPLE__)
class Watch {
public:
    static Watch& Get() {
        static Watch watch;
        return watch;
    }

    bool Available() const {
        return _available;
    }

    void Register(std::uintptr_t begin, std::uintptr_t end) {
        begin = pageStart(begin);
        end = pageEnd(end);
        if (!_available || end <= begin) return;
        std::vector<std::uint8_t> pages((end - begin) / _page, Written);
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
            for (auto page = first; page < last; page += _page) pages[(page - begin) / _page] = Written | protection;
            cursor = last;
        }
        const Lock lock(_lock);
        forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
            pages[(page - begin) / _page] = static_cast<std::uint8_t>(Written | (state & ProtectionMask));
        });
        erase(begin, end);
        _ranges.emplace(begin, Range{end, std::move(pages)});
    }

    bool Unregister(std::uintptr_t begin, std::uintptr_t end) {
        begin = pageStart(begin);
        end = pageEnd(end);
        if (!_available || end <= begin) return false;
        const Lock lock(_lock);
        return erase(begin, end);
    }

    bool Covers(std::uintptr_t begin, std::uintptr_t end) {
        if (!_available) return false;
        const Lock lock(_lock);
        return covers(begin, end);
    }

    bool Collect(std::uintptr_t begin, std::uintptr_t end, void (*written)(void*, std::uintptr_t, std::uintptr_t), void* context) {
        if (!_available) return false;
        begin = pageStart(begin);
        end = pageEnd(end);
        if (end <= begin) return true;
        std::vector<std::pair<std::uintptr_t, std::uintptr_t>> runs;
        {
            const Lock lock(_lock);
            if (!covers(begin, end)) return false;
            forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
                if ((state & Written) == 0) return;
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
        begin = pageStart(begin);
        end = pageEnd(end);
        if (!_available || end <= begin) return;
        const Lock lock(_lock);
        forEachPage(begin, end, [&](std::uintptr_t, std::uint8_t& state) {
            state = static_cast<std::uint8_t>(Written | (protection & ProtectionMask));
        });
    }

    void BeginHostWrite(std::uintptr_t begin, std::uintptr_t end) {
        begin = pageStart(begin);
        end = pageEnd(end);
        if (!_available || end <= begin) return;
        const Lock lock(_lock);
        _hostWrites.emplace_back(begin, end);
        forEachPage(begin, end, [&](std::uintptr_t page, std::uint8_t& state) {
            if ((state & Protected) != 0) ::mprotect(reinterpret_cast<void*>(page), _page, state & ProtectionMask);
            state = static_cast<std::uint8_t>((state & ProtectionMask) | Written);
        });
    }

    void EndHostWrite(std::uintptr_t begin, std::uintptr_t end) {
        begin = pageStart(begin);
        end = pageEnd(end);
        if (!_available || end <= begin) return;
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

private:
    static constexpr std::uint8_t ProtectionMask = PROT_READ | PROT_WRITE | PROT_EXEC;
    static constexpr std::uint8_t Written = 0x10;
    static constexpr std::uint8_t Protected = 0x20;

    struct Range {
        std::uintptr_t end;
        std::vector<std::uint8_t> pages;
    };

    struct Turns {
        std::atomic<std::uint64_t> next{0};
        std::atomic<std::uint64_t> serving{0};
    };

    struct Lock {
        explicit Lock(Turns& turns) : _turns(turns), _turn(turns.next.fetch_add(1, std::memory_order_relaxed)) {
            while (_turns.serving.load(std::memory_order_acquire) != _turn) ::sched_yield();
        }
        ~Lock() {
            _turns.serving.store(_turn + 1, std::memory_order_release);
        }
        Turns& _turns;
        std::uint64_t _turn;
    };

    Watch() {
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

    static void fault(int signal, siginfo_t* info, void* context) {
        auto& watch = Get();
        if (watch.record(reinterpret_cast<std::uintptr_t>(info->si_addr))) return;
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
    }

    bool record(std::uintptr_t address) {
        const auto page = pageStart(address);
        const Lock lock(_lock);
        auto it = _ranges.upper_bound(page);
        if (it == _ranges.begin()) return false;
        --it;
        if (page >= it->second.end) return false;
        auto& state = it->second.pages[(page - it->first) / _page];
        if ((state & Protected) != 0) {
            state = static_cast<std::uint8_t>((state & ProtectionMask) | Written);
            ::mprotect(reinterpret_cast<void*>(page), _page, state & ProtectionMask);
            return true;
        }
        return (state & PROT_WRITE) != 0;
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
            const int wanted = (state & Protected) != 0 ? (state & ProtectionMask & ~PROT_WRITE) : -1;
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
                if ((state & Protected) != 0) ::mprotect(reinterpret_cast<void*>(page), _page, state & ProtectionMask);
            }
            if (rangeBegin < cutBegin) _ranges.emplace(rangeBegin, Range{cutBegin, std::vector<std::uint8_t>(pages.begin(), pages.begin() + (cutBegin - rangeBegin) / _page)});
            if (cutEnd < rangeEnd) it = _ranges.emplace(cutEnd, Range{rangeEnd, std::vector<std::uint8_t>(pages.begin() + (cutEnd - rangeBegin) / _page, pages.end())}).first;
        }
        return removed;
    }

    const std::uintptr_t _page = static_cast<std::uintptr_t>(::getpagesize());
    bool _available = false;
    Turns _lock;
    std::map<std::uintptr_t, Range> _ranges;
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> _hostWrites;
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
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
#endif
}

bool GuestWriteWatchUnregister_nid_postfix(const void* pointer, std::size_t bytes) {
#ifdef APS5_GUEST_WRITE_WATCH
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    return Watch::Get().Unregister(begin, begin + bytes);
#else
    static_cast<void>(pointer);
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
#else
    static_cast<void>(pointer);
    static_cast<void>(bytes);
#endif
}

void GuestWriteWatchEndHostWrite_nid_postfix(const void* pointer, std::size_t bytes) {
#if defined(__APPLE__)
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    Watch::Get().EndHostWrite(begin, begin + bytes);
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

int GuestWriteWatchProtection_nid_postfix(std::uintptr_t address, std::uintptr_t limit, std::uintptr_t* runEnd) {
#if defined(__APPLE__)
    return Watch::Get().Protection(address, limit, runEnd);
#else
    static_cast<void>(address);
    *runEnd = limit;
    return -1;
#endif
}

}
