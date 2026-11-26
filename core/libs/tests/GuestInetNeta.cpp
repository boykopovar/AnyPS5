#include "prx/libSceLibcInternal/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "inet_neta check failed\n");
        std::abort();
    }
}

static void ExpectText(std::uint32_t address, std::size_t size, const char* expected) {
    char buffer[64];
    std::memset(buffer, 'x', sizeof(buffer));
    errno = 0;
    char* result = inet_neta_nid_postfix(address, buffer, size);
    Require(result == buffer);
    Require(std::strcmp(buffer, expected) == 0);
}

static void ExpectTooSmall(std::uint32_t address, std::size_t size) {
    char buffer[64];
    errno = 0;
    Require(inet_neta_nid_postfix(address, buffer, size) == nullptr);
    Require(errno == EMSGSIZE);
}

int main() {
    ExpectText(0x0a000000u, 64, "10");
    ExpectText(0xc0a80100u, 64, "192.168.1");
    ExpectText(0x7f000001u, 64, "127.1");
    ExpectText(0x00000001u, 64, "1");
    ExpectText(0u, 64, "0.0.0.0");
    ExpectText(0xffffffffu, 64, "255.255.255.255");
    ExpectText(0x0a000000u, 5, "10");
    ExpectText(0u, 8, "0.0.0.0");
    ExpectText(0xc0a80100u, 13, "192.168.1");
    ExpectTooSmall(0x0a000000u, 4);
    ExpectTooSmall(0u, 7);
    ExpectTooSmall(0xc0a80100u, 12);
    return 0;
}
