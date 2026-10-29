#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <sys/resource.h>
#endif

namespace {
using namespace AgcDriver::GuestMemory;
void require(bool condition, const char* reason) { if (!condition) throw std::runtime_error(reason); }
struct Mapping {
    std::size_t size;
    std::int64_t physical = -1;
    void* data = nullptr;
    explicit Mapping(std::size_t size) : size(size) {
        require(sceKernelAllocateDirectMemory(0, 0x10000000000ll, size, 65536, 3, &physical) == 0, "allocate direct memory failed");
        if (sceKernelMapDirectMemory(&data, size, 3, 0, physical, 65536) != 0) {
            sceKernelReleaseDirectMemory(physical, size);

            throw std::runtime_error("map direct memory failed");
        }
    }
    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;
    ~Mapping() {
        if (data != nullptr) sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), size);
        if (physical >= 0) sceKernelReleaseDirectMemory(physical, size);
    }
    std::uint64_t Address() const { return reinterpret_cast<std::uint64_t>(data); }
};

void benchmark(bool labels = false, bool backing = true) {
    require(WriteWatched(), "write tracking unavailable for benchmark");
    std::thread([] {}).join();
    Mapping mapping(2u << 20u);
#ifndef _WIN32
    if (!backing) GuestArena::GuestArenaSetSharedBackingWriter_nid_no_patch(nullptr);
#else
    static_cast<void>(backing);
#endif
    std::vector<std::byte> source(1u << 20u, std::byte{17});
    const auto sizes = labels ? std::array<std::size_t, 3>{4, 8, 16} : std::array<std::size_t, 3>{4096, 65536, 1048576};
    for (const auto bytes : sizes) {
        std::vector<double> times;
        std::uint64_t faults = 0;
        for (unsigned pass = 0; pass < 10; ++pass) {
#ifndef _WIN32
            rusage before{}, after{};
            getrusage(RUSAGE_THREAD, &before);
#endif
            const auto start = std::chrono::steady_clock::now();
            for (unsigned i = 0; i < 256; ++i) {
                source[0] = static_cast<std::byte>(i);
                BumpCollectEpoch();
                if (labels) {
                    CheckRange(mapping.data, bytes, 4, true);
                    StoreOwnBytes(mapping.Address(), std::span(source).first(bytes));
                } else Write(mapping.Address(), std::span(source).first(bytes), 1);
            }
            const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 256;
#ifndef _WIN32
            getrusage(RUSAGE_THREAD, &after);
            if (pass != 0) faults += after.ru_minflt - before.ru_minflt;
#endif
            if (pass != 0) times.push_back(us);
            require(std::memcmp(mapping.data, source.data(), bytes) == 0, "copied data differs");
        }
        std::sort(times.begin(), times.end());
        std::cout << "bytes=" << bytes << " median_us=" << times[4] << " faults=" << faults << " operations=2304\n";
    }
}

void smallStores() {
    Mapping mapping(2u << 16u);
    auto* data = static_cast<std::byte*>(mapping.data);
    std::memset(data, 19, mapping.size);
    std::array<std::byte, 16> source;
    source.fill(std::byte{83});
    for (const auto offset : {4u, 4092u, 65532u}) for (const auto size : {4u, 8u, 16u}) {
        const auto address = mapping.Address() + offset;
        const auto before = CollectWritesUncached(address, size);
        const auto stored = StoreOwnBytes(address, std::span(source).first(size));
        require(std::memcmp(data + offset, source.data(), size) == 0, "small backing store differs");
        require(data[offset - 1] == std::byte{19} && data[offset + 16] == std::byte{19}, "small store changed neighboring bytes");
        if (before != 0) {
            require(stored > before && StoredOver(address, size, before), "small driver store was not stamped");
            require(UnchangedSinceCollected(address, size, before), "driver store was classified as a guest write");
            data[offset] = std::byte{97};
            StoreOwnBytes(address, std::span(source).first(size));
            require(!UnchangedSinceCollected(address, size, stored), "guest write before a driver store was lost");
            const auto after = CollectWritesUncached(address, size);
            data[offset] = std::byte{101};
            require(!UnchangedSinceCollected(address, size, after), "guest write after a driver store was lost");
        }
    }
    std::array<std::byte, 16> ordinary{};
    StoreOwnBytes(reinterpret_cast<std::uintptr_t>(ordinary.data()), source);
    require(ordinary == source, "small ordinary store fallback differs");
    require(StoreOwnBytes(0, std::span<const std::byte>{}) == 0, "empty store produced a version");
}

void partialCollect() {
    constexpr std::size_t page = 4096;
    Mapping mapping(page * 4);
    auto* data = static_cast<volatile std::byte*>(mapping.data);
#ifdef _WIN32
    void* alias = nullptr;
    require(sceKernelMapDirectMemory(&alias, mapping.size, 3, 0, mapping.physical, 65536) == 0, "partial collect alias failed");
#endif
    require(WriteWatched(), "write tracking unavailable for partial collect");
    const auto before = CollectWritesUncached(mapping.Address(), mapping.size);
    require(before != 0, "partial collect range is not watched");
    data[page * 3] = std::byte{91};
    require(CollectWritesUncached(mapping.Address(), page) != 0, "prefix collect failed");
    require(!UnchangedSinceCollected(mapping.Address() + page * 3, page, before), "partial collect lost a neighboring guest write");
    const auto settled = CollectWritesUncached(mapping.Address(), mapping.size);
    require(UnchangedSinceCollected(mapping.Address(), mapping.size, settled), "fully collected shared range remained dirty");
#ifdef _WIN32
    const auto collect = [&](void* view, std::size_t offset, std::size_t bytes, std::size_t capacity, bool clear) {
        std::array<void*, 4> pages{};
        std::size_t count = capacity;
        const auto address = reinterpret_cast<std::uintptr_t>(view);
        require(GuestArena::GuestArenaCollectWrites_nid_postfix(address + offset, bytes, pages.data(), &count, clear), "shared subpage collection failed");
        std::vector<std::size_t> offsets;
        for (std::size_t i = 0; i < count; ++i) offsets.push_back(reinterpret_cast<std::uintptr_t>(pages[i]) - address);
        return offsets;
    };
    const std::vector<std::size_t> none;
    collect(alias, 0, mapping.size, 4, true);
    data[page * 3] = std::byte{92};
    require(collect(mapping.data, 0, page * 3, 4, false) == none, "a shared write was reported for the pages next to it");
    require(collect(mapping.data, 0, mapping.size, 4, false) == std::vector<std::size_t>{page * 3}, "probe missed a shared write");
    require(collect(mapping.data, 0, mapping.size, 4, false) == std::vector<std::size_t>{page * 3}, "probe consumed a shared write");
    require(collect(mapping.data, 0, page, 4, true) == none, "prefix reset reported an unwritten page");
    require(collect(alias, 0, mapping.size, 4, false) == std::vector<std::size_t>{page * 3}, "prefix reset consumed an alias write");
    require(collect(mapping.data, page * 3, page, 4, true) == std::vector<std::size_t>{page * 3}, "prefix reset consumed a neighboring write");
    require(collect(mapping.data, page * 3, page, 4, false).empty(), "reset subpage remained dirty");
    require(collect(mapping.data, 0, mapping.size, 4, false).empty(), "collected subpages remained dirty");
    require(collect(alias, 0, mapping.size, 4, true) == std::vector<std::size_t>{page * 3}, "collecting one view consumed an alias write");
    static_cast<volatile std::byte*>(alias)[page] = std::byte{93};
    require(collect(mapping.data, page * 2, page, 4, true) == none, "an alias write was reported for another page");
    require(collect(mapping.data, 0, mapping.size, 4, true) == std::vector<std::size_t>{page}, "alias write did not invalidate the other view");
    require(collect(mapping.data, 0, mapping.size, 4, false).empty(), "full collect left pending subpages");
    require(collect(alias, 0, mapping.size, 4, true) == std::vector<std::size_t>{page}, "alias write was consumed for its own view");
    for (std::size_t at = 0; at < mapping.size; at += page) static_cast<volatile std::byte*>(alias)[at + 8] = std::byte{99};
    require(collect(mapping.data, page, page * 2, 4, true) == std::vector<std::size_t>{page, page * 2}, "partial resets consumed intervening writes");
    require(collect(mapping.data, page, page * 2, 4, false).empty(), "interior reset left its subpages dirty");
    require(collect(mapping.data, 0, mapping.size, 4, true) == std::vector<std::size_t>{0, page * 3}, "full collect after an interior reset did not preserve pending subpages");
    require(collect(alias, 0, mapping.size, 4, true) == std::vector<std::size_t>{0, page, page * 2, page * 3}, "an interior reset consumed the alias's writes");
    static_cast<volatile std::byte*>(alias)[0] = std::byte{94};
    static_cast<volatile std::byte*>(alias)[page * 2] = std::byte{95};
    static_cast<volatile std::byte*>(alias)[page * 3] = std::byte{96};
    require(collect(mapping.data, 0, mapping.size, 1, true) == std::vector<std::size_t>{0}, "limited collect returned the wrong prefix");
    require(collect(mapping.data, 0, mapping.size, 4, false) == std::vector<std::size_t>{page * 2, page * 3}, "limited collect consumed unreported writes");
    require(collect(mapping.data, page, page * 3, 1, true) == std::vector<std::size_t>{page * 2}, "limited collect continuation lost writes");
    require(collect(mapping.data, page * 3, page, 4, true) == std::vector<std::size_t>{page * 3}, "limited collect consumed a write after its last report");
    require(collect(mapping.data, 0, mapping.size, 4, false).empty(), "limited collection did not settle");
    require(collect(alias, 0, mapping.size, 4, true) == std::vector<std::size_t>{0, page * 2, page * 3}, "limited collection consumed the alias's writes");
    data[page] = std::byte{97};
    data[page * 2] = std::byte{98};
    require(collect(alias, 0, mapping.size, 4, true) == std::vector<std::size_t>{page, page * 2}, "writes to two pages were not reported apart");
    require(collect(mapping.data, 0, mapping.size, 4, true) == std::vector<std::size_t>{page, page * 2}, "writes to two pages were not reported for their own view");
    require(collect(mapping.data, 0, mapping.size, 4, false).empty() && collect(alias, 0, mapping.size, 4, false).empty(), "page tracking did not settle");
    require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(alias), mapping.size) == 0, "partial collect alias unmap failed");
#endif
    std::cout << "Partial shared write collection tests passed\n";
}

template<class TAction> void fails(TAction&& fn) {
    bool failed = false;
    try { fn(); } catch (const std::runtime_error&) { failed = true; }
    require(failed, "invalid write was accepted");
}

#ifndef _WIN32
std::size_t directMappings() {
    std::ifstream maps("/proc/self/maps");
    require(maps.is_open(), "cannot inspect mapping lifetimes");
    std::size_t count = 0;
    for (std::string line; std::getline(maps, line);) if (line.find("/memfd:direct memory") != std::string::npos) ++count;
    return count;
}
#endif

void run() {
    constexpr std::size_t part = 65536;
    Mapping mapping(2u << 20u);
    auto* bytes = static_cast<std::byte*>(mapping.data);
    std::memset(bytes, 19, mapping.size);
    std::vector<std::byte> source(part, std::byte{73});
    const auto before = CollectWritesUncached(mapping.Address(), mapping.size);
    Write(mapping.Address() + 123, source, 1);
    require(std::memcmp(bytes + 123, source.data(), source.size()) == 0, "shared write differs");
    require(bytes[122] == std::byte{19} && bytes[123 + part] == std::byte{19}, "write changed neighboring bytes");
    if (before != 0) {
        require(!UnchangedSince(mapping.Address(), mapping.size, before), "driver write did not advance versions");
        require(Watched(mapping.Address(), mapping.size), "host view disabled guest tracking");
        const auto after = CollectWritesUncached(mapping.Address(), mapping.size);
        require(UnchangedSince(mapping.Address(), mapping.size, after), "fresh write version is stale");
        bytes[part + 456] = std::byte{91};
        CollectWritesUncached(mapping.Address(), mapping.size);
        require(!UnchangedSince(mapping.Address(), mapping.size, after), "subsequent guest write was lost");
#ifndef _WIN32
        std::size_t dirty = 0;
        const auto collect = [&] {
            return GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(mapping.Address(), mapping.size,
                [](void* p, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::size_t*>(p) += end - begin; }, &dirty);
        };
        require(collect(), "guest mapping is no longer watched");
        dirty = 0;
        require(GuestArena::GuestArenaWriteSharedBacking_nid_no_patch(mapping.Address(), source.data(), source.size()), "shared writer rejected direct backing");
        require(collect() && dirty == 0, "host write dirtied the tracked guest mapping");
        bytes[part * 4] = std::byte{31};
        require(collect() && dirty == 4096, "guest write tracking was disarmed");
#endif
    }
    require(sceKernelMprotect(mapping.data, part, 1) == 0, "read-only protection failed");
    fails([&] { Write(mapping.Address(), source, 1); });
    require(sceKernelMprotect(mapping.data, part, 3) == 0, "write protection restore failed");
    std::vector<std::byte> ordinary(part);
    Write(reinterpret_cast<std::uint64_t>(ordinary.data()), source, 1);
    require(ordinary == source, "ordinary memory fallback failed");
    {
        Mapping replacement(part);
        void* originalAlias = nullptr;
        require(sceKernelMapDirectMemory(&originalAlias, part, 3, 0, mapping.physical + part, part) == 0, "physical alias failed");
        std::memset(originalAlias, 29, part);
        void* middle = bytes + part;
        require(sceKernelMapDirectMemory(&middle, part, 3, 0x10, replacement.physical, part) == 0, "fixed backing replacement failed");
        std::vector<std::byte> across(part * 3);
        for (std::size_t i = 0; i < across.size(); ++i) across[i] = static_cast<std::byte>((i / part) + 41);
        Write(mapping.Address(), across, 1);
        require(std::memcmp(mapping.data, across.data(), across.size()) == 0, "write across backing boundaries differs");
        require(std::memcmp(replacement.data, across.data() + part, part) == 0, "physical alias did not see the write");
        require(std::all_of(static_cast<std::byte*>(originalAlias), static_cast<std::byte*>(originalAlias) + part, [](auto x) { return x == std::byte{29}; }), "replaced virtual range wrote the old backing");
#ifndef _WIN32
        require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(middle), part) == 0, "partial unmap failed");
        const auto saved = bytes[0];
        across[0] = std::byte{99};
        require(!GuestArena::GuestArenaWriteSharedBacking_nid_no_patch(mapping.Address(), across.data(), across.size()), "writer accepted an unmapped gap");
        require(bytes[0] == saved, "failed shared write changed its prefix");
#endif
        require(sceKernelMapDirectMemory(&middle, part, 3, 0x10, mapping.physical + part, part) == 0, "original backing restore failed");
        require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(originalAlias), part) == 0, "alias unmap failed");
    }
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < 8; ++i) threads.emplace_back([&, i] {
        std::vector<std::byte> input(part, static_cast<std::byte>(i + 57));
        for (unsigned repeat = 0; repeat < 32; ++repeat) Write(mapping.Address() + i * part, input, 1);
    });
    for (auto& thread : threads) thread.join();
    for (unsigned i = 0; i < 8; ++i) require(std::all_of(bytes + i * part, bytes + (i + 1) * part, [i](auto x) { return x == static_cast<std::byte>(i + 57); }), "concurrent write differs");
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--partial-collect") {
            if (!WriteWatched()) {
                std::cout << "Partial shared write collection skipped: write tracking unavailable\n";
                return 77;
            }
            partialCollect();
        }
        else if (argc == 2 && std::string_view(argv[1]) == "--benchmark") benchmark();
        else if (argc == 2 && std::string_view(argv[1]) == "--benchmark-labels") benchmark(true);
        else if (argc == 2 && std::string_view(argv[1]) == "--benchmark-fallback") benchmark(false, false);
        else if (argc == 2 && std::string_view(argv[1]) == "--benchmark-labels-fallback") benchmark(true, false);
        else {
#ifndef _WIN32
            const auto before = directMappings();
#endif
            run();
            smallStores();
#ifndef _WIN32
            require(directMappings() == before, "physical backing write views leaked");
#endif
            std::cout << "Shared backing write tests passed\n";
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
