#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal bitset to_string: %s\n", message);
    std::abort();
}

constexpr std::size_t Guard = 8;

void Call(char* destination, const void* source, std::size_t bits, char zero, char one) {
    __std_bitset_to_string_1_nid_postfix(destination, source, bits, zero, one);
}

void Call(std::uint16_t* destination, const void* source, std::size_t bits, std::uint16_t zero, std::uint16_t one) {
    __std_bitset_to_string_2_nid_postfix(destination, source, bits, zero, one);
}

template <typename TElement>
void CheckPattern(std::size_t bits, std::uint8_t seed, TElement zero, TElement one, TElement fill) {
    std::vector<std::uint8_t> source((bits + 7) / 8 + 1);
    for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<std::uint8_t>(seed + i * 151u);
    if (bits % 8 != 0) source[bits / 8] |= static_cast<std::uint8_t>(0xffu << (bits % 8));
    const auto before = source;
    std::vector<TElement> output(Guard + bits + Guard, fill);
    Call(output.data() + Guard, source.data(), bits, zero, one);
    for (std::size_t pos = 0; pos < bits; ++pos) {
        const bool set = ((source[pos / 8] >> (pos % 8)) & 1u) != 0;
        Require(output[Guard + bits - 1 - pos] == (set ? one : zero), "character position N - 1 - pos must hold bit pos");
    }
    for (std::size_t i = 0; i < Guard; ++i) Require(output[i] == fill && output[Guard + bits + i] == fill, "only the N characters of the string may be written");
    Require(source == before, "the bitset was modified");
}

template <std::size_t TBits>
void CheckAgainstHostBitset(std::uint64_t seed) {
    std::bitset<TBits> value;
    for (std::size_t pos = 0; pos < TBits; ++pos) value[pos] = ((seed >> (pos % 64)) ^ (pos / 3)) & 1u;
    std::vector<char> narrow(TBits);
    Call(narrow.data(), &value, TBits, 'o', 'x');
    Require(std::string(narrow.begin(), narrow.end()) == value.to_string('o', 'x'), "the narrow string differs from std::bitset::to_string");
    std::vector<std::uint16_t> wide(TBits);
    Call(wide.data(), &value, TBits, std::uint16_t{0x30}, std::uint16_t{0x31});
    const auto expected = value.to_string();
    for (std::size_t i = 0; i < TBits; ++i) Require(wide[i] == static_cast<std::uint16_t>(expected[i]), "the wide string differs from std::bitset::to_string");
}

template <typename TElement>
void CheckInvalid(TElement zero, TElement one) {
    std::uint8_t source = 0x5a;
    TElement destination[8]{};
    Call(static_cast<TElement*>(nullptr), nullptr, 0, zero, one);
    Call(destination, &source, 0, zero, one);
    Require(destination[0] == TElement{}, "a zero-bit string must not be written");
    const auto throws = [&](TElement* target, const void* bits) {
        bool threw = false;
        try { Call(target, bits, 8, zero, one); }
        catch (const std::invalid_argument&) { threw = true; }
        Require(threw, "a null destination or bitset with a nonzero size must throw");
    };
    throws(nullptr, &source);
    throws(destination, nullptr);
}

}

int main() {
    for (std::size_t bits = 1; bits <= 130; ++bits) {
        for (const std::uint8_t seed : {std::uint8_t{0x00}, std::uint8_t{0xa5}, std::uint8_t{0xff}}) {
            CheckPattern<char>(bits, seed, '0', '1', '#');
            CheckPattern<char>(bits, seed, static_cast<char>(0xff), 'a', '\0');
            CheckPattern<std::uint16_t>(bits, seed, 0x30, 0x31, 0xfffe);
            CheckPattern<std::uint16_t>(bits, seed, 0xffff, 0x4e00, 0);
        }
    }
    for (const std::size_t bits : {255u, 256u, 257u, 1000u, 4096u}) {
        CheckPattern<char>(bits, 0x3c, '0', '1', '#');
        CheckPattern<std::uint16_t>(bits, 0x3c, 0x30, 0x31, 0xfffe);
    }
    CheckAgainstHostBitset<1>(0x1);
    CheckAgainstHostBitset<63>(0x8000000000000001ull);
    CheckAgainstHostBitset<64>(0xdeadbeefcafef00dull);
    CheckAgainstHostBitset<100>(0x0123456789abcdefull);
    CheckAgainstHostBitset<257>(0xf0f0f0f00f0f0f0full);
    CheckInvalid<char>('0', '1');
    CheckInvalid<std::uint16_t>(0x30, 0x31);
    std::puts("bitset to_string helpers write the standard's characters");
    return 0;
}
