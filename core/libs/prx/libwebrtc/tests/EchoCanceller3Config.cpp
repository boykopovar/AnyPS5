#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc EchoCanceller3Config check failed at line %u\n", location.line());
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

struct ByteRange {
    std::size_t first;
    std::size_t last;
};

constexpr ByteRange FirmwareUnwrittenPadding[] {{0x71, 0x77}, {0x14e, 0x14f}, {0x179, 0x17f}, {0x184, 0x187}, {0x249, 0x24f}};

bool MembersEqual(const WebRtcEchoCanceller3Config& left, const WebRtcEchoCanceller3Config& right) {
    for (std::size_t index = 0; index < sizeof(left.bytes); ++index) {
        bool padding = false;
        for (const auto& range : FirmwareUnwrittenPadding) padding = padding || (index >= range.first && index <= range.last);
        if (!padding && left.bytes[index] != right.bytes[index]) return false;
    }
    return true;
}

WebRtcEchoCanceller3Config Pattern(unsigned seed) {
    WebRtcEchoCanceller3Config config;
    for (std::size_t index = 0; index < sizeof(config.bytes); ++index)
        config.bytes[index] = static_cast<unsigned char>(index * 31 + seed);
    return config;
}

void TestCopyConstructor() {
    WebRtcEchoCanceller3Config source = Pattern(7);
    WebRtcEchoCanceller3Config copy;
    std::memset(&copy, 0xA5, sizeof(copy));
    _ZN6webrtc20EchoCanceller3ConfigC1ERKS0__nid_postfix(&copy, &source);
    Require(MembersEqual(copy, source));
}

void TestAssignment() {
    WebRtcEchoCanceller3Config source = Pattern(11);
    WebRtcEchoCanceller3Config target = Pattern(3);
    Require(_ZN6webrtc20EchoCanceller3ConfigaSERKS0__nid_postfix(&target, &source) == &target);
    Require(MembersEqual(target, source));
    const WebRtcEchoCanceller3Config assigned = target;
    Require(_ZN6webrtc20EchoCanceller3ConfigaSERKS0__nid_postfix(&target, &target) == &target);
    Require(std::memcmp(&target, &assigned, sizeof(target)) == 0);
}

void TestNullArgumentsThrow() {
    WebRtcEchoCanceller3Config config = Pattern(5);
    WebRtcEchoCanceller3Config expected = config;
    RequireInvalidArgument([&] { _ZN6webrtc20EchoCanceller3ConfigC1ERKS0__nid_postfix(nullptr, &config); });
    RequireInvalidArgument([&] { _ZN6webrtc20EchoCanceller3ConfigC1ERKS0__nid_postfix(&config, nullptr); });
    RequireInvalidArgument([&] { _ZN6webrtc20EchoCanceller3ConfigaSERKS0__nid_postfix(nullptr, &config); });
    RequireInvalidArgument([&] { _ZN6webrtc20EchoCanceller3ConfigaSERKS0__nid_postfix(&config, nullptr); });
    Require(std::memcmp(&config, &expected, sizeof(config)) == 0);
}

}

int main() {
    TestCopyConstructor();
    TestAssignment();
    TestNullArgumentsThrow();
    return 0;
}
