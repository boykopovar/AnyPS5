#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "inet_net check failed\n");
        std::abort();
    }
}

static constexpr int Inet = 2;
static constexpr int Inet6 = 28;
static constexpr int ErrorEinval = 22;
static constexpr int ErrorEnoent = 2;
static constexpr int ErrorEmsgsize = 40;
static constexpr int ErrorEafnosupport = 47;

static void ExpectNtop(int af, const std::uint8_t* address, int bits, std::size_t size, const char* expected) {
    char buffer[80];
    std::memset(buffer, 'x', sizeof(buffer));
    *__error_nid_postfix() = 0;
    Require(inet_net_ntop_nid_postfix(af, address, bits, buffer, size) == buffer);
    Require(std::strcmp(buffer, expected) == 0);
}

static void ExpectNtopError(int af, const std::uint8_t* address, int bits, std::size_t size, int error) {
    char buffer[80];
    *__error_nid_postfix() = 0;
    Require(inet_net_ntop_nid_postfix(af, address, bits, buffer, size) == nullptr);
    Require(*__error_nid_postfix() == error);
}

static void ExpectPton(int af, const char* text, const std::uint8_t* expected, std::size_t length, int bits) {
    std::uint8_t buffer[16];
    std::memset(buffer, 0xee, sizeof(buffer));
    *__error_nid_postfix() = 0;
    Require(inet_net_pton_nid_postfix(af, text, buffer, sizeof(buffer)) == bits);
    Require(std::memcmp(buffer, expected, length) == 0);
}

static void ExpectPtonError(int af, const char* text, int error) {
    std::uint8_t buffer[16];
    *__error_nid_postfix() = 0;
    Require(inet_net_pton_nid_postfix(af, text, buffer, sizeof(buffer)) == -1);
    Require(*__error_nid_postfix() == error);
}

int main() {
    const std::uint8_t netmask[4] = {192, 5, 5, 240};
    ExpectNtop(Inet, netmask, 28, 64, "192.5.5.240/28");
    const std::uint8_t lan[4] = {192, 168, 1, 0};
    ExpectNtop(Inet, lan, 24, 64, "192.168.1/24");
    ExpectNtop(Inet, lan, 0, 64, "0/0");
    const std::uint8_t host[4] = {10, 0, 0, 1};
    ExpectNtop(Inet, host, 32, 64, "10.0.0.1/32");
    ExpectNtopError(Inet, host, 33, 64, ErrorEinval);
    ExpectNtopError(Inet, host, 32, 4, ErrorEmsgsize);
    ExpectNtopError(99, host, 32, 64, ErrorEafnosupport);

    const std::uint8_t documentation[16] = {0x20, 0x01, 0x0d, 0xb8};
    ExpectNtop(Inet6, documentation, 32, 64, "2001:db8/32");
    std::uint8_t zero[16] = {};
    ExpectNtop(Inet6, zero, 0, 64, "::/0");
    std::uint8_t loopback[16] = {};
    loopback[15] = 1;
    ExpectNtop(Inet6, loopback, 128, 64, "::1/128");

    ExpectPton(Inet, "192.5.5.240/28", netmask, 4, 28);
    const std::uint8_t ten[1] = {10};
    ExpectPton(Inet, "10", ten, 1, 8);
    ExpectPton(Inet, "0x0a", ten, 1, 8);
    const std::uint8_t tenOne[2] = {10, 1};
    ExpectPton(Inet, "10.1", tenOne, 2, 16);
    const std::uint8_t classD[1] = {224};
    ExpectPton(Inet, "224", classD, 1, 4);
    ExpectPtonError(Inet, "300", ErrorEnoent);
    ExpectPtonError(Inet, "1.2.3.4/33", ErrorEnoent);

    const std::uint8_t prefix64[8] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 1};
    ExpectPton(Inet6, "2001:db8::1/64", prefix64, 8, 64);
    const std::uint8_t host128[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ExpectPton(Inet6, "2001:db8::1/128", host128, 16, 128);

    const std::uint8_t nsap[7] = {0x47, 0x00, 0x05, 0x80, 0x00, 0x5a, 0x00};
    std::uint8_t binary[16] = {};
    Require(inet_nsap_addr_nid_postfix("0x47.0005.8000.5a00", binary, 16) == 7);
    Require(std::memcmp(binary, nsap, sizeof(nsap)) == 0);
    Require(inet_nsap_addr_nid_postfix("1234", binary, 16) == 0);

    char text[800];
    std::memset(text, 0, sizeof(text));
    Require(inet_nsap_ntoa_nid_postfix(7, nsap, text) == text);
    Require(std::strcmp(text, "0x47.0005.8000.5A00") == 0);
    Require(std::strcmp(inet_nsap_ntoa_nid_postfix(0, nsap, nullptr), "0x") == 0);
    return 0;
}
