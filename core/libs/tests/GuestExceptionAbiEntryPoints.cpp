#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "exception ABI entry point check failed at line %d\n", line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), __LINE__)

}

int main() {
    REQUIRE(!__cxa_uncaught_exception_nid_postfix() && __cxa_uncaught_exceptions_nid_postfix() == 0);
    REQUIRE(!_ZSt18uncaught_exceptionv_nid_postfix());

    void* const globals = __cxa_get_globals_nid_postfix();
    REQUIRE(globals != nullptr && __cxa_get_globals_fast_nid_postfix() == globals && __cxa_get_globals_nid_postfix() == globals);

    void* const first = __cxa_allocate_dependent_exception_nid_postfix();
    void* const second = __cxa_allocate_dependent_exception_nid_postfix();
    REQUIRE(first != nullptr && second != nullptr && first != second);
    __cxa_free_dependent_exception_nid_postfix(first);
    __cxa_free_dependent_exception_nid_postfix(second);
    __cxa_free_dependent_exception_nid_postfix(nullptr);

    std::puts("exception ABI entry points passed");
    return 0;
}
