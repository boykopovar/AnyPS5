#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("pipelined DMA: " + what);
}

std::array<std::uint32_t, 7> DmaData(bool immediate, std::uint64_t source, std::uint32_t data, std::uint64_t destination, std::uint32_t bytes) {
    return {0xc0055000u, immediate ? (2u << 29u) : 0u, immediate ? data : static_cast<std::uint32_t>(source), immediate ? 0u : static_cast<std::uint32_t>(source >> 32u), static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u), bytes};
}

void Submit(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
    _putenv_s("APS5_PIPELINE_DMA", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
    setenv("APS5_PIPELINE_DMA", "1", 1);
#endif
    try {
        alignas(256) static std::array<std::uint32_t, 1024> first{};
        alignas(256) static std::array<std::uint32_t, 1024> second{};
        first.fill(0);
        second.fill(0);
        using namespace AgcDriver::DriverDetail;
        const auto drainedBefore = storesDrained.load();
        const auto cpuBefore = storesOnCpu.load() + storesOnGpu.load() + storesBehindCompletions.load();
        std::vector<std::uint32_t> words;
        const auto fill = DmaData(true, 0, 0x5eed1234u, reinterpret_cast<std::uintptr_t>(first.data()), static_cast<std::uint32_t>(sizeof(first)));
        const auto copy = DmaData(false, reinterpret_cast<std::uintptr_t>(first.data()), 0, reinterpret_cast<std::uintptr_t>(second.data()), 512);
        words.insert(words.end(), fill.begin(), fill.end());
        words.insert(words.end(), copy.begin(), copy.end());
        Submit(words);
        AgcDriverWaitIdle_nid_postfix();
        for (const auto word : first) check(word == 0x5eed1234u, "the immediate fill did not land");
        for (std::size_t i = 0; i < second.size(); ++i) check(second[i] == (i < 128 ? 0x5eed1234u : 0u), "the copy did not see the fill before it, or wrote past its bytes");
        const auto stores = storesOnCpu.load() + storesOnGpu.load() + storesBehindCompletions.load() - cpuBefore;
        check(stores == 2, "the DMA_DATA packets were not committed (" + std::to_string(stores) + " committed stores)");
        check(storesDrained.load() == drainedBefore, "a DMA_DATA packet still drained");
        LibcRunShutdown_nid_postfix();
        std::puts("pipelined DMA tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
