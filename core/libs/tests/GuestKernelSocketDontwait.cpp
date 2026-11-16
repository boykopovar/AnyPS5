#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

constexpr int DontWait = 0x80;
constexpr int WouldBlock = 35;

int main() {
    int pair[2] = {-1, -1};
    int res = socketpair_nid_postfix(1, 1, 0, pair);
    Require(res == 0);
    Require(pair[0] >= 0);
    Require(pair[1] >= 0);

    char buffer[16];
    *__error_nid_postfix() = 0;
    std::int64_t recvd = recv_nid_postfix(pair[0], buffer, sizeof(buffer), DontWait);
    Require(recvd == -1);
    Require(*__error_nid_postfix() == WouldBlock);

    *__error_nid_postfix() = 0;
    std::int64_t sent = send_nid_postfix(pair[1], "test", 4, DontWait);
    Require(sent == 4);

    *__error_nid_postfix() = 0;
    recvd = recv_nid_postfix(pair[0], buffer, sizeof(buffer), DontWait);
    Require(recvd == 4);
    Require(std::memcmp(buffer, "test", 4) == 0);

    *__error_nid_postfix() = 0;
    recvd = recv_nid_postfix(pair[0], buffer, sizeof(buffer), DontWait);
    Require(recvd == -1);
    Require(*__error_nid_postfix() == WouldBlock);

    *__error_nid_postfix() = 0;
    sent = send_nid_postfix(pair[0], "ping", 4, DontWait);
    Require(sent == 4);

    *__error_nid_postfix() = 0;
    recvd = recv_nid_postfix(pair[1], buffer, sizeof(buffer), DontWait);
    Require(recvd == 4);
    Require(std::memcmp(buffer, "ping", 4) == 0);

    *__error_nid_postfix() = 0;
    recvd = recv_nid_postfix(pair[1], buffer, sizeof(buffer), DontWait);
    Require(recvd == -1);
    Require(*__error_nid_postfix() == WouldBlock);

    close_nid_postfix(pair[0]);
    close_nid_postfix(pair[1]);
    return 0;
}
