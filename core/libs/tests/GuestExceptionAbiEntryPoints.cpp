#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <typeinfo>

namespace {

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "exception ABI entry point check failed at line %d\n", line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), __LINE__)

template <typename TCaught, typename TThrow>
void RequireThrows(TThrow&& thrower, const char* what, int line) {
    bool caught = false;
    try { thrower(); }
    catch (const TCaught& error) {
        caught = true;
        Require(std::strcmp(error.what(), what) == 0, line);
    }
    Require(caught, line);
}

}

int main() {
    REQUIRE(!__cxa_uncaught_exception_nid_postfix() && __cxa_uncaught_exceptions_nid_postfix() == 0);
    REQUIRE(!_ZSt18uncaught_exceptionv_nid_postfix() && _ZSt19uncaught_exceptionsv_nid_postfix() == 0);

    void* const globals = __cxa_get_globals_nid_postfix();
    REQUIRE(globals != nullptr && __cxa_get_globals_fast_nid_postfix() == globals && __cxa_get_globals_nid_postfix() == globals);

    void* const first = __cxa_allocate_dependent_exception_nid_postfix();
    void* const second = __cxa_allocate_dependent_exception_nid_postfix();
    REQUIRE(first != nullptr && second != nullptr && first != second);
    __cxa_free_dependent_exception_nid_postfix(first);
    __cxa_free_dependent_exception_nid_postfix(second);
    __cxa_free_dependent_exception_nid_postfix(nullptr);

    RequireThrows<std::bad_typeid>([] { __cxa_bad_typeid_nid_postfix(); }, "std::bad_typeid", __LINE__);
    RequireThrows<std::bad_array_new_length>([] { __cxa_throw_bad_array_new_length_nid_postfix(); }, "bad allocation", __LINE__);
    RequireThrows<std::bad_alloc>([] { _ZSt11_Xbad_allocv_nid_postfix(); }, "bad allocation", __LINE__);
    RequireThrows<std::length_error>([] { _ZSt14_Xlength_errorPKc_nid_postfix("string too long"); }, "string too long", __LINE__);
    RequireThrows<std::invalid_argument>([] { _ZSt18_Xinvalid_argumentPKc_nid_postfix("invalid stoi argument"); }, "invalid stoi argument", __LINE__);
    RequireThrows<std::logic_error>([] { _ZSt14_Xlength_errorPKc_nid_postfix("caught as its base"); }, "caught as its base", __LINE__);

    std::puts("exception ABI entry points passed");
    return 0;
}
