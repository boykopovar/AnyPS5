#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "pipe check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}
} // namespace

int main() {
    int pipefd[2];
    Require(pipe_nid_postfix(pipefd) == 0);

    char buffer[4096];
    for (std::size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = static_cast<char>(i & 0xff);
    }

    std::int64_t written = write_nid_postfix(pipefd[1], buffer, sizeof(buffer));
    Require(written == static_cast<std::int64_t>(sizeof(buffer)));

    char read_buf[4096];
    std::int64_t bytes_read = read_nid_postfix(pipefd[0], read_buf, sizeof(read_buf));
    Require(bytes_read == static_cast<std::int64_t>(sizeof(read_buf)));
    for (std::size_t i = 0; i < sizeof(buffer); ++i) {
        Require(read_buf[i] == buffer[i]);
    }

    Require(close_nid_postfix(pipefd[0]) == 0);
    Require(close_nid_postfix(pipefd[1]) == 0);

    return 0;
}
