#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

static void Require(bool value) {
    if (!value) std::abort();
}

static void RequireFailure(int result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

int main() {
    int fds[2];
    Require(pipe_nid_postfix(fds) == 0);
    Require(fds[0] >= 0);
    Require(fds[1] >= 0);
    Require(close_nid_postfix(fds[0]) == 0);
    Require(close_nid_postfix(fds[1]) == 0);

    Require(pipe2_nid_postfix(fds, 4) == 0);
    char buf[1];
    RequireFailure(read_nid_postfix(fds[0], buf, 1), 35);
    Require(close_nid_postfix(fds[0]) == 0);
    Require(close_nid_postfix(fds[1]) == 0);

    Require(pipe2_nid_postfix(fds, 0x00100000) == 0);
    Require(close_nid_postfix(fds[0]) == 0);
    Require(close_nid_postfix(fds[1]) == 0);

    RequireFailure(pipe2_nid_postfix(nullptr, 0), 14);
    RequireFailure(pipe2_nid_postfix(fds, 0x80000000), 22);

    Require(pipe2_nid_postfix(fds, 4) == 0);
    std::vector<char> large(65536, 'A');
    std::size_t written = 0;
    while (true) {
        const auto n = write_nid_postfix(fds[1], large.data(), large.size());
        if (n < 0) {
            Require(*__error_nid_postfix() == 35);
            break;
        }
        written += static_cast<std::size_t>(n);
        if (written >= 65536 * 2) break;
    }
    Require(close_nid_postfix(fds[0]) == 0);
    Require(close_nid_postfix(fds[1]) == 0);
    return 0;
}
