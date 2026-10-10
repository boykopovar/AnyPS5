#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI __inet_pton_nid_postfix(int, const char*, void*);
const char* APS5_VABI __inet_ntop_nid_postfix(int, const void*, char*, std::uint32_t);
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    for (int family : {2, 28}) {
        const char* input = family == 2 ? "192.0.2.17" : "2001:db8::17";
        std::array<unsigned char, 16> address{};
        Require(__inet_pton_nid_postfix(family, input, address.data()) == 1);
        char output[64]{};
        Require(__inet_ntop_nid_postfix(family, address.data(), output, sizeof(output)) == output);
        Require(std::strcmp(input, output) == 0);
        output[0] = 'x';
        Require(__inet_ntop_nid_postfix(family, address.data(), output, 1) == nullptr);
        Require(*__error_nid_postfix() == 28 && output[0] == 'x');
        const auto original = address;
        Require(__inet_pton_nid_postfix(family, "not-an-address", address.data()) == 0);
        Require(address == original);
    }
    std::array<unsigned char, 16> address{};
    Require(__inet_pton_nid_postfix(10, "::1", address.data()) == -1);
    Require(*__error_nid_postfix() == 47); // Linux AF_INET6 is not PS5 AF_INET6
    Require(__inet_pton_nid_postfix(2, "256.1.2.3", address.data()) == 0);
    Require(__inet_pton_nid_postfix(28, "::ffff:192.0.2.17", address.data()) == 1);
    Require(address[10] == 255 && address[11] == 255 && address[12] == 192 && address[15] == 17);

    for (const char* rejected : {"01.2.3.4", "1.2.3", "1.2.3.4.5", "1.2.3.4.", " 1.2.3.4", "0x1.2.3.4", "1..3.4", "1.2.3.-4"}) {
        address.fill(0xA5);
        Require(__inet_pton_nid_postfix(2, rejected, address.data()) == 0);
        Require(address[0] == 0xA5);
    }
    for (const char* rejected : {"::ffff:01.2.3.4", "::ffff:1.2.3.04", "1:2:3:4:5:6:7:8::", "::1:2:3:4:5:6:7:8", "1::2::3", ":::", ":1::", "1::2:", ":1:2:3:4:5:6:7",
                                 "1:2:3:4:5:6:7", "1:2:3:4:5:6:7:8:9", "12345::", "fe80::1%eth0", "1.2.3.4::", "::1.2.3.4:1", "::g", "1:2:3:4:5:6:1.2.3.4:7",
                                 "1:2:3:4:5:6:7:1.2.3.4", ""}) {
        address.fill(0xA5);
        Require(__inet_pton_nid_postfix(28, rejected, address.data()) == 0);
        Require(address[0] == 0xA5 && address[15] == 0xA5);
    }
    Require(__inet_pton_nid_postfix(28, "1:2:3:4:5:6:7::", address.data()) == 1);
    Require(address[13] == 7 && address[14] == 0 && address[1] == 1);
    Require(__inet_pton_nid_postfix(28, "ABCD:ef01::1", address.data()) == 1);
    Require(address[0] == 0xAB && address[1] == 0xCD && address[2] == 0xEF && address[3] == 0x01 && address[15] == 1);

    struct Text { const char* bytesAsText; const char* expected; };
    const Text samples[] = {
        {"::ffff:0.0.0.1", "::ffff:0.0.0.1"}, {"::ffff:0:1:0", "::ffff:0:1:0"}, {"::1", "::1"}, {"::", "::"}, {"1::", "1::"},
        {"::2", "::2"}, {"::1.2.3.4", "::1.2.3.4"}, {"1:0:0:2:0:0:0:3", "1:0:0:2::3"}, {"1:0:2:0:3:0:4:0", "1:0:2:0:3:0:4:0"},
        {"0:0:1:0:0:2:0:0", "::1:0:0:2:0:0"}, {"::ffff:1.2.3.4", "::ffff:1.2.3.4"}, {"2001:DB8::17", "2001:db8::17"}};
    for (const Text& sample : samples) {
        Require(__inet_pton_nid_postfix(28, sample.bytesAsText, address.data()) == 1);
        char output[64]{};
        Require(__inet_ntop_nid_postfix(28, address.data(), output, sizeof(output)) == output);
        Require(std::strcmp(output, sample.expected) == 0);
        const auto length = std::strlen(sample.expected);
        std::memset(output, 'x', sizeof(output));
        Require(__inet_ntop_nid_postfix(28, address.data(), output, static_cast<std::uint32_t>(length + 1)) == output);
        Require(std::strcmp(output, sample.expected) == 0);
        std::memset(output, 'x', sizeof(output));
        *__error_nid_postfix() = 0;
        Require(__inet_ntop_nid_postfix(28, address.data(), output, static_cast<std::uint32_t>(length)) == nullptr);
        Require(*__error_nid_postfix() == 28 && output[0] == 'x');
    }
    address.fill(0xFF);
    char widest[64]{};
    Require(__inet_ntop_nid_postfix(28, address.data(), widest, sizeof(widest)) == widest);
    Require(std::strcmp(widest, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff") == 0);
    const unsigned char fourth[4] = {255, 0, 7, 100};
    Require(__inet_ntop_nid_postfix(2, fourth, widest, 12) == widest && std::strcmp(widest, "255.0.7.100") == 0);
    Require(__inet_ntop_nid_postfix(2, fourth, widest, 11) == nullptr && *__error_nid_postfix() == 28);
    Require(__inet_ntop_nid_postfix(10, fourth, widest, sizeof(widest)) == nullptr && *__error_nid_postfix() == 47);
}
