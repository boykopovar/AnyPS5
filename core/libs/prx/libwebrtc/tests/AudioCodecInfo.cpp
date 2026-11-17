#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Check(int sampleRateHz, std::uint64_t numChannels, int defaultBitrateBps, int minBitrateBps, int maxBitrateBps) {
    struct Guarded {
        std::array<unsigned char, 16> before;
        WebRtcAudioCodecInfo info;
        std::array<unsigned char, 16> after;
    } guarded;
    std::memset(&guarded, 0xA5, sizeof(guarded));
    guarded.info.allowComfortNoise = false;
    guarded.info.supportsNetworkAdaption = true;
    _ZN6webrtc14AudioCodecInfoC1Eimiii_nid_postfix(&guarded.info, sampleRateHz, numChannels, defaultBitrateBps, minBitrateBps, maxBitrateBps);
    Require(guarded.info.sampleRateHz == sampleRateHz);
    Require(guarded.info.numChannels == numChannels);
    Require(guarded.info.defaultBitrateBps == defaultBitrateBps);
    Require(guarded.info.minBitrateBps == minBitrateBps);
    Require(guarded.info.maxBitrateBps == maxBitrateBps);
    Require(guarded.info.allowComfortNoise);
    Require(!guarded.info.supportsNetworkAdaption);
    for (auto byte : guarded.before) Require(byte == 0xA5);
    for (auto byte : guarded.after) Require(byte == 0xA5);
}

void TestNullObject() {
    ExpectErr<std::invalid_argument>([] { _ZN6webrtc14AudioCodecInfoC1Eimiii_nid_postfix(nullptr, 48000, 1, 32000, 0, 64000); });
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
};

void TestGuardedPointer() {
    GuardedPage page;
    page.SetReadOnly();
    ExpectErr<std::invalid_argument>([&] { _ZN6webrtc14AudioCodecInfoC1Eimiii_nid_postfix(page.As<WebRtcAudioCodecInfo>(), 48000, 1, 32000, 0, 64000); });
}

}

int main() {
    try {
        Check(48000, 1, 32000, 6000, 64000);
        Check(16000, 2, 24000, 24000, 24000);
        Check(96000, 6, 128000, 0, 256000);
        Check(1, std::numeric_limits<std::uint64_t>::max(), 0, 0, 0);
        Check(std::numeric_limits<int>::max(), 1, std::numeric_limits<int>::max(), 0, std::numeric_limits<int>::max());
        Check(48000, 1, 32000, 40000, 64000);
        Check(48000, 1, 32000, 0, 31999);
        Check(0, 0, -5, -1, -9);
        Check(-1, 1, 32000, 0, 64000);
        TestNullObject();
        TestGuardedPointer();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    std::puts("libwebrtc AudioCodecInfo tests passed");
}
