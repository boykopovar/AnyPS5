#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "bmalloc::FreeList check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

using Constructor = void (APS5_VABI *)(bmalloc::FreeList*);
using InitializeBump = void (APS5_VABI *)(bmalloc::FreeList*, char*, unsigned);
using InitializeList = void (APS5_VABI *)(bmalloc::FreeList*, bmalloc::FreeCell*, std::uintptr_t, unsigned);
using Clear = void (APS5_VABI *)(bmalloc::FreeList*);
static_assert(std::is_same_v<decltype(&_ZN7bmalloc8FreeListC1Ev), Constructor>);
static_assert(std::is_same_v<decltype(&_ZN7bmalloc8FreeList5clearEv), Clear>);
static_assert(std::is_same_v<decltype(&_ZN7bmalloc8FreeList14initializeListEPNS_8FreeCellEmj), InitializeList>);
static_assert(std::is_same_v<decltype(&_ZN7bmalloc8FreeList14initializeBumpEPcj), InitializeBump>);

void Poison(bmalloc::FreeList& list) {
    std::memset(&list, 0xAB, sizeof(bmalloc::FreeList));
}

void RequireZero(const bmalloc::FreeList& list) {
    Require(list.m_scrambledHead == 0);
    Require(list.m_secret == 0);
    Require(list.m_payloadEnd == nullptr);
    Require(list.m_remaining == 0);
    Require(list.m_originalSize == 0);
}

void TestConstructor() {
    bmalloc::FreeList list;
    Poison(list);
    _ZN7bmalloc8FreeListC1Ev(&list);
    RequireZero(list);
    std::printf("bmalloc::FreeList constructor: all fields zero\n");
}

void TestInitializeList() {
    bmalloc::FreeCell cell{};
    bmalloc::FreeList list;
    constexpr std::uintptr_t secret = 0xFEDCBA9876543210;
    const auto address = reinterpret_cast<std::uintptr_t>(&cell);

    Poison(list);
    _ZN7bmalloc8FreeList14initializeListEPNS_8FreeCellEmj(&list, &cell, secret, 4096);
    Require(list.m_scrambledHead == (address ^ secret));
    Require((list.m_scrambledHead ^ list.m_secret) == address);
    Require(list.m_secret == secret);
    Require(list.m_payloadEnd == nullptr);
    Require(list.m_remaining == 0);
    Require(list.m_originalSize == 4096);

    Poison(list);
    _ZN7bmalloc8FreeList14initializeListEPNS_8FreeCellEmj(&list, nullptr, secret, 8);
    Require(list.m_scrambledHead == secret);

    Poison(list);
    _ZN7bmalloc8FreeList14initializeListEPNS_8FreeCellEmj(&list, &cell, 0, 8);
    Require(list.m_scrambledHead == address);

    Poison(list);
    _ZN7bmalloc8FreeList14initializeListEPNS_8FreeCellEmj(&list, &cell, secret, 0xFFFFFFFFu);
    Require(list.m_originalSize == 0xFFFFFFFFu);

    std::printf("bmalloc::FreeList initializeList: scrambling and fields passed\n");
}

void TestInitializeBump() {
    bmalloc::FreeList list;
    char end = 0;

    Poison(list);
    _ZN7bmalloc8FreeList14initializeBumpEPcj(&list, &end, 100);
    Require(list.m_scrambledHead == 0);
    Require(list.m_secret == 0);
    Require(list.m_payloadEnd == &end);
    Require(list.m_remaining == 100);
    Require(list.m_originalSize == 100);

    Poison(list);
    _ZN7bmalloc8FreeList14initializeBumpEPcj(&list, nullptr, 0);
    RequireZero(list);

    std::printf("bmalloc::FreeList initializeBump: fields and empty bump passed\n");
}

void TestClear() {
    bmalloc::FreeCell cell{};
    bmalloc::FreeList list;
    char end = 0;

    Poison(list);
    _ZN7bmalloc8FreeList14initializeListEPNS_8FreeCellEmj(&list, &cell, 0xFEDCBA9876543210, 16);
    _ZN7bmalloc8FreeList5clearEv(&list);
    RequireZero(list);

    Poison(list);
    _ZN7bmalloc8FreeList14initializeBumpEPcj(&list, &end, 32);
    _ZN7bmalloc8FreeList5clearEv(&list);
    RequireZero(list);

    std::printf("bmalloc::FreeList clear: both initialized states reset\n");
}

void TestNullSelf() {
    RequireThrows([] { _ZN7bmalloc8FreeListC1Ev(nullptr); });
    RequireThrows([] { _ZN7bmalloc8FreeList5clearEv(nullptr); });
    RequireThrows([] { _ZN7bmalloc8FreeList14initializeListEPNS_8FreeCellEmj(nullptr, nullptr, 0, 0); });
    RequireThrows([] { _ZN7bmalloc8FreeList14initializeBumpEPcj(nullptr, nullptr, 0); });
    std::printf("bmalloc::FreeList: null self throws for all four exports\n");
}

}

int main() {
    TestConstructor();
    TestInitializeList();
    TestInitializeBump();
    TestClear();
    TestNullSelf();
}
