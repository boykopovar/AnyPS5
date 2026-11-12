#include "prx/libSceCompanionUtil/libSceCompanionUtil.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr std::int32_t kNoEvent = static_cast<std::int32_t>(0x80AD0008);
constexpr std::uint8_t kSentinel = 0x5a;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "libSceCompanionUtil GetEvent check failed at line %u\n", location.line());
    std::abort();
}

void CheckBufferIsAnsweredWithoutBeingTouched() {
    std::uint8_t buffer[64];
    std::memset(buffer, kSentinel, sizeof(buffer));
    Require(sceCompanionUtilGetEvent(buffer) == kNoEvent);
    for (const std::uint8_t byte : buffer) Require(byte == kSentinel);
}

}

int main() {
    Require(sceCompanionUtilGetEvent(nullptr) == kNoEvent);
    CheckBufferIsAnsweredWithoutBeingTouched();
    return 0;
}
