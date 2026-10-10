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
}
