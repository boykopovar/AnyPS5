#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal getdst test failed: %s\n", message);
    std::abort();
}

template <typename F>
bool Throws(F&& body) {
    try {
        body();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}

int main() {
    Require(_Getdst_nid_postfix(nullptr) == nullptr, "null input returned non-null");

    const char* r = _Getdst_nid_postfix(":0101");
    Require(r != nullptr, "default rule returned null");
    const auto* b = reinterpret_cast<const unsigned char*>(r);
    Require(b[0] == 0, "byte 0 != 0");
    Require(b[1] == 0, "byte 1 != 0");
    Require(b[2] == 0, "byte 2 != 0");
    Require(b[3] == 0, "byte 3 != 0");
    Require(b[4] == 0, "byte 4 != 0");
    Require(b[5] == 0xff, "byte 5 != 0xff");
    std::free(const_cast<char*>(r));

    Require(Throws([] { _Getdst_nid_postfix(reinterpret_cast<const char*>(0x10)); }), "unreadable pointer did not throw");

    return 0;
}
