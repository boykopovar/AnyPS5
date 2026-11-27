#include "prx/libwebrtc/libwebrtc.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RaceChecker check failed at line %u\n", location.line());
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
    RequireInvalidArgument([] { _ZN3rtc11RaceCheckerC1Ev_nid_postfix(nullptr); });
}

void TestUnmappedCheckerIsRejected() {
    errno = 71;
    try {
        _ZN3rtc11RaceCheckerC1Ev_nid_postfix(reinterpret_cast<WebRtcRaceChecker*>(0x1));
    } catch (const std::invalid_argument&) {
        Require(errno == 71);
        return;
    }
    Require(false);
}

void TestReadOnlyCheckerIsRejected() {
#ifdef _WIN32
    const std::size_t size = 0x1000;
    void* base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    const std::size_t size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED)
        base = nullptr;
#endif
    Require(base != nullptr);
#ifdef _WIN32
    DWORD prev = 0;
    Require(VirtualProtect(base, size, PAGE_READONLY, &prev) != 0);
#else
    Require(mprotect(base, size, PROT_READ) == 0);
#endif
    RequireInvalidArgument([&] { _ZN3rtc11RaceCheckerC1Ev_nid_postfix(static_cast<WebRtcRaceChecker*>(base)); });
#ifdef _WIN32
    VirtualFree(base, 0, MEM_RELEASE);
#else
    munmap(base, size);
#endif
}

}

int main() {
    TestConstructorClearsTheAccessCount();
    TestNullCheckerThrows();
    TestUnmappedCheckerIsRejected();
    TestReadOnlyCheckerIsRejected();
    return 0;
}
