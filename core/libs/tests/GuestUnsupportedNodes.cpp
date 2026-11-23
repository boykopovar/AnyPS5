#include "prx/libkernel/include/Api.hpp"
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
    Require(NotImplemented([] { dladdr_nid_postfix(nullptr, nullptr); }));
    Require(NotImplemented([] { mknod_nid_postfix("node", 0600, 0); }));
    return 0;
}
