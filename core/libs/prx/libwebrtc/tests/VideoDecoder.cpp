#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/GuestStringStorage.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void* lastFreed = nullptr;
void* lastAllocated = nullptr;

void* APS5_VABI TestAllocate(std::size_t size) { return lastAllocated = std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { lastFreed = pointer; std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoDecoder check failed at line %u\n", location.line());
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

struct FakeDecoder {
    const void* const* vtable;
    const char* name;
    const void* asked;
};

const char* APS5_VABI ImplementationName(const WebRtcVideoDecoder* decoder) {
    auto* fake = reinterpret_cast<FakeDecoder*>(const_cast<WebRtcVideoDecoder*>(decoder));
    fake->asked = decoder;
    return fake->name;
}

std::array<const void*, 9> Vtable() {
    std::array<const void*, 9> vtable {};
    vtable[8] = reinterpret_cast<const void*>(&ImplementationName);
    return vtable;
}

WebRtcVideoDecoderInfo Filled() {
    WebRtcVideoDecoderInfo info;
    std::memset(&info, 0xA5, sizeof(info));
    info.isHardwareAccelerated = true;
    return info;
}

WebRtcVideoDecoderInfo* GetDecoderInfo(WebRtcVideoDecoderInfo* info, FakeDecoder& fake) {
    return _ZNK6webrtc12VideoDecoder14GetDecoderInfoEv_nid_postfix(info, reinterpret_cast<const WebRtcVideoDecoder*>(&fake));
}

void TestInlineName() {
    const auto vtable = Vtable();
    for (const char* name : {"unknown", "", "libvpx-vp9-1234"}) {
        FakeDecoder fake {vtable.data(), name, nullptr};
        WebRtcVideoDecoderInfo info = Filled();
        Require(GetDecoderInfo(&info, fake) == &info);
        Require(fake.asked == &fake);
        Require(info.implementationName.reserved == 0 && info.implementationName.size == std::strlen(name));
        Require(info.implementationName.capacity == 15 && std::strcmp(info.implementationName.buffer, name) == 0);
        Require(!info.isHardwareAccelerated);
    }
}

void TestHeapName() {
    const auto vtable = Vtable();
    const std::string name = "libvpx-vp9-12345";
    FakeDecoder fake {vtable.data(), name.c_str(), nullptr};
    WebRtcVideoDecoderInfo info = Filled();
    GetDecoderInfo(&info, fake);
    Require(info.implementationName.reserved == 0 && info.implementationName.size == 16 && info.implementationName.capacity == 31);
    Require(info.implementationName.pointer == lastAllocated && name == WebRtcGuestStringData(info.implementationName));
    Require(!info.isHardwareAccelerated);
    GuestStringStorage::Deallocate(info.implementationName.pointer, info.implementationName.capacity + 1);
    Require(lastFreed == lastAllocated);
}

void TestLargeName() {
    const auto vtable = Vtable();
    const std::string name(5000, 'n');
    FakeDecoder fake {vtable.data(), name.c_str(), nullptr};
    WebRtcVideoDecoderInfo info = Filled();
    GetDecoderInfo(&info, fake);
    Require(info.implementationName.size == 5000 && info.implementationName.capacity == (5000 | 15));
    Require(reinterpret_cast<std::uintptr_t>(info.implementationName.pointer) % 32 == 0);
    Require(static_cast<void**>(static_cast<void*>(info.implementationName.pointer))[-1] == lastAllocated);
    Require(name == WebRtcGuestStringData(info.implementationName));
    GuestStringStorage::Deallocate(info.implementationName.pointer, info.implementationName.capacity + 1);
    Require(lastFreed == lastAllocated);
}

void TestInvalidArgumentsThrow() {
    auto vtable = Vtable();
    FakeDecoder fake {vtable.data(), nullptr, nullptr};
    WebRtcVideoDecoderInfo info = Filled();
    RequireInvalidArgument([&] { GetDecoderInfo(&info, fake); });
    fake.name = "unknown";
    RequireInvalidArgument([&] { GetDecoderInfo(nullptr, fake); });
    RequireInvalidArgument([&] { _ZNK6webrtc12VideoDecoder14GetDecoderInfoEv_nid_postfix(&info, nullptr); });
    vtable[8] = nullptr;
    RequireInvalidArgument([&] { GetDecoderInfo(&info, fake); });
    FakeDecoder empty {nullptr, "unknown", nullptr};
    RequireInvalidArgument([&] { GetDecoderInfo(&info, empty); });
    Require(info.isHardwareAccelerated);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestInlineName();
    TestHeapName();
    TestLargeName();
    TestInvalidArgumentsThrow();
    return 0;
}
