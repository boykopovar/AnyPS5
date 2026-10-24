#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
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
    ThreadsEndWithCachedBlocks();
    if (failures != 0) {
        std::fprintf(stderr, "%d host allocator checks failed\n", failures);
        return 1;
    }
    std::printf("Host allocator tests passed\n");
    return 0;
}
