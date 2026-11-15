#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/IosBase.hpp"
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

    GuardedStream flagged;
    flagged.stream.flags = 0x201;
    flagged.stream.width = 7;
    flagged.stream.precision = 6;
    const auto set = _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(0x8040);
    Require(set.function != nullptr && set.argument == 0x8040, "setiosflags carries its mask");
    Require(set.function(&flagged.stream, set.argument) == &flagged.stream, "the setiosflags function returns its stream");
    Require(flagged.stream.flags == 0x8241, "setiosflags sets the masked flags and keeps the others");
    Require(flagged.stream.width == 7 && flagged.stream.precision == 6, "setiosflags leaves width and precision alone");
    Require(flagged.before == 0x12345678 && flagged.after == 0x87654321, "setiosflags writes only the stream");
    set.function(&flagged.stream, set.argument);
    Require(flagged.stream.flags == 0x8241, "setting the same flags again changes nothing");
    const auto none = _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(0);
    none.function(&flagged.stream, none.argument);
    Require(flagged.stream.flags == 0x8241, "an empty mask changes nothing");
    const auto wide = _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(0x30004);
    flagged.stream.flags = 0x20001;
    wide.function(&flagged.stream, wide.argument);
    Require(flagged.stream.flags == 0x5, "bits above the format mask are cleared where the mask names them and never set");
    const auto all = _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(-1);
    flagged.stream.flags = 0;
    all.function(&flagged.stream, all.argument);
    Require(flagged.stream.flags == 0xffff, "a full mask sets every format flag");
    Require(_ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(1).function == set.function, "every setiosflags result shares one function");
    Require(Throws([&] { set.function(nullptr, set.argument); }), "a null stream is rejected by setiosflags");

    const auto reset = sceLibcInternalResetiosflags_nid_no_patch(0x8040);
    flagged.stream.flags = 0x8241;
    flagged.stream.width = 7;
    flagged.stream.precision = 6;
    Require(reset.function != nullptr && reset.function != set.function && reset.argument == 0x8040, "resetiosflags carries its mask");
    Require(reset.function(&flagged.stream, reset.argument) == &flagged.stream, "the resetiosflags function returns its stream");
    Require(flagged.stream.flags == 0x201, "resetiosflags clears the masked flags and keeps the others");
    Require(flagged.stream.width == 7 && flagged.stream.precision == 6, "resetiosflags leaves width and precision alone");
    reset.function(&flagged.stream, reset.argument);
    Require(flagged.stream.flags == 0x201, "clearing the same flags again changes nothing");
    const auto resetNone = sceLibcInternalResetiosflags_nid_no_patch(0);
    resetNone.function(&flagged.stream, resetNone.argument);
    Require(flagged.stream.flags == 0x201, "an empty reset mask changes nothing");
    const auto resetAll = sceLibcInternalResetiosflags_nid_no_patch(-1);
    flagged.stream.flags = 0x2ffff;
    resetAll.function(&flagged.stream, resetAll.argument);
    Require(flagged.stream.flags == 0, "a full reset mask clears every flag");
    Require(Throws([&] { reset.function(nullptr, reset.argument); }), "a null stream is rejected by resetiosflags");

    struct BaseCase { std::int32_t base; std::int32_t field; };
    for (const BaseCase check : {BaseCase{8, 0x20}, BaseCase{10, 0x10}, BaseCase{16, 0x40}, BaseCase{0, 0}, BaseCase{2, 0}, BaseCase{-8, 0}, BaseCase{36, 0}}) {
        const auto base = sceLibcInternalSetbase_nid_no_patch(check.base);
        Require(base.function != nullptr && base.argument == check.base, "setbase carries its base");
        for (const std::int32_t before : {0, 0x10, 0x20, 0x40, 0x70}) {
            flagged.stream.flags = 0x8201 | before;
            Require(base.function(&flagged.stream, base.argument) == &flagged.stream, "the setbase function returns its stream");
            Require(flagged.stream.flags == (0x8201 | check.field), "setbase replaces the base field and keeps the other flags");
        }
    }
    Require(flagged.stream.width == 7 && flagged.stream.precision == 6, "setbase leaves width and precision alone");
    Require(flagged.before == 0x12345678 && flagged.after == 0x87654321, "the manipulators write only the stream");
    const auto decimal = sceLibcInternalSetbase_nid_no_patch(10);
    Require(Throws([&] { decimal.function(nullptr, decimal.argument); }), "a null stream is rejected by setbase");

    std::puts("libc internal ios_base checks passed");
    return 0;
}
