#include "prx/libSceLibcInternal/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "inet6 options check failed\n");
        std::abort();
    }
}

int main() {
    Require(inet6_option_space_nid_postfix(0) == 24);
    Require(inet6_option_space_nid_postfix(6) == 24);
    Require(inet6_option_space_nid_postfix(7) == 32);

    alignas(16) std::uint8_t buffer[64];
    std::memset(buffer, 0, sizeof(buffer));
    GuestCmsghdr* cmsg = nullptr;
    Require(inet6_option_init_nid_postfix(buffer, &cmsg, 49) == 0);
    Require(cmsg == reinterpret_cast<GuestCmsghdr*>(buffer));
    Require(cmsg->cmsg_len == 16 && cmsg->cmsg_level == 41 && cmsg->cmsg_type == 49);
    Require(inet6_option_init_nid_postfix(buffer, &cmsg, 7) == -1);

    const std::uint8_t option[4] = {0x05, 0x02, 0x00, 0x00};
    Require(inet6_option_append_nid_postfix(cmsg, option, 1, 0) == 0);
    Require(cmsg->cmsg_len == 24);
    Require(buffer[1] == 0);
    Require(std::memcmp(&buffer[18], option, sizeof(option)) == 0);
    Require(buffer[22] == 0x01 && buffer[23] == 0x00);

    std::uint8_t* tptr = nullptr;
    Require(inet6_option_next_nid_postfix(cmsg, &tptr) == 0);
    Require(tptr == &buffer[18]);
    Require(inet6_option_next_nid_postfix(cmsg, &tptr) == 0);
    Require(tptr == &buffer[22]);
    Require(inet6_option_next_nid_postfix(cmsg, &tptr) == -1);
    Require(tptr == nullptr);

    tptr = nullptr;
    Require(inet6_option_find_nid_postfix(cmsg, &tptr, 5) == 0);
    Require(tptr == &buffer[18]);

    std::uint8_t extbuf[16];
    std::memset(extbuf, 0, sizeof(extbuf));
    Require(inet6_opt_init_nid_postfix(extbuf, 8) == 2);
    Require(extbuf[1] == 0);
    void* data = nullptr;
    Require(inet6_opt_append_nid_postfix(extbuf, 8, 2, 0x05, 2, 1, &data) == 6);
    Require(data == &extbuf[4]);
    const std::uint8_t value[2] = {0xaa, 0xbb};
    Require(inet6_opt_set_val_nid_postfix(data, 0, const_cast<std::uint8_t*>(value), 2) == 2);
    Require(inet6_opt_finish_nid_postfix(extbuf, 8, 6) == 8);
    Require(extbuf[6] == 0x01 && extbuf[7] == 0x00);

    std::uint8_t type = 0;
    std::uint32_t length = 0;
    void* found = nullptr;
    Require(inet6_opt_next_nid_postfix(extbuf, 8, 0, &type, &length, &found) == 6);
    Require(type == 0x05 && length == 2 && found == &extbuf[4]);
    found = nullptr;
    Require(inet6_opt_find_nid_postfix(extbuf, 8, 0, 0x05, &length, &found) == 6);
    Require(found == &extbuf[4]);
    Require(inet6_opt_find_nid_postfix(extbuf, 8, 0, 0x09, &length, &found) == -1);
    Require(found == nullptr);

    std::uint8_t out[2] = {};
    Require(inet6_opt_get_val_nid_postfix(data, 0, out, 2) == 2);
    Require(out[0] == 0xaa && out[1] == 0xbb);
    return 0;
}
