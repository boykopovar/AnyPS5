#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern std::uint64_t _ZNSt8__sce_v27codecvtIwc9_MbstatetE2idE_nid_postfix;
}

void* APS5_VABI TestHeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer != nullptr ? 0 : -1;
}

constexpr int Ok = 0;
constexpr int Partial = 1;
constexpr int Error = 2;

static unsigned failures = 0;

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

using Noconv = bool (APS5_VABI *)(const void*);
using Count = int (APS5_VABI *)(const void*);
using In = int (APS5_VABI *)(void*, void*, const char*, const char*, const char**, char16_t*, char16_t*, char16_t**);
using Out = int (APS5_VABI *)(void*, void*, const char16_t*, const char16_t*, const char16_t**, char*, char*, char**);
using Unshift = int (APS5_VABI *)(void*, void*, char*, char*, char**);
using Length = int (APS5_VABI *)(void*, void*, const char*, const char*, std::size_t);

template<typename T> static T Slot(const void* facet, int index) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(facet))[index]);
}

struct InResult {
    int result;
    std::size_t read;
    std::u16string written;
};

static InResult RunIn(void* facet, const std::string& bytes, std::size_t room, std::uint8_t stateFill = 0) {
    std::array<std::uint64_t, 4> state{};
    std::memset(state.data(), stateFill, sizeof(state));
    std::array<char16_t, 64> output{};
    const char* next1 = nullptr;
    char16_t* next2 = nullptr;
    const int result = Slot<In>(facet, 7)(facet, state.data(), bytes.data(), bytes.data() + bytes.size(), &next1, output.data(), output.data() + room, &next2);
    return {result, static_cast<std::size_t>(next1 - bytes.data()), std::u16string(output.data(), next2)};
}

struct OutResult {
    int result;
    std::size_t read;
    std::string written;
    std::array<std::uint64_t, 4> state;
};

static OutResult RunOut(void* facet, const std::u16string& wide, std::size_t room) {
    OutResult outcome{};
    std::array<char, 64> output{};
    const char16_t* next1 = nullptr;
    char* next2 = nullptr;
    outcome.result = Slot<Out>(facet, 8)(facet, outcome.state.data(), wide.data(), wide.data() + wide.size(), &next1, output.data(), output.data() + room, &next2);
    outcome.read = static_cast<std::size_t>(next1 - wide.data());
    outcome.written.assign(output.data(), next2);
    return outcome;
}

static int RunLength(void* facet, const std::string& bytes, std::size_t count) {
    std::array<std::uint64_t, 4> state{};
    return Slot<Length>(facet, 10)(facet, state.data(), bytes.data(), bytes.data() + bytes.size(), count);
}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&TestHeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&TestHeapFree);
    heapApi[2] = reinterpret_cast<void*>(&TestHeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&TestHeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&TestHeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&TestHeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&TestHeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    static const GuestLocale::SceV2Locinfo info{0, "C", "C"};
    auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    _ZNSt8__sce_v26locale7_Locimp8_MakelocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(&info, 0x3f, locale, nullptr);
    void* facet = locale->facets[_ZNSt8__sce_v27codecvtIwc9_MbstatetE2idE_nid_postfix];
    Require(facet != nullptr, "_Makeloc installs the wide codecvt");

    Require(!Slot<Noconv>(facet, 4)(facet), "always_noconv is false");
    Require(Slot<Count>(facet, 5)(facet) == 1, "max_length");
    Require(Slot<Count>(facet, 6)(facet) == 1, "encoding");

    const auto mixed = RunIn(facet, "A\xc3\xa9" "B", 8);
    Require(mixed.result == Ok && mixed.read == 4 && mixed.written == u"AéB", "in converts a two-byte sequence");
    const auto truncated = RunIn(facet, "\xe2\x82", 8);
    Require(truncated.result == Partial && truncated.read == 0 && truncated.written.empty(), "in stops at an incomplete sequence");
    const auto invalid = RunIn(facet, "A\xff", 8);
    Require(invalid.result == Error && invalid.read == 1 && invalid.written == u"A", "in fails on an invalid byte");
    const auto full = RunIn(facet, "AB", 1);
    Require(full.result == Partial && full.read == 1 && full.written == u"A", "in stops when the output is full");
    const auto nul = RunIn(facet, std::string("\0A", 2), 8);
    Require(nul.result == Ok && nul.read == 2 && nul.written == std::u16string(u"\0A", 2), "in steps over a null byte");
    const auto ignored = RunIn(facet, "\xc3\xa9", 8, 0xff);
    Require(ignored.result == Ok && ignored.written == u"é", "in starts from a clean state whatever it is given");
    const auto empty = RunIn(facet, "", 0);
    Require(empty.result == Ok && empty.read == 0, "in on empty input");

    const auto euro = RunOut(facet, u"A€", 16);
    Require(euro.result == Ok && euro.read == 2 && euro.written == "A\xe2\x82\xac", "out writes a three-byte sequence");
    const auto tight = RunOut(facet, u"€", 2);
    Require(tight.result == Partial && tight.read == 0 && tight.written.empty(), "out stops when a sequence does not fit");
    const auto exact = RunOut(facet, u"€", 3);
    Require(exact.result == Ok && exact.read == 1 && exact.written == "\xe2\x82\xac", "out fills an exact room");
    const auto none = RunOut(facet, u"A", 0);
    Require(none.result == Partial && none.read == 0, "out with no room");
    const auto done = RunOut(facet, u"", 0);
    Require(done.result == Ok, "out with no input");

    std::array<std::uint64_t, 4> state{};
    std::array<char, 8> output{};
    char* next = nullptr;
    Require(Slot<Unshift>(facet, 9)(facet, state.data(), output.data(), output.data() + output.size(), &next) == Ok && next == output.data(), "unshift writes nothing from the initial state");

    Require(RunLength(facet, "A\xc3\xa9" "B", 2) == 3, "length counts the bytes of the first characters");
    Require(RunLength(facet, "A\xc3\xa9" "B", 10) == 4, "length stops at the end");
    Require(RunLength(facet, "\xff" "A", 10) == 0, "length stops at an invalid byte");
    Require(RunLength(facet, std::string("\0\0", 2), 10) == 2, "length counts null bytes");
    Require(RunLength(facet, "AB", 0) == 0, "length with a zero count");

    std::array<std::uint64_t, 4> pending{};
    std::uint16_t ignoredWide = 0;
    Require(static_cast<int>(mbrtowc_nid_postfix(&ignoredWide, "\xe2\x82", 2, pending.data())) == -2, "mbrtowc keeps a pending sequence");
    auto pendingForIn = pending;
    std::array<char16_t, 4> wideOut{};
    const char tail[] = "\xac";
    const char* tailNext = nullptr;
    char16_t* wideNext = nullptr;
    Require(Slot<In>(facet, 7)(facet, pendingForIn.data(), tail, tail + 1, &tailNext, wideOut.data(), wideOut.data() + wideOut.size(), &wideNext) == Error && pendingForIn == pending, "in ignores a pending sequence in the given state");
    auto pendingForLength = pending;
    Require(Slot<Length>(facet, 10)(facet, pendingForLength.data(), tail, tail + 1, 4) == 1, "length continues the given state");

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
