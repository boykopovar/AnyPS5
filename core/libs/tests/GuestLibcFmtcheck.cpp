#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "fmtcheck check failed\n");
        std::abort();
    }
}

static void ExpectFirst(const char* first, const char* second) {
    Require(fmtcheck_nid_postfix(first, second) == first);
}

static void ExpectSecond(const char* first, const char* second) {
    Require(fmtcheck_nid_postfix(first, second) == second);
}

int main() {
    ExpectFirst("%d %s", "%d %s");
    ExpectSecond("%d", "%s");
    ExpectSecond("%ld", "%d");
    ExpectFirst("%lld", "%qd");
    ExpectSecond(nullptr, "x");
    ExpectFirst("100%%", "x");
    ExpectFirst("%5.2f", "%5.2f");
    ExpectFirst("%*d", "%*d");
    ExpectFirst("%.*d", "%.*d");
    ExpectSecond("%", "x");
    ExpectFirst("hello", "world");
    ExpectFirst("%p", "%ld");
    ExpectSecond("%zd", "%td");
    ExpectFirst("%hhd", "%d");
    ExpectSecond("%Lf", "%f");
    ExpectSecond("%n", "%hn");
    return 0;
}
