#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <array>
#include <cstdlib>
#include <stdexcept>
#ifndef _WIN32
#include <unistd.h>
#endif

static void Require(bool condition) { if (!condition) std::abort(); }

#ifdef _WIN32
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
#else
int main() {
    int descriptors[2];
    Require(pipe(descriptors) == 0);
    std::array<char, 64> name{};

    *__error_nid_postfix() = 0;
    Require(ttyname_nid_postfix(descriptors[0]) == nullptr && *__error_nid_postfix() == 25);
    Require(ttyname_r_nid_postfix(descriptors[0], name.data(), name.size()) == 25);
    Require(ttyname_r_nid_postfix(-1, name.data(), name.size()) == 9);
    Require(ttyname_r_nid_postfix(descriptors[0], nullptr, 16) == 22);
    Require(close(descriptors[0]) == 0 && close(descriptors[1]) == 0);
    return 0;
}
#endif
