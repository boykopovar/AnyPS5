#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>

namespace {

constexpr int GuestEilseq = 86;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libc internal multibyte check failed at line %u\n", location.line());
    std::abort();
}

template <typename TCall>
void RejectsPreservingErrno(TCall call, std::source_location location = std::source_location::current()) {
    errno = 71;
    bool rejected = false;
    try {
        call();
    } catch (const std::exception&) {
        rejected = errno == 71;
    }
    Require(rejected, location);
}

void CheckMbtowc() {
    std::array<unsigned char, 16> state{};
    std::uint16_t wide = 0xffff;

    Require(_Mbtowc_nid_postfix(&wide, "A", 1, state.data()) == 1);
    Require(wide == 0x0041);
    Require(mbsinit_nid_postfix(state.data()) != 0);

    wide = 0x1234;
    Require(_Mbtowc_nid_postfix(&wide, "", 1, state.data()) == 0);
    Require(wide == 0);

    const char twoByte[] = "\xC3\xA9";
    wide = 0;
    Require(_Mbtowc_nid_postfix(&wide, twoByte, 2, state.data()) == 2);
    Require(wide == 0x00E9);

    const char threeByte[] = "\xE3\x81\x82";
    wide = 0;
    Require(_Mbtowc_nid_postfix(&wide, threeByte, 1, state.data()) == -2);
    Require(wide == 0);
    Require(mbsinit_nid_postfix(state.data()) == 0);
    Require(_Mbtowc_nid_postfix(&wide, threeByte + 1, 2, state.data()) == 2);
    Require(wide == 0x3042);
    Require(mbsinit_nid_postfix(state.data()) != 0);

    Require(_Mbtowc_nid_postfix(nullptr, threeByte, 3, state.data()) == 3);
    Require(_Mbtowc_nid_postfix(&wide, "Z", 0, state.data()) == -2);

    Require(_Mbtowc_nid_postfix(&wide, threeByte, 1, state.data()) == -2);
    Require(mbsinit_nid_postfix(state.data()) == 0);
    Require(_Mbtowc_nid_postfix(nullptr, nullptr, 0, state.data()) == 0);
    Require(mbsinit_nid_postfix(state.data()) != 0);

    wide = 0xBEEF;
    errno = 0;
    const char invalid[] = "\xF0\x9F\x98\x80";
    Require(_Mbtowc_nid_postfix(&wide, invalid, 4, state.data()) == -1);
    Require(errno == GuestEilseq);
    Require(wide == 0xBEEF);

    wide = 0;
    Require(_Mbtowc_nid_postfix(&wide, "B", 1, nullptr) == 1);
    Require(wide == 0x0042);
    Require(_Mbtowc_nid_postfix(nullptr, nullptr, 0, nullptr) == 0);
}

void CheckWctombAndRoundTrip() {
    std::array<unsigned char, 16> state{};
    state.fill(0x5a);
    Require(_Wctomb_nid_postfix(nullptr, 0, state.data()) == 0);
    for (unsigned char byte : state) Require(byte == 0);
    Require(_Wctomb_nid_postfix(nullptr, 0, nullptr) == 0);

    char out[4]{};
    Require(_Wctomb_nid_postfix(out, 0x0041, state.data()) == 1);
    Require(out[0] == 'A');

    Require(_Wctomb_nid_postfix(out, 0x0000, state.data()) == 1);
    Require(out[0] == '\0');

    Require(_Wctomb_nid_postfix(out, 0x00E9, state.data()) == 2);
    Require(static_cast<unsigned char>(out[0]) == 0xC3 && static_cast<unsigned char>(out[1]) == 0xA9);

    Require(_Wctomb_nid_postfix(out, 0x3042, state.data()) == 3);
    Require(static_cast<unsigned char>(out[0]) == 0xE3 && static_cast<unsigned char>(out[1]) == 0x81 &&
            static_cast<unsigned char>(out[2]) == 0x82);

    const std::uint16_t samples[] = {0x0001, 0x007F, 0x0080, 0x07FF, 0x0800, 0x3042, 0xFFFF};
    for (std::uint16_t value : samples) {
        std::array<unsigned char, 16> roundState{};
        char encoded[4]{};
        const int length = _Wctomb_nid_postfix(encoded, value, roundState.data());
        Require(length >= 1 && length <= 3);
        std::uint16_t decoded = 0;
        Require(_Mbtowc_nid_postfix(&decoded, encoded, static_cast<std::size_t>(length), roundState.data()) == length);
        Require(decoded == value);
    }
}

void CheckGuestPointers() {
    std::array<unsigned char, 16> state{};
    std::uint16_t wide = 0;
    auto* unmappedChar = reinterpret_cast<char*>(std::uintptr_t{0x10});
    auto* unmappedWide = reinterpret_cast<std::uint16_t*>(std::uintptr_t{0x10});
    auto* unmappedState = reinterpret_cast<void*>(std::uintptr_t{0x10});

    RejectsPreservingErrno([&] { _Mbtowc_nid_postfix(&wide, unmappedChar, 1, state.data()); });
    RejectsPreservingErrno([&] { _Mbtowc_nid_postfix(unmappedWide, "A", 1, state.data()); });
    RejectsPreservingErrno([&] { _Mbtowc_nid_postfix(&wide, "A", 1, unmappedState); });
    RejectsPreservingErrno([&] { _Mbtowc_nid_postfix(nullptr, nullptr, 0, unmappedState); });
    RejectsPreservingErrno([&] { _Wctomb_nid_postfix(unmappedChar, 0x0041, state.data()); });
    RejectsPreservingErrno([&] { _Wctomb_nid_postfix(nullptr, 0, unmappedState); });
    char out[4]{};
    RejectsPreservingErrno([&] { _Wctomb_nid_postfix(out, 0x0041, unmappedState); });
}

} // namespace

int main() {
    CheckMbtowc();
    CheckWctombAndRoundTrip();
    CheckGuestPointers();

    std::puts("libSceLibcInternal _Mbtowc and _Wctomb tests passed");
    return 0;
}
