#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Key {
    std::string_view legacy;
    std::string_view bcp;
};

constexpr std::array<Key, 37> Keys{{
    {"calendar", "ca"}, {"cf", "cf"}, {"colalternate", "ka"}, {"colbackwards", "kb"},
    {"colcasefirst", "kf"}, {"colcaselevel", "kc"}, {"colhiraganaquaternary", "kh"}, {"collation", "co"},
    {"colnormalization", "kk"}, {"colnumeric", "kn"}, {"colreorder", "kr"}, {"colstrength", "ks"},
    {"currency", "cu"}, {"d0", "d0"}, {"dx", "dx"}, {"em", "em"}, {"fw", "fw"}, {"h0", "h0"},
    {"hours", "hc"}, {"i0", "i0"}, {"k0", "k0"}, {"kv", "kv"}, {"lb", "lb"}, {"lw", "lw"}, {"m0", "m0"},
    {"measure", "ms"}, {"mu", "mu"}, {"numbers", "nu"}, {"rg", "rg"}, {"s0", "s0"}, {"sd", "sd"},
    {"ss", "ss"}, {"t0", "t0"}, {"timezone", "tz"}, {"va", "va"}, {"variabletop", "vt"}, {"x0", "x0"}
}};

char Lower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool Letter(char value) {
    const char lower = Lower(value);
    return lower >= 'a' && lower <= 'z';
}

bool Digit(char value) {
    return value >= '0' && value <= '9';
}

bool SameIgnoringCase(std::string_view left, std::string_view right) {
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) { return Lower(a) == Lower(b); });
}

std::string ReadKeyword(const char* keyword, const char* caller) {
    std::string text;
    while (true) {
        char chunk[0x1000];
        const auto address = reinterpret_cast<std::uintptr_t>(keyword) + text.size();
        const std::size_t count = sizeof(chunk) - address % sizeof(chunk);
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(chunk, keyword + text.size(), count);
        errno = savedErrno;
        if (!copied) throw std::invalid_argument(std::string(caller) + ": unreadable keyword");
        const auto* end = static_cast<const char*>(std::memchr(chunk, 0, count));
        text.append(chunk, end ? static_cast<std::size_t>(end - chunk) : count);
        if (end) return text;
    }
}

const Key* Find(std::string_view keyword) {
    for (const Key& key : Keys) {
        if (SameIgnoringCase(keyword, key.legacy) || SameIgnoringCase(keyword, key.bcp)) return &key;
    }
    return nullptr;
}

}

extern "C" {

const char* APS5_VABI uloc_toLegacyKey_nid_postfix(const char* keyword) {
    if (!keyword) return nullptr;
    const std::string text = ReadKeyword(keyword, __func__);
    if (text.empty()) return nullptr;
    if (const Key* key = Find(text)) return key->legacy.data();
    const bool wellFormed = std::all_of(text.begin(), text.end(), [](char value) { return Letter(value) || Digit(value); });
    return wellFormed ? keyword : nullptr;
}

const char* APS5_VABI uloc_toUnicodeLocaleKey_nid_postfix(const char* keyword) {
    if (!keyword) return nullptr;
    const std::string text = ReadKeyword(keyword, __func__);
    if (text.empty()) return nullptr;
    if (const Key* key = Find(text)) return key->bcp.data();
    const bool wellFormed = text.size() == 2 && (Letter(text[0]) || Digit(text[0])) && Letter(text[1]);
    return wellFormed ? keyword : nullptr;
}

}
