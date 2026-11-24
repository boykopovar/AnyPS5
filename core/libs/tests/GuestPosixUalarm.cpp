#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    Require(ualarm_nid_postfix(0, 0) == 0);
    Require(ualarm_nid_postfix(500000, 0) == 0);
    const std::uint32_t remaining = ualarm_nid_postfix(0, 0);
    Require(remaining > 400000 && remaining <= 500000);
    Require(ualarm_nid_postfix(0, 0) == 0);

    return 0;
}
