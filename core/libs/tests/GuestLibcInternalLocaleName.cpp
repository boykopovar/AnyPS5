#include "prx/libSceLibcInternal/include/LocaleName.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal locale name: %s\n", message);
    std::abort();
}

bool Rejects(const char* name) {
    try {
        _ZSt15_sceLibcLocinfoPKc_nid_postfix(name);
    } catch (...) {
        return true;
    }
    return false;
}

}

int main() {
    Require(!Rejects("C"), "the C locale is accepted");
    Require(!Rejects(""), "the empty name is accepted");
    Require(Rejects(nullptr), "a null name throws bad locale name");
    Require(Rejects("POSIX"), "another name throws bad locale name");
    Require(Rejects("Cx"), "a name that only starts with C throws bad locale name");
    Require(Rejects("c"), "the comparison is case sensitive");
    return 0;
}
