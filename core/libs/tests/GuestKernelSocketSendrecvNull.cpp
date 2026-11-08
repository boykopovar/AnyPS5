#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    int pair[2] = {-1, -1};
    int res = socketpair_nid_postfix(1, 1, 0, pair);
    Require(res == 0);
    Require(pair[0] >= 0);
    Require(pair[1] >= 0);

    char buffer[16] = "test";
    *__error_nid_postfix() = 0;
    std::int64_t sent = sendto_nid_postfix(pair[0], buffer, 4, 0, nullptr, 0);
    Require(sent == 4);

    *__error_nid_postfix() = 0;
    std::int64_t recvd = recvfrom_nid_postfix(pair[1], buffer, sizeof(buffer), 0, nullptr, nullptr);
    Require(recvd == 4);

    close_nid_postfix(pair[0]);
    close_nid_postfix(pair[1]);
    return 0;
}
