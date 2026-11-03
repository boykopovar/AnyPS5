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
    std::fprintf(stderr, "libwebrtc ColorSpace check failed at line %u\n", location.line());
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

WebRtcColorSpace Hdr() {
    WebRtcColorSpace colorSpace;
    std::memset(&colorSpace, 0, sizeof(colorSpace));
    colorSpace.primaries = 9;
    colorSpace.transfer = 16;
    colorSpace.matrix = 9;
    colorSpace.range = 1;
    colorSpace.chromaSitingHorizontal = 1;
    colorSpace.chromaSitingVertical = 2;
    colorSpace.hdrMetadata = {{0.708f, 0.292f}, {0.17f, 0.797f}, {0.131f, 0.046f}, {0.3127f, 0.329f}, 1000.0f, 0.005f, 1000, 400};
    colorSpace.hasHdrMetadata = true;
    return colorSpace;
}

bool SameFields(const WebRtcColorSpace& left, const WebRtcColorSpace& right) {
    if (left.primaries != right.primaries || left.transfer != right.transfer || left.matrix != right.matrix || left.range != right.range ||
        left.chromaSitingHorizontal != right.chromaSitingHorizontal || left.chromaSitingVertical != right.chromaSitingVertical ||
        left.hasHdrMetadata != right.hasHdrMetadata)
        return false;
    return !left.hasHdrMetadata || std::memcmp(&left.hdrMetadata, &right.hdrMetadata, sizeof(WebRtcHdrMetadata)) == 0;
}

void TestCopyConstructor() {
    WebRtcColorSpace source = Hdr();
    WebRtcColorSpace copy;
    std::memset(&copy, 0xA5, sizeof(copy));
    _ZN6webrtc10ColorSpaceC1ERKS0__nid_postfix(&copy, &source);
    Require(SameFields(copy, source));
    Require(copy.hdrMetadata.maxContentLightLevel == 1000 && copy.hdrMetadata.maxFrameAverageLightLevel == 400);
    Require(copy.hdrMetadata.whitePoint[0] == 0.3127f && copy.hdrMetadata.luminanceMin == 0.005f);

    WebRtcColorSpace sdr;
    std::memset(&sdr, 0, sizeof(sdr));
    sdr.primaries = 1;
    sdr.transfer = 1;
    sdr.matrix = 1;
    sdr.range = 2;
    std::memset(&copy, 0xA5, sizeof(copy));
    _ZN6webrtc10ColorSpaceC1ERKS0__nid_postfix(&copy, &sdr);
    Require(SameFields(copy, sdr) && !copy.hasHdrMetadata);
}

void TestAssignment() {
    WebRtcColorSpace source = Hdr();
    WebRtcColorSpace target;
    std::memset(&target, 0, sizeof(target));
    Require(_ZN6webrtc10ColorSpaceaSERKS0__nid_postfix(&target, &source) == &target);
    Require(SameFields(target, source));

    WebRtcColorSpace empty;
    std::memset(&empty, 0, sizeof(empty));
    Require(_ZN6webrtc10ColorSpaceaSERKS0__nid_postfix(&target, &empty) == &target);
    Require(SameFields(target, empty) && !target.hasHdrMetadata);

    WebRtcColorSpace self = Hdr();
    Require(_ZN6webrtc10ColorSpaceaSERKS0__nid_postfix(&self, &self) == &self);
    Require(SameFields(self, Hdr()));
}

void TestTailPaddingUntouched() {
    constexpr std::size_t copied = offsetof(WebRtcColorSpace, hasHdrMetadata) + sizeof(bool);
    WebRtcColorSpace source = Hdr();
    unsigned char* sourceBytes = reinterpret_cast<unsigned char*>(&source);
    std::memset(sourceBytes + copied, 0x11, sizeof(source) - copied);
    WebRtcColorSpace copy;
    std::memset(&copy, 0xA5, sizeof(copy));
    _ZN6webrtc10ColorSpaceC1ERKS0__nid_postfix(&copy, &source);
    const unsigned char* copyBytes = reinterpret_cast<const unsigned char*>(&copy);
    Require(std::memcmp(copyBytes, sourceBytes, copied) == 0);
    for (std::size_t i = copied; i < sizeof(copy); ++i)
        Require(copyBytes[i] == 0xA5);

    WebRtcColorSpace target;
    std::memset(&target, 0xA5, sizeof(target));
    Require(_ZN6webrtc10ColorSpaceaSERKS0__nid_postfix(&target, &source) == &target);
    const unsigned char* targetBytes = reinterpret_cast<const unsigned char*>(&target);
    Require(std::memcmp(targetBytes, sourceBytes, copied) == 0);
    for (std::size_t i = copied; i < sizeof(target); ++i)
        Require(targetBytes[i] == 0xA5);
}

void TestNullArgumentsThrow() {
    WebRtcColorSpace colorSpace = Hdr();
    RequireInvalidArgument([&] { _ZN6webrtc10ColorSpaceC1ERKS0__nid_postfix(nullptr, &colorSpace); });
    RequireInvalidArgument([&] { _ZN6webrtc10ColorSpaceC1ERKS0__nid_postfix(&colorSpace, nullptr); });
    RequireInvalidArgument([&] { _ZN6webrtc10ColorSpaceaSERKS0__nid_postfix(nullptr, &colorSpace); });
    RequireInvalidArgument([&] { _ZN6webrtc10ColorSpaceaSERKS0__nid_postfix(&colorSpace, nullptr); });
    Require(SameFields(colorSpace, Hdr()));
}

}

int main() {
    TestCopyConstructor();
    TestAssignment();
    TestTailPaddingUntouched();
    TestNullArgumentsThrow();
    return 0;
}
