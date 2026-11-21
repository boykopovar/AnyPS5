#include "prx/libc/include/Api.hpp"
#include <cstdlib>
#include <stdexcept>
#include <system_error>

namespace {

void Require(bool value) { if (!value) std::abort(); }

struct Marker {
    int& destroyed;
    ~Marker() { ++destroyed; }
};

[[gnu::noinline]] void ThrowThroughCleanup(int& destroyed) {
    Marker marker {destroyed};
    _ZSt16_Throw_Cpp_errori_nid_postfix(4);
}

}

int main() {
    int destroyed = 0;
    bool caught = false;
    try { ThrowThroughCleanup(destroyed); }
    catch (const std::system_error&) { caught = true; }
    Require(caught && destroyed == 1);

    destroyed = 0;
    caught = false;
    try { ThrowThroughCleanup(destroyed); }
    catch (const std::runtime_error&) { caught = true; }
    Require(caught && destroyed == 1);

    destroyed = 0;
    caught = false;
    bool wrongType = false;
    try { ThrowThroughCleanup(destroyed); }
    catch (const std::logic_error&) { wrongType = true; }
    catch (...) { caught = true; }
    Require(caught && !wrongType && destroyed == 1);
    return 0;
}
