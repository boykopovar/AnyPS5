#include "prx/libSceLibcInternal/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "eui64 check failed\n");
        std::abort();
    }
}

static void ExpectParse(const char* text, const std::uint8_t (&expected)[8]) {
    GuestEui64 id{};
    Require(eui64_aton_nid_postfix(text, &id) == 0);
    Require(std::memcmp(id.octet, expected, sizeof(expected)) == 0);
}

int main() {
    const std::uint8_t canonical[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    ExpectParse("00-11-22-33-44-55-66-77", canonical);
    ExpectParse("00:11:22:33:44:55:66:77", canonical);
    ExpectParse("0x0011223344556677", canonical);
    const std::uint8_t mac[8] = {0x00, 0x11, 0x22, 0xff, 0xfe, 0x33, 0x44, 0x55};
    ExpectParse("00-11-22-33-44-55", mac);
    ExpectParse("00:11:22:33:44:55", mac);

    GuestEui64 id{};
    Require(eui64_aton_nid_postfix("not an id", &id) == -1);
    Require(eui64_aton_nid_postfix("00-11-22", &id) == -1);

    char text[32];
    std::memset(text, 'x', sizeof(text));
    const GuestEui64 wanted{{0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}};
    std::memset(text, 'x', sizeof(text));
    Require(eui64_ntoa_nid_postfix(&wanted, text, 24) == 0);
    Require(std::strcmp(text, "00-11-22-33-44-55-66-77") == 0);

    std::memset(text, 'x', sizeof(text));
    Require(eui64_ntoa_nid_postfix(&wanted, text, 23) == -1);
    Require(std::strcmp(text, "00-11-22-33-44-55-66-7") == 0);
    return 0;
}
