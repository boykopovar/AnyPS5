#include "prx/libwebrtc/libwebrtc.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

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

void TestGuardedPointersThrow() {
#ifdef _WIN32
    SYSTEM_INFO sys{};
    GetSystemInfo(&sys);
    const std::size_t pageSize = sys.dwPageSize;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(page != nullptr);
    DWORD oldProtect = 0;
    Require(VirtualProtect(page, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* page = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
    Require(mprotect(page, pageSize, PROT_READ) == 0);
#endif
    FakeBuffer native(CountAddRef, FieldType, 0);
    const WebRtcVideoFrameBufferTypes emptyView{nullptr, 0};
    auto expectThrow = [](auto call) {
        try {
            call();
        } catch (const std::invalid_argument&) {
            return;
        }
        Require(false);
    };
    expectThrow([&] { _ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(static_cast<void**>(page), &native.base); });
    expectThrow([&] { _ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(static_cast<void**>(page), &native.base, emptyView); });
#ifdef _WIN32
    Require(VirtualProtect(page, pageSize, PAGE_NOACCESS, &oldProtect) != 0);
#else
    Require(mprotect(page, pageSize, PROT_NONE) == 0);
#endif
    void* out = nullptr;
    expectThrow([&] { _ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(&out, static_cast<WebRtcVideoFrameBuffer*>(page)); });
    expectThrow([&] { _ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(&out, static_cast<WebRtcVideoFrameBuffer*>(page), emptyView); });
    Require(native.addRefs == 0);
#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, pageSize);
#endif
}

}

int main() {
    TestGetI420();
    TestToI420();
    TestGetMappedFrameBuffer();
    TestGuardedPointersThrow();
    return 0;
}
