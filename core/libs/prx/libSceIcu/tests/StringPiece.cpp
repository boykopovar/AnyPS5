#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU StringPiece check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

}

int main() {
    using Piece = Icu::Abi::StringPiece;
    const char bytes[]{'a', 'b', 0, 'c', 0};
    for (auto construct : {_ZN6icu_6811StringPieceC1EPKc_nid_postfix, _ZN6icu_6811StringPieceC2EPKc_nid_postfix}) {
        Piece text{};
        construct(&text, bytes);
        Require(text.pointer == bytes && text.length == 2);
        construct(&text, nullptr);
        Require(text.pointer == nullptr && text.length == 0);
        Invalid([&] { construct(nullptr, bytes); });
    }
    Piece source{bytes, 4};
    for (auto construct : {_ZN6icu_6811StringPieceC1ERKS0_i_nid_postfix, _ZN6icu_6811StringPieceC2ERKS0_i_nid_postfix}) {
        for (auto position : {-10, 0, 2, 4, 5, std::numeric_limits<std::int32_t>::max()}) {
            const auto start = position < 0 ? 0 : position > 4 ? 4 : position;
            Piece text{};
            construct(&text, &source, position);
            Require(text.pointer == bytes + start && text.length == 4 - start);
        }
    }
    for (auto construct : {_ZN6icu_6811StringPieceC1ERKS0_ii_nid_postfix, _ZN6icu_6811StringPieceC2ERKS0_ii_nid_postfix}) {
        for (auto position : {-10, 0, 2, 4, 5}) {
            for (auto length : {-1, 0, 1, 3, 100}) {
                const auto start = position < 0 ? 0 : position > 4 ? 4 : position;
                const auto count = length < 0 ? 0 : length > 4 - start ? 4 - start : length;
                Piece text{};
                construct(&text, &source, position, length);
                Require(text.pointer == bytes + start && text.length == count);
            }
        }
    }
    Piece text{};
    _ZN6icu_6811StringPiece3setEPKc_nid_postfix(&text, bytes);
    Require(text.pointer == bytes && text.length == 2);
    _ZN6icu_6811StringPiece3setEPKc_nid_postfix(&text, nullptr);
    Require(text.pointer == nullptr && text.length == 0);
    const std::array<char, 4> low{'a', 0, 'b', static_cast<char>(0x80)};
    const std::array<char, 4> high{'a', 0, 'b', static_cast<char>(0x7f)};
    const Piece first{low.data(), 4}, second{high.data(), 4}, empty{nullptr, 0};
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&first, second) == -1);
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&second, first) == 1);
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&first, first) == 0);
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&empty, first) == -1);
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&first, empty) == 1);
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&empty, empty) == 0);
    const Piece shortPrefix{low.data(), 3};
    Require(_ZN6icu_6811StringPiece7compareES0__nid_postfix(&first, shortPrefix) == 1);
    Piece null{nullptr, 1};
    Invalid([&] { _ZN6icu_6811StringPiece7compareES0__nid_postfix(&null, first); });
    Invalid([&] { _ZN6icu_6811StringPieceC1ERKS0_i_nid_postfix(&text, &null, 0); });
    Invalid([&] { _ZN6icu_6811StringPieceC2ERKS0_ii_nid_postfix(&text, nullptr, 0, 0); });
    Invalid([] { _ZN6icu_6811StringPiece3setEPKc_nid_postfix(nullptr, nullptr); });
    std::puts("ICU StringPiece tests passed");
}
