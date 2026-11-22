#include "prx/libc/include/General.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <array>
#include <cstdlib>
#include <stdexcept>

static void Require(bool condition) { if (!condition) std::abort(); }

template<class TCall>
static bool NotImplemented(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    std::array<char, 16> name{};
    Require(NotImplemented([] { ttyname_nid_postfix(0); }));
    Require(NotImplemented([&] { ttyname_r_nid_postfix(0, name.data(), name.size()); }));
    return 0;
}
