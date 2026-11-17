#include "prx/libScePsm/libScePsm.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "PSM CONSTANT RETURNS: %s\n", message);
    std::abort();
}

}

int main() {
    Require(libScePsmUnknown_0PIOM2UIUpQ_nid_no_patch() == 0, "0PIOM2UIUpQ result");
    Require(libScePsmUnknown_W8PzsJgkCGU_nid_no_patch() == 0, "W8PzsJgkCGU result");
    Require(libScePsmUnknown_PN1KQl3wnoU_nid_no_patch() == static_cast<int>(0x80B30006), "PN1KQl3wnoU result");
    std::puts("Psm constant return tests passed");
    return 0;
}
