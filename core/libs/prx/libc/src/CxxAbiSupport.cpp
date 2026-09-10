#include "prx/libc/include/exceptions/Runtime.hpp"
#include <cstddef>
#ifndef _UNWIND_H
#define _UNWIND_H
#endif
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
extern "C" {

void* APS5_VABI __cxa_demangle_nid_postfix(const char* mangled, char* buf, std::size_t* len, int* status) {
    return abi::__cxa_demangle(mangled, buf, len, status);
}

int APS5_VABI __cxa_thread_atexit_impl_nid_postfix(void (*func)(void*), void* arg, void* dso) {
    return __cxxabiv1::__cxa_thread_atexit(func, arg, dso);
}

void _ZNSt8ios_baseD2Ev_nid_postfix(std::ios_base* self) { self->~ios_base(); }
const std::error_category* _ZSt17iostream_categoryv_nid_postfix() { return &std::iostream_category(); }

}
