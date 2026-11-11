#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <source_location>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "pipe capacity check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

bool Succeeds(int fd, int cmd, int arg, int* result) {
    *__error_nid_postfix() = 77;
    int res = fcntl_nid_postfix(fd, cmd, arg);
    if (res >= 0 && result) {
        *result = res;
    }
    return res >= 0 && *__error_nid_postfix() == 77;
}

bool Fails(int fd, int cmd, int arg, int error) {
    *__error_nid_postfix() = 0;
    return fcntl_nid_postfix(fd, cmd, arg) == -1 && *__error_nid_postfix() == error;
}
} // namespace

int main() {
    int pipefd[2];
    Require(pipe_nid_postfix(pipefd) == 0);

    int capacity = 0;
    Require(Succeeds(pipefd[0], 1034, 0, &capacity));
    Require(capacity >= 4096);

    int new_capacity = 0;
    if (Succeeds(pipefd[0], 1035, capacity * 2, &new_capacity)) {
        Require(new_capacity >= capacity * 2);
    }

    Require(Fails(pipefd[0], 1035, 1, 22));

    char buffer[4096];
    for (std::size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = static_cast<char>(i & 0xff);
    }

    ssize_t written = write_nid_postfix(pipefd[1], buffer, sizeof(buffer));
    Require(written == static_cast<ssize_t>(sizeof(buffer)));

    char read_buf[4096];
    ssize_t bytes_read = read_nid_postfix(pipefd[0], read_buf, sizeof(read_buf));
    Require(bytes_read == static_cast<ssize_t>(sizeof(read_buf)));

    Require(close_nid_postfix(pipefd[0]) == 0);
    Require(close_nid_postfix(pipefd[1]) == 0);

    return 0;
}
