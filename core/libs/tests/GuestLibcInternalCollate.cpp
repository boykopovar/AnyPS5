#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;

}

namespace {

int derivedCompares = 0;
int derivedHashes = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal collate: %s\n", message);
    std::abort();
}

template<typename TAction>
void Reject(TAction action, const char* message) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, message);
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

int APS5_VABI DerivedCompare(const GuestLocale::CollateFacet*, const char*, const char*, const char*, const char*) {
    ++derivedCompares;
    return 7;
}

std::int64_t APS5_VABI DerivedHash(const GuestLocale::CollateFacet*, const char*, const char*) {
    ++derivedHashes;
    return 42;
}

int DoCompare(const GuestLocale::CollateFacet* facet, const std::string& left, const std::string& right) {
    return sceLibcInternalCollateDoCompare_nid_no_patch(facet, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
}

int Compare(const GuestLocale::CollateFacet* facet, const std::string& left, const std::string& right) {
    return sceLibcInternalCollateCompare_nid_no_patch(facet, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
}

std::int64_t DoHash(const GuestLocale::CollateFacet* facet, const std::string& text) {
    return sceLibcInternalCollateDoHash_nid_no_patch(facet, text.data(), text.data() + text.size());
}

std::int64_t Hash(const GuestLocale::CollateFacet* facet, const std::string& text) {
    return sceLibcInternalCollateHash_nid_no_patch(facet, text.data(), text.data() + text.size());
}

void CheckClassicFacet(const GuestLocale::CollateFacet* facet) {
    Require(DoCompare(facet, "abc", "abd") == -1, "do_compare orders a smaller byte first");
    Require(DoCompare(facet, "abd", "abc") == 1, "do_compare orders a larger byte last");
    Require(DoCompare(facet, "abc", "abc") == 0, "do_compare finds equal ranges equal");
    Require(DoCompare(facet, "ab", "abc") == -1, "do_compare orders a prefix first");
    Require(DoCompare(facet, "abc", "ab") == 1, "do_compare orders the longer range last");
    Require(DoCompare(facet, "", "") == 0, "do_compare finds empty ranges equal");
    Require(DoCompare(facet, "\x80", "a") == 1, "do_compare compares unsigned bytes");
    Require(DoCompare(facet, std::string("a\0b", 3), std::string("a\0c", 3)) == -1, "do_compare reads past embedded nulls");
    Require(sceLibcInternalCollateDoCompare_nid_no_patch(facet, nullptr, nullptr, nullptr, nullptr) == 0, "do_compare accepts null empty ranges");
    const char text[] = "abc";
    Reject([&] { sceLibcInternalCollateDoCompare_nid_no_patch(facet, text + 2, text, text, text + 3); }, "do_compare rejects a reversed range");

    Require(Compare(facet, "abc", "abd") == -1, "compare forwards to do_compare");
    Require(Compare(facet, "abd", "abc") == 1, "compare returns do_compare's positive order");
    Require(Compare(facet, "abc", "abc") == 0, "compare returns do_compare's equality");

    Require(DoHash(facet, "abc") == -1792535898324117685LL, "do_hash matches the libc collate hash");
    Require(DoHash(facet, "") == static_cast<std::int64_t>(0xcbf29ce484222325ULL), "do_hash of an empty range is the offset basis");
    Require(Hash(facet, "abc") == DoHash(facet, "abc"), "hash forwards to do_hash");
    Require(sceLibcInternalCollateDoHash_nid_no_patch(facet, nullptr, nullptr) == DoHash(facet, ""), "do_hash accepts a null empty range");
    Reject([&] { sceLibcInternalCollateDoHash_nid_no_patch(facet, text + 1, text); }, "do_hash rejects a reversed range");

    GuestLocale::String shortResult{};
    Require(sceLibcInternalCollateDoTransform_nid_no_patch(&shortResult, facet, "xyz", "xyz" + 3) == &shortResult, "do_transform returns its result");
    Require(shortResult.size == 3 && shortResult.capacity == 15 && std::memcmp(shortResult.buffer, "xyz", 4) == 0, "do_transform copies a short range inline");

    const std::string longText(40, 'q');
    GuestLocale::String longResult{};
    Require(sceLibcInternalCollateDoTransform_nid_no_patch(&longResult, facet, longText.data(), longText.data() + longText.size()) == &longResult, "do_transform returns a long result");
    Require(longResult.size == 40 && longResult.capacity == (40 | 15), "do_transform sizes a long result");
    Require(std::memcmp(longResult.pointer, longText.c_str(), 41) == 0, "do_transform copies a long range");
    _ZdlPv_nid_postfix(longResult.pointer);
    Reject([&] { sceLibcInternalCollateDoTransform_nid_no_patch(nullptr, facet, "a", "a" + 1); }, "do_transform rejects a null result");
}

void CheckDerivedFacet() {
    const auto& base = *reinterpret_cast<const GuestLocale::CollateVtable*>(&_ZTVSt7collateIcE_nid_postfix[2]);
    GuestLocale::CollateVtable derived = base;
    derived.compare = &DerivedCompare;
    derived.hash = &DerivedHash;
    GuestLocale::CollateFacet facet{};
    facet.base.vtable = &derived.facet;

    Require(Compare(&facet, "abc", "abd") == 7 && derivedCompares == 1, "compare calls the facet's virtual do_compare");
    Require(Hash(&facet, "abc") == 42 && derivedHashes == 1, "hash calls the facet's virtual do_hash");
    Require(DoCompare(&facet, "abc", "abd") == -1 && derivedCompares == 1, "do_compare is the collate<char> member, not the override");
    Require(DoHash(&facet, "abc") == -1792535898324117685LL && derivedHashes == 1, "do_hash is the collate<char> member, not the override");

    Reject([] { sceLibcInternalCollateCompare_nid_no_patch(nullptr, "a", "a" + 1, "b", "b" + 1); }, "compare rejects a null facet");
    Reject([] { sceLibcInternalCollateHash_nid_no_patch(nullptr, "a", "a" + 1); }, "hash rejects a null facet");
    GuestLocale::CollateFacet empty{};
    Reject([&] { sceLibcInternalCollateCompare_nid_no_patch(&empty, "a", "a" + 1, "b", "b" + 1); }, "compare rejects a facet without a vtable");
    Reject([&] { sceLibcInternalCollateHash_nid_no_patch(&empty, "a", "a" + 1); }, "hash rejects a facet without a vtable");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    GuestLocale::Facet* facet = nullptr;
    const GuestLocale::Implementation* const* classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(_ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&facet, classic) == 1 && facet != nullptr, "the classic collate facet is built");
    CheckClassicFacet(reinterpret_cast<const GuestLocale::CollateFacet*>(facet));
    reinterpret_cast<const GuestLocale::CollateVtable*>(facet->vtable)->facet.deleteObject(facet);
    CheckDerivedFacet();

    std::puts("libc internal collate<char> member checks passed");
    return 0;
}
