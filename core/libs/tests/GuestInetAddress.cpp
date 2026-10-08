#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <initializer_list>

extern "C" {
std::uint32_t APS5_VABI __inet_addr_nid_postfix(const char*);
char* APS5_VABI __inet_ntoa_nid_postfix(std::uint32_t);
}

static void Require(bool condition) {
    if (!condition) std::abort();
}

int main() {
    for (const char* text : {"0.0.0.0", "127.0.0.1", "192.0.2.42", "255.255.255.255"}) {
        const auto address = __inet_addr_nid_postfix(text);
        Require(std::strcmp(__inet_ntoa_nid_postfix(address), text) == 0);
    }
    Require(std::strcmp(__inet_ntoa_nid_postfix(__inet_addr_nid_postfix("192.0.2.42")), "192.0.2.42") == 0);
    Require(__inet_addr_nid_postfix("invalid") == UINT32_MAX);
}
