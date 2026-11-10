#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

    const std::uint32_t classA = Network(10, 1, 2, 3);
    Require(inet_netof_nid_postfix(classA) == 10, "inet_netof class A");
    Require(__inet_netof_nid_postfix(classA) == 10, "__inet_netof class A");
    Require(inet_lnaof_nid_postfix(classA) == 0x010203, "inet_lnaof class A");
    Require(__inet_lnaof_nid_postfix(classA) == 0x010203, "__inet_lnaof class A");
    Require(inet_makeaddr_nid_postfix(10, 0x010203) == classA, "inet_makeaddr class A");
    Require(__inet_makeaddr_nid_postfix(10, 0x010203) == classA, "__inet_makeaddr class A");

    const std::uint32_t classB = Network(172, 16, 5, 6);
    Require(inet_netof_nid_postfix(classB) == 0xac10, "inet_netof class B");
    Require(__inet_netof_nid_postfix(classB) == 0xac10, "__inet_netof class B");
    Require(inet_lnaof_nid_postfix(classB) == 0x0506, "inet_lnaof class B");
    Require(__inet_lnaof_nid_postfix(classB) == 0x0506, "__inet_lnaof class B");
    Require(inet_makeaddr_nid_postfix(0xac10, 0x0506) == classB, "inet_makeaddr class B");
    Require(__inet_makeaddr_nid_postfix(0xac10, 0x0506) == classB, "__inet_makeaddr class B");

    const std::uint32_t classC = Network(192, 168, 1, 100);
    Require(inet_netof_nid_postfix(classC) == 0xc0a801, "inet_netof class C");
    Require(__inet_netof_nid_postfix(classC) == 0xc0a801, "__inet_netof class C");
    Require(inet_lnaof_nid_postfix(classC) == 100, "inet_lnaof class C");
    Require(__inet_lnaof_nid_postfix(classC) == 100, "__inet_lnaof class C");
    Require(inet_makeaddr_nid_postfix(0xc0a801, 100) == classC, "inet_makeaddr class C");
    Require(__inet_makeaddr_nid_postfix(0xc0a801, 100) == classC, "__inet_makeaddr class C");

    const std::uint32_t classD = Network(224, 1, 2, 3);
    Require(inet_netof_nid_postfix(classD) == 0xe00102u, "inet_netof class D");
    Require(inet_lnaof_nid_postfix(classD) == 0x03u, "inet_lnaof class D");

    const std::uint32_t classE = Network(240, 5, 6, 7);
    Require(inet_netof_nid_postfix(classE) == 0xf00506u, "inet_netof class E");
    Require(inet_lnaof_nid_postfix(classE) == 0x07u, "inet_lnaof class E");

    Require(inet_makeaddr_nid_postfix(127, 0x123456) == Network(127, 0x12, 0x34, 0x56), "inet_makeaddr threshold < 128");
    Require(inet_makeaddr_nid_postfix(128, 0x1234) == Network(0, 128, 0x12, 0x34), "inet_makeaddr threshold == 128");
    Require(inet_makeaddr_nid_postfix(65535, 0x5678) == Network(255, 255, 0x56, 0x78), "inet_makeaddr threshold 65535");
    Require(inet_makeaddr_nid_postfix(65536, 0x9a) == Network(1, 0, 0, 0x9a), "inet_makeaddr threshold == 65536");
    Require(inet_makeaddr_nid_postfix(16777215, 0xbc) == Network(255, 255, 255, 0xbc), "inet_makeaddr threshold 16777215");
    Require(inet_makeaddr_nid_postfix(16777216, 0xde) == Network(1, 0, 0, 0xde), "inet_makeaddr threshold >= 16777216");

    Require(inet_network_nid_postfix("127.0.0.1") == 0x7f000001u, "inet_network dotted quad");
    Require(__inet_network_nid_postfix("127.0.0.1") == 0x7f000001u, "__inet_network dotted quad");
    Require(inet_network_nid_postfix("10.1") == 0x0a01u, "inet_network two parts");
    Require(inet_network_nid_postfix("192.168.1") == 0xc0a801u, "inet_network three parts");
    Require(inet_network_nid_postfix("0xff") == 0xffu, "inet_network hex");
    Require(inet_network_nid_postfix("0x1234") == 0xffffffffu && inet_network_nid_postfix("300") == 0xffffffffu, "inet_network component > 0xff");
    Require(inet_network_nid_postfix("0177.0.0.1") == 0x7f000001u, "inet_network octal dotted quad");
    Require(inet_network_nid_postfix("012") == 10u, "inet_network octal single");
    Require(inet_network_nid_postfix("08") == 0xffffffffu && inet_network_nid_postfix("09") == 0xffffffffu, "inet_network invalid octal digit");
    Require(inet_network_nid_postfix("1.2.3.4.5") == 0xffffffffu, "inet_network more than four parts");
    Require(inet_network_nid_postfix("0x") == 0u, "inet_network bare 0x");
    Require(inet_network_nid_postfix("x1f") == 0x1fu, "inet_network x prefix");
    Require(inet_network_nid_postfix("1 x") == 1u, "inet_network space before char");
    Require(inet_network_nid_postfix("1x") == 0xffffffffu, "inet_network junk immediately after");
    Require(inet_network_nid_postfix("bad") == 0xffffffffu, "inet_network bad");
    Require(inet_network_nid_postfix("256.1.1.1") == 0xffffffffu, "inet_network out of range");
    Require(inet_network_nid_postfix("0XAB") == 0xabu && inet_network_nid_postfix("0xAb.0XcD") == 0xabcdu && inet_network_nid_postfix("xF") == 0xfu, "inet_network upper-case hex");
    Require(inet_network_nid_postfix("x") == 0xffffffffu && inet_network_nid_postfix("X") == 0xffffffffu, "inet_network x without digits");
    Require(inet_network_nid_postfix("1..2") == 0xffffffffu && inet_network_nid_postfix(".1") == 0xffffffffu && inet_network_nid_postfix("1.") == 0xffffffffu, "inet_network empty part");
    Require(inet_network_nid_postfix("1\t") == 1u && inet_network_nid_postfix("1\n") == 1u && inet_network_nid_postfix("1\v") == 1u, "inet_network trailing whitespace");
    Require(inet_network_nid_postfix("1-") == 0xffffffffu, "inet_network trailing junk");

    bool threw = false;
    try { inet_network_nid_postfix(nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw, "inet_network null throws");
    threw = false;
    try { __inet_network_nid_postfix(nullptr); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw, "__inet_network null throws");

    return 0;
}
