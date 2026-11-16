#include "prx/libc/include/Api.hpp"
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(_Wctob_nid_postfix(static_cast<std::uint32_t>(L'A')) == 'A');
    return 0;
}
