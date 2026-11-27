#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceJsc/include/Api.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "OpaqueJSString check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static String Latin1(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static String UTF16(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

static void Expect(const GuestJSString* string, std::u16string_view expected) {
    Require(string);
    Require(!string->nullString);
    Require(string->references.load() == 1);
    Require(JSStringGetLength_nid_postfix(string) == expected.size());
    const auto* characters = JSStringGetCharactersPtr_nid_postfix(string);
    for (std::size_t i = 0; i < expected.size(); ++i) Require(characters[i] == expected[i]);
}

struct GuardedPages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 3, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 4, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 3, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 4) == 0);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* WritableBeforeGuard() const { return base + pageSize * 2; }
    unsigned char* Guard() const { return base + pageSize * 3; }
};

template <typename Action>
static bool ThrowsNotImplemented(Action action, const char* symbol) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(symbol) + " not implemented";
    }
    return false;
}

static void TestGuestPointers() {
    constexpr const char* kCopySymbol = "_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE";
    constexpr const char* kMoveSymbol = "_ZN14OpaqueJSString9tryCreateEON3WTF6StringE";
    GuardedPages pages;

    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(reinterpret_cast<const String*>(0x10))); }, kCopySymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(reinterpret_cast<const String*>(pages.Guard()))); }, kCopySymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(reinterpret_cast<const String*>(pages.Guard() - 4))); }, kCopySymbol));

    Require(!_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(reinterpret_cast<const String*>(pages.ReadOnly())).ptr);

    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(0x10))); }, kMoveSymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(pages.ReadOnly()))); }, kMoveSymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(pages.Guard()))); }, kMoveSymbol));

    unsigned char* splitMove = pages.ReadOnly() - 4;
    std::memset(splitMove, 0x5A, 4);
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(splitMove))); }, kMoveSymbol));
    for (std::size_t i = 0; i < 4; ++i) Require(splitMove[i] == 0x5A);

    alignas(String) unsigned char badStringRaw[sizeof(String)]{};
    auto* badImpl = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(pages.Guard());
    std::memcpy(badStringRaw, &badImpl, sizeof(badImpl));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(reinterpret_cast<const String*>(badStringRaw))); }, kCopySymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(badStringRaw))); }, kMoveSymbol));

    NKWebKit::WtfAbi::StringImpl badDataImpl{2, 8, pages.Guard() - 4, 4};
    auto* badDataPtr = &badDataImpl;
    std::memcpy(badStringRaw, &badDataPtr, sizeof(badDataPtr));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(reinterpret_cast<const String*>(badStringRaw))); }, kCopySymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(badStringRaw))); }, kMoveSymbol));
    NKWebKit::WtfAbi::StringImpl* stillThere = nullptr;
    std::memcpy(&stillThere, badStringRaw, sizeof(stillThere));
    Require(stillThere == badDataPtr && badDataImpl.refCount == 2);

    auto* readOnlyImpl = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(pages.ReadOnly());
    std::memcpy(badStringRaw, &readOnlyImpl, sizeof(readOnlyImpl));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(badStringRaw))); }, kMoveSymbol));

    unsigned char* splitImpl = pages.Guard() - 8;
    std::memset(splitImpl, 0, 8);
    auto* splitImplPtr = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(splitImpl);
    std::memcpy(badStringRaw, &splitImplPtr, sizeof(splitImplPtr));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(reinterpret_cast<const String*>(badStringRaw))); }, kCopySymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(badStringRaw))); }, kMoveSymbol));

    unsigned char* truncatedSub = pages.Guard() - 24;
    NKWebKit::WtfAbi::StringImpl subHeader{2, 0, nullptr, 6};
    std::memcpy(truncatedSub, &subHeader, sizeof(subHeader));
    auto* subPtr = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(truncatedSub);
    std::memcpy(badStringRaw, &subPtr, sizeof(subPtr));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(badStringRaw))); }, kMoveSymbol));

    NKWebKit::WtfAbi::StringImpl terminatingFlagsImpl{2, 0, nullptr, 16};
    auto* terminatingFlagsPtr = &terminatingFlagsImpl;
    std::memcpy(badStringRaw, &terminatingFlagsPtr, sizeof(terminatingFlagsPtr));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(badStringRaw))); }, kMoveSymbol));
    std::memcpy(&stillThere, badStringRaw, sizeof(stillThere));
    Require(stillThere == terminatingFlagsPtr && terminatingFlagsImpl.refCount == 2);

    NKWebKit::WtfAbi::StringImpl ownedImpl{2, 0, nullptr, 3};
    auto* ownedPtr = &ownedImpl;
    std::memcpy(badStringRaw, &ownedPtr, sizeof(ownedPtr));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(badStringRaw))); }, kMoveSymbol));
    std::memcpy(&stillThere, badStringRaw, sizeof(stillThere));
    Require(stillThere == ownedPtr && ownedImpl.refCount == 2);

    alignas(NKWebKit::WtfAbi::StringImpl) unsigned char cyclicSubRaw[24 + sizeof(NKWebKit::WtfAbi::StringImpl*)]{};
    NKWebKit::WtfAbi::StringImpl cyclicHeader{2, 0, nullptr, 6};
    std::memcpy(cyclicSubRaw, &cyclicHeader, sizeof(cyclicHeader));
    auto* cyclicPtr = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(cyclicSubRaw);
    std::memcpy(cyclicSubRaw + 24, &cyclicPtr, sizeof(cyclicPtr));
    std::memcpy(badStringRaw, &cyclicPtr, sizeof(cyclicPtr));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(reinterpret_cast<String*>(badStringRaw))); }, kMoveSymbol));
    std::memcpy(&stillThere, badStringRaw, sizeof(stillThere));
    Require(stillThere == cyclicPtr);
}

int main() {
    {
        String null;
        Require(!_ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(&null).ptr);
        Require(!_ZN14OpaqueJSString9tryCreateEON3WTF6StringE(&null).ptr);
    }
    {
        String empty = Latin1("");
        auto result = _ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(&empty);
        Expect(result.ptr, u"");
    }
    {
        String latin1 = Latin1("caf\xe9");
        auto result = _ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(&latin1);
        Expect(result.ptr, u"café");
        Require(latin1.impl && latin1.impl->length == 4);
    }
    {
        String wide = UTF16(u"あ\U0001F600");
        auto result = _ZN14OpaqueJSString9tryCreateEON3WTF6StringE(&wide);
        Expect(result.ptr, u"あ\U0001F600");
        Require(!wide.impl);
        JSStringRetain_nid_postfix(result.ptr);
        JSStringRelease_nid_postfix(result.ptr);
        Require(result.ptr->references.load() == 1);
    }
    bool threw = false;
    try {
        _ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        _ZN14OpaqueJSString9tryCreateEON3WTF6StringE(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    TestGuestPointers();
    return 0;
}
