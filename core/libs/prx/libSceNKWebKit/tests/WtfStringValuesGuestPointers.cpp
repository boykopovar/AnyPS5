#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using NKWebKit::WtfAbi::SplitResult;
using NKWebKit::WtfAbi::SplitResultIterator;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF string value guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

static unsigned char* MapPages(std::size_t pages) {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, pages * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, pages * 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(page);
}

static void Protect(void* page, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, readable ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
    Require(mprotect(page, 4096, readable ? PROT_READ : PROT_NONE) == 0);
#endif
}

static void UnmapPages(void* page, std::size_t pages) {
#ifdef _WIN32
    static_cast<void>(pages);
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, pages * 4096) == 0);
#endif
}

static String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

static bool Equals(const String& string, const char* text) {
    std::size_t length = std::strlen(text);
    if (!string.impl || string.impl->length != length) return false;
    for (std::size_t i = 0; i < length; ++i) {
        char16_t unit = string.impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(string.impl->data)[i]
                                                       : static_cast<const char16_t*>(string.impl->data)[i];
        if (unit != static_cast<unsigned char>(text[i])) return false;
    }
    return true;
}

static void TestReplace() {
    auto* unreadable = reinterpret_cast<const unsigned char*>(0x10);
    String latin1 = Latin1("a,b,c");
    const char16_t wide[] = {u'a', u',', u'b', 0x263A};
    String utf16 = _ZN3WTF10StringImpl6createEPKDsj(wide, 4);

    Require(FaultsAsNotImplemented([&] { return _ZN3WTF10StringImpl7replaceEDsPKhj(latin1.impl, ',', unreadable, 3); },
        "_ZN3WTF10StringImpl7replaceEDsPKhj"));
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF10StringImpl7replaceEDsPKhj(utf16.impl, ',', unreadable, 3); },
        "_ZN3WTF10StringImpl7replaceEDsPKhj"));
    String untouched = _ZN3WTF10StringImpl7replaceEDsPKhj(latin1.impl, ';', unreadable, 3);
    Require(untouched.impl == latin1.impl);

    unsigned char* pages = MapPages(2);
    Protect(pages + 4096, false);
    unsigned char* tail = pages + 4096 - 3;
    std::memcpy(tail, "<->", 3);
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF10StringImpl7replaceEDsPKhj(latin1.impl, ',', tail, 0x10000000U); },
        "_ZN3WTF10StringImpl7replaceEDsPKhj"));
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF10StringImpl7replaceEDsPKhj(latin1.impl, ',', tail, 4); },
        "_ZN3WTF10StringImpl7replaceEDsPKhj"));
    Require(Equals(_ZN3WTF10StringImpl7replaceEDsPKhj(latin1.impl, ',', tail, 3), "a<->b<->c"));
    String wideReplaced = _ZN3WTF10StringImpl7replaceEDsPKhj(utf16.impl, ',', tail, 3);
    Require(wideReplaced.impl && wideReplaced.impl->length == 6 && !(wideReplaced.impl->hashAndFlags & 4));
    const char16_t expected[] = {u'a', u'<', u'-', u'>', u'b', 0x263A};
    Require(std::memcmp(wideReplaced.impl->data, expected, sizeof(expected)) == 0);

    unsigned char* chunked = MapPages(3);
    std::memset(chunked, 'x', 3 * 4096);
    String chunkedReplaced = _ZN3WTF10StringImpl7replaceEDsPKhj(latin1.impl, ',', chunked + 1, 3 * 4096 - 1);
    Require(chunkedReplaced.impl && chunkedReplaced.impl->length == 3 + 2 * (3 * 4096 - 1));
    const auto* replacedBytes = static_cast<const unsigned char*>(chunkedReplaced.impl->data);
    Require(replacedBytes[0] == 'a' && replacedBytes[1] == 'x' && replacedBytes[3 * 4096] == 'b' && replacedBytes[chunkedReplaced.impl->length - 2] == 'x');
    UnmapPages(chunked, 3);
    UnmapPages(pages, 2);
}

static void TestSplitIterator() {
    auto* unreadable = reinterpret_cast<SplitResultIterator*>(0x10);
    Require(FaultsAsNotImplemented([&] { _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(unreadable); return 0; },
        "_ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF10StringView11SplitResult8IteratorppEv(unreadable); },
        "_ZN3WTF10StringView11SplitResult8IteratorppEv"));

    SplitResultIterator dangling{reinterpret_cast<const SplitResult*>(0x10), 0, 0, false};
    Require(FaultsAsNotImplemented([&] { _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(&dangling); return 0; },
        "_ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF10StringView11SplitResult8IteratorppEv(&dangling); },
        "_ZN3WTF10StringView11SplitResult8IteratorppEv"));
    Require(dangling.position == 0 && dangling.length == 0 && !dangling.isDone);

    SplitResult result{StringView{"ab,cd", 5, true}, ',', false};
    unsigned char* page = MapPages(1);
    auto* readOnly = reinterpret_cast<SplitResultIterator*>(page);
    *readOnly = SplitResultIterator{&result, 0, 0, false};
    Protect(page, true);
    Require(FaultsAsNotImplemented([&] { _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(readOnly); return 0; },
        "_ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv"));
    Require(FaultsAsNotImplemented([&] { return &_ZN3WTF10StringView11SplitResult8IteratorppEv(readOnly); },
        "_ZN3WTF10StringView11SplitResult8IteratorppEv"));
    Require(readOnly->position == 0 && readOnly->length == 0 && !readOnly->isDone);
    UnmapPages(page, 1);

    unsigned char* writable = MapPages(1);
    auto* iterator = reinterpret_cast<SplitResultIterator*>(writable);
    *iterator = SplitResultIterator{&result, 0, 0, false};
    std::memset(writable + 17, 0xA5, 7);
    _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(iterator);
    Require(iterator->position == 0 && iterator->length == 2 && !iterator->isDone);
    Require(&_ZN3WTF10StringView11SplitResult8IteratorppEv(iterator) == iterator);
    Require(iterator->position == 3 && iterator->length == 2 && !iterator->isDone);
    _ZN3WTF10StringView11SplitResult8IteratorppEv(iterator);
    Require(iterator->isDone);
    for (unsigned i = 17; i < 24; ++i) Require(writable[i] == 0xA5);
    UnmapPages(writable, 1);
}

static void TestStringMethods() {
    auto* unreadable = reinterpret_cast<String*>(0x10);
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF6String25splitAllowingEmptyEntriesEDs(unreadable, ','); },
        "_ZNK3WTF6String25splitAllowingEmptyEntriesEDs"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF6String29charactersWithNullTerminationEv(unreadable); },
        "_ZNK3WTF6String29charactersWithNullTerminationEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF6String5splitEDs(unreadable, ','); }, "_ZNK3WTF6String5splitEDs"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF6String7toFloatEPb(unreadable, nullptr); }, "_ZNK3WTF6String7toFloatEPb"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF6String8toDoubleEPb(unreadable, nullptr); }, "_ZNK3WTF6String8toDoubleEPb"));
    Require(FaultsAsNotImplemented([&] { return _ZNKR3WTF6String12isolatedCopyEv(unreadable); }, "_ZNKR3WTF6String12isolatedCopyEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNO3WTF6String12isolatedCopyEv(unreadable); }, "_ZNO3WTF6String12isolatedCopyEv"));

    String number = Latin1("2.5");
    auto* unwritable = reinterpret_cast<bool*>(0x10);
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF6String7toFloatEPb(&number, unwritable); }, "_ZNK3WTF6String7toFloatEPb"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF6String8toDoubleEPb(&number, unwritable); }, "_ZNK3WTF6String8toDoubleEPb"));
    String null;
    Require(FaultsAsNotImplemented([&] { return _ZNK3WTF6String8toDoubleEPb(&null, unwritable); }, "_ZNK3WTF6String8toDoubleEPb"));

    unsigned char* page = MapPages(1);
    auto* readOnly = reinterpret_cast<String*>(page);
    String source = Latin1("a,,b");
    readOnly->impl = std::exchange(source.impl, nullptr);
    StringImpl* impl = readOnly->impl;
    Protect(page, true);
    unsigned refCount = impl->refCount;
    Require(FaultsAsNotImplemented([&] { return _ZNO3WTF6String12isolatedCopyEv(readOnly); }, "_ZNO3WTF6String12isolatedCopyEv"));
    Require(readOnly->impl == impl && impl->refCount == refCount);
    Require(_ZNK3WTF6String5splitEDs(readOnly, ',').size == 2);
    Require(_ZNK3WTF6String25splitAllowingEmptyEntriesEDs(readOnly, ',').size == 3);
    Require(_ZNK3WTF6String29charactersWithNullTerminationEv(readOnly).size == 5);
    Require(Equals(_ZNKR3WTF6String12isolatedCopyEv(readOnly), "a,,b"));

    bool* okPage = reinterpret_cast<bool*>(MapPages(1));
    okPage[0] = true;
    Require(_ZNK3WTF6String8toDoubleEPb(&number, okPage) == 2.5 && okPage[0]);
    Require(_ZNK3WTF6String7toFloatEPb(readOnly, okPage) == 0.0f && !okPage[0]);
    okPage[0] = true;
    Require(_ZNK3WTF6String7toFloatEPb(&null, okPage) == 0.0f && !okPage[0]);
    UnmapPages(okPage, 1);

    UnmapPages(page, 1);
    String owner(impl);
}

int main() {
    TestReplace();
    TestSplitIterator();
    TestStringMethods();
    return 0;
}
