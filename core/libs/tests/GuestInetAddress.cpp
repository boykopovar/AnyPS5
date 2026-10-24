#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
int APS5_VABI inet_aton_nid_postfix(const char* text, std::uint32_t* address);
std::uint32_t APS5_VABI inet_addr_nid_postfix(const char* text);
char* APS5_VABI inet_ntoa_nid_postfix(std::uint32_t address);
char* APS5_VABI inet_ntoa_r_nid_postfix(std::uint32_t address, char* buffer, std::uint32_t size);
char* APS5_VABI __inet_ntoa_r_nid_postfix(std::uint32_t address, char* buffer, std::uint32_t size);
int* APS5_VABI __error_nid_postfix();
}

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

std::uint32_t Network(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
    const std::uint8_t bytes[4] = {a, b, c, d};
    std::uint32_t value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

bool Parses(const char* text, std::uint32_t expected) {
    std::uint32_t address = 0x5a5a5a5a;
    return inet_aton_nid_postfix(text, &address) == 1 && address == expected;
}

bool Rejects(const char* text) {
    std::uint32_t address = 0x5a5a5a5a;
    return inet_aton_nid_postfix(text, &address) == 0 && address == 0x5a5a5a5a;
}

}

int main() {
    Require(Parses("192.168.1.2", Network(192, 168, 1, 2)), "dotted quad");
    Require(Parses("10.1", Network(10, 0, 0, 1)), "two parts");
    Require(Parses("10.1.300", Network(10, 1, 1, 44)), "three parts");
    Require(Parses("0x7f.1", Network(127, 0, 0, 1)), "hex part");
    Require(Parses("017.0.0.1", Network(15, 0, 0, 1)), "octal part");
    Require(Parses("3232235777", Network(192, 168, 1, 1)), "single number");
    Require(Parses("1.2.3.4 trailing", Network(1, 2, 3, 4)), "trailing after space");
    Require(Parses("0", Network(0, 0, 0, 0)), "zero");
    Require(Parses("0xffffffff", Network(255, 255, 255, 255)), "broadcast");
    Require(Rejects("256.1.1.1") && Rejects("1.2.3.256") && Rejects("1.2.65536"), "part out of range");
    Require(Rejects("1.2.3.4.5") && Rejects("08") && Rejects("0x") && Rejects("1.2.3.4x"), "malformed");
    Require(Rejects("") && Rejects(".1") && Rejects("1..2") && Rejects("-1"), "missing digits");
    Require(inet_aton_nid_postfix("1.2.3.4", nullptr) == 1 && inet_aton_nid_postfix("x", nullptr) == 0, "null destination");

    Require(inet_addr_nid_postfix("127.0.0.1") == Network(127, 0, 0, 1), "inet_addr");
    Require(inet_addr_nid_postfix("bad") == 0xffffffffu && inet_addr_nid_postfix("255.255.255.255") == 0xffffffffu, "inet_addr none");

    char* const text = inet_ntoa_nid_postfix(Network(10, 20, 30, 40));
    Require(std::string(text) == "10.20.30.40", "inet_ntoa");
    Require(inet_ntoa_nid_postfix(Network(255, 255, 255, 255)) == text && std::string(text) == "255.255.255.255", "inet_ntoa static buffer");
    char buffer[16];
    Require(inet_ntoa_r_nid_postfix(Network(1, 2, 3, 4), buffer, sizeof(buffer)) == buffer && std::string(buffer) == "1.2.3.4", "inet_ntoa_r");
    char tiny[4] = {'k', 'e', 'e', 'p'};
    Require(inet_ntoa_r_nid_postfix(Network(1, 2, 3, 4), tiny, sizeof(tiny)) == tiny, "inet_ntoa_r small buffer returns it");

    char exact[14];
    std::memset(exact, 'z', sizeof(exact));
    Require(__inet_ntoa_r_nid_postfix(Network(192, 168, 0, 255), exact, sizeof(exact)) == exact && std::string(exact) == "192.168.0.255", "__inet_ntoa_r");
    char oneShort[13];
    std::memset(oneShort, 'z', sizeof(oneShort));
    *__error_nid_postfix() = 0;
    Require(__inet_ntoa_r_nid_postfix(Network(192, 168, 0, 255), oneShort, sizeof(oneShort)) == oneShort && *__error_nid_postfix() == 28, "__inet_ntoa_r small buffer");
    Require(std::string(oneShort, sizeof(oneShort)) == std::string(sizeof(oneShort), 'z') && std::string(text) == "255.255.255.255", "__inet_ntoa_r leaves the buffers");
    return 0;
}
