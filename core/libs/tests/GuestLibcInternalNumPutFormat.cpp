#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace {

constexpr std::int32_t Dec = 0x10;
constexpr std::int32_t Oct = 0x20;
constexpr std::int32_t Hex = 0x40;
constexpr std::int32_t ShowBase = 0x80;
constexpr std::int32_t ShowPoint = 0x100;
constexpr std::int32_t Uppercase = 0x200;
constexpr std::int32_t ShowPos = 0x400;
constexpr std::int32_t Scientific = 0x800;
constexpr std::int32_t Fixed = 0x1000;
constexpr std::int32_t HexFloat = Fixed | Scientific;
constexpr char Untouched = 0x7f;

using IntegerFormat = char* (APS5_VABI*)(const void*, char*, const char*, std::int32_t);
using FloatFormat = char* (APS5_VABI*)(const void*, char*, char, std::int32_t);

constexpr std::array<IntegerFormat, 2> IntegerFormats{
    _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_IfmtEPcPKcNSt5_IosbIiE9_FmtflagsE_nid_postfix,
    _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_IfmtEPcPKcNSt5_IosbIiE9_FmtflagsE_nid_postfix,
};

constexpr std::array<FloatFormat, 2> FloatFormats{
    _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FfmtEPccNSt5_IosbIiE9_FmtflagsE_nid_postfix,
    _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_FfmtEPccNSt5_IosbIiE9_FmtflagsE_nid_postfix,
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "num_put format: %s\n", message);
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

void CheckWritten(const std::array<char, 16>& buffer, const char* expected, const char* message) {
    const std::size_t length = std::strlen(expected);
    Require(std::memcmp(buffer.data(), expected, length + 1) == 0, message);
    for (std::size_t index = length + 1; index < buffer.size(); ++index) Require(buffer[index] == Untouched, message);
}

void ExpectInteger(const char* specifier, std::int32_t flags, const char* expected) {
    for (const auto format : IntegerFormats) {
        std::array<char, 16> buffer;
        buffer.fill(Untouched);
        Require(format(nullptr, buffer.data(), specifier, flags) == buffer.data(), expected);
        CheckWritten(buffer, expected, expected);
    }
}

void ExpectFloat(char specifier, std::int32_t flags, const char* expected) {
    for (const auto format : FloatFormats) {
        std::array<char, 16> buffer;
        buffer.fill(Untouched);
        Require(format(nullptr, buffer.data(), specifier, flags) == buffer.data(), expected);
        CheckWritten(buffer, expected, expected);
    }
}

void IntegerConversions() {
    ExpectInteger("ld", 0, "%ld");
    ExpectInteger("lu", Dec, "%lu");
    ExpectInteger("Ld", Dec, "%lld");
    ExpectInteger("Lu", 0, "%llu");
    ExpectInteger("ld", Oct, "%lo");
    ExpectInteger("Lu", Oct | Uppercase, "%llo");
    ExpectInteger("lu", Hex, "%lx");
    ExpectInteger("Ld", Hex | Uppercase, "%llX");
    ExpectInteger("ld", Dec | Uppercase, "%ld");
    ExpectInteger("ld", Oct | Hex, "%ld");
    ExpectInteger("lu", Dec | Hex, "%lu");
    ExpectInteger("Lu", Dec | Oct | Hex, "%llu");
    ExpectInteger("ld", ShowPos, "%+ld");
    ExpectInteger("ld", ShowBase, "%#ld");
    ExpectInteger("Ld", ShowPos | ShowBase | Hex | Uppercase, "%+#llX");
    ExpectInteger("lu", ShowPoint | Scientific | Fixed, "%lu");
    ExpectInteger("hd", 0, "%hd");
}

void FloatConversions() {
    ExpectFloat('\0', 0, "%.*g");
    ExpectFloat('L', 0, "%.*Lg");
    ExpectFloat('\0', Uppercase, "%.*G");
    ExpectFloat('\0', Fixed, "%.*f");
    ExpectFloat('L', Fixed | Uppercase, "%.*Lf");
    ExpectFloat('\0', Scientific, "%.*e");
    ExpectFloat('\0', Scientific | Uppercase, "%.*E");
    ExpectFloat('\0', HexFloat, "%a");
    ExpectFloat('L', HexFloat | Uppercase, "%LA");
    ExpectFloat('\0', ShowPos, "%+.*g");
    ExpectFloat('\0', ShowPoint, "%#.*g");
    ExpectFloat('L', ShowPos | ShowPoint | Uppercase, "%+#.*LG");
    ExpectFloat('\0', ShowPos | ShowPoint | HexFloat, "%+#a");
    ExpectFloat('\0', ShowBase | Hex | Oct, "%.*g");
}

void UnusableMemory() {
    for (const auto format : IntegerFormats) Require(Throws([format] { format(nullptr, nullptr, "ld", 0); }), "null integer buffer");
    for (const auto format : IntegerFormats) {
        std::array<char, 16> buffer;
        Require(Throws([format, &buffer] { format(nullptr, buffer.data(), nullptr, 0); }), "null integer specifier");
    }
    for (const auto format : FloatFormats) Require(Throws([format] { format(nullptr, nullptr, 'L', 0); }), "null float buffer");
}

}

int main() {
    IntegerConversions();
    FloatConversions();
    UnusableMemory();
}
