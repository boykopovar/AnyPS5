#include "Runtime.hpp"
#include <limits>

extern "C" {
void __cxa_vec_cleanup_nid_postfix(void* array, std::size_t count, std::size_t size, void (*destroy)(void*)) {
    if (!destroy) return;
    try {
        while (count) destroy(static_cast<unsigned char*>(array) + --count * size);
    } catch (...) { LibcException::Terminate(); }
}
void __cxa_vec_ctor_nid_postfix(void* array, std::size_t count, std::size_t size, void (*construct)(void*), void (*destroy)(void*)) {
    if (!construct) return;
    std::size_t complete = 0;
    try {
        for (; complete < count; ++complete) construct(static_cast<unsigned char*>(array) + complete * size);
    } catch (...) {
        __cxa_vec_cleanup_nid_postfix(array, complete, size, destroy);
        throw;
    }
}
void __cxa_vec_cctor_nid_postfix(void* destination, void* source, std::size_t count, std::size_t size, void (*copy)(void*, void*), void (*destroy)(void*)) {
    if (!copy) return;
    std::size_t complete = 0;
    try {
        for (; complete < count; ++complete)
            copy(static_cast<unsigned char*>(destination) + complete * size, static_cast<unsigned char*>(source) + complete * size);
    } catch (...) {
        __cxa_vec_cleanup_nid_postfix(destination, complete, size, destroy);
        throw;
    }
}
void __cxa_vec_dtor_nid_postfix(void* array, std::size_t count, std::size_t size, void (*destroy)(void*)) {
    if (!destroy) return;
    bool unwinding = LibcException::globals.uncaught != 0;
    try {
        while (count) destroy(static_cast<unsigned char*>(array) + --count * size);
    } catch (...) {
        if (unwinding) LibcException::Terminate();
        __cxa_vec_cleanup_nid_postfix(array, count, size, destroy);
        throw;
    }
}
}

namespace LibcException {
std::size_t ArraySize(std::size_t count, std::size_t size, std::size_t padding) {
    if ((padding && padding < sizeof(std::size_t)) || (size && count > (std::numeric_limits<std::size_t>::max() - padding) / size))
        __cxa_throw_bad_array_new_length_nid_postfix();
    return count * size + padding;
}
void* ArrayAllocate(std::size_t size) {
    void* result = std::malloc(size ? size : 1);
    if (!result) _ZSt11_Xbad_allocv_nid_postfix();
    return result;
}
template<class Deallocate>
void* NewArray(std::size_t count, std::size_t size, std::size_t padding, void (*construct)(void*), void (*destroy)(void*), void* (*allocate)(std::size_t), Deallocate deallocate) {
    std::size_t total = ArraySize(count, size, padding);
    auto* storage = static_cast<unsigned char*>(allocate(total));
    if (!storage) return nullptr;
    auto* array = storage + padding;
    if (padding) std::memcpy(array - sizeof(count), &count, sizeof(count));
    try { __cxa_vec_ctor_nid_postfix(array, count, size, construct, destroy); }
    catch (...) { deallocate(storage, total); throw; }
    return array;
}
template<class Deallocate>
void DeleteArray(void* array, std::size_t size, std::size_t padding, void (*destroy)(void*), Deallocate deallocate) {
    if (!array) return;
    auto* storage = static_cast<unsigned char*>(array) - padding;
    std::size_t count = 0;
    if (padding) {
        if (padding < sizeof(count)) Terminate();
        std::memcpy(&count, static_cast<unsigned char*>(array) - sizeof(count), sizeof(count));
    }
    std::size_t total = ArraySize(count, size, padding);
    try { if (padding) __cxa_vec_dtor_nid_postfix(array, count, size, destroy); }
    catch (...) { deallocate(storage, total); throw; }
    deallocate(storage, total);
}
}

extern "C" {
void* __cxa_vec_new_nid_postfix(std::size_t n, std::size_t s, std::size_t p, void (*ctor)(void*), void (*dtor)(void*)) {
    return LibcException::NewArray(n, s, p, ctor, dtor, LibcException::ArrayAllocate, [](void* ptr, std::size_t) { std::free(ptr); });
}
void* __cxa_vec_new2_nid_postfix(std::size_t n, std::size_t s, std::size_t p, void (*ctor)(void*), void (*dtor)(void*), void* (*alloc)(std::size_t), void (*dealloc)(void*)) {
    return LibcException::NewArray(n, s, p, ctor, dtor, alloc, [dealloc](void* ptr, std::size_t) { dealloc(ptr); });
}
void* __cxa_vec_new3_nid_postfix(std::size_t n, std::size_t s, std::size_t p, void (*ctor)(void*), void (*dtor)(void*), void* (*alloc)(std::size_t), void (*dealloc)(void*, std::size_t)) {
    return LibcException::NewArray(n, s, p, ctor, dtor, alloc, dealloc);
}
void __cxa_vec_delete_nid_postfix(void* array, std::size_t s, std::size_t p, void (*dtor)(void*)) {
    LibcException::DeleteArray(array, s, p, dtor, [](void* ptr, std::size_t) { std::free(ptr); });
}
void __cxa_vec_delete2_nid_postfix(void* array, std::size_t s, std::size_t p, void (*dtor)(void*), void (*dealloc)(void*)) {
    LibcException::DeleteArray(array, s, p, dtor, [dealloc](void* ptr, std::size_t) { dealloc(ptr); });
}
void __cxa_vec_delete3_nid_postfix(void* array, std::size_t s, std::size_t p, void (*dtor)(void*), void (*dealloc)(void*, std::size_t)) {
    LibcException::DeleteArray(array, s, p, dtor, dealloc);
}
}
