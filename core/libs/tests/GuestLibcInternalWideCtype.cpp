#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <array>
#include <cstddef>
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

constexpr short ClassA = 0x01;
constexpr short ClassB = 0x02;
constexpr short ClassC = 0x04;

struct Fixture {
    const std::uintptr_t* vtable;
    std::uint32_t references;
    std::uint32_t reserved;
    const short* classification;
    const void* lowerMap;
    const std::int16_t* upperMap;
    std::array<short, 256> classes;
    std::array<std::int16_t, 256> upper;
    std::uint32_t narrowCalls;
    std::uint32_t widenCalls;
    std::uint32_t overrideCalls;
    char16_t widenBase;
    std::array<short, 256> replacementClasses;
    const short* replacementClassification;
    char16_t* mutateDestination;
    char16_t mutationValue;
    bool replaceClassificationOnNarrow;
};

static_assert(offsetof(Fixture, classification) == 0x10);
static_assert(offsetof(Fixture, lowerMap) == 0x18);
static_assert(offsetof(Fixture, upperMap) == 0x20);

using NarrowFunction = char (APS5_VABI*)(const GuestLocale::Facet*, char16_t, char);
using WidenFunction = char16_t (APS5_VABI*)(const GuestLocale::Facet*, char);
using DoIsFunction = bool (APS5_VABI*)(const GuestLocale::Facet*, short, char16_t);
using WideVtable = std::array<std::uintptr_t, 18>;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest libc internal wide ctype: %s\n", message);
    std::abort();
}

template<typename Function>
void StoreFunction(WideVtable& table, std::size_t slot, Function function) {
    static_assert(sizeof(Function) <= sizeof(table[slot]));
    std::memcpy(&table[slot + 2], &function, sizeof(function));
}

const GuestLocale::Facet* AsFacet(const Fixture& fixture) {
    return reinterpret_cast<const GuestLocale::Facet*>(&fixture);
}

char APS5_VABI Narrow(const GuestLocale::Facet* self, char16_t character, char fallback) {
    auto& fixture = *const_cast<Fixture*>(reinterpret_cast<const Fixture*>(self));
    ++fixture.narrowCalls;
    Require(fallback == 0, "narrow receives the zero fallback");
    if (fixture.replaceClassificationOnNarrow) {
        fixture.classification = fixture.replacementClassification;
        fixture.replaceClassificationOnNarrow = false;
    }
    if (fixture.mutateDestination) {
        *fixture.mutateDestination = fixture.mutationValue;
        fixture.mutateDestination = nullptr;
    }
    if (character == 0x0100 || character == 0xffff) return fallback;
    return character <= 0x00ff ? static_cast<char>(character) : fallback;
}

char16_t APS5_VABI Widen(const GuestLocale::Facet* self, char character) {
    auto& fixture = *const_cast<Fixture*>(reinterpret_cast<const Fixture*>(self));
    ++fixture.widenCalls;
    return static_cast<char16_t>(fixture.widenBase + static_cast<unsigned char>(character));
}

bool APS5_VABI OverrideDoIs(const GuestLocale::Facet* self, short mask, char16_t character) {
    auto& fixture = *const_cast<Fixture*>(reinterpret_cast<const Fixture*>(self));
    ++fixture.overrideCalls;
    return (mask & ClassA) != 0 && character != u'!';
}

void Initialize(Fixture& fixture, WideVtable& table, bool overrideDoIs) {
    fixture.vtable = table.data() + 2;
    fixture.references = 1;
    fixture.reserved = 0;
    fixture.classification = fixture.classes.data();
    fixture.lowerMap = nullptr;
    fixture.upperMap = fixture.upper.data();
    fixture.narrowCalls = 0;
    fixture.widenCalls = 0;
    fixture.overrideCalls = 0;
    fixture.widenBase = 0x4000;
    fixture.replacementClasses.fill(0);
    fixture.replacementClassification = fixture.replacementClasses.data();
    fixture.mutateDestination = nullptr;
    fixture.mutationValue = 0;
    fixture.replaceClassificationOnNarrow = false;
    StoreFunction(table, 4, overrideDoIs ? DoIsFunction(OverrideDoIs) : DoIsFunction(sceLibcInternalWideCtypeDoIs_nid_no_patch));
    StoreFunction(table, 12, WidenFunction(Widen));
    StoreFunction(table, 14, NarrowFunction(Narrow));
}

void CheckClassification(Fixture& first, Fixture& second) {
    first.classes.fill(0);
    second.classes.fill(0);
    first.classes[0] = ClassA;
    first.classes[static_cast<unsigned char>('A')] = ClassA | ClassB;
    first.classes[static_cast<unsigned char>('B')] = ClassB;
    second.classes[0] = ClassC;
    second.classes[static_cast<unsigned char>('A')] = ClassC;
    second.classes[static_cast<unsigned char>('B')] = ClassA;
    Require(sceLibcInternalWideCtypeDoIs_nid_no_patch(AsFacet(first), ClassA, u'A'), "first facet uses its own classification table");
    Require(!sceLibcInternalWideCtypeDoIs_nid_no_patch(AsFacet(second), ClassA, u'A'), "second facet does not use the first table");
    Require(sceLibcInternalWideCtypeDoIs_nid_no_patch(AsFacet(first), ClassA, 0x0100), "zero narrow result indexes classification entry zero");
    Require(!sceLibcInternalWideCtypeDoIs_nid_no_patch(AsFacet(second), ClassA, 0xffff), "U+FFFF uses the second facet's zero entry");
    Require(!sceLibcInternalWideCtypeDoIs_nid_no_patch(AsFacet(first), 0, u'A'), "zero mask matches nothing");
    first.replacementClasses[static_cast<unsigned char>('A')] = 0;
    first.replaceClassificationOnNarrow = true;
    Require(sceLibcInternalWideCtypeDoIs_nid_no_patch(AsFacet(first), ClassA, u'A'), "scalar classification captures its table before narrow dispatch");
    first.classification = first.classes.data();
    first.replaceClassificationOnNarrow = true;
    std::array<short, 2> rangeMasks{-1, -1};
    const std::array<char16_t, 2> tableSwapInput{u'A', u'B'};
    sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), tableSwapInput.data(), tableSwapInput.data() + tableSwapInput.size(), rangeMasks.data());
    Require(rangeMasks[0] == (ClassA | ClassB) && rangeMasks[1] == 0, "range classification reloads its table after each narrow callback");
    first.classification = first.classes.data();

    const std::array<char16_t, 4> input{u'A', 0x0100, u'B', 0xffff};
    std::array<short, 5> output;
    output.fill(-1);
    const auto* end = sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), input.data(), input.data() + input.size(), output.data());
    Require(end == input.data() + input.size(), "range classification returns the end pointer");
    Require(output[0] == (ClassA | ClassB) && output[1] == ClassA && output[2] == ClassB && output[3] == ClassA, "range classification preserves complete table masks");
    Require(output[4] == -1, "range classification writes exactly one mask per input");
    Require(sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), input.data(), input.data(), nullptr) == input.data(), "empty range returns without writing");
}

void CheckScanNot(Fixture& first, Fixture& second, WideVtable& firstTable) {
    first.classes.fill(0);
    first.classes[static_cast<unsigned char>('A')] = ClassA;
    second.classes.fill(0);
    second.classes[static_cast<unsigned char>('A')] = ClassA;
    StoreFunction(firstTable, 4, DoIsFunction(OverrideDoIs));
    const std::array<char16_t, 3> text{u'A', u'B', u'!'};
    Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(AsFacet(first), ClassA, text.data(), text.data() + text.size()) == text.data() + 2, "scan_not dispatches through the current do_is virtual");
    Require(first.overrideCalls == 3, "scan_not invokes the selected virtual once per examined character");
    StoreFunction(firstTable, 4, DoIsFunction(sceLibcInternalWideCtypeDoIs_nid_no_patch));
    Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(AsFacet(second), ClassA, text.data(), text.data() + text.size()) == text.data() + 1, "scan_not uses the second facet's do_is entry");
    Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(AsFacet(first), ClassA, text.data() + 2, text.data() + 2) == text.data() + 2, "empty scan range returns its end");
}

void CheckUpper(Fixture& first) {
    first.upper.fill(0);
    first.upper[static_cast<unsigned char>('a')] = -2;
    first.upper[static_cast<unsigned char>('b')] = 0x41;
    const auto beforeWiden = first.widenCalls;
    Require(sceLibcInternalWideCtypeDoToupper_nid_no_patch(AsFacet(first), u'a') == 0x40fe, "scalar uppercase sign extends the mapped byte before widening");
    Require(first.widenCalls == beforeWiden + 1, "mapped scalar uppercase calls widen once");
    Require(sceLibcInternalWideCtypeDoToupper_nid_no_patch(AsFacet(first), 0x0100) == 0x0100, "zero narrow result preserves the original wide character");
    Require(first.widenCalls == beforeWiden + 1, "zero narrow result skips map lookup and widen");

    std::array<char16_t, 4> text{u'a', u'b', 0x0100, u'!'};
    const auto* end = sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(AsFacet(first), text.data(), text.data() + 3);
    Require(end == text.data() + 3, "range uppercase returns the end pointer");
    Require(text[0] == 0x40fe && text[1] == 0x4041 && text[2] == 0x0100 && text[3] == u'!', "range uppercase maps signed entries and preserves zero-narrow values");
    Require(sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(AsFacet(first), text.data() + 2, text.data() + 2) == text.data() + 2, "empty uppercase range returns its end");
    Require(first.widenCalls == beforeWiden + 3, "range uppercase widens only nonzero narrow results");
    text[2] = 0x0100;
    first.mutateDestination = &text[2];
    first.mutationValue = 0x7777;
    sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(AsFacet(first), text.data() + 2, text.data() + 3);
    Require(text[2] == 0x7777, "zero-narrow range element does not store over a callback mutation");
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

void CheckGuestPointers(Fixture& first) {
    auto* unmapped = reinterpret_cast<char16_t*>(std::uintptr_t{0x10});
    auto* unmappedMasks = reinterpret_cast<short*>(std::uintptr_t{0x10});
    std::array<char16_t, 3> text{u'a', u'b', u'!'};
    std::array<short, 3> masks{-1, -1, -1};
    const auto before = first.narrowCalls;
    RequireRejected([&] { sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), unmapped, unmapped + 3, masks.data()); }, "range classification rejects an unreadable range");
    RequireRejected([&] { sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), text.data() + 3, text.data(), masks.data()); }, "range classification rejects a reversed range");
    Require(masks == std::array<short, 3>{-1, -1, -1}, "rejected range classification writes nothing");
    RequireRejected([&] { sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), text.data(), text.data() + 3, unmappedMasks); }, "range classification rejects an unwritable destination");
    RequireRejected([&] { sceLibcInternalWideCtypeDoScanNot_nid_no_patch(AsFacet(first), ClassA, unmapped, unmapped + 3); }, "scan_not rejects an unreadable range");
    RequireRejected([&] { sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(AsFacet(first), unmapped, unmapped + 3); }, "range uppercase rejects an unreadable range");
    Require(first.narrowCalls == before + 3, "only the unwritable destination case classified its characters");
    Require(text == std::array<char16_t, 3>{u'a', u'b', u'!'}, "rejected calls leave the characters alone");
    Require(sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), unmapped, unmapped, unmappedMasks) == unmapped, "an empty classification range reads and writes nothing");
    Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(AsFacet(first), ClassA, unmapped, unmapped) == unmapped, "an empty scan range reads nothing");
    Require(sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(AsFacet(first), unmapped, unmapped) == unmapped, "an empty uppercase range reads nothing");
}

void CheckChunkedRanges(Fixture& first) {
    constexpr std::size_t huge = std::size_t{1} << 40;
    auto* unmapped = reinterpret_cast<char16_t*>(std::uintptr_t{0x10});
    std::array<short, 3> masks{-1, -1, -1};
    Capped([&] {
        RequireRejected([&] { sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), unmapped, unmapped + huge, masks.data()); }, "range classification rejects an oversized unmapped range");
        RequireRejected([&] { sceLibcInternalWideCtypeDoScanNot_nid_no_patch(AsFacet(first), ClassA, unmapped, unmapped + huge); }, "scan_not rejects an oversized unmapped range");
        RequireRejected([&] { sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(AsFacet(first), unmapped, unmapped + huge); }, "range uppercase rejects an oversized unmapped range");
#ifndef _WIN32
        GuardedPage page;
        auto* text = page.Tail<char16_t>(3);
        text[0] = u'a';
        text[1] = u'A';
        text[2] = u'!';
        RequireRejected([&] { sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), text, text + huge, masks.data()); }, "range classification rejects an oversized range before a guard page");
        RequireRejected([&] { sceLibcInternalWideCtypeDoScanNot_nid_no_patch(AsFacet(first), ClassA, text, text + huge); }, "scan_not rejects an oversized range before a guard page");
        RequireRejected([&] { sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(AsFacet(first), text, text + huge); }, "range uppercase rejects an oversized range before a guard page");
        Require(text[0] == u'a' && text[1] == u'A' && text[2] == u'!', "rejected oversized uppercase writes nothing");
#endif
    });
    Require(masks == std::array<short, 3>{-1, -1, -1}, "rejected oversized classification writes nothing");
    std::vector<char16_t> text(0x1803);
    for (std::size_t index = 0; index < text.size(); ++index) text[index] = static_cast<char16_t>(u"aAb!\u0100Bz"[index % 7]);
    std::vector<short> classes(text.size() + 1, -1);
    Capped([&] { Require(sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), text.data(), text.data() + text.size(), classes.data()) == text.data() + text.size(), "chunked classification returns the end"); });
    for (std::size_t index = 0; index < text.size(); ++index) {
        short single = -1;
        sceLibcInternalWideCtypeDoIsRange_nid_no_patch(AsFacet(first), text.data() + index, text.data() + index + 1, &single);
        Require(classes[index] == single, "chunked classification matches single-character ranges");
    }
    Require(classes.back() == -1, "chunked classification writes one mask per character");
    std::vector<char16_t> letters(text.size(), u'A');
    letters[0x1005] = u'!';
    Capped([&] {
        Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(AsFacet(first), ClassA, letters.data(), letters.data() + letters.size()) == letters.data() + 0x1005, "chunked scan_not stops in a later chunk");
        Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(AsFacet(first), ClassA, letters.data(), letters.data() + 0x1005) == letters.data() + 0x1005, "chunked scan_not returns the end when every character matches");
    });
    auto upper = text;
    Capped([&] { Require(sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(AsFacet(first), upper.data(), upper.data() + upper.size()) == upper.data() + upper.size(), "chunked uppercase returns the end"); });
    for (std::size_t index = 0; index < text.size(); ++index) {
        auto single = text[index];
        sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(AsFacet(first), &single, &single + 1);
        Require(upper[index] == single, "chunked uppercase matches single-character ranges");
    }
}

}

int main() {
    Fixture first{};
    Fixture second{};
    WideVtable firstTable{};
    WideVtable secondTable{};
    Initialize(first, firstTable, true);
    Initialize(second, secondTable, false);
    CheckClassification(first, second);
    CheckScanNot(first, second, firstTable);
    CheckUpper(first);
    CheckGuestPointers(first);
    CheckChunkedRanges(first);
    return 0;
}
