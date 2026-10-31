#include "prx/libSceLibcInternal/include/FloatWords.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

template<class Float, class Bits>
void Check(Bits bits, std::uint16_t* (APS5_VABI *least)(Float*), std::uint16_t* (APS5_VABI *most)(Float*)) {
    Float value = std::bit_cast<Float>(bits);
    auto* low = least(&value);
    auto* high = most(&value);
    Require(reinterpret_cast<void*>(low) == &value);
    Require(reinterpret_cast<unsigned char*>(high) == reinterpret_cast<unsigned char*>(&value) + sizeof(Float) - 2);
    Require(std::bit_cast<Bits>(value) == bits);
    std::uint16_t word;
    std::memcpy(&word, low, sizeof(word));
    Require(word == static_cast<std::uint16_t>(bits));
    std::memcpy(&word, high, sizeof(word));
    Require(word == static_cast<std::uint16_t>(bits >> (sizeof(Float) * 8 - 16)));
    word = 0x1234;
    std::memcpy(low, &word, sizeof(word));
    Require(std::bit_cast<Bits>(value) == ((bits & ~Bits{0xffff}) | Bits{0x1234}));
    word = 0xabcd;
    std::memcpy(high, &word, sizeof(word));
    Bits expected = ((bits & ~Bits{0xffff}) | Bits{0x1234}) & ~(Bits{0xffff} << (sizeof(Float) * 8 - 16));
    expected |= Bits{0xabcd} << (sizeof(Float) * 8 - 16);
    Require(std::bit_cast<Bits>(value) == expected);
    for (auto function : {least, most}) {
        bool threw = false;
        try {
            function(nullptr);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }
}

}

int main() {
    for (std::uint32_t bits : {0u, 0x80000000u, 0x3f800000u, 0x7f800000u, 0x7fc01234u, 0xffffffffu, 1u})
        Check<float>(bits, _FPlsw_nid_postfix, _FPmsw_nid_postfix);
    for (std::uint64_t bits : {0ull, 0x8000000000000000ull, 0x3ff0000000000000ull, 0x7ff0000000000000ull,
                              0x7ff8000000001234ull, 0xffffffffffffffffull, 1ull})
        Check<double>(bits, _Plsw_nid_postfix, _Pmsw_nid_postfix);
    std::puts("float word access passed");
}
