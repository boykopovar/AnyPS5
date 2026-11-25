#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main() {
    auto* guestErrno = __error_nid_postfix();
    const auto savedErrno = *guestErrno;
    const auto Require = [&](bool value, const char* what) {
        if (value) return;
        std::fprintf(stderr, "failed: %s\n", what);
        *guestErrno = savedErrno;
        std::exit(EXIT_FAILURE);
    };

    unsigned char state[16]{};
    std::uint16_t output[] = {0x7777, 0x8888, 0x9999};
    *guestErrno = 7;
    Require(mbrtowc_nid_postfix(output, "\xe2", 1, state) == static_cast<std::size_t>(-2),
        "the first byte leaves an incomplete character");
    Require(output[0] == 0x7777 && output[1] == 0x8888 && output[2] == 0x9999,
        "an incomplete character leaves output untouched");
    Require(!mbsinit_nid_postfix(state), "the incomplete character leaves a pending state");
    Require(*guestErrno == 7, "an incomplete character preserves errno");

    const char tail[] = "\x82\xac" "A";
    const char* cursor = tail;
    Require(mbsrtowcs_nid_postfix(output, &cursor, 1, state) == 1, "resume writes exactly one character");
    Require(output[0] == 0x20ac, "resume completes the pending character");
    Require(output[1] == 0x8888 && output[2] == 0x9999, "resume leaves the output guards untouched");
    Require(cursor == tail + 2, "resume consumes only the two remaining bytes");
    Require(mbsinit_nid_postfix(state), "resume returns the state to initial");
    Require(*guestErrno == 7, "resume preserves errno");
    *guestErrno = savedErrno;
    return 0;
}
