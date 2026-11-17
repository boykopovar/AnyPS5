#include "prx/libSceNet/include/Net.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <thread>
#include <vector>

static void Require(bool condition) {
    if (!condition) std::abort();
}

static void Exhausted() {
    *sceNetErrnoLoc() = 33;
    Require(sceNetPoolCreate("overflow", 4096, 0) == static_cast<int>(0x80410117u));
    Require(*sceNetErrnoLoc() == 23);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string_view argument(argv[1]);
    std::array<std::uint32_t, 8> parameter{};
    const bool initialized = argument != "unset";
    if (initialized) {
        parameter[0] = static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 16));
        Require(sceNetInitParam(reinterpret_cast<const NetInitializeParameter*>(parameter.data())) == 0);
    }
    const std::size_t capacity = (parameter[0] & 1u) != 0 ? 128 : 32;
    std::vector<int> pools;
    for (std::size_t i = 0; i < capacity; ++i) {
        const int id = sceNetPoolCreate("capacity", 4096, 0);
        Require(id > 0);
        for (const int previous : pools) Require(previous != id);
        pools.push_back(id);
    }
    Exhausted();
    const int released = pools.back();
    pools.pop_back();
    Require(sceNetPoolDestroy(released) == 0);
    const int replacement = sceNetPoolCreate("replacement", 8192, 0);
    Require(replacement > 0);
    NetMemoryPoolStats stats{};
    Require(sceNetGetMemoryPoolStats(replacement, &stats) == 0 && stats.pool_size == 8192);
    for (const int id : pools) {
        Require(id != replacement);
        Require(sceNetGetMemoryPoolStats(id, &stats) == 0 && stats.pool_size == 4096);
    }
    pools.push_back(replacement);
    Exhausted();
    Require(sceNetTerm() == 0);
    Exhausted();
    for (const int id : pools) Require(sceNetPoolDestroy(id) == 0);

    std::array<std::vector<int>, 8> allocated;
    std::array<std::thread, 8> workers;
    for (std::size_t worker = 0; worker < workers.size(); ++worker) {
        workers[worker] = std::thread([&, worker] {
            for (int attempt = 0; attempt < 20; ++attempt) {
                *sceNetErrnoLoc() = 33;
                const int id = sceNetPoolCreate("concurrent", 4096, 0);
                if (id > 0) allocated[worker].push_back(id);
                else Require(id == static_cast<int>(0x80410117u) && *sceNetErrnoLoc() == 23);
            }
        });
    }
    for (auto& worker : workers) worker.join();
    std::size_t count = 0;
    for (const auto& ids : allocated) {
        count += ids.size();
        for (const int id : ids) Require(sceNetPoolDestroy(id) == 0);
    }
    Require(count == capacity);
    std::printf("pool capacity %zu: sequential, release, term and concurrent checks passed\n", capacity);
}
