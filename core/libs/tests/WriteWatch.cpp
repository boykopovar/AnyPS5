#include "prx/libc/include/GuestWriteWatch.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <sys/mman.h>
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
    std::set<std::size_t> Collect() {
        std::set<std::size_t> pages;
        struct Sink {
            Region* region;
            std::set<std::size_t>* pages;
        } sink{this, &pages};
        const bool complete = GuestWriteWatchCollect_nid_postfix(Address(0), bytes, [](void* context, std::uintptr_t begin, std::uintptr_t end) {
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
}

void CheckThreads() {
    constexpr std::size_t Threads = 4;
    constexpr std::size_t PagesPerThread = 64;
    constexpr int Rounds = 200;
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
    while (finished.load() != Threads) take();
    for (auto& writer : writers) writer.join();
    take();
    for (std::size_t index = 0; index < seen.size(); ++index)
        Require(seen[index] == static_cast<std::uint8_t>(Rounds), "a write that raced a collection was not reported");
    Require(region.Collect().empty(), "writes are reported after the writers stopped");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected collect, register, host, protection or threads");
        const std::string_view mode(argv[1]);
        if (mode == "collect") CheckCollect();
        else if (mode == "register") CheckRegisterAgain();
        else if (mode == "host") CheckHostWrite();
        else if (mode == "protection") CheckProtection();
        else if (mode == "threads") CheckThreads();
        else throw std::runtime_error("unknown write watch test");
        std::printf("write watch %s passed\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
