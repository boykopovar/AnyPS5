#include "prx/libScePsm/libScePsm.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
    Require(libScePsmUnknown_0PIOM2UIUpQ_nid_no_patch() == 0, "0PIOM2UIUpQ result");
    Require(libScePsmUnknown_W8PzsJgkCGU_nid_no_patch() == 0, "W8PzsJgkCGU result");
    Require(libScePsmUnknown_PN1KQl3wnoU_nid_no_patch() == static_cast<int>(0x80B30006), "PN1KQl3wnoU result");
    std::puts("Psm constant return tests passed");
    return 0;
}
