#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "pipe fault check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr int GuestEfault = 14;

int main() {
    int first[2]{-1, -1};
    Require(pipe_nid_postfix(first) == 0);
    Require(close_nid_postfix(first[0]) == 0 && close_nid_postfix(first[1]) == 0);
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        *__error_nid_postfix() = 0;
        Require(pipe_nid_postfix(reinterpret_cast<int*>(address)) == -1 && *__error_nid_postfix() == GuestEfault);
    }
    int second[2]{-1, -1};
    Require(pipe_nid_postfix(second) == 0 && second[0] == first[0] && second[1] == first[1]);
    Require(close_nid_postfix(second[0]) == 0 && close_nid_postfix(second[1]) == 0);
}
