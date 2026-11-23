#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

template<class TCall>
bool NotImplemented(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    Require(NotImplemented([] { ctermid_nid_postfix(nullptr); }));
    Require(NotImplemented([] { brk_nid_postfix(nullptr); }));
    Require(NotImplemented([] { sbrk_nid_postfix(0); }));
    Require(NotImplemented([] { getpass_nid_postfix("prompt"); }));
    return 0;
}
