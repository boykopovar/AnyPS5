#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <sys/resource.h>
#endif

extern "C" {
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelReleaseDirectMemory(std::int64_t, std::size_t);
int APS5_VABI sceKernelMunmap(void*, std::size_t);
int APS5_VABI sceKernelMprotect(const void*, std::size_t, int);
}

namespace {

using namespace AgcDriver::GuestMemory;
using Testing::Case;
using Testing::Require;

constexpr std::size_t Part = 65536;

struct Mapping {
    explicit Mapping(std::size_t bytes) : size(bytes) {
        Require(sceKernelAllocateDirectMemory(0, 0x10000000000ll, size, 65536, 3, &physical) == 0, "allocate direct memory failed");
        if (sceKernelMapDirectMemory(&data, size, 3, 0, physical, 65536) != 0) {
            sceKernelReleaseDirectMemory(physical, size);
            physical = -1;
            Testing::Fail("map direct memory failed");
        }
    }

    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;

    ~Mapping() {
        if (data != nullptr) sceKernelMunmap(data, size);
        if (physical >= 0) sceKernelReleaseDirectMemory(physical, size);
    }

    std::uint64_t Address() const {
        return reinterpret_cast<std::uint64_t>(data);
    }

    std::byte* Bytes() const {
        return static_cast<std::byte*>(data);
    }

    std::size_t size;
    std::int64_t physical = -1;
    void* data = nullptr;
};

class Alias {
public:
    Alias(std::size_t bytes, std::int64_t physical) : size(bytes) {
        Require(sceKernelMapDirectMemory(&data, size, 3, 0, physical, size) == 0, "physical alias failed");
    }

    Alias(const Alias&) = delete;
    Alias& operator=(const Alias&) = delete;

    ~Alias() {
        if (data != nullptr) sceKernelMunmap(data, size);
    }

    std::byte* Bytes() const {
        return static_cast<std::byte*>(data);
    }

    void Unmap() {
        Require(sceKernelMunmap(data, size) == 0, "alias unmap failed");
        data = nullptr;
    }

private:
    std::size_t size;
    void* data = nullptr;
};

struct BenchmarkMode {
    bool labels;
    bool backing;
};

bool BenchmarkRequested() {
    return !Testing::Arguments().empty();
}

BenchmarkMode RequestedBenchmark() {
    const auto& arguments = Testing::Arguments();
    if (arguments.empty()) Testing::Skip("runs only with a benchmark, benchmark-labels, benchmark-fallback or benchmark-labels-fallback argument");
    Require(arguments.size() == 1, "invalid test arguments");
    const auto& mode = arguments.front();
    if (mode == "benchmark") return {false, true};
    if (mode == "benchmark-labels") return {true, true};
    if (mode == "benchmark-fallback") return {false, false};
    if (mode == "benchmark-labels-fallback") return {true, false};
    Testing::Fail("invalid test argument " + mode);
}

void SkipInBenchmarkRun() {
    if (BenchmarkRequested()) Testing::Skip("a benchmark run covers only the benchmark");
}

#ifndef _WIN32
std::size_t DirectMappings() {
    std::ifstream maps("/proc/self/maps");
    Require(maps.is_open(), "cannot inspect mapping lifetimes");
    std::size_t count = 0;
    for (std::string line; std::getline(maps, line);) {
        if (line.find("/memfd:direct memory") != std::string::npos) ++count;
    }
    return count;
}
#endif

template<typename TBody>
void WithoutLeakedViews(TBody body) {
    SkipInBenchmarkRun();
#ifndef _WIN32
    const auto before = DirectMappings();
#endif
    body();
#ifndef _WIN32
    Testing::RequireEqual(DirectMappings(), before, "physical backing write views leaked");
#endif
}

struct WrittenMapping {
    WrittenMapping() : mapping(2u << 20u), source(Part, std::byte{73}) {
        std::memset(mapping.data, 19, mapping.size);
        before = CollectWritesUncached(mapping.Address(), mapping.size);
        Write(mapping.Address() + 123, source, 1);
    }

    Mapping mapping;
    std::vector<std::byte> source;
    std::uint64_t before = 0;
};

const Case sharedWrite{"Write_SharedMapping_CopiesWithoutTouchingNeighbours", [] {
    WithoutLeakedViews([] {
        const WrittenMapping written;
        const auto* bytes = written.mapping.Bytes();
        Require(std::memcmp(bytes + 123, written.source.data(), written.source.size()) == 0, "shared write differs");
        Require(bytes[122] == std::byte{19} && bytes[123 + Part] == std::byte{19}, "write changed neighboring bytes");
    });
}};

const Case sharedWriteVersions{"Write_SharedMapping_AdvancesVersionsAndKeepsGuestTracking", [] {
    WithoutLeakedViews([] {
        const WrittenMapping written;
        const auto& mapping = written.mapping;
        if (written.before == 0) return;
        Require(!UnchangedSince(mapping.Address(), mapping.size, written.before), "driver write did not advance versions");
        Require(Watched(mapping.Address(), mapping.size), "host view disabled guest tracking");
        const auto after = CollectWritesUncached(mapping.Address(), mapping.size);
        Require(UnchangedSince(mapping.Address(), mapping.size, after), "fresh write version is stale");
        mapping.Bytes()[Part + 456] = std::byte{91};
        CollectWritesUncached(mapping.Address(), mapping.size);
        Require(!UnchangedSince(mapping.Address(), mapping.size, after), "subsequent guest write was lost");
    });
}};

#ifndef _WIN32
const Case hostBackingWrite{"WriteSharedBacking_HostWrite_DoesNotDirtyOrDisarmTheGuestMapping", [] {
    WithoutLeakedViews([] {
        const WrittenMapping written;
        const auto& mapping = written.mapping;
        if (written.before == 0) return;
        std::size_t dirty = 0;
        const auto collect = [&] {
            return GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(mapping.Address(), mapping.size,
                [](void* p, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::size_t*>(p) += end - begin; }, &dirty);
        };
        Require(collect(), "guest mapping is no longer watched");
        dirty = 0;
        Require(GuestArena::GuestArenaWriteSharedBacking_nid_no_patch(mapping.Address(), written.source.data(), written.source.size()), "shared writer rejected direct backing");
        Require(collect() && dirty == 0, "host write dirtied the tracked guest mapping");
        mapping.Bytes()[Part * 4] = std::byte{31};
        Require(collect() && dirty == 4096, "guest write tracking was disarmed");
    });
}};
#endif

const Case readOnly{"Write_ReadOnlyMapping_Throws", [] {
    WithoutLeakedViews([] {
        const WrittenMapping written;
        const auto& mapping = written.mapping;
        Require(sceKernelMprotect(mapping.data, Part, 1) == 0, "read-only protection failed");
        Testing::RequireThrows<std::runtime_error>([&] { Write(mapping.Address(), written.source, 1); }, "invalid write was accepted");
        Require(sceKernelMprotect(mapping.data, Part, 3) == 0, "write protection restore failed");
    });
}};

const Case ordinaryWrite{"Write_OrdinaryMemory_FallsBackToACopy", [] {
    WithoutLeakedViews([] {
        const std::vector<std::byte> source(Part, std::byte{73});
        std::vector<std::byte> ordinary(Part);
        Write(reinterpret_cast<std::uint64_t>(ordinary.data()), source, 1);
        Require(ordinary == source, "ordinary memory fallback failed");
    });
}};

const Case replacedBacking{"Write_AcrossReplacedBacking_WritesTheCurrentBackings", [] {
    WithoutLeakedViews([] {
        const WrittenMapping written;
        const auto& mapping = written.mapping;
        auto* bytes = mapping.Bytes();
        const Mapping replacement(Part);
        Alias originalAlias(Part, mapping.physical + static_cast<std::int64_t>(Part));
        std::memset(originalAlias.Bytes(), 29, Part);
        void* middle = bytes + Part;
        Require(sceKernelMapDirectMemory(&middle, Part, 3, 0x10, replacement.physical, Part) == 0, "fixed backing replacement failed");
        std::vector<std::byte> across(Part * 3);
        for (std::size_t i = 0; i < across.size(); ++i) across[i] = static_cast<std::byte>((i / Part) + 41);
        Write(mapping.Address(), across, 1);
        Require(std::memcmp(mapping.data, across.data(), across.size()) == 0, "write across backing boundaries differs");
        Require(std::memcmp(replacement.data, across.data() + Part, Part) == 0, "physical alias did not see the write");
        Require(std::all_of(originalAlias.Bytes(), originalAlias.Bytes() + Part, [](auto x) { return x == std::byte{29}; }), "replaced virtual range wrote the old backing");
#ifndef _WIN32
        Require(sceKernelMunmap(middle, Part) == 0, "partial unmap failed");
        const auto saved = bytes[0];
        across[0] = std::byte{99};
        Require(!GuestArena::GuestArenaWriteSharedBacking_nid_no_patch(mapping.Address(), across.data(), across.size()), "writer accepted an unmapped gap");
        Require(bytes[0] == saved, "failed shared write changed its prefix");
#endif
        Require(sceKernelMapDirectMemory(&middle, Part, 3, 0x10, mapping.physical + static_cast<std::int64_t>(Part), Part) == 0, "original backing restore failed");
        originalAlias.Unmap();
    });
}};

const Case concurrentWrites{"Write_ConcurrentThreadsOnDisjointParts_EachWriteLands", [] {
    WithoutLeakedViews([] {
        const WrittenMapping written;
        const auto& mapping = written.mapping;
        const auto* bytes = mapping.Bytes();
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < 8; ++i) {
            threads.emplace_back([&mapping, i] {
                std::vector<std::byte> input(Part, static_cast<std::byte>(i + 57));
                for (unsigned repeat = 0; repeat < 32; ++repeat) Write(mapping.Address() + i * Part, input, 1);
            });
        }
        for (auto& thread : threads) thread.join();
        for (unsigned i = 0; i < 8; ++i) {
            Require(std::all_of(bytes + i * Part, bytes + (i + 1) * Part, [i](auto x) { return x == static_cast<std::byte>(i + 57); }), "concurrent write differs in part " + std::to_string(i));
        }
    });
}};

const Case smallStores{"StoreOwnBytes_SmallStoresIntoSharedMapping_AreStampedAsDriverStores", [] {
    WithoutLeakedViews([] {
        const Mapping mapping(2u << 16u);
        auto* data = mapping.Bytes();
        std::memset(data, 19, mapping.size);
        std::array<std::byte, 16> source;
        source.fill(std::byte{83});
        for (const auto offset : {4u, 4092u, 65532u}) {
            for (const auto size : {4u, 8u, 16u}) {
                const auto at = " (offset " + std::to_string(offset) + ", size " + std::to_string(size) + ")";
                const auto address = mapping.Address() + offset;
                const auto before = CollectWritesUncached(address, size);
                const auto stored = StoreOwnBytes(address, std::span(source).first(size));
                Require(std::memcmp(data + offset, source.data(), size) == 0, "small backing store differs" + at);
                Require(data[offset - 1] == std::byte{19} && data[offset + 16] == std::byte{19}, "small store changed neighboring bytes" + at);
                if (before == 0) continue;
                Require(stored > before && StoredOver(address, size, before), "small driver store was not stamped" + at);
                Require(UnchangedSinceCollected(address, size, before), "driver store was classified as a guest write" + at);
                data[offset] = std::byte{97};
                StoreOwnBytes(address, std::span(source).first(size));
                Require(!UnchangedSinceCollected(address, size, stored), "guest write before a driver store was lost" + at);
                const auto after = CollectWritesUncached(address, size);
                data[offset] = std::byte{101};
                Require(!UnchangedSinceCollected(address, size, after), "guest write after a driver store was lost" + at);
            }
        }
    });
}};

const Case ordinaryStore{"StoreOwnBytes_OrdinaryMemory_FallsBackToACopy", [] {
    WithoutLeakedViews([] {
        std::array<std::byte, 16> source;
        source.fill(std::byte{83});
        std::array<std::byte, 16> ordinary{};
        StoreOwnBytes(reinterpret_cast<std::uintptr_t>(ordinary.data()), source);
        Require(ordinary == source, "small ordinary store fallback differs");
    });
}};

const Case emptyStore{"StoreOwnBytes_EmptyStore_ProducesNoVersion", [] {
    WithoutLeakedViews([] {
        Testing::RequireEqual(StoreOwnBytes(0, std::span<const std::byte>{}), std::uint64_t{0}, "empty store produced a version");
    });
}};

const Case benchmark{"Write_Benchmark_PrintsMedianTimes", [] {
    const auto mode = RequestedBenchmark();
    Require(WriteWatched(), "write tracking unavailable for benchmark");
    std::thread([] {}).join();
    const Mapping mapping(2u << 20u);
#ifndef _WIN32
    if (!mode.backing) GuestArena::GuestArenaSetSharedBackingWriter_nid_no_patch(nullptr);
#endif
    std::vector<std::byte> source(1u << 20u, std::byte{17});
    const auto sizes = mode.labels ? std::array<std::size_t, 3>{4, 8, 16} : std::array<std::size_t, 3>{4096, 65536, 1048576};
    for (const auto bytes : sizes) {
        std::vector<double> times;
        std::uint64_t faults = 0;
        for (unsigned pass = 0; pass < 10; ++pass) {
#ifndef _WIN32
            rusage before{};
            rusage after{};
            getrusage(RUSAGE_THREAD, &before);
#endif
            const auto start = std::chrono::steady_clock::now();
            for (unsigned i = 0; i < 256; ++i) {
                source[0] = static_cast<std::byte>(i);
                BumpCollectEpoch();
                if (mode.labels) {
                    CheckRange(mapping.data, bytes, 4, true);
                    StoreOwnBytes(mapping.Address(), std::span(source).first(bytes));
                } else {
                    Write(mapping.Address(), std::span(source).first(bytes), 1);
                }
            }
            const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 256;
#ifndef _WIN32
            getrusage(RUSAGE_THREAD, &after);
            if (pass != 0) faults += after.ru_minflt - before.ru_minflt;
#endif
            if (pass != 0) times.push_back(us);
            Require(std::memcmp(mapping.data, source.data(), bytes) == 0, "copied data differs");
        }
        std::sort(times.begin(), times.end());
        std::cout << "bytes=" << bytes << " median_us=" << times[4] << " faults=" << faults << " operations=2304\n";
    }
}};

} // namespace
