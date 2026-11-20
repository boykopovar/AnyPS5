#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#define PSAPI_VERSION 2
#include <windows.h>
#include <psapi.h>

std::size_t HostAllocatorLargeBytes() noexcept;
#endif

namespace {

int failures = 0;

constexpr std::size_t LargestCached = 4096;
constexpr std::size_t LargeThreadBytes = 4 * 1024 * 1024;
constexpr std::size_t LargeClassBytes = 256 * 1024;

template <typename Function>
void OnFreshThread(Function function) {
    std::thread worker(function);
    worker.join();
}

template <typename Function>
void InChunks(std::size_t first, std::size_t last, std::size_t chunk, Function function) {
    for (std::size_t low = first; low <= last; low += chunk) {
        const auto high = std::min(low + chunk - 1, last);
        OnFreshThread([&] { function(low, high); });
    }
}

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

void FillAndVerify(unsigned char* bytes, std::size_t size, unsigned char seed) {
    for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<unsigned char>(seed + i);
    for (std::size_t i = 0; i < size; ++i) {
        if (bytes[i] != static_cast<unsigned char>(seed + i)) {
            Check(false, "a block holds the bytes written to it");
            return;
        }
    }
}

void EverySizeRoundTrips() {
    for (int round = 0; round < 4; ++round) {
        std::vector<unsigned char*> blocks;
        for (std::size_t size = 1; size <= 2048; ++size) {
            auto* block = new unsigned char[size];
#ifdef _WIN32
            Check(_msize(block) >= size, "a block holds at least the bytes asked for");
#endif
            FillAndVerify(block, size, static_cast<unsigned char>(size));
            blocks.push_back(block);
        }
        for (auto* block : blocks) delete[] block;
    }
}

void SizedDeletesKeepTheirClass() {
    for (int round = 0; round < 1000; ++round) {
        for (std::size_t size = 16; size <= LargestCached; size += 16) {
            void* block = ::operator new(size);
            std::memset(block, 0x5a, size);
            ::operator delete(block, size);
        }
    }
    for (std::size_t size = 1; size <= LargestCached; ++size) {
        auto* block = static_cast<unsigned char*>(::operator new(size));
        FillAndVerify(block, size, 3);
        ::operator delete(block, size);
    }
}

void BlocksCrossIntoLibstdcxx() {
    for (int round = 0; round < 2000; ++round) {
        std::string grown(static_cast<std::size_t>(round % 300) + 20, 'a');
        grown.reserve(grown.size() * 3 + 1);
        grown.append(static_cast<std::size_t>(round % 500), 'b');
        std::istringstream input(std::string(static_cast<std::size_t>(round % 700) + 1, 'c') + "\nnext");
        std::string line(40, 'd');
        std::getline(input, line);
        Check(line.size() == static_cast<std::size_t>(round % 700) + 1, "getline replaces the buffer made here");
        Check(grown.size() == static_cast<std::size_t>(round % 300) + 20 + static_cast<std::size_t>(round % 500), "a grown string keeps its bytes");
    }
}

#ifdef _WIN32
void* (*volatile OperatorNew)(std::size_t) = &::operator new;
void (*volatile SizedDelete)(void*, std::size_t) noexcept = &::operator delete;

void SizedDeletesBetweenGranulesComeBack(std::size_t first, std::size_t last) {
    for (std::size_t size = first; size <= last; ++size) {
        if (size % 16 == 0) continue;
        auto* first = static_cast<unsigned char*>(OperatorNew(size));
        FillAndVerify(first, size, 5);
        SizedDelete(first, size);
        auto* again = static_cast<unsigned char*>(OperatorNew(size));
        Check(again == first, "a block freed with its size comes back for that size");
        Check(_msize(again) >= size, "a reused block holds the bytes asked for");
        FillAndVerify(again, size, 6);
        SizedDelete(again, size);
    }
}

void BlocksJustPastTheLargestClassServeIt() {
    for (std::size_t size = LargestCached + 1; size < LargestCached + 16; ++size) {
        void* room = OperatorNew(LargestCached);
        auto* big = static_cast<unsigned char*>(OperatorNew(size));
        SizedDelete(big, size);
        auto* reused = static_cast<unsigned char*>(OperatorNew(LargestCached));
        Check(reused == big, "a block less than a granule over the largest class serves that class");
        Check(_msize(reused) >= LargestCached, "a block kept in the largest class holds it");
        FillAndVerify(reused, LargestCached, 10);
        SizedDelete(reused, LargestCached);
        SizedDelete(room, LargestCached);
    }
}

void OwnBlocksServeTheirWholeGranule(std::size_t first, std::size_t last) {
    for (std::size_t size = first; size <= last; ++size) {
        if (size % 16 == 0) continue;
        const auto whole = (size + 15) / 16 * 16;
        auto* block = static_cast<unsigned char*>(OperatorNew(size));
        SizedDelete(block, size);
        auto* larger = static_cast<unsigned char*>(OperatorNew(whole));
        Check(larger == block, "a block freed with fewer bytes than its granule still serves the whole granule");
        Check(_msize(larger) >= whole, "a block reused for its whole granule holds it");
        FillAndVerify(larger, whole, 7);
        SizedDelete(larger, whole);
    }
}

void LibstdcxxBlocksServeOnlyTheirBytes(std::size_t first, std::size_t last) {
    using Allocation = void* (*)(std::size_t);
    const auto runtime = GetModuleHandleW(L"libstdc++-6.dll");
    const auto allocate = runtime != nullptr ? reinterpret_cast<Allocation>(reinterpret_cast<void*>(GetProcAddress(runtime, "_Znwy"))) : nullptr;
    Check(allocate != nullptr, "libstdc++-6.dll exports operator new");
    if (allocate == nullptr) return;
    std::size_t exact = 0;
    for (std::size_t bytes = first; bytes <= last; ++bytes) {
        if (bytes % 16 == 0) continue;
        const auto whole = (bytes + 15) / 16 * 16;
        void* roomInClass = OperatorNew(whole);
        void* roomBelow = whole > 16 ? OperatorNew(whole - 16) : nullptr;
        void* made = allocate(bytes);
        if (_msize(made) == bytes) ++exact;
        SizedDelete(made, bytes);
        void* same = OperatorNew(bytes);
        Check(same == made, "a block libstdc++ made comes back for the bytes it holds");
        SizedDelete(same, bytes);
        auto* larger = static_cast<unsigned char*>(OperatorNew(whole));
        const bool fits = _msize(larger) >= whole;
        Check(fits, "a block libstdc++ made is never handed out for more bytes than it holds");
        if (fits) FillAndVerify(larger, whole, 8);
        if (roomBelow != nullptr) {
            auto* lower = static_cast<unsigned char*>(OperatorNew(whole - 16));
            Check(lower == made, "a block too small for its class serves the class below");
            Check(_msize(lower) >= whole - 16, "the class below fits in the block");
            FillAndVerify(lower, whole - 16, 9);
            SizedDelete(lower, whole - 16);
            SizedDelete(roomBelow, whole - 16);
        }
        SizedDelete(larger, whole);
        SizedDelete(roomInClass, whole);
    }
    Check(exact > 0, "the C runtime heap reports the exact size libstdc++ asked for");
}
#endif

void BlocksMoveBetweenThreads() {
    constexpr int perThread = 20000;
    std::vector<std::unique_ptr<std::uint64_t[]>> made(perThread);
    std::thread maker([&] {
        for (int i = 0; i < perThread; ++i) {
            made[i] = std::make_unique<std::uint64_t[]>(static_cast<std::size_t>(i % 64) + 1);
            made[i][0] = static_cast<std::uint64_t>(i);
        }
    });
    maker.join();
    std::thread freer([&] {
        for (int i = 0; i < perThread; ++i) {
            Check(made[i][0] == static_cast<std::uint64_t>(i), "a block made on another thread keeps its bytes");
            made[i].reset();
        }
    });
    freer.join();
}

std::atomic<int> exitedThreads{0};

std::size_t PrivateBytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) return 0;
    return counters.PrivateUsage;
#else
    return 0;
#endif
}

void BlocksFlowToAnotherThread() {
    constexpr int rounds = 12;
    constexpr int perRound = 200000;
    std::size_t afterEarlyRounds = 0;
    for (int round = 0; round < rounds; ++round) {
        std::mutex lock;
        std::vector<unsigned char*> handed;
        std::atomic<bool> done{false};
        std::thread freer([&] {
            std::vector<unsigned char*> taken;
            for (;;) {
                const bool finished = done.load(std::memory_order_acquire);
                {
                    std::lock_guard guard(lock);
                    taken.swap(handed);
                }
                for (auto* block : taken) {
                    const auto size = static_cast<std::size_t>(block[0]) * 4 + 1;
                    Check(block[size - 1] == block[0], "a block freed on another thread keeps its bytes until then");
                    ::operator delete(block);
                }
                taken.clear();
                if (finished) break;
                std::this_thread::yield();
            }
        });
        for (int i = 0; i < perRound; ++i) {
            const auto seed = static_cast<unsigned char>(i % 256);
            const auto size = static_cast<std::size_t>(seed) * 4 + 1;
            auto* block = static_cast<unsigned char*>(::operator new(size));
            block[0] = seed;
            block[size - 1] = seed;
            std::lock_guard guard(lock);
            handed.push_back(block);
        }
        done.store(true, std::memory_order_release);
        freer.join();
        if (round == 3) afterEarlyRounds = PrivateBytes();
    }
    const auto afterLastRound = PrivateBytes();
    std::printf("private bytes after 4 and after %d rounds of blocks freed on another thread: %zu, %zu\n", rounds, afterEarlyRounds, afterLastRound);
    Check(afterLastRound < afterEarlyRounds + (64u << 20), "blocks freed on another thread do not pile up");
}

#ifdef _WIN32
void LargeBlocksStayWithinTheirShare() {
    constexpr std::size_t size = 2048;
    std::vector<void*> blocks;
    blocks.reserve(1000);
    for (std::size_t i = 0; i < 1000; ++i) blocks.push_back(OperatorNew(size));
    Check(HostAllocatorLargeBytes() == 0, "a thread that only allocates keeps no large blocks");
    for (auto* block : blocks) SizedDelete(block, size);
    Check(HostAllocatorLargeBytes() == LargeClassBytes, "one large class keeps no more than its share of the budget");
    for (std::size_t i = 0; i < LargeClassBytes / size; ++i) blocks[i] = OperatorNew(size);
    Check(HostAllocatorLargeBytes() == 0, "blocks taken from the cache leave its byte count");
    for (std::size_t i = 0; i < LargeClassBytes / size; ++i) SizedDelete(blocks[i], size);
}

void LargeBlocksStayWithinTheirBudget() {
    std::vector<std::pair<void*, std::size_t>> blocks;
    for (std::size_t size = 1040; size <= LargestCached; size += 16) {
        for (int copy = 0; copy < 16; ++copy) blocks.emplace_back(OperatorNew(size), size);
    }
    for (auto& block : blocks) SizedDelete(block.first, block.second);
    const auto kept = HostAllocatorLargeBytes();
    Check(kept <= LargeThreadBytes, "a thread's large blocks stay within its budget");
    Check(kept + LargestCached > LargeThreadBytes, "a thread's budget fills before large blocks are refused");
    for (std::size_t round = 0; round < 4; ++round) {
        std::vector<void*> taken;
        for (std::size_t size = 1040; size <= LargestCached; size += 16) taken.push_back(OperatorNew(size));
        for (std::size_t i = 0; i < taken.size(); ++i) SizedDelete(taken[i], 1040 + i * 16);
        Check(HostAllocatorLargeBytes() <= LargeThreadBytes, "taking and returning large blocks keeps the budget");
    }
}

void IdleLargeClassesAreTrimmed() {
    std::vector<std::pair<void*, std::size_t>> idle;
    idle.reserve(4096);
    for (std::size_t size = 1040; size <= 3000; size += 16) {
        for (int copy = 0; copy < 24; ++copy) idle.emplace_back(OperatorNew(size), size);
    }
    for (auto& block : idle) SizedDelete(block.first, block.second);
    Check(HostAllocatorLargeBytes() + LargestCached > LargeThreadBytes, "idle classes fill the budget");
    constexpr std::size_t size = LargestCached;
    void* last = nullptr;
    for (std::uint32_t i = 0; i < 3u * 65536u; ++i) {
        last = OperatorNew(size);
        SizedDelete(last, size);
    }
    Check(HostAllocatorLargeBytes() <= 2 * size, "classes idle across a trim give their blocks back");
    void* again = OperatorNew(size);
    Check(again == last, "the class in use keeps its block across the trims");
    SizedDelete(again, size);
}

void ClassesOnlyFreedIntoAreTrimmed() {
    constexpr std::size_t hot = LargestCached;
    constexpr std::size_t cold = 3000;
    std::vector<void*> spare;
    spare.reserve(16);
    for (int i = 0; i < 16; ++i) spare.push_back(OperatorNew(cold));
    std::size_t next = 0;
    for (std::uint32_t i = 0; i < 4u * 65536u; ++i) {
        void* block = OperatorNew(hot);
        SizedDelete(block, hot);
        if (i % 16384u == 0 && i <= 14u * 16384u && next < spare.size()) SizedDelete(spare[next++], cold);
    }
    Check(HostAllocatorLargeBytes() <= hot, "a class that is only freed into gives its blocks back across the trims");
}

void ClassesInUseKeepTheirBlocksAcrossTrims() {
    constexpr std::size_t size = 2048;
    for (std::uint32_t i = 0; i < 3u * 65536u; ++i) {
        void* first = OperatorNew(size);
        void* second = OperatorNew(size);
        SizedDelete(first, size);
        SizedDelete(second, size);
        if (HostAllocatorLargeBytes() != 2 * size) {
            Check(false, "a class in use keeps both its blocks across a trim");
            return;
        }
    }
}

void LargeBlocksFollowTheFreeingThread() {
    constexpr std::size_t size = 3000;
    std::vector<void*> made;
    made.reserve(1000);
    std::size_t madeKept = 1;
    std::thread maker([&] {
        for (int i = 0; i < 1000; ++i) made.push_back(OperatorNew(size));
        madeKept = HostAllocatorLargeBytes();
    });
    maker.join();
    std::size_t freedKept = 0;
    std::thread freer([&] {
        for (auto* block : made) SizedDelete(block, size);
        freedKept = HostAllocatorLargeBytes();
        for (std::size_t i = 0; i < freedKept / size; ++i) made[i] = OperatorNew(size);
        for (std::size_t i = 0; i < freedKept / size; ++i) SizedDelete(made[i], size);
    });
    freer.join();
    Check(madeKept == 0, "the thread that allocated large blocks keeps none of them");
    Check(freedKept > 0 && freedKept <= LargeClassBytes, "the thread that freed large blocks keeps them within one class's share");
}

void LargeBlocksFlowToAnotherThread() {
    constexpr int rounds = 8;
    constexpr int perRound = 50000;
    std::size_t afterEarlyRounds = 0;
    for (int round = 0; round < rounds; ++round) {
        std::mutex lock;
        std::vector<unsigned char*> handed;
        std::atomic<bool> done{false};
        std::thread freer([&] {
            std::vector<unsigned char*> taken;
            for (;;) {
                const bool finished = done.load(std::memory_order_acquire);
                {
                    std::lock_guard guard(lock);
                    taken.swap(handed);
                }
                for (auto* block : taken) {
                    const auto size = static_cast<std::size_t>(block[0]) * 12 + 1025;
                    Check(block[size - 1] == block[0], "a large block freed on another thread keeps its bytes until then");
                    SizedDelete(block, size);
                }
                taken.clear();
                if (finished) break;
                std::this_thread::yield();
            }
        });
        for (int i = 0; i < perRound; ++i) {
            const auto seed = static_cast<unsigned char>(i % 256);
            const auto size = static_cast<std::size_t>(seed) * 12 + 1025;
            auto* block = static_cast<unsigned char*>(OperatorNew(size));
            block[0] = seed;
            block[size - 1] = seed;
            std::lock_guard guard(lock);
            handed.push_back(block);
        }
        done.store(true, std::memory_order_release);
        freer.join();
        if (round == 2) afterEarlyRounds = PrivateBytes();
    }
    const auto afterLastRound = PrivateBytes();
    std::printf("private bytes after 3 and after %d rounds of large blocks freed on another thread: %zu, %zu\n", rounds, afterEarlyRounds, afterLastRound);
    Check(afterLastRound < afterEarlyRounds + (64u << 20), "large blocks freed on another thread do not pile up");
}

void LargeBlocksOfEndedThreadsGoBack() {
    constexpr int waves = 16;
    std::size_t afterEarlyWaves = 0;
    for (int wave = 0; wave < waves; ++wave) {
        std::vector<std::thread> threads;
        for (int t = 0; t < 16; ++t) {
            threads.emplace_back([] {
                std::vector<std::pair<void*, std::size_t>> blocks;
                for (int i = 0; i < 2000; ++i) {
                    const auto size = static_cast<std::size_t>(i % 3072) + 1025;
                    blocks.emplace_back(OperatorNew(size), size);
                }
                for (auto& block : blocks) SizedDelete(block.first, block.second);
            });
        }
        for (auto& thread : threads) thread.join();
        if (wave == 4) afterEarlyWaves = PrivateBytes();
    }
    const auto afterLastWave = PrivateBytes();
    std::printf("private bytes after 5 and after %d waves of ending threads with large blocks: %zu, %zu\n", waves, afterEarlyWaves, afterLastWave);
    Check(afterLastWave < afterEarlyWaves + (128u << 20), "threads that end give their cached large blocks back to the heap");
}

void BlocksGrowPastTheLargestClass() {
    std::vector<std::uint32_t> grown;
    for (std::uint32_t i = 0; i < 100000; ++i) {
        grown.push_back(i * 2654435761u);
        if (i % 4099 == 0) {
            for (std::uint32_t k = 0; k <= i; k += 997) {
                if (grown[k] != k * 2654435761u) {
                    Check(false, "a vector grown past the largest class keeps its elements");
                    return;
                }
            }
        }
    }
    std::string text;
    for (int i = 0; i < 200000; ++i) text.push_back(static_cast<char>('a' + i % 26));
    for (int i = 0; i < 200000; i += 331) {
        if (text[static_cast<std::size_t>(i)] != static_cast<char>('a' + i % 26)) {
            Check(false, "a string grown past the largest class keeps its characters");
            return;
        }
    }
    for (std::size_t size = LargestCached - 64; size <= LargestCached + 256; size += 8) {
        auto* block = static_cast<unsigned char*>(OperatorNew(size));
        FillAndVerify(block, size, 11);
        SizedDelete(block, size);
    }
}
#endif

void ThreadsEndWithCachedBlocks() {
    constexpr int waves = 24;
    std::size_t afterEarlyWaves = 0;
    for (int wave = 0; wave < waves; ++wave) {
        std::vector<std::thread> threads;
        for (int t = 0; t < 32; ++t) {
            threads.emplace_back([] {
                std::vector<void*> blocks;
                for (int i = 0; i < 4000; ++i) blocks.push_back(::operator new(static_cast<std::size_t>(i % 1024) + 1));
                for (auto* block : blocks) ::operator delete(block);
                ++exitedThreads;
            });
        }
        for (auto& thread : threads) thread.join();
        if (wave == 7) afterEarlyWaves = PrivateBytes();
    }
    Check(exitedThreads.load() == waves * 32, "every thread ran to its end");
    const auto afterLastWave = PrivateBytes();
    std::printf("private bytes after 8 and after %d waves of ending threads: %zu, %zu\n", waves, afterEarlyWaves, afterLastWave);
    Check(afterLastWave < afterEarlyWaves + (128u << 20), "threads that end give their cached blocks back to the heap");
    auto* after = new std::uint64_t[128];
    FillAndVerify(reinterpret_cast<unsigned char*>(after), 128 * sizeof(std::uint64_t), 9);
    delete[] after;
}

}

int main() {
    OnFreshThread(EverySizeRoundTrips);
    OnFreshThread(SizedDeletesKeepTheirClass);
    OnFreshThread(BlocksCrossIntoLibstdcxx);
#ifdef _WIN32
    InChunks(8, LargestCached, 64, SizedDeletesBetweenGranulesComeBack);
    OnFreshThread(BlocksJustPastTheLargestClassServeIt);
    InChunks(8, LargestCached, 64, OwnBlocksServeTheirWholeGranule);
    InChunks(8, LargestCached, 16, LibstdcxxBlocksServeOnlyTheirBytes);
    OnFreshThread(LargeBlocksStayWithinTheirShare);
    OnFreshThread(LargeBlocksStayWithinTheirBudget);
    OnFreshThread(IdleLargeClassesAreTrimmed);
    OnFreshThread(ClassesOnlyFreedIntoAreTrimmed);
    OnFreshThread(ClassesInUseKeepTheirBlocksAcrossTrims);
    LargeBlocksFollowTheFreeingThread();
    LargeBlocksFlowToAnotherThread();
    LargeBlocksOfEndedThreadsGoBack();
    OnFreshThread(BlocksGrowPastTheLargestClass);
#endif
    BlocksMoveBetweenThreads();
    BlocksFlowToAnotherThread();
    ThreadsEndWithCachedBlocks();
    if (failures != 0) {
        std::fprintf(stderr, "%d host allocator checks failed\n", failures);
        return 1;
    }
    std::printf("Host allocator tests passed\n");
    return 0;
}
