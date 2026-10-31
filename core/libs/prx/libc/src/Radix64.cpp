#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstring>

namespace {

constexpr char kRadix64Table[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

constexpr int DecodeDigit(char c) {
    if (c == '.') return 0;
    if (c == '/') return 1;
    if (c >= '0' && c <= '9') return c - '0' + 2;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 12;
    if (c >= 'a' && c <= 'z') return c - 'a' + 38;
    return -1;
}

}

extern "C" std::int64_t APS5_VABI a64l_nid_postfix(const char* s) {
    if (!s || *s == '\0') return 0;
    std::uint32_t value = 0;
    int shift = 0;
    for (int i = 0; i < 6 && s[i] != '\0'; ++i, shift += 6) {
        const int digit = DecodeDigit(s[i]);
        if (digit < 0) break;
        value |= static_cast<std::uint32_t>(digit) << shift;
    }
    return static_cast<std::int64_t>(static_cast<std::int32_t>(value));
}

extern "C" int APS5_VABI l64a_r_nid_postfix(std::int64_t value, char* buffer, int buflen) {
    if (!buffer || buflen <= 0) return -1;
    auto v = static_cast<std::uint32_t>(value);
    if (v == 0) {
        buffer[0] = '\0';
        return 0;
    }
    char tmp[7];
    int len = 0;
    while (v != 0 && len < 6) {
        tmp[len++] = kRadix64Table[v & 0x3f];
        v >>= 6;
    }
    tmp[len] = '\0';
    if (buflen <= len) return -1;
    std::memcpy(buffer, tmp, len + 1);
    return 0;
}

extern "C" char* APS5_VABI l64a_nid_postfix(std::int64_t value) {
    thread_local char buf[7];
    buf[0] = '\0';
    l64a_r_nid_postfix(value, buf, static_cast<int>(sizeof(buf)));
    return buf;
}
