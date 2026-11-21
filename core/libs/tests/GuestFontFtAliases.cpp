#include "prx/libSceFontFt/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Check(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "GuestFontFtAliases check failed at line %d\n", line);
    std::abort();
}
#define Require(value) Check((value), __LINE__)

constexpr int InvalidParameter = static_cast<int>(0x80460002u);
constexpr int NotInitialized = static_cast<int>(0x80460020u);
constexpr int NoSpace = static_cast<int>(0x8046005cu);

std::uint64_t Field(const unsigned char* table, std::size_t offset) {
    std::uint64_t value = 0;
    std::memcpy(&value, table + offset, sizeof(value));
    return value;
}

std::uint32_t Count(const unsigned char* table) {
    std::uint32_t value = 0;
    std::memcpy(&value, table + 4, sizeof(value));
    return value;
}

template<typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    Require(sceFontFtSetAliasPath("serif", "/font.ttf") == NotInitialized);
    Require(sceFontFtSetAliasFont("serif", 1) == NotInitialized);
    Require(sceFontFtInitAliases(nullptr) == InvalidParameter);
    FontFtAliases* handle = reinterpret_cast<FontFtAliases*>(std::uintptr_t{0x20});
    Require(sceFontFtInitAliases(&handle) == InvalidParameter && handle == reinterpret_cast<FontFtAliases*>(std::uintptr_t{0x20}));
    handle = reinterpret_cast<FontFtAliases*>(std::uintptr_t{0x10});
    Require(sceFontFtInitAliases(&handle) == 0 && handle);
    const auto* table = reinterpret_cast<const unsigned char*>(handle);
    const auto base = reinterpret_cast<std::uint64_t>(table);
    Require(Field(table, 0) == 0x1fa0 && Count(table) == 0);
    Require(Field(table, 8) == base + 0x2020 && Field(table, 0x10) == base + 0x20 && Field(table, 0x18) == base + 0x2020);
    FontFtAliases* again = handle;
    Require(sceFontFtInitAliases(&again) == 0 && again == handle);

    Require(sceFontFtSetAliasPath("serif", "/a/b.ttf") == 0);
    Require(std::memcmp(table + 0x20, "serif\0/a/b.ttf\0", 15) == 0);
    Require(Count(table) == 1 && Field(table, 8) == base + 0x2010);
    Require(Field(table, 0x2010) == base + 0x20 && Field(table, 0x2018) == base + 0x26);
    Require(Field(table, 0x10) == base + 0x2f && Field(table, 0x18) == base + 0x2020);

    Require(sceFontFtSetAliasPath("x", nullptr) == InvalidParameter && table[0x2f] == 'x' && table[0x30] == 0);
    Require(sceFontFtSetAliasPath("y", "") == InvalidParameter && table[0x2f] == 'y' && Count(table) == 1);
    Require(sceFontFtSetAliasPath("", "/c") == 0 && Count(table) == 2);
    Require(Field(table, 0x2000) == base + 0x2f && Field(table, 0x2008) == base + 0x30 && Field(table, 0x10) == base + 0x33);

    Require(Throws([] { sceFontFtSetAliasFont("mono", 3); }));
    Require(std::memcmp(table + 0x33, "mono", 5) == 0 && Count(table) == 2);

    const std::string longPath(0x2100, 'p');
    Require(sceFontFtSetAliasPath("big", longPath.c_str()) == NoSpace && Count(table) == 2);
    const std::string longAlias(0x2100, 'a');
    Require(sceFontFtSetAliasPath(longAlias.c_str(), "/d") == NoSpace && Count(table) == 2);

    Require(sceFontFtInitAliases(&again) == 0 && Field(table, 0x10) == base + 0x20 && Count(table) == 0);
    for (int index = 0; index < 4; ++index) Require(sceFontFtSetAliasPath("ab", "/xy") == 0);
    Require(Count(table) == 4 && Field(table, 8) == base + 0x2020 - 4 * 16 && Field(table, 0x18) == base + 0x2020);

    Require(sceFontFtTermAliases(nullptr) == InvalidParameter);
    FontFtAliases* empty = nullptr;
    Require(sceFontFtTermAliases(&empty) == NotInitialized && !empty);
    std::vector<unsigned char> foreign(0x2020, 0x55);
    auto* foreignHandle = reinterpret_cast<FontFtAliases*>(foreign.data());
    Require(sceFontFtTermAliases(&foreignHandle) == NotInitialized && !foreignHandle && foreign[0] == 0x55 && foreign[0x201f] == 0x55);
    foreign[0] = 0xa0;
    foreign[1] = 0x1f;
    foreignHandle = reinterpret_cast<FontFtAliases*>(foreign.data());
    Require(sceFontFtTermAliases(&foreignHandle) == 0 && !foreignHandle && foreign[0] == 0 && foreign[0x201f] == 0);
    Require(Count(table) == 4);
    Require(sceFontFtTermAliases(&handle) == 0 && !handle);
    for (std::size_t offset = 0; offset < 0x2020; ++offset) Require(table[offset] == 0);
    Require(sceFontFtSetAliasPath("serif", "/a") == NotInitialized);
    std::puts("GuestFontFtAliases passed");
    return 0;
}
