#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
}

namespace {

unsigned releases = 0;
void* lastReleased = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal ios_base destructor: %s\n", message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) {
    ++releases;
    lastReleased = pointer;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct GuardedStream {
    std::uint64_t before = 0x12345678;
    GuestLocale::IosBase stream{};
    std::uint64_t after = 0x87654321;
};

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    GuardedStream object;
    object.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(&object.stream);
    Require(object.stream.locale == nullptr, "the destructor releases the classic locale");
    Require(object.before == 0x12345678 && object.after == 0x87654321, "the destructor writes only the stream");

    GuardedStream expected;
    GuardedStream actual;
    expected.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    actual.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    expected.stream.width = actual.stream.width = 7;
    expected.stream.flags = actual.stream.flags = 0x201;
    _ZNSt8ios_baseD2Ev_nid_postfix(&expected.stream);
    sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(&actual.stream);
    Require(actual.stream.locale == expected.stream.locale && actual.stream.width == expected.stream.width && actual.stream.flags == expected.stream.flags,
        "D1 leaves the stream as D2 does");

    Require(Throws([] { sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(nullptr); }), "a null stream is rejected");

    auto* heapStream = new (_Znwm_nid_postfix(sizeof(GuestLocale::IosBase))) GuestLocale::IosBase{};
    heapStream->locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    releases = 0;
    sceLibcInternalIosBaseDeletingDestructor_nid_no_patch(heapStream);
    Require(releases == 1 && lastReleased == heapStream, "the deleting destructor frees the stream through operator delete");
    Require(Throws([] { sceLibcInternalIosBaseDeletingDestructor_nid_no_patch(nullptr); }), "a null stream is rejected by the deleting destructor");

    std::puts("libc internal ios_base destructor checks passed");
    return 0;
}
