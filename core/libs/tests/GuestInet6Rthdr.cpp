#include "prx/libSceLibcInternal/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "inet6 rthdr check failed\n");
        std::abort();
    }
}

int main() {
    Require(inet6_rth_space_nid_postfix(0, 2) == 40);
    Require(inet6_rth_space_nid_postfix(0, 128) == 0);
    Require(inet6_rth_space_nid_postfix(1, 2) == 0);
    Require(inet6_rthdr_space_nid_postfix(0, 1) == 40);
    Require(inet6_rthdr_space_nid_postfix(0, 0) == 0);
    Require(inet6_rthdr_space_nid_postfix(0, 24) == 0);

    alignas(16) std::uint8_t buffer[128];
    std::memset(buffer, 0xee, sizeof(buffer));
    Require(inet6_rth_init_nid_postfix(buffer, 40, 0, 2) == buffer);
    Require(buffer[1] == 4 && buffer[2] == 0 && buffer[3] == 0);
    Require(inet6_rth_init_nid_postfix(buffer, 16, 0, 2) == nullptr);

    GuestInAddr6 first{{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};
    GuestInAddr6 second{{2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}};
    GuestInAddr6 third{{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}};
    Require(inet6_rth_add_nid_postfix(buffer, &first) == 0);
    Require(inet6_rth_add_nid_postfix(buffer, &second) == 0);
    Require(inet6_rth_add_nid_postfix(buffer, &third) == -1);
    Require(buffer[3] == 2);
    Require(std::memcmp(buffer + 8, first.s6_addr, 16) == 0);
    Require(inet6_rth_segments_nid_postfix(buffer) == 2);
    Require(inet6_rth_getaddr_nid_postfix(buffer, 1) == reinterpret_cast<GuestInAddr6*>(buffer + 24));
    Require(inet6_rth_getaddr_nid_postfix(buffer, 2) == nullptr);

    alignas(16) std::uint8_t reversed[128];
    std::memset(reversed, 0, sizeof(reversed));
    Require(inet6_rth_reverse_nid_postfix(buffer, reversed) == 0);
    Require(reversed[3] == 2);
    Require(std::memcmp(reversed + 8, second.s6_addr, 16) == 0);
    Require(std::memcmp(reversed + 24, first.s6_addr, 16) == 0);

    alignas(16) std::uint8_t control[128];
    std::memset(control, 0, sizeof(control));
    auto* cmsg = inet6_rthdr_init_nid_postfix(control, 0);
    Require(cmsg != nullptr && cmsg->cmsg_len == 24 && cmsg->cmsg_level == 41 && cmsg->cmsg_type == 51);
    Require(inet6_rthdr_add_nid_postfix(cmsg, &first, 0) == 0);
    Require(cmsg->cmsg_len == 40);
    Require(inet6_rthdr_segments_nid_postfix(cmsg) == 1);
    Require(inet6_rthdr_getaddr_nid_postfix(cmsg, 1) == reinterpret_cast<GuestInAddr6*>(control + 40));
    Require(inet6_rthdr_getflags_nid_postfix(cmsg, 0) == 0);
    Require(inet6_rthdr_add_nid_postfix(cmsg, &second, 1) == -1);
    return 0;
}
