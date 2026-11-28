#include "prx/libSceNKWebKit/include/PropertyDescriptorExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using NKWebKit::JscAbi::PropertyDescriptor;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "PropertyDescriptor guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Call>
static bool Throws(Call call) {
    try {
        static_cast<void>(call());
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static unsigned char* MapPages() {
#ifdef _WIN32
    void* pages = VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(pages != nullptr);
#else
    void* pages = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(pages != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(pages);
}

static void Protect(unsigned char* page, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, readable ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
    Require(mprotect(page, 4096, readable ? PROT_READ : PROT_NONE) == 0);
#endif
}

static void UnmapPages(unsigned char* pages) {
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 8192) == 0);
#endif
}

template <typename Descriptor>
static bool AllFail(Descriptor* descriptor) {
    return Throws([&] { return _ZNK3JSC18PropertyDescriptor8writableEv(descriptor); }) &&
        Throws([&] { return _ZNK3JSC18PropertyDescriptor10enumerableEv(descriptor); }) &&
        Throws([&] { return _ZNK3JSC18PropertyDescriptor12configurableEv(descriptor); }) &&
        Throws([&] { return _ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(descriptor); }) &&
        Throws([&] { return _ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(descriptor); }) &&
        Throws([&] { _ZN3JSC18PropertyDescriptor11setWritableEb(descriptor, true); return 0; }) &&
        Throws([&] { _ZN3JSC18PropertyDescriptor13setEnumerableEb(descriptor, true); return 0; }) &&
        Throws([&] { _ZN3JSC18PropertyDescriptor15setConfigurableEb(descriptor, true); return 0; }) &&
        Throws([&] { _ZN3JSC18PropertyDescriptor12setUndefinedEv(descriptor); return 0; }) &&
        Throws([&] { _ZN3JSC18PropertyDescriptor13setDescriptorENS_7JSValueEj(descriptor, 0xfffe000000001234ull, 0); return 0; }) &&
        Throws([&] { _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(descriptor, 1); return 0; }) &&
        Throws([&] { _ZN3JSC18PropertyDescriptor9setSetterENS_7JSValueE(descriptor, 1); return 0; });
}

int main() {
    Require(AllFail(reinterpret_cast<PropertyDescriptor*>(std::uintptr_t{0x10})));

    unsigned char* pages = MapPages();
    Protect(pages + 4096, false);
    Require(AllFail(reinterpret_cast<PropertyDescriptor*>(pages + 4096 - 16)));

    auto* descriptor = reinterpret_cast<PropertyDescriptor*>(pages);
    *descriptor = {0x1234, 0, 0x9abc, 0x2, 0x8};
    const PropertyDescriptor before = *descriptor;
    Protect(pages, true);
    Require(!_ZNK3JSC18PropertyDescriptor8writableEv(descriptor) && _ZNK3JSC18PropertyDescriptor10enumerableEv(descriptor));
    Require(_ZNK3JSC18PropertyDescriptor12configurableEv(descriptor) && _ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(descriptor));
    Require(_ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(descriptor));
    Require(Throws([&] { _ZN3JSC18PropertyDescriptor11setWritableEb(descriptor, true); return 0; }));
    Require(Throws([&] { _ZN3JSC18PropertyDescriptor12setUndefinedEv(descriptor); return 0; }));
    Require(Throws([&] { _ZN3JSC18PropertyDescriptor13setDescriptorENS_7JSValueEj(descriptor, 0xfffe000000001234ull, 0); return 0; }));
    Require(Throws([&] { _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(descriptor, 7); return 0; }));
    Require(std::memcmp(descriptor, &before, sizeof(before)) == 0);

    unsigned char* writablePages = MapPages();
    auto* writable = reinterpret_cast<PropertyDescriptor*>(writablePages + 4096 - sizeof(PropertyDescriptor));
    *writable = {0, 0, 0, 0x2, 0};
    _ZN3JSC18PropertyDescriptor11setWritableEb(writable, true);
    Require(writable->attributes == 0 && writable->seenAttributes == 0x1);
    _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(writable, 7);
    Require(writable->getter == 7 && (writable->attributes & 0x10) && _ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(writable));

    UnmapPages(writablePages);
    UnmapPages(pages);
    return 0;
}
