#include "prx/libc/include/Api.hpp"
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
    Require(NotImplemented([] { timer_create_nid_postfix(0, nullptr, nullptr); }));
    Require(NotImplemented([] { timer_delete_nid_postfix(nullptr); }));
    Require(NotImplemented([] { timer_settime_nid_postfix(nullptr, 0, nullptr, nullptr); }));
    Require(NotImplemented([] { timer_gettime_nid_postfix(nullptr, nullptr); }));
    Require(NotImplemented([] { timer_getoverrun_nid_postfix(nullptr); }));
    return 0;
}
