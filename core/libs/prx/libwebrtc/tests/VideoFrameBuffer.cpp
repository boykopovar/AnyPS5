#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoFrameBuffer check failed at line %u\n", location.line());
    std::abort();
}

struct FakeBuffer {
    WebRtcVideoFrameBuffer base;
    const void* vtable[16];
    int addRefs = 0;
    std::int32_t type = 0;
    FakeBuffer(void (APS5_VABI* addRef)(const WebRtcVideoFrameBuffer*), std::int32_t (APS5_VABI* typeGetter)(const WebRtcVideoFrameBuffer*), std::int32_t typeValue)
        : base{vtable}, vtable{}, type(typeValue) {
        vtable[0] = reinterpret_cast<const void*>(addRef);
        vtable[4] = reinterpret_cast<const void*>(typeGetter);
    }
};

FakeBuffer* Fake(const WebRtcVideoFrameBuffer* buffer) {
    return reinterpret_cast<FakeBuffer*>(const_cast<WebRtcVideoFrameBuffer*>(buffer));
}

void APS5_VABI CountAddRef(const WebRtcVideoFrameBuffer* buffer) {
    ++Fake(buffer)->addRefs;
}

void APS5_VABI CountAddRefTwice(const WebRtcVideoFrameBuffer* buffer) {
    Fake(buffer)->addRefs += 2;
}

std::int32_t APS5_VABI FieldType(const WebRtcVideoFrameBuffer* buffer) {
    return Fake(buffer)->type;
}

std::int32_t APS5_VABI AlwaysNative(const WebRtcVideoFrameBuffer*) {
    return 0;
}

void TestGetI420() {
    FakeBuffer buffer(CountAddRef, FieldType, 1);
    Require(_ZNK6webrtc19I420BufferInterface7GetI420Ev_nid_postfix(&buffer.base) == &buffer.base);
    Require(buffer.addRefs == 0);
}

void TestToI420() {
    FakeBuffer first(CountAddRef, FieldType, 1);
    FakeBuffer second(CountAddRefTwice, FieldType, 1);
    void* result = nullptr;
    Require(_ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(&result, &first.base) == &result);
    Require(result == &first.base);
    Require(first.addRefs == 1 && second.addRefs == 0);
    void* other = nullptr;
    Require(_ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(&other, &second.base) == &other);
    Require(other == &second.base);
    Require(first.addRefs == 1 && second.addRefs == 2);
}

void TestGetMappedFrameBuffer() {
    FakeBuffer native(CountAddRef, FieldType, 0);
    FakeBuffer overridden(CountAddRef, AlwaysNative, 1);
    const std::int32_t types[] = {0, 1};
    const WebRtcVideoFrameBufferTypes view{types, 2};
    void* result = &native;
    Require(_ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(&result, &native.base, view) == &result);
    Require(result == nullptr);
    Require(native.addRefs == 0);
    result = &native;
    _ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(&result, &native.base, WebRtcVideoFrameBufferTypes{nullptr, 0});
    Require(result == nullptr);
    result = &overridden;
    Require(_ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(&result, &overridden.base, view) == &result);
    Require(result == nullptr);
}

}

int main() {
    TestGetI420();
    TestToI420();
    TestGetMappedFrameBuffer();
    return 0;
}
