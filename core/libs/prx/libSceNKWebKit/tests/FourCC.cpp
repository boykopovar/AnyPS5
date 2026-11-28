#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include <array>
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

using NKWebKit::WebCoreAbi::OptionalFourCC;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Require(bool value) {
    if (!value) {
        std::fprintf(stderr, "FourCC result or hidden-return ABI mismatch\n");
        std::abort();
    }
}

static void Check(const String& string, bool engaged, std::uint32_t value) {
    struct {
        std::uint32_t before = 0x12345678;
        OptionalFourCC result;
        std::uint32_t after = 0x87654321;
    } storage;
    std::memset(&storage.result, 0xa5, sizeof(storage.result));
    auto* returned = _ZN7WebCore6FourCC10fromStringERKN3WTF6StringE(&storage.result, &string);
    Require(returned == &storage.result);
    Require(storage.before == 0x12345678 && storage.after == 0x87654321);
    const auto* bytes = reinterpret_cast<const unsigned char*>(&storage.result);
    Require(bytes[0] == static_cast<unsigned char>(engaged));
    if (engaged) {
        std::uint32_t actual;
        std::memcpy(&actual, bytes + 4, sizeof(actual));
        Require(actual == value);
    }
}

static void Test(const std::u16string& text, bool engaged, std::uint32_t value) {
    for (bool wide : {false, true}) {
        std::string narrow;
        for (auto ch : text) narrow.push_back(static_cast<char>(ch));
        StringImpl impl{4, static_cast<unsigned>(text.size()), wide ? static_cast<const void*>(text.data()) : narrow.data(), wide ? 0u : 4u};
        String string(&impl);
        Check(string, engaged, value);
        string.impl = nullptr;
    }
}

static bool Rejected(const String* string) {
    OptionalFourCC result{};
    errno = 71;
    bool rejected = false;
    try {
        _ZN7WebCore6FourCC10fromStringERKN3WTF6StringE(&result, string);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    return rejected && errno == 71;
}

static unsigned char* MapGuardedPage() {
#ifdef _WIN32
    auto* pages = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(pages + 4096, 4096, PAGE_NOACCESS, &previous) != 0);
#else
    void* mapped = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* pages = static_cast<unsigned char*>(mapped);
    Require(mprotect(pages + 4096, 4096, PROT_NONE) == 0);
#endif
    return pages;
}

static void UnmapGuardedPage(unsigned char* pages) {
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 8192) == 0);
#endif
}

static void TestGuestPointers() {
    Require(Rejected(reinterpret_cast<const String*>(std::uintptr_t{0x10})));
    {
        String unmapped(reinterpret_cast<StringImpl*>(std::uintptr_t{0x10}));
        Require(Rejected(&unmapped));
        unmapped.impl = nullptr;
    }
    StringImpl impl{4, 4, reinterpret_cast<const void*>(std::uintptr_t{0x10}), 4};
    String string(&impl);
    Require(Rejected(&string));
    impl.hashAndFlags = 0;
    Require(Rejected(&string));

    unsigned char* pages = MapGuardedPage();
    std::memcpy(pages + 4096 - 4, "mp4a", 4);
    impl.data = pages + 4096 - 4;
    impl.hashAndFlags = 4;
    Check(string, true, 0x6d703461);
    impl.hashAndFlags = 0;
    Require(Rejected(&string));
    const char16_t wide[] = u"mp4a";
    std::memcpy(pages + 4096 - 8, wide, 8);
    impl.data = pages + 4096 - 8;
    Check(string, true, 0x6d703461);

    auto requireNotImplemented = [](auto&& fn) {
        errno = 73;
        const auto before = NotImplementedCalls_nid_no_patch();
        bool threw = false;
        try {
            fn();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw && errno == 73 && NotImplementedCalls_nid_no_patch() == before + 1);
    };

    requireNotImplemented([&] {
        _ZN7WebCore6FourCC10fromStringERKN3WTF6StringE(reinterpret_cast<OptionalFourCC*>(pages + 4096), &string);
    });
    unsigned char* const splitResult = pages + 4096 - 4;
    std::memset(splitResult, 0x5A, 4);
    requireNotImplemented([&] {
        _ZN7WebCore6FourCC10fromStringERKN3WTF6StringE(reinterpret_cast<OptionalFourCC*>(splitResult), &string);
    });
    for (std::size_t i = 0; i < 4; ++i) Require(splitResult[i] == 0x5A);

#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(pages, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(pages, 4096, PROT_READ) == 0);
#endif
    requireNotImplemented([&] {
        _ZN7WebCore6FourCC10fromStringERKN3WTF6StringE(reinterpret_cast<OptionalFourCC*>(pages), &string);
    });

    UnmapGuardedPage(pages);
    string.impl = nullptr;
    std::puts("FourCC::fromString: unreadable strings throw and keep errno, reads stay within the four characters");
}

int main() {
    TestGuestPointers();
    Test(u"avc1", true, 0x61766331);
    Test(u"", false, 0);
    Test(u"abc", false, 0);
    Test(u"abcde", false, 0);
    Test(std::u16string{0, 0x7f, 1, 0x1f}, true, 0x007f3f3f);
    Test(std::u16string{0x80, 0xff, 0x20, 0x7e}, true, 0x3f3f207e);
    for (char16_t ch = 0; ch < 256; ++ch) {
        const std::uint32_t byte = ch && (ch < 32 || ch > 127) ? '?' : ch;
        Test(std::u16string{u'a', ch, u'c', u'1'}, true, 0x61006331 | (byte << 16));
    }
    const char16_t unicode[] = {0x100, 0x20ac, 0xd800, 0xdc00};
    StringImpl impl{4, 4, unicode, 0};
    String wide(&impl);
    Check(wide, true, 0x3f3f3f3f);
    wide.impl = nullptr;
    String nullString;
    Check(nullString, false, 0);
    return 0;
}
