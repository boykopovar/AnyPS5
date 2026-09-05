#pragma once
#include "Unwind.hpp"
#include <atomic>
#include <typeinfo>
#include <cxxabi.h>

namespace LibcException {
struct Header {
    std::type_info* type {};
    void (*destructor)(void*) {};
    void (*unexpected)() {};
    void (*terminate)() {};
    Header* next {};
    int handlers {};
    int selector {};
    const unsigned char* action {};
    const unsigned char* lsda {};
    std::uintptr_t landing {};
    void* adjusted {};
    _Unwind_Exception unwind {};
};
struct alignas(16) Allocation {
    std::atomic<std::size_t> references {1};
    Header header {};
};
struct Globals { Header* caught {}; unsigned uncaught {}; };
inline thread_local Globals globals;
inline constexpr std::uint64_t PrimaryClass = 0x414e5950432b2b00;
inline constexpr std::uint64_t DependentClass = PrimaryClass | 1;

inline Header* FromUnwind(_Unwind_Exception* e) {
    return reinterpret_cast<Header*>(reinterpret_cast<unsigned char*>(e) - offsetof(Header, unwind));
}
inline Header* FromObject(void* p) { return static_cast<Header*>(p) - 1; }
inline Allocation* AllocationOf(Header* header) {
    return reinterpret_cast<Allocation*>(reinterpret_cast<unsigned char*>(header) - offsetof(Allocation, header));
}
inline bool Native(std::uint64_t value) { return (value & ~std::uint64_t(1)) == PrimaryClass; }
inline Header* Primary(Header* header) {
    return header->unwind.exception_class == DependentClass ? FromObject(reinterpret_cast<void*>(header->type)) : header;
}
const char* Kind(const std::type_info*);
bool Match(const std::type_info* caught, const std::type_info* thrown, void*& object);
void Release(void*);
[[noreturn]] void Terminate();
}

extern "C" {
void* __cxa_allocate_exception_nid_postfix(std::size_t);
void __cxa_free_exception_nid_postfix(void*);
[[noreturn]] void __cxa_throw_nid_postfix(void*, std::type_info*, void (*)(void*));
[[noreturn]] void __cxa_rethrow_nid_postfix();
void* __cxa_begin_catch_nid_postfix(void*);
void __cxa_end_catch_nid_postfix();
}
