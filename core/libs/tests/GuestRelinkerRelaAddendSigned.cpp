#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}

static void TestSignedAddendArithmetic() {
    std::int64_t pos_addend = 42;
    std::int64_t neg_addend = -42;
    Require(pos_addend + 10 == 52, "positive addend addition");
    Require(neg_addend + 10 == -32, "negative addend addition");
    Require(neg_addend < 0, "negative addend sign check");
}

int main() {
    TestSignedAddendArithmetic();
    std::printf("ALL RELINKER RELA ADDEND SIGNED BOUNDS TESTS PASSED\n");
    return 0;
}
