#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc I420Buffer check failed at line %u\n", location.line());
    std::abort();
}

void Check(std::int32_t width, std::int32_t height, std::int32_t strideY, std::int32_t strideU, std::int32_t strideV) {
    const std::int32_t chromaHeight = (height + 1) / 2;
    std::vector<std::uint8_t> data(static_cast<std::size_t>(strideY * height + (strideU + strideV) * chromaHeight) + 1);
    WebRtcI420Buffer buffer {nullptr, width, height, strideY, strideU, strideV, data.data()};
    Require(_ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(&buffer) == data.data());
    Require(_ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(&buffer) == data.data() + strideY * height);
    Require(_ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(&buffer) == data.data() + strideY * height + strideU * chromaHeight);
}

}

int main() {
    Check(640, 480, 640, 320, 320);
    Check(641, 361, 641, 321, 321);
    Check(1, 1, 1, 1, 1);
    Check(1920, 1080, 1984, 992, 1000);
    Check(0, 0, 0, 0, 0);
    try {
        _ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return 0;
    }
    Require(false);
    return 1;
}
