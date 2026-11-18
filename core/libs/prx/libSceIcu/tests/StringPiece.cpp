#include "prx/libSceIcu/include/StringPiece.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace {

constexpr auto Equal = &_ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix;
static_assert(std::is_same_v<decltype(+Equal), std::int8_t (APS5_VABI*)(const Icu::StringPiece*, const Icu::StringPiece*)>);

void Check(Icu::StringPiece left, Icu::StringPiece right, int expected) {
    errno = EDOM;
    if (Equal(&left, &right) != expected || errno != EDOM || Equal(&right, &left) != expected || errno != EDOM) {
        std::fprintf(stderr, "StringPiece equality failed: lengths %d/%d, expected %d\n", left.length, right.length, expected);
        std::abort();
    }
}

}

int main() {
    const Icu::StringPiece negative{nullptr, -1};
    try {
        Equal(&negative, &negative);
        return EXIT_FAILURE;
    } catch (const std::invalid_argument&) {
    }
    const Icu::StringPiece missing{nullptr, 1};
    try {
        Equal(&missing, &missing);
        return EXIT_FAILURE;
    } catch (const std::invalid_argument&) {
    }
    Check({nullptr, 0}, {"ignored", 0}, 1);
    Check({nullptr, 0}, {"x", 1}, 0);
    Check({"prefixA", 6}, {"prefixB", 6}, 1);
    Check({"prefix", 6}, {"prefix", 5}, 0);
    Check({"x", 1}, {"y", 1}, 0);
    Check({"a\0bZ", 4}, {"a\0xZ", 4}, 0);
    std::array<char, 3074> a{}, b{};
    for (std::size_t i = 0; i < a.size(); ++i) a[i] = static_cast<char>(i % 251);
    b = a;
    const auto size = static_cast<std::int32_t>(a.size());
    Check({a.data(), size}, {b.data(), size}, 1);
    for (const int i : {0, 1023, 1024, 2048, size - 1}) {
        b[i] ^= 1;
        Check({a.data(), size}, {b.data(), size}, 0);
        b[i] ^= 1;
    }
}
