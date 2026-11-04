#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal codecvt: %s\n", message);
    std::abort();
}

void* APS5_VABI HeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI HeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI HeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI HeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI HeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer == nullptr ? 12 : 0;
}

constexpr int kOk = 0;
constexpr int kPartial = 1;
constexpr int kError = 2;
constexpr int kNoconv = 3;

void APS5_VABI FacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI FacetRelease(GuestLocale::Facet*) { return nullptr; }
bool APS5_VABI DerivedAlwaysNoconv(const GuestLocale::Facet*) { return false; }
int APS5_VABI DerivedConstant(const GuestLocale::Facet*) { return 4; }
int APS5_VABI DerivedIn(GuestLocale::Facet*, std::uint64_t* state, const char*, const char* last1, const char** next1, char* first3, char*, char** next3) {
    *state = 0x11;
    *next1 = last1;
    *next3 = first3 + 1;
    return kOk;
}
int APS5_VABI DerivedOut(GuestLocale::Facet*, std::uint64_t* state, const char* first1, const char*, const char** next1, char*, char* last3, char** next3) {
    *state = 0x22;
    *next1 = first1 + 1;
    *next3 = last3;
    return kPartial;
}
int APS5_VABI DerivedUnshift(GuestLocale::Facet*, std::uint64_t* state, char*, char* last3, char** next3) {
    *state = 0x33;
    *next3 = last3;
    return kError;
}
std::size_t APS5_VABI DerivedLength(GuestLocale::Facet*, std::uint64_t* state, const char*, const char*, std::size_t count) {
    *state = 0x44;
    return 0xffffffff00000000ull | (count + 1);
}
const GuestLocale::CodecvtVtable g_derivedVtable{{FacetNop, FacetNop, FacetNop, FacetRelease}, DerivedAlwaysNoconv, DerivedConstant, DerivedConstant, DerivedIn, DerivedOut, DerivedUnshift, DerivedLength};

void CheckClassic(GuestLocale::Facet* classic) {
    const char source[] = "abcde";
    const char* const last = source + 5;
    std::uint64_t state = 0x5a;
    std::array<char, 4> target{'w', 'x', 'y', 'z'};
    const char* next1 = nullptr;
    char* next3 = nullptr;

    Require(sceLibcInternalCodecvtDoAlwaysNoconv_nid_no_patch(classic), "do_always_noconv of codecvt<char, char> is true");

    Require(sceLibcInternalCodecvtDoIn_nid_no_patch(classic, &state, source, last, &next1, target.data(), target.data() + target.size(), &next3) == kNoconv, "do_in returns noconv");
    Require(next1 == source && next3 == target.data(), "do_in leaves from_next at from and to_next at to");
    Require(sceLibcInternalCodecvtIn_nid_no_patch(classic, &state, source, last, &next1, target.data(), target.data() + target.size(), &next3) == kNoconv, "in returns noconv");
    Require(next1 == source && next3 == target.data(), "in leaves from_next at from and to_next at to");

    next1 = nullptr;
    next3 = nullptr;
    Require(sceLibcInternalCodecvtDoOut_nid_no_patch(classic, &state, source, last, &next1, target.data(), target.data() + target.size(), &next3) == kNoconv, "do_out returns noconv");
    Require(next1 == source && next3 == target.data(), "do_out leaves from_next at from and to_next at to");
    Require(sceLibcInternalCodecvtOut_nid_no_patch(classic, &state, source, last, &next1, target.data(), target.data() + target.size(), &next3) == kNoconv, "out returns noconv");
    Require(next1 == source && next3 == target.data(), "out leaves from_next at from and to_next at to");

    next3 = nullptr;
    Require(sceLibcInternalCodecvtDoUnshift_nid_no_patch(classic, &state, target.data(), target.data() + target.size(), &next3) == kNoconv, "do_unshift of codecvt<char, char> returns noconv");
    Require(next3 == target.data(), "do_unshift stores no characters");
    next3 = nullptr;
    Require(sceLibcInternalCodecvtUnshift_nid_no_patch(classic, &state, target.data(), target.data() + target.size(), &next3) == kNoconv, "unshift returns noconv");
    Require(next3 == target.data(), "unshift stores no characters");

    Require(sceLibcInternalCodecvtDoLength_nid_no_patch(classic, &state, source, last, 3) == 3, "do_length is max when max is smaller");
    Require(sceLibcInternalCodecvtDoLength_nid_no_patch(classic, &state, source, last, 9) == 5, "do_length is from_end - from when that is smaller");
    Require(sceLibcInternalCodecvtLength_nid_no_patch(classic, &state, source, last, 2) == 2, "length is max when max is smaller");
    Require(sceLibcInternalCodecvtLength_nid_no_patch(classic, &state, source, last, 7) == 5, "length is from_end - from when that is smaller");
    Require(sceLibcInternalCodecvtLength_nid_no_patch(classic, &state, source, source, 7) == 0, "length of an empty range is 0");

    Require(state == 0x5a, "the conversions leave the state alone");
    Require(std::memcmp(target.data(), "wxyz", target.size()) == 0, "the conversions write no characters");
}

void CheckDerived() {
    GuestLocale::Facet derived{&g_derivedVtable.facet, 1, 0};
    const char source[] = "abcde";
    const char* const last = source + 5;
    std::array<char, 4> target{};
    std::uint64_t state = 0;
    const char* next1 = nullptr;
    char* next3 = nullptr;

    Require(sceLibcInternalCodecvtIn_nid_no_patch(&derived, &state, source, last, &next1, target.data(), target.data() + target.size(), &next3) == kOk, "in returns the overriding do_in's result");
    Require(state == 0x11 && next1 == last && next3 == target.data() + 1, "in passes its arguments to the overriding do_in");
    Require(sceLibcInternalCodecvtOut_nid_no_patch(&derived, &state, source, last, &next1, target.data(), target.data() + target.size(), &next3) == kPartial, "out returns the overriding do_out's result");
    Require(state == 0x22 && next1 == source + 1 && next3 == target.data() + target.size(), "out passes its arguments to the overriding do_out");
    Require(sceLibcInternalCodecvtUnshift_nid_no_patch(&derived, &state, target.data(), target.data() + target.size(), &next3) == kError, "unshift returns the overriding do_unshift's result");
    Require(state == 0x33 && next3 == target.data() + target.size(), "unshift passes its arguments to the overriding do_unshift");
    Require(sceLibcInternalCodecvtLength_nid_no_patch(&derived, &state, source, last, 6) == 7, "length returns the int the overriding do_length returned");
    Require(state == 0x44, "length passes its state to the overriding do_length");

    state = 0x5a;
    next1 = nullptr;
    next3 = nullptr;
    Require(sceLibcInternalCodecvtDoAlwaysNoconv_nid_no_patch(&derived), "do_always_noconv ignores the overriding vtable");
    Require(sceLibcInternalCodecvtDoIn_nid_no_patch(&derived, &state, source, last, &next1, target.data(), target.data() + target.size(), &next3) == kNoconv && next1 == source && next3 == target.data(), "do_in ignores the overriding vtable");
    Require(sceLibcInternalCodecvtDoOut_nid_no_patch(&derived, &state, source, last, &next1, target.data(), target.data() + target.size(), &next3) == kNoconv && next1 == source && next3 == target.data(), "do_out ignores the overriding vtable");
    Require(sceLibcInternalCodecvtDoUnshift_nid_no_patch(&derived, &state, target.data(), target.data() + target.size(), &next3) == kNoconv && next3 == target.data(), "do_unshift ignores the overriding vtable");
    Require(sceLibcInternalCodecvtDoLength_nid_no_patch(&derived, &state, source, last, 6) == 5, "do_length ignores the overriding vtable");
    Require(state == 0x5a, "the base do_ members leave the state alone");
}

void CheckNullFacet() {
    std::uint64_t state = 0;
    const char source[] = "ab";
    std::array<char, 2> target{};
    const char* next1 = nullptr;
    char* next3 = nullptr;
    GuestLocale::Facet empty{nullptr, 1, 0};
    for (GuestLocale::Facet* facet : {static_cast<GuestLocale::Facet*>(nullptr), &empty}) {
        int rejected = 0;
        try { sceLibcInternalCodecvtIn_nid_no_patch(facet, &state, source, source + 2, &next1, target.data(), target.data() + 2, &next3); } catch (const std::invalid_argument&) { ++rejected; }
        try { sceLibcInternalCodecvtOut_nid_no_patch(facet, &state, source, source + 2, &next1, target.data(), target.data() + 2, &next3); } catch (const std::invalid_argument&) { ++rejected; }
        try { sceLibcInternalCodecvtUnshift_nid_no_patch(facet, &state, target.data(), target.data() + 2, &next3); } catch (const std::invalid_argument&) { ++rejected; }
        try { sceLibcInternalCodecvtLength_nid_no_patch(facet, &state, source, source + 2, 2); } catch (const std::invalid_argument&) { ++rejected; }
        Require(rejected == 4, "the public members reject a null facet or a facet without a vtable");
    }
}

}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&HeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&HeapFree);
    heapApi[2] = reinterpret_cast<void*>(&HeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&HeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&HeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&HeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&HeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    GuestLocale::Facet* classic = nullptr;
    Require(_ZNSt7codecvtIcc9_MbstatetE7_GetcatEPPKNSt6locale5facetEPKS2__nid_postfix(&classic, nullptr) == 2 && classic != nullptr, "_Getcat builds the C-locale facet");
    CheckClassic(classic);
    CheckDerived();
    CheckNullFacet();
    classic->vtable->deleteObject(classic);
    return 0;
}
