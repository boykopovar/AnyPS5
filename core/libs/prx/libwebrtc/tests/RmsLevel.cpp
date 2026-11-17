#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

using ConstructFn = void (APS5_VABI*)(WebRtcRmsLevel*);
using AnalyzeFn = void (APS5_VABI*)(WebRtcRmsLevel*, const std::int16_t*, std::size_t);
using AverageFn = int (APS5_VABI*)(WebRtcRmsLevel*);

const auto construct = reinterpret_cast<ConstructFn>(&_ZN6webrtc8RmsLevelC1Ev_nid_postfix);
const auto destroy = reinterpret_cast<ConstructFn>(&_ZN6webrtc8RmsLevelD1Ev_nid_postfix);
const auto analyze = reinterpret_cast<AnalyzeFn>(&_ZN6webrtc8RmsLevel7AnalyzeEN3rtc9ArrayViewIKsLln4711EEE_nid_postfix);
const auto average = reinterpret_cast<AverageFn>(&_ZN6webrtc8RmsLevel7AverageEv_nid_postfix);

bool Cleared(const WebRtcRmsLevel& level) {
    return level.sumSquare == 0.f && level.sampleCount == 0 && level.maxSumSquare == 0.f && !level.hasBlockSize;
}

WebRtcRmsLevel Constructed() {
    WebRtcRmsLevel level;
    std::memset(&level, 0x5a, sizeof(level));
    construct(&level);
    return level;
}

void Analyze(WebRtcRmsLevel& level, const std::vector<std::int16_t>& samples) {
    analyze(&level, samples.data(), samples.size());
}

void TestConstruct() {
    auto level = Constructed();
    Require(Cleared(level));
    Require(average(&level) == 127);
    Require(Cleared(level));
    destroy(&level);
}

void TestLevels() {
    auto level = Constructed();
    Analyze(level, std::vector<std::int16_t>(160, 1000));
    Require(level.sampleCount == 160 && level.hasBlockSize && level.blockSize == 160);
    Require(level.sumSquare == 1.6e8f && level.maxSumSquare == 1.6e8f);
    Analyze(level, std::vector<std::int16_t>(160, 0));
    Require(level.sampleCount == 320 && level.sumSquare == 1.6e8f && level.maxSumSquare == 1.6e8f);
    Require(average(&level) == 33);
    Require(Cleared(level));

    Analyze(level, std::vector<std::int16_t>(160, 1000));
    Require(average(&level) == 30);
    Analyze(level, std::vector<std::int16_t>(160, 1));
    Require(average(&level) == 90);
    std::vector<std::int16_t> fullScale(160);
    for (std::size_t index = 0; index < fullScale.size(); ++index) fullScale[index] = index % 2 == 0 ? 32767 : -32767;
    Analyze(level, fullScale);
    Require(average(&level) == 0);
    Analyze(level, std::vector<std::int16_t>(160, 0));
    Require(average(&level) == 127);
    std::vector<std::int16_t> faint(4800, 0);
    faint[17] = 1;
    Analyze(level, faint);
    Require(average(&level) == 126);
}

void TestBlockSize() {
    auto level = Constructed();
    Analyze(level, std::vector<std::int16_t>(160, 1000));
    analyze(&level, nullptr, 0);
    Require(level.sampleCount == 160 && level.blockSize == 160);
    Analyze(level, std::vector<std::int16_t>(80, 0));
    Require(level.sampleCount == 80 && level.blockSize == 80 && level.sumSquare == 0.f && level.maxSumSquare == 0.f);
    Require(average(&level) == 127);
}

void TestNull() {
    ExpectErr<std::invalid_argument>([] { construct(nullptr); });
    ExpectErr<std::invalid_argument>([] { destroy(nullptr); });
    ExpectErr<std::invalid_argument>([] { analyze(nullptr, nullptr, 0); });
    ExpectErr<std::invalid_argument>([] { average(nullptr); });
}

struct GuardedPage {
    void* base = nullptr;
    std::size_t size = 0;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED)
            base = nullptr;
#endif
        Require(base != nullptr);
    }

    ~GuardedPage() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    template<typename T> T* As() const { return static_cast<T*>(base); }

    void SetReadOnly() const {
#ifdef _WIN32
        DWORD prev = 0;
        Require(VirtualProtect(base, size, PAGE_READONLY, &prev) != 0);
#else
        Require(mprotect(base, size, PROT_READ) == 0);
#endif
    }

    void SetInaccessible() const {
#ifdef _WIN32
        DWORD prev = 0;
        Require(VirtualProtect(base, size, PAGE_NOACCESS, &prev) != 0);
#else
        Require(mprotect(base, size, PROT_NONE) == 0);
#endif
    }
};

void TestGuardedPointers() {
    {
        GuardedPage page;
        construct(page.As<WebRtcRmsLevel>());
        page.SetReadOnly();
        std::vector<std::int16_t> samples(160, 1000);
        ExpectErr<std::invalid_argument>([&] { construct(page.As<WebRtcRmsLevel>()); });
        ExpectErr<std::invalid_argument>([&] { analyze(page.As<WebRtcRmsLevel>(), samples.data(), samples.size()); });
        ExpectErr<std::invalid_argument>([&] { average(page.As<WebRtcRmsLevel>()); });
    }
    {
        GuardedPage dataPage;
        dataPage.SetInaccessible();
        auto level = Constructed();
        ExpectErr<std::invalid_argument>([&] { analyze(&level, dataPage.As<const std::int16_t>(), 160); });
    }
}

}

int main() {
    TestConstruct();
    TestLevels();
    TestBlockSize();
    TestNull();
    TestGuardedPointers();
    std::puts("libwebrtc RmsLevel tests passed");
    return 0;
}
