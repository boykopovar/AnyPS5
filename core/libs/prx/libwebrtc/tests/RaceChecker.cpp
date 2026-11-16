#include "prx/libwebrtc/libwebrtc.hpp"

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

}

int main() {
    TestConstructorClearsTheAccessCount();
    TestNullCheckerThrows();
    return 0;
}
