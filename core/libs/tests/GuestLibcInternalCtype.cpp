#include "prx/libSceLibcInternal/include/Ctype.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <new>
#include <vector>
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifndef _WIN32
std::size_t hostAllocationLimit = SIZE_MAX;
void* operator new(std::size_t bytes) {
    if (bytes > hostAllocationLimit) {
        std::fprintf(stderr, "host allocation of %zu bytes exceeds the test cap\n", bytes);
        std::abort();
    }
    if (void* pointer = std::malloc(bytes ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    if (bytes > hostAllocationLimit) {
        std::fprintf(stderr, "host allocation of %zu bytes exceeds the test cap\n", bytes);
        std::abort();
    }
    return std::malloc(bytes ? bytes : 1);
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
#endif

namespace {

template<typename TCall>
void Capped(TCall call) {
#ifndef _WIN32
    hostAllocationLimit = 1 << 20;
#endif
    call();
#ifndef _WIN32
    hostAllocationLimit = SIZE_MAX;
#endif
}

#ifndef _WIN32
class GuardedPage {
public:
    GuardedPage() {
        const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        mapping = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapping == MAP_FAILED || mprotect(static_cast<unsigned char*>(mapping) + page, page, PROT_NONE) != 0) std::abort();
        end = static_cast<unsigned char*>(mapping) + page;
        bytes = page * 2;
    }
    ~GuardedPage() { munmap(mapping, bytes); }
    template<typename T>
    T* Tail(std::size_t count) const { return reinterpret_cast<T*>(end) - count; }

private:
    void* mapping = nullptr;
    unsigned char* end = nullptr;
    std::size_t bytes = 0;
};
#endif

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal ctype: %s\n", message);
    std::abort();
}

void TestNarrowCase() {
    GuestLocale::CtypeFacet facet{};
    facet.lower = _Getptolower_nid_postfix();
    facet.upper = _Getptoupper_nid_postfix();
    Require(_ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(&facet, 'A') == 'a', "tolower A");
    Require(_ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(&facet, 'z') == 'z', "tolower z");
    Require(_ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(&facet, '[') == '[', "tolower [");
    Require(_ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(&facet, 'q') == 'Q', "toupper q");
    Require(_ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(&facet, '7') == '7', "toupper 7");
    Require(_ZNKSt5ctypeIcE10do_toupperEc_nid_postfix(&facet, static_cast<char>(0xe4)) == static_cast<char>(0xe4), "toupper high byte");
    Require(_ZNKSt5ctypeIcE10do_tolowerEc_nid_postfix(&facet, static_cast<char>(0xc4)) == static_cast<char>(0xc4), "tolower high byte");

    char text[] = "Mixed Case 42!";
    const char* end = text + std::strlen(text);
    Require(_ZNKSt5ctypeIcE10do_tolowerEPcPKc_nid_postfix(&facet, text, end) == end, "tolower range end");
    Require(std::strcmp(text, "mixed case 42!") == 0, "tolower range");
    Require(_ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(&facet, text, end) == end, "toupper range end");
    Require(std::strcmp(text, "MIXED CASE 42!") == 0, "toupper range");
    Require(_ZNKSt5ctypeIcE10do_toupperEPcPKc_nid_postfix(&facet, text, text) == text, "empty range");
}

void TestWideCase() {
    Require(_ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(nullptr, u'G') == u'g', "wide tolower G");
    Require(_ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(nullptr, u'-') == u'-', "wide tolower -");
    Require(_ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(nullptr, 0x4e2d) == 0x4e2d, "wide tolower cjk");
    Require(_ZNKSt5ctypeIwE10do_tolowerEw_nid_postfix(nullptr, 0xffff) == 0xffff, "wide tolower weof");

    std::array<std::uint16_t, 5> text {u'A', u'B', u'3', 0x4e2d, u'z'};
    const std::uint16_t* end = text.data() + text.size();
    Require(_ZNKSt5ctypeIwE10do_tolowerEPwPKw_nid_postfix(nullptr, text.data(), end) == end, "wide tolower range end");
    Require(text == std::array<std::uint16_t, 5> {u'a', u'b', u'3', 0x4e2d, u'z'}, "wide tolower range");
}

void TestWidenNarrow() {
    Require(_ZNKSt5ctypeIwE8do_widenEc_nid_postfix(nullptr, 'x') == u'x', "widen x");
    Require(_ZNKSt5ctypeIwE8do_widenEc_nid_postfix(nullptr, '\0') == 0, "widen nul");
    Require(_ZNKSt5ctypeIwE8do_widenEc_nid_postfix(nullptr, static_cast<char>(0x80)) == 0xffff, "widen invalid byte");
    Require(_ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(nullptr, u'x', '?') == 'x', "narrow x");
    Require(_ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(nullptr, 0x00e9, '?') == '?', "narrow e acute");
    Require(_ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(nullptr, 0x4e2d, '*') == '*', "narrow cjk");

    const char bytes[] = {'a', 'Z', '\0', static_cast<char>(0x80), '7'};
    std::array<std::uint16_t, 5> wide {};
    Require(_ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(nullptr, bytes, bytes + 5, wide.data()) == bytes + 5, "widen range end");
    Require(wide == std::array<std::uint16_t, 5> {u'a', u'Z', 0, 0xffff, u'7'}, "widen range");

    const std::array<std::uint16_t, 4> source {u'o', 0x00e9, u'k', 0x4e2d};
    std::array<char, 4> narrow {};
    Require(_ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, source.data(), source.data() + source.size(), '#', narrow.data()) == source.data() + source.size(), "narrow range end");
    Require(narrow == std::array<char, 4> {'o', '#', 'k', '#'}, "narrow range");
    Require(_ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, source.data(), source.data(), '#', narrow.data()) == source.data(), "narrow empty range");
}

template<typename TCall>
void RequireRejected(TCall call, const char* message) {
    *__error_nid_postfix() = 71;
    try {
        call();
    } catch (const std::invalid_argument&) {
        Require(*__error_nid_postfix() == 71, message);
        return;
    }
    Require(false, message);
}

void TestGuestPointers() {
    const auto* unmappedBytes = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    const auto* unmappedWide = reinterpret_cast<const std::uint16_t*>(std::uintptr_t{0x10});
    const char bytes[] = {'a', 'b', 'c', 'd'};
    const std::array<std::uint16_t, 4> source {u'w', u'x', u'y', u'z'};
    std::array<std::uint16_t, 4> wide {1, 2, 3, 4};
    std::array<char, 4> narrow {'1', '2', '3', '4'};
    RequireRejected([&] { _ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(nullptr, unmappedBytes, unmappedBytes + 4, wide.data()); }, "widen rejects an unreadable range");
    RequireRejected([&] { _ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(nullptr, bytes, bytes + 4, const_cast<std::uint16_t*>(unmappedWide)); }, "widen rejects an unwritable destination");
    RequireRejected([&] { _ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(nullptr, bytes + 4, bytes, wide.data()); }, "widen rejects a reversed range");
    Require(wide == std::array<std::uint16_t, 4> {1, 2, 3, 4}, "rejected widen calls write nothing");
    RequireRejected([&] { _ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, unmappedWide, unmappedWide + 4, '?', narrow.data()); }, "narrow rejects an unreadable range");
    RequireRejected([&] { _ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, source.data(), source.data() + 4, '?', const_cast<char*>(unmappedBytes)); }, "narrow rejects an unwritable destination");
    RequireRejected([&] { _ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, source.data() + 4, source.data(), '?', narrow.data()); }, "narrow rejects a reversed range");
    Require(narrow == std::array<char, 4> {'1', '2', '3', '4'}, "rejected narrow calls write nothing");
    Require(_ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(nullptr, unmappedBytes, unmappedBytes, nullptr) == unmappedBytes, "an empty widen range reads and writes nothing");
    Require(_ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, unmappedWide, unmappedWide, '?', nullptr) == unmappedWide, "an empty narrow range reads and writes nothing");
}

void TestChunkedRanges() {
    constexpr std::size_t huge = std::size_t{1} << 40;
    const auto* unmappedBytes = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    const auto* unmappedWide = reinterpret_cast<const std::uint16_t*>(std::uintptr_t{0x10});
    std::array<std::uint16_t, 4> wide {1, 2, 3, 4};
    std::array<char, 4> narrow {'1', '2', '3', '4'};
    Capped([&] {
        RequireRejected([&] { _ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(nullptr, unmappedBytes, unmappedBytes + huge, wide.data()); }, "widen rejects an oversized unmapped range");
        RequireRejected([&] { _ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, unmappedWide, unmappedWide + huge, '?', narrow.data()); }, "narrow rejects an oversized unmapped range");
#ifndef _WIN32
        GuardedPage page;
        auto* bytes = page.Tail<char>(4);
        auto* source = page.Tail<std::uint16_t>(4);
        RequireRejected([&] { _ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(nullptr, bytes, bytes + huge, wide.data()); }, "widen rejects an oversized range before a guard page");
        RequireRejected([&] { _ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, source, source + huge, '?', narrow.data()); }, "narrow rejects an oversized range before a guard page");
#endif
    });
    Require(wide == std::array<std::uint16_t, 4> {1, 2, 3, 4} && narrow == std::array<char, 4> {'1', '2', '3', '4'}, "oversized ranges write nothing");
    std::vector<char> text(0x2345);
    for (std::size_t index = 0; index < text.size(); ++index) text[index] = static_cast<char>(index * 37 + 11);
    std::vector<std::uint16_t> widened(text.size() + 1, 0xa55a);
    Capped([&] { Require(_ZNKSt5ctypeIwE8do_widenEPKcS2_Pw_nid_postfix(nullptr, text.data(), text.data() + text.size(), widened.data()) == text.data() + text.size(), "chunked widen returns the end"); });
    for (std::size_t index = 0; index < text.size(); ++index) Require(widened[index] == _ZNKSt5ctypeIwE8do_widenEc_nid_postfix(nullptr, text[index]), "chunked widen matches the scalar widen");
    Require(widened.back() == 0xa55a, "chunked widen writes one unit per character");
    for (std::size_t index = 0; index < text.size(); ++index) widened[index] = static_cast<std::uint16_t>(index * 41 + 3);
    std::vector<char> narrowed(text.size() + 1, 'Z');
    Capped([&] { Require(_ZNKSt5ctypeIwE9do_narrowEPKwS2_cPc_nid_postfix(nullptr, widened.data(), widened.data() + text.size(), '?', narrowed.data()) == widened.data() + text.size(), "chunked narrow returns the end"); });
    for (std::size_t index = 0; index < text.size(); ++index) Require(narrowed[index] == _ZNKSt5ctypeIwE9do_narrowEwc_nid_postfix(nullptr, widened[index], '?'), "chunked narrow matches the scalar narrow");
    Require(narrowed.back() == 'Z', "chunked narrow writes one byte per character");
}

}

int main() {
    TestNarrowCase();
    TestWideCase();
    TestWidenNarrow();
    TestGuestPointers();
    TestChunkedRanges();
    return 0;
}
