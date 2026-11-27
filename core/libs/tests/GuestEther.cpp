#include "prx/libSceLibcInternal/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "ether check failed\n");
        std::abort();
    }
}

static void ExpectOctets(const EtherAddress* address, const std::uint8_t (&expected)[6]) {
    Require(address != nullptr);
    Require(std::memcmp(address->octet, expected, sizeof(expected)) == 0);
}

int main() {
    const std::uint8_t mac[6] = {0x00, 0x11, 0x22, 0xaa, 0xbb, 0xcc};
    const EtherAddress* parsed = ether_aton_nid_postfix("00:11:22:aa:bb:cc");
    ExpectOctets(parsed, mac);
    Require(std::strcmp(ether_ntoa_nid_postfix(parsed), "00:11:22:aa:bb:cc") == 0);

    const std::uint8_t sparse[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    ExpectOctets(ether_aton_nid_postfix("0:1:2:3:4:5"), sparse);

    const EtherAddress upper{{0x01, 0x0f, 0xa0, 0xff, 0x10, 0x00}};
    Require(std::strcmp(ether_ntoa_nid_postfix(&upper), "01:0f:a0:ff:10:00") == 0);

    Require(ether_aton_nid_postfix("00:11:22:33:44") == nullptr);
    Require(ether_aton_nid_postfix("zz:00:00:00:00:00") == nullptr);
    Require(ether_aton_nid_postfix(nullptr) == nullptr);
    Require(ether_ntoa_nid_postfix(nullptr) == nullptr);

    const std::uint8_t truncated[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ExpectOctets(ether_aton_nid_postfix("100:0:0:0:0:0"), truncated);
    return 0;
}
