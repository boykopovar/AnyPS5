#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF C string guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool Rejected(Function function) {
    errno = 71;
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    return rejected && errno == 71;
}

static unsigned char* MapGuardedPages() {
#ifdef _WIN32
    auto* pages = static_cast<unsigned char*>(VirtualAlloc(nullptr, 3 * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(pages + 2 * 4096, 4096, PAGE_NOACCESS, &previous) != 0);
#else
    void* mapped = mmap(nullptr, 3 * 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* pages = static_cast<unsigned char*>(mapped);
    Require(mprotect(pages + 2 * 4096, 4096, PROT_NONE) == 0);
#endif
    return pages;
}

static void UnmapGuardedPages(unsigned char* pages) {
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 3 * 4096) == 0);
#endif
}

static bool Is8Bit(const String& value, const std::string& expected) {
    const StringImpl* impl = value.impl;
    return impl && (impl->hashAndFlags & 4) && impl->length == expected.size()
        && std::memcmp(impl->data, expected.data(), expected.size()) == 0;
}

static bool Is16Bit(const String& value, const std::u16string& expected) {
    const StringImpl* impl = value.impl;
    return impl && !(impl->hashAndFlags & 4) && impl->length == expected.size()
        && std::memcmp(impl->data, expected.data(), expected.size() * sizeof(char16_t)) == 0;
}

static bool Holds(const CString& value, const std::string& expected) {
    return value.buffer && value.buffer->length == expected.size()
        && std::memcmp(value.buffer + 1, expected.c_str(), expected.size() + 1) == 0;
}

int main() {
    unsigned char* pages = MapGuardedPages();
    char* spanning = reinterpret_cast<char*>(pages + 4096 - 3);
    std::memcpy(spanning, "h\xc3\xa9llo", 7);
    char* unterminated = reinterpret_cast<char*>(pages + 2 * 4096 - 3);
    std::memcpy(unterminated, "abc", 3);
    const auto* unmapped = reinterpret_cast<const char*>(std::uintptr_t{0x10});

    for (auto construct : {&_ZN3WTF6StringC1EPKc, &_ZN3WTF6StringC2EPKc}) {
        String value;
        construct(&value, spanning);
        Require(Is8Bit(value, "h\xc3\xa9llo"));
        String rejected;
        Require(Rejected([&] { construct(&rejected, unterminated); }));
        Require(Rejected([&] { construct(&rejected, unmapped); }));
        Require(!rejected.impl);
        String null(reinterpret_cast<StringImpl*>(std::uintptr_t{1}));
        construct(&null, nullptr);
        Require(!null.impl);
    }

    {
        String value = _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(spanning));
        Require(Is16Bit(value, u"héllo"));
        Require(Rejected([&] { _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(unterminated)); }));
        Require(Rejected([&] { _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(unmapped)); }));
        Require(!_ZN3WTF6String8fromUTF8EPKh(nullptr).impl);

        String latin;
        _ZN3WTF6StringC1EPKc(&latin, spanning);
        Require(_ZN3WTF5equalEPKNS_10StringImplEPKh(latin.impl, reinterpret_cast<const unsigned char*>(spanning)));
        Require(!_ZN3WTF5equalEPKNS_10StringImplEPKh(latin.impl, reinterpret_cast<const unsigned char*>("hello")));
        Require(Rejected([&] { _ZN3WTF5equalEPKNS_10StringImplEPKh(latin.impl, reinterpret_cast<const unsigned char*>(unterminated)); }));
        Require(Rejected([&] { _ZN3WTF5equalEPKNS_10StringImplEPKh(latin.impl, reinterpret_cast<const unsigned char*>(unmapped)); }));
        Require(_ZN3WTF5equalEPKNS_10StringImplEPKh(nullptr, nullptr));
    }

    for (auto construct : {&_ZN3WTF7CStringC1EPKc, &_ZN3WTF7CStringC2EPKc}) {
        CString value;
        construct(&value, spanning);
        Require(Holds(value, "h\xc3\xa9llo"));
        Require(_ZN3WTFeqERKNS_7CStringEPKc(value, spanning));
        Require(!_ZN3WTFeqERKNS_7CStringEPKc(value, "hello"));
        CString empty;
        construct(&empty, "");
        Require(Holds(empty, ""));
        CString rejected;
        Require(Rejected([&] { construct(&rejected, unterminated); }));
        Require(Rejected([&] { construct(&rejected, unmapped); }));
        Require(!rejected.buffer);
        CString null;
        construct(&null, nullptr);
        Require(!null.buffer && _ZN3WTFeqERKNS_7CStringEPKc(null, nullptr));
    }

    UnmapGuardedPages(pages);
    return 0;
}
