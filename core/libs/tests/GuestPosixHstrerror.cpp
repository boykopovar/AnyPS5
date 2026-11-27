#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "hstrerror check failed\n");
        std::abort();
    }
}

static void Expect(int code, const char* text) {
    Require(std::strcmp(hstrerror_nid_postfix(code), text) == 0);
}

int main() {
    Expect(0, "Resolver Error 0 (no error)");
    Expect(1, "Unknown host");
    Expect(2, "Host name lookup failure");
    Expect(3, "Unknown server error");
    Expect(4, "No address associated with name");
    Expect(5, "Unknown resolver error");
    Expect(std::numeric_limits<int>::max(), "Unknown resolver error");
    Expect(-1, "Resolver internal error");
    Expect(std::numeric_limits<int>::min(), "Resolver internal error");
    return 0;
}
