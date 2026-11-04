#include "prx/libSceLibcInternal/include/WideCollate.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;

}

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal collate<wchar_t>: %s\n", message);
    std::abort();
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void* APS5_VABI Allocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI Release(void* pointer) { std::free(pointer); }
void* APS5_VABI AllocateZeroed(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI Reallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI Align(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

struct Recorded {
    int calls = 0;
    const GuestLocale::CollateFacet* self = nullptr;
    const char* first1 = nullptr;
    const char* last1 = nullptr;
    const char* first2 = nullptr;
    const char* last2 = nullptr;
    GuestLocale::String* result = nullptr;
};

Recorded recorded;

int APS5_VABI OverrideCompare(const GuestLocale::CollateFacet* self, const char* first1, const char* last1, const char* first2, const char* last2) {
    recorded = {recorded.calls + 1, self, first1, last1, first2, last2, nullptr};
    return 42;
}

GuestLocale::String* APS5_VABI OverrideTransform(GuestLocale::String* result, const GuestLocale::CollateFacet* self, const char* first, const char* last) {
    recorded = {recorded.calls + 1, self, first, last, nullptr, nullptr, result};
    result->size = 99;
    return result;
}

std::int64_t APS5_VABI OverrideHash(const GuestLocale::CollateFacet* self, const char* first, const char* last) {
    recorded = {recorded.calls + 1, self, first, last, nullptr, nullptr, nullptr};
    return 0x1234;
}

const char* Bytes(const char16_t* text) {
    return reinterpret_cast<const char*>(text);
}

int DoCompare(const GuestLocale::CollateFacet* facet, const std::u16string& left, const std::u16string& right) {
    return sceLibcInternalWideCollateDoCompare_nid_no_patch(facet, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
}

int Compare(const GuestLocale::CollateFacet* facet, const std::u16string& left, const std::u16string& right) {
    return sceLibcInternalWideCollateCompare_nid_no_patch(facet, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
}

std::uint64_t DoHash(const GuestLocale::CollateFacet* facet, const std::u16string& text) {
    return static_cast<std::uint64_t>(sceLibcInternalWideCollateDoHash_nid_no_patch(facet, text.data(), text.data() + text.size()));
}

std::uint64_t Hash(const GuestLocale::CollateFacet* facet, const std::u16string& text) {
    return static_cast<std::uint64_t>(sceLibcInternalWideCollateHash_nid_no_patch(facet, text.data(), text.data() + text.size()));
}

std::u16string Units(const GuestLocale::String& string) {
    const char* data = string.capacity > 7 ? string.pointer : string.buffer;
    return std::u16string(reinterpret_cast<const char16_t*>(data), string.size);
}

void CheckClassicFacet(const GuestLocale::CollateFacet* facet) {
    Require(DoCompare(facet, u"abc", u"abd") == -1, "do_compare orders a smaller character first");
    Require(DoCompare(facet, u"abd", u"abc") == 1, "do_compare orders a larger character last");
    Require(DoCompare(facet, u"abc", u"abc") == 0, "do_compare reports equal ranges");
    Require(DoCompare(facet, u"ab", u"abc") == -1, "do_compare orders a prefix first");
    Require(DoCompare(facet, u"", u"") == 0, "do_compare reports two empty ranges equal");
    Require(DoCompare(facet, u"Ā", u"\u0001") == 1, "do_compare orders by the whole 16-bit character");
    Require(sceLibcInternalWideCollateDoCompare_nid_no_patch(facet, nullptr, nullptr, nullptr, nullptr) == 0, "do_compare accepts two null ranges");
    Require(Compare(facet, u"abc", u"abd") == -1 && Compare(facet, u"Ā", u"\u0001") == 1 && Compare(facet, u"abc", u"abc") == 0, "compare returns do_compare for the classic facet");

    Require(DoHash(facet, u"") == 0xcbf29ce484222325ull, "do_hash of the empty range is the offset basis");
    Require(DoHash(facet, u"foobar") == 0x85944171f73967e8ull, "do_hash hashes one round per character");
    Require(DoHash(facet, u"Ā") == 0xaf62bd4c860004dfull, "do_hash hashes a character above 0xff as one value");
    Require(Hash(facet, u"foobar") == DoHash(facet, u"foobar"), "hash returns do_hash for the classic facet");

    GuestLocale::String result;
    std::memset(&result, 0xcd, sizeof(result));
    const std::u16string text = u"aĀc";
    Require(sceLibcInternalWideCollateDoTransform_nid_no_patch(&result, facet, text.data(), text.data() + text.size()) == &result, "do_transform returns its result");
    Require(result.size == 3 && result.capacity == 7 && Units(result) == text, "do_transform copies the characters in C collation");
    std::memset(&result, 0xcd, sizeof(result));
    Require(sceLibcInternalWideCollateTransform_nid_no_patch(&result, facet, text.data(), text.data() + text.size()) == &result, "transform returns its result");
    Require(result.size == 3 && result.capacity == 7 && Units(result) == text, "transform returns do_transform for the classic facet");

    const std::u16string longText(9, u'☃');
    Require(sceLibcInternalWideCollateTransform_nid_no_patch(&result, facet, longText.data(), longText.data() + longText.size()) == &result, "transform returns a long result");
    Require(result.size == 9 && result.capacity == 15 && Units(result) == longText, "transform allocates storage for a long range");
    std::free(result.pointer);

    const std::u16string embedded(u"a\0b", 3);
    Require(Throws([&] { sceLibcInternalWideCollateDoTransform_nid_no_patch(&result, facet, embedded.data(), embedded.data() + embedded.size()); }), "do_transform rejects embedded null characters");
    Require(Throws([&] { sceLibcInternalWideCollateDoCompare_nid_no_patch(facet, text.data() + 2, text.data(), text.data(), text.data()); }), "do_compare rejects a reversed range");
    Require(Throws([&] { sceLibcInternalWideCollateDoHash_nid_no_patch(facet, nullptr, text.data()); }), "do_hash rejects a half-null range");
}

void CheckVirtualDispatch(const GuestLocale::CollateFacet* classic) {
    GuestLocale::CollateVtable overriding = *reinterpret_cast<const GuestLocale::CollateVtable*>(classic->base.vtable);
    overriding.compare = OverrideCompare;
    overriding.transform = OverrideTransform;
    overriding.hash = OverrideHash;
    const GuestLocale::CollateFacet derived{{&overriding.facet, 0, 0}, nullptr, nullptr};
    const std::u16string left = u"abc";
    const std::u16string right = u"abd";

    Require(Compare(&derived, left, right) == 42 && recorded.calls == 1, "compare calls the facet's do_compare");
    Require(recorded.self == &derived && recorded.first1 == Bytes(left.data()) && recorded.last1 == Bytes(left.data() + 3), "compare passes the facet and first range");
    Require(recorded.first2 == Bytes(right.data()) && recorded.last2 == Bytes(right.data() + 3), "compare passes the second range");

    Require(Hash(&derived, left) == 0x1234 && recorded.calls == 2, "hash calls the facet's do_hash");
    Require(recorded.self == &derived && recorded.first1 == Bytes(left.data()) && recorded.last1 == Bytes(left.data() + 3), "hash passes the facet and range");

    GuestLocale::String result{};
    Require(sceLibcInternalWideCollateTransform_nid_no_patch(&result, &derived, left.data(), left.data() + 3) == &result && recorded.calls == 3, "transform calls the facet's do_transform");
    Require(recorded.result == &result && recorded.self == &derived && result.size == 99, "transform passes the result and facet");

    Require(DoCompare(&derived, left, right) == -1, "do_compare does not dispatch through the facet");
    Require(DoHash(&derived, u"foobar") == 0x85944171f73967e8ull, "do_hash does not dispatch through the facet");
    std::memset(&result, 0xcd, sizeof(result));
    Require(sceLibcInternalWideCollateDoTransform_nid_no_patch(&result, &derived, left.data(), left.data() + 3) == &result && Units(result) == left, "do_transform does not dispatch through the facet");
    Require(recorded.calls == 3, "the do_ members never reach the overriding facet");

    const GuestLocale::CollateFacet empty{{nullptr, 0, 0}, nullptr, nullptr};
    Require(Throws([&] { Compare(nullptr, left, right); }) && Throws([&] { Compare(&empty, left, right); }), "compare rejects a missing facet");
    Require(Throws([&] { Hash(nullptr, left); }) && Throws([&] { Hash(&empty, left); }), "hash rejects a missing facet");
    Require(Throws([&] { sceLibcInternalWideCollateTransform_nid_no_patch(&result, &empty, left.data(), left.data() + 3); }), "transform rejects a missing facet");
}

}

int main() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed), reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign), reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    GuestLocale::Facet* facet = nullptr;
    Require(_ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&facet, &_ZSt21_sceLibcClassicLocale_nid_postfix) == 1 && facet != nullptr, "the classic locale provides collate<wchar_t>");
    const auto* classic = reinterpret_cast<const GuestLocale::CollateFacet*>(facet);
    Require(classic->base.vtable == &GuestWideCollateVtable_nid_no_patch()->facet, "the classic facet uses the wide collate vtable");

    CheckClassicFacet(classic);
    CheckVirtualDispatch(classic);
    reinterpret_cast<const GuestLocale::CollateVtable*>(facet->vtable)->facet.deleteObject(facet);

    std::puts("libc internal collate<wchar_t> member checks passed");
    return 0;
}
