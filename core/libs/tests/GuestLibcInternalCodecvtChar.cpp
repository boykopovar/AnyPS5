#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
}

namespace {

constexpr int kOk = 0;
constexpr int kNoconv = 3;
constexpr std::uint64_t kStateSentinel = 0x1122334455667788ULL;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal codecvt char: %s\n", message);
    std::abort();
}

void APS5_VABI Destroy(GuestLocale::Facet*) {}
void APS5_VABI Retain(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI Release(GuestLocale::Facet*) { return nullptr; }
void* APS5_VABI TestHeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer == nullptr ? 12 : 0;
}

void RegisterHeap() {
    static std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&TestHeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&TestHeapFree);
    heapApi[2] = reinterpret_cast<void*>(&TestHeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&TestHeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&TestHeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&TestHeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&TestHeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
}

int APS5_VABI One(const GuestLocale::Facet*) { return 1; }

struct Calls {
    int in = 0;
    int out = 0;
    int unshift = 0;
    int length = 0;
    GuestLocale::Facet* self = nullptr;
    std::uint64_t* state = nullptr;
};

Calls g_calls;

void Record(GuestLocale::Facet* self, std::uint64_t* state) {
    g_calls.self = self;
    g_calls.state = state;
}

int APS5_VABI OverrideIn(GuestLocale::Facet* self, std::uint64_t* state, const char*, const char* last1, const char** next1, char*, char* last2, char** next2) {
    Record(self, state);
    ++g_calls.in;
    *next1 = last1;
    *next2 = last2;
    return 41;
}

int APS5_VABI OverrideOut(GuestLocale::Facet* self, std::uint64_t* state, const char*, const char* last1, const char** next1, char*, char* last2, char** next2) {
    Record(self, state);
    ++g_calls.out;
    *next1 = last1;
    *next2 = last2;
    return 42;
}

int APS5_VABI OverrideUnshift(GuestLocale::Facet* self, std::uint64_t* state, char*, char* last2, char** next2) {
    Record(self, state);
    ++g_calls.unshift;
    *next2 = last2;
    return 43;
}

std::size_t APS5_VABI OverrideLength(GuestLocale::Facet* self, std::uint64_t* state, const char*, const char*, std::size_t count) {
    Record(self, state);
    ++g_calls.length;
    return count + 0x100000000ULL;
}

const GuestLocale::CodecvtVtable g_internalVtable{
    {Destroy, Destroy, Retain, Release},
    _ZNKSt7codecvtIcc9_MbstatetE16do_always_noconvEv_nid_postfix,
    One,
    One,
    _ZNKSt7codecvtIcc9_MbstatetE5do_inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix,
    _ZNKSt7codecvtIcc9_MbstatetE6do_outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix,
    _ZNKSt7codecvtIcc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix,
    sceLibcInternalCodecvtCharDoLength_nid_no_patch};

const GuestLocale::CodecvtVtable g_overrideVtable{
    {Destroy, Destroy, Retain, Release},
    _ZNKSt7codecvtIcc9_MbstatetE16do_always_noconvEv_nid_postfix,
    One,
    One,
    OverrideIn,
    OverrideOut,
    OverrideUnshift,
    OverrideLength};

using Conversion = int (APS5_VABI*)(GuestLocale::Facet*, std::uint64_t*, const char*, const char*, const char**, char*, char*, char**);
using Unshift = int (APS5_VABI*)(GuestLocale::Facet*, std::uint64_t*, char*, char*, char**);
using Length = std::size_t (APS5_VABI*)(GuestLocale::Facet*, std::uint64_t*, const char*, const char*, std::size_t);

void CheckConversion(Conversion convert, GuestLocale::Facet* facet, const char* message) {
    const char source[] = "codecvt";
    char target[8] = {'t', 'a', 'r', 'g', 'e', 't', '!', '\0'};
    const char* next1 = nullptr;
    char* next2 = nullptr;
    std::uint64_t state = kStateSentinel;
    Require(convert(facet, &state, source, source + 7, &next1, target, target + 8, &next2) == kNoconv, message);
    Require(next1 == source && next2 == target, "a noconv conversion leaves both next pointers at the range starts");
    Require(state == kStateSentinel, "a noconv conversion leaves the state alone");
    Require(target[0] == 't' && target[6] == '!' && target[7] == '\0', "a noconv conversion writes nothing");
    next1 = source + 3;
    next2 = target + 3;
    Require(convert(facet, &state, source + 2, source + 2, &next1, target + 5, target + 5, &next2) == kNoconv, "an empty range is noconv");
    Require(next1 == source + 2 && next2 == target + 5, "an empty range still stores both next pointers");
}

void CheckUnshift(Unshift unshift, GuestLocale::Facet* facet) {
    char target[4] = {'a', 'b', 'c', 'd'};
    char* next2 = nullptr;
    std::uint64_t state = kStateSentinel;
    Require(unshift(facet, &state, target + 1, target + 4, &next2) == kOk, "unshift returns ok");
    Require(next2 == target + 1, "unshift stores the start of the output range");
    Require(state == kStateSentinel, "unshift leaves the state alone");
    Require(target[0] == 'a' && target[1] == 'b' && target[2] == 'c' && target[3] == 'd', "unshift writes nothing");
}

void CheckLength(Length length, GuestLocale::Facet* facet) {
    const char source[16] = {};
    std::uint64_t state = kStateSentinel;
    Require(length(facet, &state, source, source + 16, 5) == 5, "length is the count when the range is longer");
    Require(length(facet, &state, source, source + 16, 16) == 16, "length is the range when it equals the count");
    Require(length(facet, &state, source, source + 16, 17) == 16, "length is the range when the count is longer");
    Require(length(facet, &state, source + 4, source + 4, 9) == 0, "length of an empty range is 0");
    Require(length(facet, &state, source, source + 16, 0) == 0, "length for a count of 0 is 0");
    Require(length(facet, &state, source, source + 16, ~std::size_t{0}) == 16, "length takes the whole 64-bit count");
    Require(state == kStateSentinel, "length leaves the state alone");
}

void CheckVirtuals() {
    GuestLocale::Facet facet{&g_internalVtable.facet, 0, 0};
    Require(_ZNKSt7codecvtIcc9_MbstatetE16do_always_noconvEv_nid_postfix(&facet), "do_always_noconv is true");
    Require(_ZNKSt7codecvtIcc9_MbstatetE16do_always_noconvEv_nid_postfix(nullptr), "do_always_noconv ignores the facet");
    CheckConversion(_ZNKSt7codecvtIcc9_MbstatetE5do_inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix, nullptr, "do_in returns noconv");
    CheckConversion(_ZNKSt7codecvtIcc9_MbstatetE6do_outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix, nullptr, "do_out returns noconv");
    CheckUnshift(_ZNKSt7codecvtIcc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix, nullptr);
    CheckLength(sceLibcInternalCodecvtCharDoLength_nid_no_patch, nullptr);
}

void CheckMembers(GuestLocale::Facet* facet) {
    CheckConversion(_ZNKSt7codecvtIcc9_MbstatetE2inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix, facet, "in returns noconv");
    CheckConversion(_ZNKSt7codecvtIcc9_MbstatetE3outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix, facet, "out returns noconv");
    CheckUnshift(_ZNKSt7codecvtIcc9_MbstatetE7unshiftERS0_PcS3_RS3__nid_postfix, facet);
    CheckLength(_ZNKSt7codecvtIcc9_MbstatetE6lengthERS0_PKcS4_m_nid_postfix, facet);
}

void CheckLibcFacet() {
    const GuestLocale::Implementation* locale = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::Facet* facet = nullptr;
    Require(_ZNSt7codecvtIcc9_MbstatetE7_GetcatEPPKNSt6locale5facetEPKS2__nid_postfix(&facet, &locale) == 2 && facet != nullptr, "libc builds its codecvt facet");
    const auto* vtable = reinterpret_cast<const GuestLocale::CodecvtVtable*>(facet->vtable);
    Require(vtable->alwaysNoconv(facet) == _ZNKSt7codecvtIcc9_MbstatetE16do_always_noconvEv_nid_postfix(facet), "do_always_noconv matches the libc facet");
    CheckConversion(vtable->in, facet, "the libc facet do_in returns noconv");
    CheckConversion(vtable->out, facet, "the libc facet do_out returns noconv");
    CheckUnshift(vtable->unshift, facet);
    CheckLength(vtable->length, facet);
    CheckMembers(facet);
    facet->vtable->deleteObject(facet);
}

void CheckDispatch() {
    GuestLocale::Facet facet{&g_overrideVtable.facet, 0, 0};
    const char source[4] = {};
    char target[4] = {};
    const char* next1 = nullptr;
    char* next2 = nullptr;
    std::uint64_t state = kStateSentinel;
    Require(_ZNKSt7codecvtIcc9_MbstatetE2inERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(&facet, &state, source, source + 4, &next1, target, target + 4, &next2) == 41, "in returns what the object's do_in returns");
    Require(g_calls.in == 1 && g_calls.self == &facet && g_calls.state == &state, "in passes the facet and the state to do_in");
    Require(next1 == source + 4 && next2 == target + 4, "in passes both ranges and next pointers to do_in");
    next1 = nullptr;
    next2 = nullptr;
    g_calls.self = nullptr;
    Require(_ZNKSt7codecvtIcc9_MbstatetE3outERS0_PKcS4_RS4_PcS6_RS6__nid_postfix(&facet, &state, source, source + 3, &next1, target, target + 2, &next2) == 42, "out returns what the object's do_out returns");
    Require(g_calls.out == 1 && g_calls.self == &facet && g_calls.state == &state, "out passes the facet and the state to do_out");
    Require(next1 == source + 3 && next2 == target + 2, "out passes both ranges and next pointers to do_out");
    next2 = nullptr;
    g_calls.self = nullptr;
    Require(_ZNKSt7codecvtIcc9_MbstatetE7unshiftERS0_PcS3_RS3__nid_postfix(&facet, &state, target, target + 1, &next2) == 43, "unshift returns what the object's do_unshift returns");
    Require(g_calls.unshift == 1 && g_calls.self == &facet && g_calls.state == &state, "unshift passes the facet and the state to do_unshift");
    Require(next2 == target + 1, "unshift passes the range and the next pointer to do_unshift");
    g_calls.self = nullptr;
    Require(_ZNKSt7codecvtIcc9_MbstatetE6lengthERS0_PKcS4_m_nid_postfix(&facet, &state, source, source + 4, 7) == 0x100000007ULL, "length returns the whole 64-bit result of the object's do_length");
    Require(g_calls.length == 1 && g_calls.self == &facet && g_calls.state == &state, "length passes the facet and the state to do_length");
    Require(g_calls.in == 1 && g_calls.out == 1 && g_calls.unshift == 1, "each member calls only its own virtual");
}

}

int main() {
    RegisterHeap();
    CheckVirtuals();
    GuestLocale::Facet facet{&g_internalVtable.facet, 0, 0};
    CheckMembers(&facet);
    CheckLibcFacet();
    CheckDispatch();
    std::puts("libc internal codecvt char passed");
    return 0;
}
