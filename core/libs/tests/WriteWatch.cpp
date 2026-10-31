#include "prx/libc/include/GuestWriteWatch.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <mach/mach.h>
#include <mach/mach_vm.h>
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

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected collect, fresh, armed, register, mapped, host, empty, protection, overlaps, threads or raised");
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
        else throw std::runtime_error("unknown write watch test");
        std::printf("write watch %s passed\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
