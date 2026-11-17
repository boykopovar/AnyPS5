#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "Testing.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>
#include <type_traits>

namespace {

using Getter = const void* (APS5_VABI*)();
static_assert(std::is_same_v<decltype(&_Getpwctytab_nid_postfix), Getter>);
static_assert(std::is_same_v<decltype(&_Getpwctrtab_nid_postfix), Getter>);

struct TableEntry {
    const void* data;
    std::int64_t offset;
};

static_assert(sizeof(TableEntry) == 16);
static_assert(offsetof(TableEntry, offset) == 8);

template<std::size_t N>
std::array<TableEntry, N> ReadTable(Getter getter, const std::array<const char*, N>& names,
                                  const std::array<std::int64_t, N>& offsets) {
    errno = 71;
    const void* address = getter();
    Require(errno == 71, "getter preserves errno");
    Require(address != nullptr, "getter returns a table");
    Require(getter() == address, "table identity is stable");
    const void* otherAddress = nullptr;
    int otherErrno = 0;
    std::thread other([&] {
        errno = 86;
        otherAddress = getter();
        otherErrno = errno;
    });
    other.join();
    Require(otherAddress == address && otherErrno == 86, "table is shared and preserves another thread's errno");
    std::array<TableEntry, N> table{};
    std::memcpy(table.data(), address, sizeof(table));
    Require(table[0].data != nullptr, "range data is present");
    for (std::size_t i = 0; i < N; ++i) {
        Require(table[i].offset == offsets[i], "range offsets match the guest layout");
        if (i == 0) continue;
        if (names[i] == nullptr) Require(table[i].data == nullptr, "table ends with a null name");
        else Require(table[i].data != nullptr && std::strcmp(static_cast<const char*>(table[i].data), names[i]) == 0,
                     "property names and order match the guest layout");
    }
    return table;
}

int Classify(const TableEntry* table, std::size_t property, std::uint32_t character) {
    const auto* ranges = static_cast<const std::uint16_t*>(table[0].data);
    for (auto i = table[property].offset; i < table[property + 1].offset; i += 3) {
        if (character >= ranges[i] && character <= ranges[i + 1]) return ranges[i + 2];
    }
    return 0;
}

std::uint32_t Transform(const TableEntry* table, std::size_t property, std::uint32_t character) {
    const auto* ranges = static_cast<const std::uint16_t*>(table[0].data);
    for (auto i = table[property].offset; i < table[property + 1].offset; i += 3) {
        if (character >= ranges[i] && character <= ranges[i + 1])
            return static_cast<std::uint16_t>(character + ranges[i + 2]);
    }
    return character;
}

void CheckClassification() {
    const auto table = ReadTable<14>(_Getpwctytab_nid_postfix,
        {nullptr, "alnum", "alpha", "cntrl", "digit", "graph", "lower", "print", "punct", "space", "upper", "xdigit", "blank", nullptr},
        {0, 0, 9, 15, 21, 24, 27, 30, 33, 45, 51, 54, 63, 69});
    std::array<std::uint16_t, 69> original{};
    std::memcpy(original.data(), table[0].data, sizeof(original));
    for (std::uint32_t c = 0; c <= 0xffff; ++c) {
        const bool upper = c >= 'A' && c <= 'Z';
        const bool lower = c >= 'a' && c <= 'z';
        const bool digit = c >= '0' && c <= '9';
        const bool graph = c >= '!' && c <= '~';
        const std::array<bool, 12> expected = {
            upper || lower || digit, upper || lower, c < 32 || c == 127, digit, graph, lower,
            c >= ' ' && c <= '~', graph && !upper && !lower && !digit, c == ' ' || (c >= '\t' && c <= '\r'),
            upper, digit || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'), c == ' ' || c == '\t',
        };
        for (std::size_t i = 0; i < expected.size(); ++i)
            Require(Classify(table.data(), i + 1, c) == static_cast<int>(expected[i]), "classification matches C-locale rules across the BMP");
    }
    for (auto c : {0x10000u, 0x10ffffu, 0x110000u, 0x80000000u, std::numeric_limits<std::uint32_t>::max()})
        for (std::size_t i = 1; i <= 12; ++i) Require(Classify(table.data(), i, c) == 0, "out-of-range values are unclassified");
    Require(std::memcmp(original.data(), table[0].data, sizeof(original)) == 0, "classification range data is unchanged");
    Require(std::memcmp(table.data(), _Getpwctytab_nid_postfix(), sizeof(table)) == 0, "classification table is unchanged");
    std::puts("wide tables: all 12 classes match across 65536 code units and out-of-range values");
}

void CheckTranslation() {
    const auto table = ReadTable<4>(_Getpwctrtab_nid_postfix,
        {nullptr, "tolower", "toupper", nullptr}, {0, 0, 3, 6});
    std::array<std::uint16_t, 6> original{};
    std::memcpy(original.data(), table[0].data, sizeof(original));
    for (std::uint32_t c = 0; c <= 0xffff; ++c) {
        const auto lower = c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
        const auto upper = c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
        Require(Transform(table.data(), 1, c) == lower, "lowercase conversion matches across the BMP");
        Require(Transform(table.data(), 2, c) == upper, "uppercase conversion matches across the BMP");
    }
    for (auto c : {0x10000u, 0x10ffffu, 0x110000u, 0x80000000u, std::numeric_limits<std::uint32_t>::max()}) {
        Require(Transform(table.data(), 1, c) == c, "lowercase conversion leaves out-of-range values unchanged");
        Require(Transform(table.data(), 2, c) == c, "uppercase conversion leaves out-of-range values unchanged");
    }
    Require(std::memcmp(original.data(), table[0].data, sizeof(original)) == 0, "translation range data is unchanged");
    Require(std::memcmp(table.data(), _Getpwctrtab_nid_postfix(), sizeof(table)) == 0, "translation table is unchanged");
    std::puts("wide tables: both transforms match across 65536 code units and out-of-range values");
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "select classification or translation");
    if (std::strcmp(argv[1], "classification") == 0) CheckClassification();
    else if (std::strcmp(argv[1], "translation") == 0) CheckTranslation();
    else Require(false, "unknown test selection");
}
