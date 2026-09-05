#include "exception/Unwind.cpp"
#include "exception/Runtime.cpp"
#include "exception/Personality.cpp"
#include "exception/StandardExceptions.cpp"
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

int __cxa_guard_acquire_nid_postfix(std::uint64_t* guardObject) {
    (void)guardObject;
    return 0;
}

void __cxa_guard_release_nid_postfix(std::uint64_t* guardObject) { (void)guardObject; }
void __cxa_guard_abort_nid_postfix(std::uint64_t* guardObject) { (void)guardObject; }

void* __cxa_vec_new_nid_postfix(std::size_t n, std::size_t s, std::size_t p, void (*ctor)(void*), void (*dtor)(void*)) {
    (void)n; (void)s; (void)p; (void)ctor; (void)dtor;
    return nullptr;
}

void* __cxa_vec_new2_nid_postfix(std::size_t n, std::size_t s, std::size_t p, void (*ctor)(void*), void (*dtor)(void*), void* (*alloc)(std::size_t), void (*dealloc)(void*)) {
    (void)n; (void)s; (void)p; (void)ctor; (void)dtor; (void)alloc; (void)dealloc;
    return nullptr;
}

void* __cxa_vec_new3_nid_postfix(std::size_t n, std::size_t s, std::size_t p, void (*ctor)(void*), void (*dtor)(void*), void* (*alloc)(std::size_t), void (*dealloc)(void*, std::size_t)) {
    (void)n; (void)s; (void)p; (void)ctor; (void)dtor; (void)alloc; (void)dealloc;
    return nullptr;
}

void __cxa_vec_ctor_nid_postfix(void* arr, std::size_t n, std::size_t s, void (*ctor)(void*), void (*dtor)(void*)) {
    (void)arr; (void)n; (void)s; (void)ctor; (void)dtor;
}

void __cxa_vec_dtor_nid_postfix(void* arr, std::size_t n, std::size_t s, void (*dtor)(void*)) {
    (void)arr; (void)n; (void)s; (void)dtor;
}

void __cxa_vec_cleanup_nid_postfix(void* arr, std::size_t n, std::size_t s, void (*dtor)(void*)) {
    (void)arr; (void)n; (void)s; (void)dtor;
}

void __cxa_vec_delete_nid_postfix(void* arr, std::size_t s, std::size_t p, void (*dtor)(void*)) {
    (void)arr; (void)s; (void)p; (void)dtor;
}

void __cxa_vec_delete2_nid_postfix(void* arr, std::size_t s, std::size_t p, void (*dtor)(void*), void (*dealloc)(void*)) {
    (void)arr; (void)s; (void)p; (void)dtor; (void)dealloc;
}

void __cxa_vec_delete3_nid_postfix(void* arr, std::size_t s, std::size_t p, void (*dtor)(void*), void (*dealloc)(void*, std::size_t)) {
    (void)arr; (void)s; (void)p; (void)dtor; (void)dealloc;
}

void __cxa_vec_cctor_nid_postfix(void* dst, void* src, std::size_t n, std::size_t s, void (*cctor)(void*, void*), void (*dtor)(void*)) {
    (void)dst; (void)src; (void)n; (void)s; (void)cctor; (void)dtor;
}

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
