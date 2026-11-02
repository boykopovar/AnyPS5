#include "prx/libc/include/GuestWriteWatch.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/ndr.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#if !defined(__APPLE__)
#error The page protection write watch is the macOS backend
#endif

using namespace GuestWriteWatch;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Region {
    explicit Region(std::size_t pageCount) : page(static_cast<std::size_t>(::getpagesize())), bytes(pageCount * page) {
        base = static_cast<std::uint8_t*>(::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));
        Require(base != MAP_FAILED, "cannot map the test region");
    }
    ~Region() {
        GuestWriteWatchUnregister_nid_postfix(base, bytes);
        ::munmap(base, bytes);
    }
    std::uintptr_t Address(std::size_t index) const {
        return reinterpret_cast<std::uintptr_t>(base) + index * page;
    }
    using Collector = bool (*)(std::uintptr_t, std::size_t, void (*)(void*, std::uintptr_t, std::uintptr_t), void*);
    std::set<std::size_t> Collect(Collector collector = &GuestWriteWatchCollect_nid_postfix, std::size_t first = 0, std::size_t count = 0) {
        std::set<std::size_t> pages;
        struct Sink {
            Region* region;
            std::set<std::size_t>* pages;
        } sink{this, &pages};
        const bool complete = collector(Address(first), count == 0 ? bytes - first * page : count * page, [](void* context, std::uintptr_t begin, std::uintptr_t end) {
            auto& target = *static_cast<Sink*>(context);
            for (auto at = begin; at < end; at += target.region->page) target.pages->insert((at - target.region->Address(0)) / target.region->page);
        }, &sink);
        Require(complete, "collection of a watched range is incomplete");
        return pages;
    }
    std::size_t page;
    std::size_t bytes;
    std::uint8_t* base = nullptr;
};

std::set<std::size_t> All(std::size_t count) {
    std::set<std::size_t> pages;
    for (std::size_t index = 0; index < count; ++index) pages.insert(index);
    return pages;
}

void CheckCollect() {
    Region region(8);
    Require(GuestWriteWatchAvailable_nid_postfix(), "write watch is unavailable");
    Require(!GuestWriteWatchCovers_nid_postfix(region.Address(0), region.bytes), "unregistered range is covered");
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(GuestWriteWatchCovers_nid_postfix(region.Address(0), region.bytes), "registered range is not covered");
    Require(region.Collect() == All(8), "a new range is not reported in full");
    Require(region.Collect().empty(), "an untouched range reports writes");
    region.base[3 * region.page + 17] = 1;
    region.base[5 * region.page] = 2;
    region.base[5 * region.page + 9] = 3;
    Require(region.Collect() == std::set<std::size_t>({3, 5}), "written pages are not reported exactly");
    Require(region.Collect().empty(), "collected writes are reported again");
    Require(region.base[3 * region.page + 17] == 1 && region.base[5 * region.page + 9] == 3, "watched writes were lost");
    std::memset(region.base + region.page - 4, 7, 8);
    Require(region.Collect() == std::set<std::size_t>({0, 1}), "a write across a page boundary is not reported on both pages");
}

void CheckFresh() {
    Region region(8);
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(region.Collect(&GuestWriteWatchCollect_nid_postfix, 0, 4) == All(4), "the collected half is not reported in full");
    region.base[region.page] = 1;
    Require(region.Collect(&GuestWriteWatchCollectFresh_nid_postfix) == std::set<std::size_t>({4, 5, 6, 7}), "a fresh collection does not report exactly the never collected pages");
    Require(region.Collect(&GuestWriteWatchCollectFresh_nid_postfix).empty(), "a fresh collection reports pages again");
    region.base[6 * region.page] = 1;
    Require(region.Collect() == std::set<std::size_t>({1, 6}), "a fresh collection does not arm the pages it reports");
    Require(region.Collect().empty(), "collected writes are reported again");
}

void CheckArmed() {
    Region region(8);
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(region.Collect(&GuestWriteWatchCollect_nid_postfix, 0, 4) == All(4), "the collected half is not reported in full");
    region.base[region.page] = 1;
    region.base[6 * region.page] = 1;
    Require(region.Collect(&GuestWriteWatchCollectArmed_nid_postfix) == std::set<std::size_t>({1}), "an armed collection does not report exactly the written armed pages");
    Require(region.Collect() == std::set<std::size_t>({4, 5, 6, 7}), "an armed collection takes never collected pages");
    Require(region.Collect().empty(), "collected writes are reported again");
}

void CheckRegisterAgain() {
    Region region(4);
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(region.Collect() == All(4), "a new range is not reported in full");
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(region.Collect() == All(4), "a range registered again is not reported in full");
    region.base[2 * region.page] = 1;
    Require(region.Collect() == std::set<std::size_t>({2}), "a range registered while protected stops reporting writes");
    GuestWriteWatchRegister_nid_postfix(region.base + region.page, region.page);
    Require(region.Collect() == std::set<std::size_t>({1}), "registering part of a range does not report that part alone");
    region.base[0] = 1;
    region.base[3 * region.page] = 1;
    Require(region.Collect() == std::set<std::size_t>({0, 3}), "the rest of a split range stops reporting writes");
}

int KernelProtection(const void* page) {
    mach_vm_address_t address = reinterpret_cast<mach_vm_address_t>(page);
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    Require(mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) == KERN_SUCCESS && address == reinterpret_cast<mach_vm_address_t>(page), "cannot read the kernel protection of a page");
    return ((info.protection & VM_PROT_READ) ? PROT_READ : 0) | ((info.protection & VM_PROT_WRITE) ? PROT_WRITE : 0) | ((info.protection & VM_PROT_EXECUTE) ? PROT_EXEC : 0);
}

void CheckRegisterMapped() {
    Region region(4);
    std::uintptr_t runEnd = 0;
    Require(::mprotect(region.base, region.bytes, PROT_NONE) == 0, "cannot reserve the test region");
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(::mmap(region.base, region.bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0) == region.base, "cannot map over the reservation");
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(GuestWriteWatchProtection_nid_postfix(region.Address(0), region.Address(4), &runEnd) == (PROT_READ | PROT_WRITE) && runEnd == region.Address(4), "a range mapped over its reservation keeps the reservation's protection");
    Require(region.Collect() == All(4), "a mapped range is not reported in full");
    region.base[2 * region.page] = 1;
    Require(region.Collect() == std::set<std::size_t>({2}), "a range mapped over its reservation stops reporting writes");
    Require(::mmap(region.base + region.page, region.page, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0) == region.base + region.page, "cannot reserve a watched page again");
    GuestWriteWatchRegister_nid_postfix(region.base + region.page, region.page);
    Require(GuestWriteWatchProtection_nid_postfix(region.Address(1), region.Address(2), &runEnd) == PROT_NONE, "a protected page reserved again reports its old protection");
    Require(KernelProtection(region.base + region.page) == PROT_NONE, "registering a protected page reserved again restored its old protection");
}

void CheckHostWrite() {
    Region region(4);
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(region.Collect() == All(4), "a new range is not reported in full");
    int descriptors[2];
    Require(::pipe(descriptors) == 0, "cannot create a pipe");
    const std::array<char, 8> payload{'w', 'a', 't', 'c', 'h', 'e', 'd', '!'};
    Require(::write(descriptors[1], payload.data(), payload.size()) == static_cast<ssize_t>(payload.size()), "cannot fill the pipe");
    errno = 0;
    Require(::read(descriptors[0], region.base + region.page, payload.size()) == -1 && errno == EFAULT, "a system call wrote a protected page without a host write");
    GuestWriteWatchBeginHostWrite_nid_postfix(region.base + region.page, payload.size());
    Require(region.Collect() == std::set<std::size_t>({1}), "a page under a host write is not reported");
    Require(::read(descriptors[0], region.base + region.page, payload.size()) == static_cast<ssize_t>(payload.size()), "a collection protected a page during a host write");
    GuestWriteWatchEndHostWrite_nid_postfix(region.base + region.page, payload.size());
    Require(std::memcmp(region.base + region.page, payload.data(), payload.size()) == 0, "the host write did not reach the page");
    Require(region.Collect() == std::set<std::size_t>({1}), "a host write is not reported after it ends");
    Require(region.Collect().empty(), "a finished host write is reported again");
    ::close(descriptors[0]);
    ::close(descriptors[1]);
}

void CheckEmptyRanges() {
    Region region(1);
    for (const auto offset : {std::size_t{0}, std::size_t{1}, region.page - 1}) {
        auto* pointer = region.base + offset;
        GuestWriteWatchRegister_nid_postfix(pointer, 0);
        Require(!GuestWriteWatchCovers_nid_postfix(region.Address(0), region.bytes), "an empty registration watches a page");
        GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
        Require(region.Collect() == All(1), "a new range is not reported in full");

        Require(!GuestWriteWatchUnregister_nid_postfix(pointer, 0), "an empty unregister removes a page");
        Require(GuestWriteWatchCovers_nid_postfix(region.Address(0), region.bytes), "an empty unregister changes coverage");
        GuestWriteWatchRegister_nid_postfix(pointer, 0);
        Require(region.Collect().empty(), "an empty registration marks a watched page fresh");

        GuestWriteWatchBeginHostWrite_nid_postfix(pointer, 0);
        Require(KernelProtection(region.base) == PROT_READ, "an empty host write unprotects a page");
        Require(region.Collect().empty(), "an empty host write reports a dirty page");
        GuestWriteWatchEndHostWrite_nid_postfix(pointer, 0);

        GuestWriteWatchBeginHostWrite_nid_postfix(pointer, 1);
        GuestWriteWatchEndHostWrite_nid_postfix(pointer, 0);
        Require(region.Collect() == All(1), "an active host write is not reported");
        Require(KernelProtection(region.base) == (PROT_READ | PROT_WRITE), "an empty end host write releases an active host write");
        GuestWriteWatchEndHostWrite_nid_postfix(pointer, 1);
        Require(region.Collect() == All(1), "a finished host write is not reported");
        Require(region.Collect().empty(), "a finished host write is reported again");

        GuestWriteWatchProtectionChanged_nid_postfix(pointer, 0, PROT_NONE);
        std::uintptr_t runEnd = 0;
        Require(GuestWriteWatchProtection_nid_postfix(region.Address(0), region.Address(1), &runEnd) == (PROT_READ | PROT_WRITE), "an empty protection change changes the recorded protection");
        Require(region.Collect().empty(), "an empty protection change reports a dirty page");
        region.base[offset] = 1;
        Require(region.Collect() == All(1), "an empty range operation prevents a later write from being tracked");
        Require(GuestWriteWatchUnregister_nid_postfix(region.base, region.bytes), "cannot unregister the test region");
    }
}

void CheckNonWriteFaults(bool execute) {
    Region region(1);
#if defined(__aarch64__)
    const std::uint32_t returnInstruction = 0xd65f03c0;
    std::memcpy(region.base, &returnInstruction, sizeof(returnInstruction));
#elif defined(__x86_64__)
    region.base[0] = 0xc3;
#endif
    for (const bool armed : {false, true}) {
        for (const bool hostWrite : {false, true}) {
            const auto child = ::fork();
            Require(child >= 0, "cannot fork a faulting child");
            if (child == 0) {
                ::alarm(2);
                if (!execute) {
                    char path[] = "/tmp/anyps5-write-watch-XXXXXX";
                    const int file = ::mkstemp(path);
                    if (file < 0) ::_exit(2);
                    ::unlink(path);
                    if (::ftruncate(file, region.bytes) != 0) ::_exit(2);
                    if (::mmap(region.base, region.bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, file, 0) != region.base) ::_exit(2);
                    if (::ftruncate(file, 0) != 0) ::_exit(2);
                    ::close(file);
                }
                GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
                if (armed) static_cast<void>(region.Collect());
                if (hostWrite) GuestWriteWatchBeginHostWrite_nid_postfix(region.base, region.bytes);
                if (execute) reinterpret_cast<void (*)()>(region.base)();
                else {
                    const volatile auto value = *static_cast<volatile std::uint8_t*>(region.base);
                    static_cast<void>(value);
                }
                ::_exit(3);
            }
            int status = 0;
            Require(::waitpid(child, &status, 0) == child, "cannot wait for a faulting child");
            Require(WIFSIGNALED(status) && (WTERMSIG(status) == SIGBUS || WTERMSIG(status) == SIGSEGV), execute ? "a non-executable watched page did not deliver its fault" : "a watched mapping with missing backing did not deliver its read fault");
        }
    }
}

void CheckProtection() {
    Region region(6);
    std::uintptr_t runEnd = 0;
    Require(GuestWriteWatchProtection_nid_postfix(region.Address(0), region.Address(6), &runEnd) == -1 && runEnd == region.Address(6), "an unwatched range reports a protection");
    GuestWriteWatchRegister_nid_postfix(region.base + 2 * region.page, 3 * region.page);
    Require(GuestWriteWatchProtection_nid_postfix(region.Address(0), region.Address(6), &runEnd) == -1 && runEnd == region.Address(2), "the run before a watched range does not end at it");
    static_cast<void>(GuestWriteWatchCollect_nid_postfix(region.Address(2), 3 * region.page, [](void*, std::uintptr_t, std::uintptr_t) {}, nullptr));
    Require(GuestWriteWatchProtection_nid_postfix(region.Address(2), region.Address(6), &runEnd) == (PROT_READ | PROT_WRITE) && runEnd == region.Address(5), "a protected page does not report the protection it was given");
    Require(::mprotect(region.base + 3 * region.page, region.page, PROT_READ) == 0, "cannot change the protection");
    GuestWriteWatchProtectionChanged_nid_postfix(region.base + 3 * region.page, region.page, PROT_READ);
    Require(GuestWriteWatchProtection_nid_postfix(region.Address(2), region.Address(6), &runEnd) == (PROT_READ | PROT_WRITE) && runEnd == region.Address(3), "a run does not end where the protection changes");
    Require(GuestWriteWatchProtection_nid_postfix(region.Address(3), region.Address(6), &runEnd) == PROT_READ && runEnd == region.Address(4), "a changed protection is not reported");
    std::set<std::size_t> pages;
    static_cast<void>(GuestWriteWatchCollect_nid_postfix(region.Address(2), 3 * region.page, [](void* context, std::uintptr_t begin, std::uintptr_t) { static_cast<std::set<std::size_t>*>(context)->insert(begin); }, &pages));
    Require(pages == std::set<std::size_t>({region.Address(3)}), "a protection change is not reported as a write");
    Require(::mprotect(region.base + 3 * region.page, region.page, PROT_READ | PROT_WRITE) == 0, "cannot restore the protection");
    GuestWriteWatchProtectionChanged_nid_postfix(region.base + 3 * region.page, region.page, PROT_READ | PROT_WRITE);
    region.base[3 * region.page] = 1;
    GuestWriteWatchUnregister_nid_postfix(region.base + 2 * region.page, 3 * region.page);
    region.base[2 * region.page] = 1;
    region.base[4 * region.page] = 1;
    Require(!GuestWriteWatchCovers_nid_postfix(region.Address(2), region.page), "an unregistered range is covered");

    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    static_cast<void>(region.Collect());
    Require(GuestWriteWatchProtect_nid_postfix(region.base, region.page, PROT_READ) == 0, "cannot protect a watched page");
    Require(KernelProtection(region.base) == PROT_READ, "a protect does not change the host protection");
    Require(GuestWriteWatchProtection_nid_postfix(region.Address(0), region.Address(6), &runEnd) == PROT_READ && runEnd == region.Address(1), "a protect is not recorded");
    Require(region.Collect() == std::set<std::size_t>({0}), "a protect is not reported as a write");
    Require(GuestWriteWatchProtect_nid_postfix(region.base, region.page, PROT_READ | PROT_WRITE) == 0, "cannot restore a watched page");
    static_cast<void>(region.Collect());
    region.base[0] = 1;
    Require(region.Collect() == std::set<std::size_t>({0}), "a write after a protect is not reported");
    Require(GuestWriteWatchProtect_nid_postfix(region.base + 1, region.page, PROT_READ) == EINVAL, "a misaligned protect does not fail with EINVAL");
}

void CheckOverlaps() {
    Region region(6);
    Require(!GuestWriteWatchOverlaps_nid_postfix(region.Address(0), region.bytes), "an unregistered range overlaps the watch");
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(GuestWriteWatchUnregister_nid_postfix(region.base, region.page), "cannot unregister the first page");
    Require(GuestWriteWatchUnregister_nid_postfix(region.base + 3 * region.page, 3 * region.page), "cannot unregister the last pages");
    Require(!GuestWriteWatchCovers_nid_postfix(region.Address(0), region.bytes), "a range around unregistered pages is covered");
    Require(GuestWriteWatchOverlaps_nid_postfix(region.Address(0), region.bytes), "a range around registered pages does not overlap the watch");
    Require(GuestWriteWatchOverlaps_nid_postfix(region.Address(1) - 1, 2), "a range ending in a registered page does not overlap the watch");
    Require(GuestWriteWatchOverlaps_nid_postfix(region.Address(3) - 1, 2), "a range starting in a registered page does not overlap the watch");
    Require(!GuestWriteWatchOverlaps_nid_postfix(region.Address(0), region.page), "an unregistered page before the registered ones overlaps the watch");
    Require(!GuestWriteWatchOverlaps_nid_postfix(region.Address(3), 3 * region.page), "unregistered pages after the registered ones overlap the watch");
    Require(!GuestWriteWatchOverlaps_nid_postfix(region.Address(1), 0), "an empty range overlaps the watch");
}

void CheckThreads() {
    constexpr std::size_t Threads = 4;
    constexpr std::size_t PagesPerThread = 64;
    constexpr int Rounds = 50;
    Region region(Threads * PagesPerThread);
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(region.Collect() == All(Threads * PagesPerThread), "a new range is not reported in full");
    std::atomic<bool> start{false};
    std::atomic<std::size_t> finished{0};
    std::array<std::thread, Threads> writers;
    for (std::size_t thread = 0; thread < Threads; ++thread) {
        writers[thread] = std::thread([&, thread] {
            while (!start.load()) std::this_thread::yield();
            for (int round = 1; round <= Rounds; ++round)
                for (std::size_t index = 0; index < PagesPerThread; ++index) region.base[(thread * PagesPerThread + index) * region.page] = static_cast<std::uint8_t>(round);
            finished.fetch_add(1);
        });
    }
    std::vector<std::uint8_t> seen(Threads * PagesPerThread, 0);
    const auto take = [&] {
        for (const auto index : region.Collect()) seen[index] = region.base[index * region.page];
    };
    start.store(true);
    while (finished.load() != Threads) {
        take();
        std::this_thread::yield();
    }
    for (auto& writer : writers) writer.join();
    take();
    for (std::size_t index = 0; index < seen.size(); ++index)
        Require(seen[index] == static_cast<std::uint8_t>(Rounds), "a write that raced a collection was not reported");
    Require(region.Collect().empty(), "writes are reported after the writers stopped");
}

int PageProtection(const void* page) {
    mach_vm_address_t address = reinterpret_cast<mach_vm_address_t>(page);
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    Require(mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) == KERN_SUCCESS && address <= reinterpret_cast<mach_vm_address_t>(page), "cannot read the kernel protection of a page");
    return ((info.protection & VM_PROT_READ) ? PROT_READ : 0) | ((info.protection & VM_PROT_WRITE) ? PROT_WRITE : 0) | ((info.protection & VM_PROT_EXECUTE) ? PROT_EXEC : 0);
}

struct TrapLog {
    std::mutex mutex;
    std::vector<std::pair<std::uintptr_t, std::uintptr_t>> calls;
    std::vector<std::thread::id> threads;
    std::atomic<std::uint8_t> fill{0};
    std::atomic<std::ptrdiff_t> alias{0};
    std::atomic<bool> bracket{false};
};

TrapLog& Traps() {
    static TrapLog log;
    return log;
}

void ResolveTrap(std::uintptr_t begin, std::uintptr_t end) {
    auto& log = Traps();
    {
        const std::lock_guard lock(log.mutex);
        log.calls.emplace_back(begin, end);
        log.threads.push_back(std::this_thread::get_id());
    }
    if (log.bracket) GuestWriteWatchBeginHostWrite_nid_postfix(reinterpret_cast<void*>(begin), end - begin);
    std::memset(reinterpret_cast<void*>(begin + log.alias.load()), log.fill.load(), end - begin);
    if (log.bracket) GuestWriteWatchEndHostWrite_nid_postfix(reinterpret_cast<void*>(begin), end - begin);
    GuestWriteWatchReleaseReads_nid_postfix(begin, end - begin);
}

std::vector<std::pair<std::uintptr_t, std::uintptr_t>> TakeTrapCalls(std::vector<std::thread::id>* threads = nullptr) {
    auto& log = Traps();
    const std::lock_guard lock(log.mutex);
    if (threads != nullptr) *threads = log.threads;
    log.threads.clear();
    return std::exchange(log.calls, {});
}

std::uint8_t ReadOnThread(const std::uint8_t* address, std::thread::id* reader = nullptr) {
    std::uint8_t value = 0;
    std::thread([&] {
        value = *reinterpret_cast<const volatile std::uint8_t*>(address);
        if (reader != nullptr) *reader = std::this_thread::get_id();
    }).join();
    return value;
}

void CheckReadTrap() {
    ::alarm(20);
    Region region(10);
    Region unregistered(1);
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    Require(region.Collect() == All(10), "a new range is not reported in full");
    const auto page = region.page;
    Require(!GuestWriteWatchTrapReads_nid_postfix(region.Address(1), page), "a read trap was set without a resolver");
    Require(GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveTrap), "cannot set the read trap resolver");
    Require(!GuestWriteWatchTrapReads_nid_postfix(unregistered.Address(0), page), "a read trap was set on unwatched memory");
    Require(!GuestWriteWatchTrapReads_nid_postfix(region.Address(1), 0), "an empty read trap was set");

    auto& log = Traps();
    log.fill = 0xa5;
    const auto begin = region.Address(1) + 100;
    Require(GuestWriteWatchTrapReads_nid_postfix(begin, page), "cannot set a read trap");
    Require(PageProtection(region.base + page) == PROT_NONE && PageProtection(region.base + 2 * page) == PROT_NONE, "a read trap does not take the access to its pages");
    Require(PageProtection(region.base) == PROT_READ && PageProtection(region.base + 3 * page) == PROT_READ, "a read trap changes pages outside its range");
    std::thread::id reader;
    Require(ReadOnThread(region.base + page + 105, &reader) == 0xa5, "a read of a trapped page does not see the resolver's bytes");
    std::vector<std::thread::id> threads;
    Require(TakeTrapCalls(&threads) == std::vector<std::pair<std::uintptr_t, std::uintptr_t>>{{begin, begin + page}}, "a trapped read does not resolve its range exactly once");
    Require(threads.size() == 1 && threads[0] != reader && threads[0] != std::this_thread::get_id(), "a trapped read is not resolved on the resolver thread");
    Require(PageProtection(region.base + page) == (PROT_READ | PROT_WRITE) && PageProtection(region.base + 2 * page) == (PROT_READ | PROT_WRITE), "the resolver's writes to its resolved pages are not let through");
    Require(ReadOnThread(region.base + 2 * page) == 0xa5 && TakeTrapCalls().empty(), "a resolved read trap resolves again");
    Require(region.Collect() == std::set<std::size_t>({1, 2}), "the resolver's writes are not reported");

    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(3), page) && GuestWriteWatchTrapReads_nid_postfix(region.Address(3), page), "cannot set a read trap twice");
    GuestWriteWatchReleaseReads_nid_postfix(region.Address(3), page);
    Require(PageProtection(region.base + 3 * page) == PROT_READ, "a released read trap set twice does not give back the watch's protection");
    Require(ReadOnThread(region.base + 3 * page) == 0 && TakeTrapCalls().empty(), "a released read trap resolves");

    log.fill = 0x11;
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(4), page), "cannot set a read trap");
    region.base[4 * page] = 0x77;
    Require(region.base[4 * page] == 0x77 && region.base[4 * page + 1] == 0x11, "a write to a trapped page does not land after the resolver's bytes");
    Require(TakeTrapCalls().size() == 1, "a write to a trapped page does not resolve it once");
    Require(region.Collect() == std::set<std::size_t>({4}), "a write to a trapped page is not reported");

    region.base[5 * page] = 1;
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(5), page), "cannot set a read trap");
    Require(region.Collect() == std::set<std::size_t>({5}), "a written trapped page is not reported");
    Require(PageProtection(region.base + 5 * page) == PROT_NONE, "a collection gives back the access to a trapped page");
    Require(ReadOnThread(region.base + 5 * page) == 0x11 && TakeTrapCalls().size() == 1, "a trapped page read after a collection is not resolved");

    log.fill = 0x22;
    const auto half = page / 2;
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(6), half), "cannot set a read trap");
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(6) + half, page), "cannot set a read trap");
    GuestWriteWatchReleaseReads_nid_postfix(region.Address(6), half);
    Require(PageProtection(region.base + 6 * page) == PROT_NONE, "releasing one read trap gives back a page another one still traps");
    Require(ReadOnThread(region.base + 6 * page) == 0, "a page shared by two read traps does not keep the released trap's bytes");
    Require(TakeTrapCalls() == std::vector<std::pair<std::uintptr_t, std::uintptr_t>>{{region.Address(6) + half, region.Address(7) + half}}, "a read of a shared page does not resolve exactly the trap still set");

    log.fill = 0x33;
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(8), page), "cannot set a read trap");
    GuestWriteWatchBeginHostWrite_nid_postfix(region.base + 8 * page, 8);
    Require(TakeTrapCalls(&threads).size() == 1 && threads[0] == std::this_thread::get_id(), "a host write over a trapped page does not resolve it first on its own thread");
    Require(!GuestWriteWatchTrapReads_nid_postfix(region.Address(8), page), "a read trap was set under a host write");
    int descriptors[2];
    Require(::pipe(descriptors) == 0, "cannot create a pipe");
    const std::array<char, 8> payload{'t', 'r', 'a', 'p', 'p', 'e', 'd', '!'};
    Require(::write(descriptors[1], payload.data(), payload.size()) == static_cast<ssize_t>(payload.size()), "cannot fill the pipe");
    Require(::read(descriptors[0], region.base + 8 * page, payload.size()) == static_cast<ssize_t>(payload.size()), "a system call cannot write a page whose read trap a host write resolved");
    GuestWriteWatchEndHostWrite_nid_postfix(region.base + 8 * page, 8);
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    Require(std::memcmp(region.base + 8 * page, payload.data(), payload.size()) == 0 && region.base[8 * page + 8] == 0x33, "a host write over a resolved trap lost bytes");

    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(9), page), "cannot set a read trap");
    Require(GuestWriteWatchProtect_nid_postfix(region.base + 9 * page, page, PROT_READ) == 0, "cannot protect a trapped page");
    Require(TakeTrapCalls().size() == 1 && PageProtection(region.base + 9 * page) == PROT_READ, "a protect of a trapped page does not resolve it and apply the new protection");

    log.fill = 0x44;
    log.bracket = true;
    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(3), page), "cannot set a read trap");
    Require(ReadOnThread(region.base + 3 * page) == 0x44 && TakeTrapCalls().size() == 1, "a resolver that brackets its stores as a host write does not resolve its trap");
    log.bracket = false;

    Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(0), page), "cannot set a read trap");
    Require(GuestWriteWatchUnregister_nid_postfix(region.base, region.bytes), "cannot unregister the test region");
    Require(PageProtection(region.base) == (PROT_READ | PROT_WRITE), "unregistering a trapped page does not give back its protection");
    Require(ReadOnThread(region.base) == 0 && TakeTrapCalls().empty(), "an unregistered read trap resolves");
    ::alarm(0);
}

void CheckReadTrapFaults() {
    const auto bytes = static_cast<std::size_t>(::getpagesize());
    for (const bool resolved : {false, true}) {
        const auto child = ::fork();
        Require(child >= 0, "cannot fork a faulting child");
        if (child == 0) {
            ::alarm(2);
            Region trapped(1);
            GuestWriteWatchRegister_nid_postfix(trapped.base, trapped.bytes);
            if (!GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveTrap) || !GuestWriteWatchTrapReads_nid_postfix(trapped.Address(0), trapped.page)) ::_exit(2);
            if (resolved) static_cast<void>(*reinterpret_cast<const volatile std::uint8_t*>(trapped.base));
            char path[] = "/tmp/anyps5-write-watch-XXXXXX";
            const int file = ::mkstemp(path);
            if (file < 0) ::_exit(2);
            ::unlink(path);
            if (::ftruncate(file, static_cast<off_t>(bytes)) != 0) ::_exit(2);
            auto* mapped = static_cast<std::uint8_t*>(::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0));
            if (mapped == MAP_FAILED || ::ftruncate(file, 0) != 0) ::_exit(2);
            ::close(file);
            GuestWriteWatchRegister_nid_postfix(mapped, bytes);
            static_cast<void>(*reinterpret_cast<const volatile std::uint8_t*>(mapped));
            ::_exit(3);
        }
        int status = 0;
        Require(::waitpid(child, &status, 0) == child, "cannot wait for a faulting child");
        Require(WIFSIGNALED(status) && (WTERMSIG(status) == SIGBUS || WTERMSIG(status) == SIGSEGV), "a read fault on a mapping with missing backing is not delivered while read traps are set");
    }
}

void CheckReadTrapThreads() {
    ::alarm(20);
    constexpr std::size_t Threads = 4;
    constexpr std::size_t PagesPerThread = 8;
    Region region(Threads * PagesPerThread);
    char path[] = "/tmp/anyps5-read-trap-XXXXXX";
    const int file = ::mkstemp(path);
    Require(file >= 0, "cannot create the shared backing");
    ::unlink(path);
    Require(::ftruncate(file, static_cast<off_t>(region.bytes)) == 0, "cannot size the shared backing");
    Require(::mmap(region.base, region.bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, file, 0) == region.base, "cannot map the shared backing");
    auto* alias = static_cast<std::uint8_t*>(::mmap(nullptr, region.bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0));
    Require(alias != MAP_FAILED, "cannot map the second view of the shared backing");
    ::close(file);
    Traps().alias = alias - region.base;
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    static_cast<void>(region.Collect());
    Require(GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveTrap), "cannot set the read trap resolver");
    Traps().fill = 0x5a;
    for (std::size_t index = 0; index < Threads * PagesPerThread; ++index)
        Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(index), region.page), "cannot set a read trap");
    std::atomic<bool> start{false};
    std::atomic<std::size_t> wrong{0};
    std::array<std::thread, Threads> readers;
    for (std::size_t thread = 0; thread < Threads; ++thread) {
        readers[thread] = std::thread([&, thread] {
            while (!start.load()) std::this_thread::yield();
            for (std::size_t index = 0; index < Threads * PagesPerThread; ++index) {
                const auto page = (index + (thread % 2) * PagesPerThread) % (Threads * PagesPerThread);
                if (*reinterpret_cast<const volatile std::uint8_t*>(region.base + page * region.page + thread) != 0x5a) wrong.fetch_add(1);
            }
        });
    }
    start.store(true);
    for (auto& reader : readers) reader.join();
    Require(wrong.load() == 0, "a concurrent read of a trapped page does not see the resolver's bytes");
    auto calls = TakeTrapCalls();
    std::sort(calls.begin(), calls.end());
    Require(calls.size() == Threads * PagesPerThread && std::adjacent_find(calls.begin(), calls.end()) == calls.end(), "concurrent reads do not resolve each trap exactly once");
    Traps().alias = 0;
    ::munmap(alias, region.bytes);
    ::alarm(0);
}

void CheckReadTrapLateFaults() {
    const pid_t child = ::fork();
    Require(child >= 0, "cannot fork a child for late read faults");
    if (child == 0) {
        ::alarm(10);
        try {
            constexpr std::size_t Readers = 32;
            Region region(1);
            GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
            static_cast<void>(region.Collect());
            Require(GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveTrap), "cannot set the read trap resolver");
            Traps().fill = 0x6b;
            Require(GuestWriteWatchTrapReads_nid_postfix(region.Address(0), region.page), "cannot set a read trap");
            mach_port_t port = MACH_PORT_NULL;
            Require(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port) == KERN_SUCCESS && mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS, "cannot create the exception port");
            std::atomic<std::size_t> ready{0};
            std::atomic<bool> start{false};
            std::atomic<std::size_t> wrong{0};
            std::array<std::thread, Readers> readers;
            for (auto& reader : readers) {
                reader = std::thread([&] {
                    if (thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, port, EXCEPTION_DEFAULT, THREAD_STATE_NONE) != KERN_SUCCESS) wrong.fetch_add(1);
                    ready.fetch_add(1);
                    while (!start.load()) std::this_thread::yield();
                    if (*reinterpret_cast<const volatile std::uint8_t*>(region.base) != 0x6b) wrong.fetch_add(1);
                });
            }
            while (ready.load() != Readers) std::this_thread::yield();
            start.store(true);
            struct Request {
                mach_msg_header_t header;
                mach_msg_body_t body;
                mach_msg_port_descriptor_t thread;
                mach_msg_port_descriptor_t task;
                NDR_record_t ndr;
                exception_type_t exception;
                mach_msg_type_number_t codes;
                integer_t code[2];
                mach_msg_trailer_t trailer;
            };
            std::vector<Request> held(Readers);
            for (auto& request : held) {
                Require(mach_msg(&request.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(Request), port, 5000, MACH_PORT_NULL) == MACH_MSG_SUCCESS, "a reader of a trapped page did not fault");
                Require(request.exception == EXC_BAD_ACCESS, "a reader of a trapped page raised another exception");
                mach_port_deallocate(mach_task_self(), request.thread.name);
                mach_port_deallocate(mach_task_self(), request.task.name);
            }
            Require(*reinterpret_cast<const volatile std::uint8_t*>(region.base) == 0x6b, "a read of a trapped page does not see the resolver's bytes");
            Require(PageProtection(region.base) & PROT_READ, "a resolved trap does not give back read access");
            for (const auto& request : held) {
                struct {
                    mach_msg_header_t header;
                    NDR_record_t ndr;
                    kern_return_t result;
                } reply{};
                reply.header.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request.header.msgh_bits), 0);
                reply.header.msgh_remote_port = request.header.msgh_remote_port;
                reply.header.msgh_size = sizeof(reply);
                reply.header.msgh_id = request.header.msgh_id + 100;
                reply.ndr = NDR_record;
                reply.result = KERN_FAILURE;
                Require(mach_msg(&reply.header, MACH_SEND_MSG, sizeof(reply), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL) == MACH_MSG_SUCCESS, "cannot pass a held fault on as a signal");
            }
            for (auto& reader : readers) reader.join();
            Require(wrong.load() == 0, "a reader whose fault arrived after its page was given back does not see the resolver's bytes");
            Require(TakeTrapCalls().size() == 1, "late read faults resolve the trap again");
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n", error.what());
            ::_exit(1);
        }
        ::_exit(0);
    }
    int status = 0;
    Require(::waitpid(child, &status, 0) == child, "cannot wait for the late read fault child");
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "32 read faults that arrive after their page was given back are not all retried");
}

void CheckReadTrapFaultBound() {
    const pid_t child = ::fork();
    Require(child >= 0, "cannot fork a child for persistent read faults");
    if (child == 0) {
        ::alarm(20);
        try {
            constexpr std::size_t Readers = 66;
            constexpr std::size_t Rounds = 40;
            const auto bytes = static_cast<std::size_t>(::getpagesize());
            Region trapped(1);
            GuestWriteWatchRegister_nid_postfix(trapped.base, trapped.bytes);
            Require(GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveTrap) && GuestWriteWatchTrapReads_nid_postfix(trapped.Address(0), trapped.page), "cannot set a read trap");
            char path[] = "/tmp/anyps5-write-watch-XXXXXX";
            const int file = ::mkstemp(path);
            Require(file >= 0, "cannot create the backing file");
            ::unlink(path);
            Require(::ftruncate(file, static_cast<off_t>(bytes)) == 0, "cannot size the backing file");
            auto* mapped = static_cast<std::uint8_t*>(::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0));
            Require(mapped != MAP_FAILED && ::ftruncate(file, 0) == 0, "cannot truncate a watched file mapping");
            ::close(file);
            GuestWriteWatchRegister_nid_postfix(mapped, bytes);
            mach_port_t port = MACH_PORT_NULL;
            Require(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port) == KERN_SUCCESS && mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS, "cannot create the exception port");
            std::atomic<std::size_t> ready{0};
            std::atomic<bool> start{false};
            std::vector<std::thread> readers;
            for (std::size_t index = 0; index < Readers; ++index) {
                readers.emplace_back([&] {
                    Require(thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, port, EXCEPTION_DEFAULT, THREAD_STATE_NONE) == KERN_SUCCESS, "cannot set a thread exception port");
                    ready.fetch_add(1);
                    while (!start.load()) std::this_thread::yield();
                    static_cast<void>(*reinterpret_cast<const volatile std::uint8_t*>(mapped));
                });
            }
            while (ready.load() != Readers) std::this_thread::yield();
            struct Request {
                mach_msg_header_t header;
                mach_msg_body_t body;
                mach_msg_port_descriptor_t thread;
                mach_msg_port_descriptor_t task;
                NDR_record_t ndr;
                exception_type_t exception;
                mach_msg_type_number_t codes;
                integer_t code[2];
                mach_msg_trailer_t trailer;
            };
            const auto receive = [&](Request& request) {
                Require(mach_msg(&request.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(Request), port, 5000, MACH_PORT_NULL) == MACH_MSG_SUCCESS, "a reader of a truncated mapping did not fault");
                mach_port_deallocate(mach_task_self(), request.task.name);
            };
            const auto pass = [](const Request& request) {
                struct {
                    mach_msg_header_t header;
                    NDR_record_t ndr;
                    kern_return_t result;
                } reply{};
                reply.header.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request.header.msgh_bits), 0);
                reply.header.msgh_remote_port = request.header.msgh_remote_port;
                reply.header.msgh_size = sizeof(reply);
                reply.header.msgh_id = request.header.msgh_id + 100;
                reply.ndr = NDR_record;
                reply.result = KERN_FAILURE;
                Require(mach_msg(&reply.header, MACH_SEND_MSG, sizeof(reply), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL) == MACH_MSG_SUCCESS, "cannot pass a held fault on as a signal");
            };
            start.store(true);
            std::vector<Request> held(Readers);
            for (auto& request : held) receive(request);
            for (std::size_t round = 0; round < Rounds; ++round) {
                for (auto& request : held) {
                    const auto thread = request.thread.name;
                    pass(request);
                    receive(request);
                    Require(request.thread.name == thread, "a held reader faulted out of turn");
                    mach_port_deallocate(mach_task_self(), thread);
                }
            }
            ::_exit(3);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n", error.what());
            ::_exit(1);
        }
    }
    int status = 0;
    Require(::waitpid(child, &status, 0) == child, "cannot wait for the persistent read fault child");
    Require(WIFSIGNALED(status) && (WTERMSIG(status) == SIGBUS || WTERMSIG(status) == SIGSEGV), "66 threads that keep faulting on a truncated mapping in turn are retried without bound");
}

void CheckRaisedSignals() {
    Require(GuestWriteWatchAvailable_nid_postfix(), "write watch is unavailable");
    for (const int signal : {SIGBUS, SIGSEGV}) {
        const pid_t child = ::fork();
        Require(child >= 0, "cannot fork");
        if (child == 0) {
            ::raise(signal);
            ::_exit(0);
        }
        int status = 0;
        Require(::waitpid(child, &status, 0) == child, "cannot wait for the child");
        Require(WIFSIGNALED(status) && WTERMSIG(status) == signal, "a raised fault signal does not terminate the process");
    }
}


std::set<std::size_t> Span(std::size_t first, std::size_t last) {
    std::set<std::size_t> pages;
    for (auto index = first; index <= last; ++index) pages.insert(index);
    return pages;
}

void CheckFaultAhead(bool enabled) {
    Region region(80);
    GuestWriteWatchRegister_nid_postfix(region.base, region.bytes);
    static_cast<void>(region.Collect());
    const auto write = [&](std::initializer_list<std::size_t> pages, std::uint8_t value) {
        for (const auto index : pages) region.base[index * region.page + 5] = value;
    };
    const auto last = [&](std::size_t first, std::size_t stop) {
        if (!enabled) return first;
        const auto toBlockEnd = (65536 - region.Address(first) % 65536) / region.page;
        return std::min({first + 32, first + std::max<std::size_t>(toBlockEnd, 1), stop}) - 1;
    };
    write({10, 11}, 1);
    Require(region.Collect() == std::set<std::size_t>({10, 11}), "writes to two adjacent pages are not reported exactly");
    write({20, 21, 22}, 2);
    Require(region.Collect() == Span(20, last(22, 80)), enabled ? "a third sequential write fault does not open the rest of its 64 KiB block" : "with fault-ahead off, sequential writes are not reported exactly");
    Require(region.base[20 * region.page + 5] == 2 && region.base[22 * region.page + 5] == 2, "sequential writes were lost");
    write({23, 40}, 3);
    Require(region.Collect() == std::set<std::size_t>({23, 40}), "writes after a collection are not reported exactly");
    write({30}, 4);
    write({26, 27, 28}, 4);
    auto expected = Span(26, last(28, 30));
    expected.insert(30);
    Require(region.Collect() == expected, "fault-ahead does not stop at a page that is already writable");
    Require(GuestWriteWatchProtect_nid_postfix(region.base + 60 * region.page, region.page, PROT_READ) == 0, "cannot protect a watched page");
    static_cast<void>(region.Collect());
    write({56, 57, 58}, 5);
    Require(region.Collect() == Span(56, last(58, 60)), "fault-ahead does not stop at a read-only page");
    if (GuestWriteWatchProtect_nid_postfix(region.base + 66 * region.page, region.page, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        static_cast<void>(region.Collect());
        write({62, 63, 64}, 7);
        Require(region.Collect() == Span(62, last(64, 66)), "fault-ahead does not stop at a writable page with another protection");
        Require(KernelProtection(region.base + 66 * region.page) == (PROT_READ | PROT_EXEC), "fault-ahead changed the protection of a page with another one");
        region.base[66 * region.page + 5] = 7;
        Require(region.Collect() == std::set<std::size_t>({66}), "a write to an executable watched page is not reported");
    }
    write({77, 78, 79}, 6);
    Require(region.Collect() == Span(77, 79), "fault-ahead reaches past the end of a watched range");
    Require(region.base[79 * region.page + 5] == 6 && region.base[60 * region.page + 5] == 0, "writes near the range end were lost");
    for (std::size_t first = 0; first + 2 < 80; ++first) {
        if ((region.Address(first + 3) % 65536) != 0 || first + 3 >= 80) continue;
        static_cast<void>(region.Collect());
        write({first, first + 1, first + 2}, 8);
        Require(region.Collect() == Span(first, first + 2), "fault-ahead crosses into the next 64 KiB block");
        break;
    }
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected collect, fresh, armed, register, mapped, host, empty, protection, overlaps, threads, raised, fault-ahead, fault-ahead-off, read-trap, read-trap-threads, read-trap-fault, read-trap-late or read-trap-fault-bound");
        const std::string_view mode(argv[1]);
        if (mode == "collect") CheckCollect();
        else if (mode == "fresh") CheckFresh();
        else if (mode == "armed") CheckArmed();
        else if (mode == "register") CheckRegisterAgain();
        else if (mode == "mapped") CheckRegisterMapped();
        else if (mode == "host") CheckHostWrite();
        else if (mode == "execute-fault") CheckNonWriteFaults(true);
        else if (mode == "read-fault") CheckNonWriteFaults(false);
        else if (mode == "empty") CheckEmptyRanges();
        else if (mode == "protection") CheckProtection();
        else if (mode == "overlaps") CheckOverlaps();
        else if (mode == "threads") CheckThreads();
        else if (mode == "raised") CheckRaisedSignals();
        else if (mode == "fault-ahead") CheckFaultAhead(true);
        else if (mode == "fault-ahead-off") CheckFaultAhead(false);
        else if (mode == "read-trap") CheckReadTrap();
        else if (mode == "read-trap-threads") CheckReadTrapThreads();
        else if (mode == "read-trap-fault") CheckReadTrapFaults();
        else if (mode == "read-trap-late") CheckReadTrapLateFaults();
        else if (mode == "read-trap-fault-bound") CheckReadTrapFaultBound();
        else throw std::runtime_error("unknown write watch test");
        std::printf("write watch %s passed\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
