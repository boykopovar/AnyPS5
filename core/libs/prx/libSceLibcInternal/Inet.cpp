#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int AddressFamilyInet = 2;
char g_ntoaBuffer[18];

bool IsDigit(unsigned char c) { return c >= '0' && c <= '9'; }
bool IsHexDigit(unsigned char c) { return IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
bool IsSpace(unsigned char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

}

extern "C" {

int APS5_VABI inet_aton_nid_postfix(const char* text, std::uint32_t* address) {
    if (text == nullptr) throw std::invalid_argument("inet_aton: null text");
    std::uint64_t value = 0;
    std::uint8_t parts[4] = {};
    std::uint8_t* part = parts;
    bool digit = false;
    auto c = static_cast<unsigned char>(*text);
    for (;;) {
        if (!IsDigit(c)) return 0;
        value = 0;
        unsigned base = 10;
        digit = false;
        if (c == '0') {
            c = static_cast<unsigned char>(*++text);
            if (c == 'x' || c == 'X') {
                base = 16;
                c = static_cast<unsigned char>(*++text);
            } else {
                base = 8;
                digit = true;
            }
        }
        for (;;) {
            if (IsDigit(c)) {
                if (base == 8 && (c == '8' || c == '9')) return 0;
                value = value * base + (c - '0');
                c = static_cast<unsigned char>(*++text);
                digit = true;
            } else if (base == 16 && IsHexDigit(c)) {
                value = (value << 4) | static_cast<std::uint64_t>(c + 10 - (c >= 'a' ? 'a' : 'A'));
                c = static_cast<unsigned char>(*++text);
                digit = true;
            } else {
                break;
            }
        }
        if (c != '.') break;
        if (part >= parts + 3 || value > 0xff) return 0;
        *part++ = static_cast<std::uint8_t>(value);
        c = static_cast<unsigned char>(*++text);
    }
    if (c != 0 && (c >= 0x80 || !IsSpace(c))) return 0;
    if (!digit) return 0;
    switch (part - parts + 1) {
    case 2:
        if (value > 0xffffff) return 0;
        value |= static_cast<std::uint64_t>(parts[0]) << 24;
        break;
    case 3:
        if (value > 0xffff) return 0;
        value |= (static_cast<std::uint64_t>(parts[0]) << 24) | (static_cast<std::uint64_t>(parts[1]) << 16);
        break;
    case 4:
        if (value > 0xff) return 0;
        value |= (static_cast<std::uint64_t>(parts[0]) << 24) | (static_cast<std::uint64_t>(parts[1]) << 16) | (static_cast<std::uint64_t>(parts[2]) << 8);
        break;
    default:
        break;
    }
    if (address != nullptr) {
        const auto host = static_cast<std::uint32_t>(value);
        const std::uint8_t network[4] = {static_cast<std::uint8_t>(host >> 24), static_cast<std::uint8_t>(host >> 16), static_cast<std::uint8_t>(host >> 8), static_cast<std::uint8_t>(host)};
        std::memcpy(address, network, sizeof(network));
    }
    return 1;
}

std::uint32_t APS5_VABI inet_addr_nid_postfix(const char* text) {
    std::uint32_t address = 0;
    return inet_aton_nid_postfix(text, &address) ? address : 0xffffffffu;
}

char* APS5_VABI inet_ntoa_r_nid_postfix(std::uint32_t address, char* buffer, std::uint32_t size) {
    __inet_ntop_nid_postfix(AddressFamilyInet, &address, buffer, size);

    return buffer;
}

char* APS5_VABI __inet_ntoa_r_nid_postfix(std::uint32_t address, char* buffer, std::uint32_t size) {
    return inet_ntoa_r_nid_postfix(address, buffer, size);
}

char* APS5_VABI inet_ntoa_nid_postfix(std::uint32_t address) {
    std::memcpy(g_ntoaBuffer, "[inet_ntoa error]", sizeof(g_ntoaBuffer));
    __inet_ntop_nid_postfix(AddressFamilyInet, &address, g_ntoaBuffer, sizeof(g_ntoaBuffer));

    return g_ntoaBuffer;
}

}
