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
#endif

namespace {

int failures = 0;

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
        for (std::size_t size = 16; size <= 1024; size += 16) {
            void* block = ::operator new(size);
            std::memset(block, 0x5a, size);
            ::operator delete(block, size);
        }
    }
    for (std::size_t size = 1; size <= 1024; ++size) {
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
    EverySizeRoundTrips();
    SizedDeletesKeepTheirClass();
    BlocksCrossIntoLibstdcxx();
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
