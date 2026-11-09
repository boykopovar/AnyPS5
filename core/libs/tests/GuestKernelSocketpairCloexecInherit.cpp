#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <cstring>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::abort();
    }
}

constexpr int Unix = 1;
constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int CloseOnExec = 0x10000000;

int main() {
    int fds[2] = {-1, -1};
    *__error_nid_postfix() = 0;
    Require(socketpair_nid_postfix(Unix, Stream | CloseOnExec, 0, fds) == 0, "socketpair with SOCK_CLOEXEC failed");
    Require(fds[0] >= 0, "fd[0] invalid");
    Require(fds[1] >= 0, "fd[1] invalid");

#ifndef _WIN32
    Require(fcntl_nid_postfix(fds[0], 1, 0) == 1, "fd[0] cloexec flag not set");
    Require(fcntl_nid_postfix(fds[1], 1, 0) == 1, "fd[1] cloexec flag not set");
#endif

    Require(close_nid_postfix(fds[0]) == 0, "close fd[0] failed");
    Require(close_nid_postfix(fds[1]) == 0, "close fd[1] failed");

    int badfds[2] = {-1, -1};
    *__error_nid_postfix() = 0;
    Require(socketpair_nid_postfix(999, Stream, 0, badfds) == -1, "socketpair with invalid domain should fail");

    return 0;
}
