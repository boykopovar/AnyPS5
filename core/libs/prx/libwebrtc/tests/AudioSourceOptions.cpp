#include "prx/libwebrtc/libwebrtc.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioSourceInterface::options check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

void TestEveryOptionIsUnset() {
    WebRtcAudioOptions options;
    std::memset(&options, 0xA5, sizeof(options));
    int source = 0;
    Require(_ZNK6webrtc20AudioSourceInterface7optionsEv_nid_postfix(&options, &source) == &options);
    Require(!options.echoCancellation.engaged && !options.autoGainControl.engaged);
    Require(!options.noiseSuppression.engaged && !options.highpassFilter.engaged && !options.stereoSwapping.engaged);
    Require(!options.audioJitterBufferMaxPackets.engaged && !options.audioJitterBufferFastAccelerate.engaged);
    Require(!options.audioJitterBufferMinDelayMs.engaged && !options.audioNetworkAdaptor.engaged);
    Require(!options.audioNetworkAdaptorConfig.engaged && !options.initRecordingOnSend.engaged);
}

void TestSourceIsNotRead() {
    WebRtcAudioOptions options;
    std::memset(&options, 0xA5, sizeof(options));
    Require(_ZNK6webrtc20AudioSourceInterface7optionsEv_nid_postfix(&options, nullptr) == &options);
    Require(!options.echoCancellation.engaged && !options.initRecordingOnSend.engaged);
}

void TestNullResultThrows() {
    int source = 0;
    RequireInvalidArgument([&] { _ZNK6webrtc20AudioSourceInterface7optionsEv_nid_postfix(nullptr, &source); });
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

void TestGuardedResultThrows() {
    GuardedPage page;
    page.SetReadOnly();
    int source = 0;
    RequireInvalidArgument([&] { _ZNK6webrtc20AudioSourceInterface7optionsEv_nid_postfix(page.As<WebRtcAudioOptions>(), &source); });
}

}

int main() {
    TestEveryOptionIsUnset();
    TestSourceIsNotRead();
    TestNullResultThrows();
    TestGuardedResultThrows();
    return 0;
}
