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

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

const std::uint8_t* APS5_VABI BaseDataY(const WebRtcI420Buffer* buffer) {
    return buffer->data;
}

const std::uint8_t* APS5_VABI BaseDataU(const WebRtcI420Buffer* buffer) {
    return buffer->data + buffer->strideY * buffer->height;
}

const std::uint8_t* APS5_VABI BaseDataV(const WebRtcI420Buffer* buffer) {
    return BaseDataU(buffer) + buffer->strideU * ((buffer->height + 1) / 2);
}

std::uint8_t overridePlanes[3];

const std::uint8_t* APS5_VABI DerivedDataY(const WebRtcI420Buffer*) {
    return &overridePlanes[0];
}

const std::uint8_t* APS5_VABI DerivedDataU(const WebRtcI420Buffer*) {
    return &overridePlanes[1];
}

const std::uint8_t* APS5_VABI DerivedDataV(const WebRtcI420Buffer*) {
    return &overridePlanes[2];
}

struct Vtable {
    const void* slots[20];
};

Vtable MakeVtable(const void* dataY, const void* dataU, const void* dataV) {
    Vtable vtable {};
    vtable.slots[17] = dataY;
    vtable.slots[18] = dataU;
    vtable.slots[19] = dataV;
    return vtable;
}

const Vtable i420Vtable = MakeVtable(reinterpret_cast<const void*>(BaseDataY), reinterpret_cast<const void*>(BaseDataU), reinterpret_cast<const void*>(BaseDataV));
const Vtable derivedVtable = MakeVtable(reinterpret_cast<const void*>(DerivedDataY), reinterpret_cast<const void*>(DerivedDataU), reinterpret_cast<const void*>(DerivedDataV));

void Check(std::int32_t width, std::int32_t height, std::int32_t strideY, std::int32_t strideU, std::int32_t strideV) {
    const std::int32_t chromaHeight = (height + 1) / 2;
    std::vector<std::uint8_t> data(static_cast<std::size_t>(strideY * height + (strideU + strideV) * chromaHeight) + 1);
    WebRtcI420Buffer buffer {i420Vtable.slots, width, height, strideY, strideU, strideV, data.data()};
    Require(_ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(&buffer) == data.data());
    Require(_ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(&buffer) == data.data() + strideY * height);
    Require(_ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(&buffer) == data.data() + strideY * height + strideU * chromaHeight);
}

void TestOverriddenGetters() {
    std::vector<std::uint8_t> data(640 * 480 * 3 / 2);
    WebRtcI420Buffer buffer {derivedVtable.slots, 640, 480, 640, 320, 320, data.data()};
    Require(_ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(&buffer) == &overridePlanes[0]);
    Require(_ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(&buffer) == &overridePlanes[1]);
    Require(_ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(&buffer) == &overridePlanes[2]);
}

void TestNullBufferThrows() {
    RequireInvalidArgument([] { _ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(nullptr); });
}

}

int main() {
    Check(640, 480, 640, 320, 320);
    Check(641, 361, 641, 321, 321);
    Check(1, 1, 1, 1, 1);
    Check(1920, 1080, 1984, 992, 1000);
    Check(0, 0, 0, 0, 0);
    TestOverriddenGetters();
    TestNullBufferThrows();
    return 0;
}
