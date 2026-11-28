#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "ftok check failed\n");
        std::abort();
    }
}

int main() {
    FileStat status{};
    Require(stat_nid_postfix(".", &status) == 0);
    const auto expected = static_cast<std::int64_t>(static_cast<std::uint32_t>(1) << 24 | (status.st_dev & 0xffu) << 16 | (status.st_ino & 0xffffu));

    Require(ftok_nid_postfix(".", 1) == expected);
    Require(ftok_nid_postfix(".", 0x101) == expected);
    Require(ftok_nid_postfix(".", 0x1ff) == ftok_nid_postfix(".", 0xff));
    Require(ftok_nid_postfix("ftok-missing-path", 1) == -1);
    Require(ftok_nid_postfix(nullptr, 1) == -1);
    return 0;
}
