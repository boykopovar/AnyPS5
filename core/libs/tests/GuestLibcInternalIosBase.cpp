#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
}

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal ios_base destructor: %s\n", message);
    std::abort();
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

    object.stream.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    object.stream.callbacks = &object;
    Require(Throws([&] { sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(&object.stream); }), "registered callbacks are rejected");
    Require(object.stream.locale == &_ZSt21_sceLibcClassicLocale_nid_postfix, "a rejected destruction keeps the locale");
    Require(Throws([] { sceLibcInternalIosBaseCompleteDestructor_nid_no_patch(nullptr); }), "a null stream is rejected");

    std::puts("libc internal ios_base complete-object destructor checks passed");
    return 0;
}
