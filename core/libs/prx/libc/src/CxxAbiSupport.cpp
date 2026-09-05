#include "exception/Runtime.hpp"
#include <cstddef>
#include <cxxabi.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <typeinfo>
#include <new>
#include <ios>
#include <locale>
#include <regex>
#include <functional>
#include <unwind.h>

extern "C" {

void* __cxa_demangle_nid_postfix(const char* mangled, char* buf, std::size_t* len, int* status) {
    (void)mangled; (void)buf; (void)len;
    if (status) *status = -2;
    return nullptr;
}

int __cxa_thread_atexit_impl_nid_postfix(void (*func)(void*), void* arg, void* dso) {
    (void)func; (void)arg; (void)dso;
    return 0;
}

void _ZNSt8ios_baseD2Ev_nid_postfix(std::ios_base* self) { self->~ios_base(); }
const std::error_category* _ZSt17iostream_categoryv_nid_postfix() { return &std::iostream_category(); }

}
