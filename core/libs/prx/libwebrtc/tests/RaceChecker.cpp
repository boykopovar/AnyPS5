#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RaceChecker check failed at line %u\n", location.line());
    std::abort();
}

void TestConstructorClearsTheAccessCount() {
    unsigned char storage[sizeof(WebRtcRaceChecker)];
    std::memset(storage, 0xA5, sizeof(storage));
    auto* checker = reinterpret_cast<WebRtcRaceChecker*>(storage);
    _ZN3rtc11RaceCheckerC1Ev_nid_postfix(checker);
    std::int32_t count = 1;
    std::memcpy(&count, storage, sizeof(count));
    Require(count == 0);
    for (std::size_t index = sizeof(std::int32_t); index < sizeof(storage); ++index)
        Require(storage[index] == 0xA5);
}

void TestNullCheckerThrows() {
    try {
        _ZN3rtc11RaceCheckerC1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        address_ = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
        Require(address_ != nullptr);
#else
        void* mapped = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        address_ = mapped;
#endif
    }
    ~GuardedPage() {
        if (!address_)
            return;
#ifdef _WIN32
        VirtualFree(address_, 0, MEM_RELEASE);
#else
        munmap(address_, 4096);
#endif
    }
    template<typename T>
    T* As() const {
        return static_cast<T*>(address_);
    }

private:
    void* address_ = nullptr;
};

void TestGuardedCheckerThrows() {
    GuardedPage guard;
    try {
        _ZN3rtc11RaceCheckerC1Ev_nid_postfix(guard.As<WebRtcRaceChecker>());
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    TestConstructorClearsTheAccessCount();
    TestNullCheckerThrows();
    TestGuardedCheckerThrows();
    return 0;
}
