#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;

}

namespace {

std::size_t allocations = 0;
std::size_t frees = 0;
std::size_t lastSize = 0;
void* lastAllocation = nullptr;
void* lastFree = nullptr;

void require(bool condition) {
    if (!condition) std::abort();
}

template<typename TAction>
void reject(TAction action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected);
}

void* APS5_VABI allocate(std::size_t bytes) {
    ++allocations;
    lastSize = bytes;
    lastAllocation = std::malloc(bytes);
    return lastAllocation;
}

void APS5_VABI release(void* pointer) {
    ++frees;
    lastFree = pointer;
    std::free(pointer);
}

void* APS5_VABI allocateZeroed(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI reallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI align(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI realign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI posixAlign(void**, std::size_t, std::size_t) { std::abort(); }

const GuestLocale::CollateVtable& Vtable(const GuestLocale::Facet* facet) {
    return *reinterpret_cast<const GuestLocale::CollateVtable*>(facet->vtable);
}

int Compare(const GuestLocale::CollateFacet* facet, const std::string& left, const std::string& right) {
    return Vtable(&facet->base).compare(facet, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
}

std::uint64_t Hash(const GuestLocale::CollateFacet* facet, const std::string& text) {
    return static_cast<std::uint64_t>(Vtable(&facet->base).hash(facet, text.data(), text.data() + text.size()));
}

// collate<wchar_t> hands the vtable pointers to its 16-bit characters, so the tests below pass the
// characters of a wide string where the vtable declares a range of char and count it in characters.
const char* Characters(const char16_t* text) {
    return reinterpret_cast<const char*>(text);
}

int CompareWide(const GuestLocale::CollateFacet* facet, const std::u16string& left, const std::u16string& right) {
    return Vtable(&facet->base).compare(facet, Characters(left.data()), Characters(left.data() + left.size()), Characters(right.data()), Characters(right.data() + right.size()));
}

std::uint64_t HashWide(const GuestLocale::CollateFacet* facet, const std::u16string& text) {
    return static_cast<std::uint64_t>(Vtable(&facet->base).hash(facet, Characters(text.data()), Characters(text.data() + text.size())));
}

GuestLocale::String* TransformWide(const GuestLocale::CollateFacet* facet, GuestLocale::String* result, const std::u16string& text) {
    return Vtable(&facet->base).transform(result, facet, Characters(text.data()), Characters(text.data() + text.size()));
}

// The wide facet mirrors the narrow one in the facet it allocates and the C-locale validation, but it
// carries its own vtable, whose entries work on the guest's 16-bit characters.
void CheckWideGetcat(const GuestLocale::Implementation* const* classic, const GuestLocale::CollateFacet* narrow) {
    require(_ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(nullptr, classic) == 1);

    const std::size_t before = allocations;
    GuestLocale::Facet existing{};
    GuestLocale::Facet* facet = &existing;
    require(_ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&facet, classic) == 1);
    require(facet == &existing && allocations == before);

    GuestLocale::Implementation french = *_ZSt21_sceLibcClassicLocale_nid_postfix;
    french.name = "fr_FR";
    const GuestLocale::Implementation* frenchPointer = &french;
    GuestLocale::Implementation unnamed = *_ZSt21_sceLibcClassicLocale_nid_postfix;
    unnamed.name = nullptr;
    const GuestLocale::Implementation* unnamedPointer = &unnamed;
    const GuestLocale::Implementation* missingPointer = nullptr;
    for (const GuestLocale::Implementation* const* rejected : {&frenchPointer, &unnamedPointer, &missingPointer}) {
        GuestLocale::Facet* absent = nullptr;
        reject([&] { _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&absent, rejected); });
        require(absent == nullptr && allocations == before);
    }
    GuestLocale::Facet* absent = nullptr;
    reject([&] { _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&absent, nullptr); });
    require(absent == nullptr && allocations == before);

    facet = nullptr;
    require(_ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&facet, classic) == 1);
    require(facet != nullptr && allocations == before + 1 && lastSize == sizeof(GuestLocale::CollateFacet) && lastAllocation == facet);
    const auto* wide = reinterpret_cast<const GuestLocale::CollateFacet*>(facet);
    require(facet->references == 0 && facet->vtable != narrow->base.vtable);
    require(wide->collation == nullptr && wide->wideCollation == nullptr);

    const std::u16string text = u"abc";
    require(CompareWide(wide, text, u"abd") == -1);
    require(CompareWide(wide, u"abd", text) == 1);
    require(CompareWide(wide, text, text) == 0);
    require(CompareWide(wide, u"ab", text) == -1);
    require(CompareWide(wide, u"", text) == -1);
    require(CompareWide(wide, u"", u"") == 0);
    // Characters above 0xff order by their whole value: 0x0100 and 0x0001 compare the other way round
    // when the same characters are read as the little-endian bytes 0x00 0x01 and 0x01 0x00.
    require(CompareWide(wide, u"\u0100", u"\u0001") == 1);
    require(CompareWide(wide, u"\u0001", u"\u0100") == -1);
    require(CompareWide(wide, u"\u00ff", u"\u0100") == -1);
    require(CompareWide(wide, u"\u00e9", u"z") == 1);
    require(CompareWide(wide, u"a\u0101b", u"a\u0100b") == 1);
    require(CompareWide(wide, u"a\u0101b", u"a\u0102b") == -1);
    const std::u16string embeddedWide(u"a\0b", 3);
    require(CompareWide(wide, embeddedWide, u"a") == 1);
    require(CompareWide(wide, text, u"abd") == Compare(narrow, "abc", "abd"));

    require(HashWide(wide, u"") == 0xcbf29ce484222325ull);
    require(HashWide(wide, text) == 0xe71fa2190541574bull);
    require(HashWide(wide, u"a") == 0xaf63dc4c8601ec8cull);
    // One round per character, so a wide string of characters below 0x100 hashes like its narrow
    // spelling and a character above 0xff hashes like one value instead of the two bytes it holds.
    require(HashWide(wide, u"foobar") == 0x85944171f73967e8ull);
    require(HashWide(wide, u"foobar") == Hash(narrow, "foobar"));
    require(HashWide(wide, u"\u0100") == 0x0af62bd4c860004dfull);

    GuestLocale::String result;
    std::memset(&result, 0xcd, sizeof(result));
    require(TransformWide(wide, &result, text) == &result);
    require(result.reserved == 0xcdcdcdcdcdcdcdcdull && result.size == 3 && result.capacity == 7 && allocations == before + 1);
    const auto* inlineUnits = reinterpret_cast<const std::uint16_t*>(result.buffer);
    require(inlineUnits[0] == u'a' && inlineUnits[1] == u'b' && inlineUnits[2] == u'c' && inlineUnits[3] == 0);

    std::memset(&result, 0xcd, sizeof(result));
    require(Vtable(facet).transform(&result, wide, nullptr, nullptr) == &result);
    require(result.size == 0 && result.capacity == 7 && reinterpret_cast<const std::uint16_t*>(result.buffer)[0] == 0);

    const std::u16string seven(7, u'q');
    require(TransformWide(wide, &result, seven) == &result);
    require(result.size == 7 && result.capacity == 7 && allocations == before + 1);
    const auto* inlineSeven = reinterpret_cast<const std::uint16_t*>(result.buffer);
    require(inlineSeven[6] == u'q' && inlineSeven[7] == 0);

    const std::u16string eight(8, u'y');
    require(TransformWide(wide, &result, eight) == &result);
    require(allocations == before + 2 && lastSize == 32 && result.size == 8 && result.capacity == 15 && result.pointer == lastAllocation);
    require(reinterpret_cast<const std::uint16_t*>(result.pointer)[0] == u'y' && reinterpret_cast<const std::uint16_t*>(result.pointer)[8] == 0);
    std::free(result.pointer);

    for (const std::size_t length : {2039, 2040}) {
        const std::u16string large(length, u'\u0100');
        require(TransformWide(wide, &result, large) == &result);
        require(result.size == length && result.capacity == (length | 7));
        const auto* units = reinterpret_cast<const char16_t*>(result.pointer);
        require(std::u16string(units, result.size) == large && units[result.size] == 0);
        require(lastSize == (length == 2039 ? 4080 : 4096 + 0x27));
        if (length == 2040) {
            require(reinterpret_cast<std::uintptr_t>(units) % 0x20 == 0);
            require(reinterpret_cast<void* const*>(units)[-1] == lastAllocation);
        } else require(result.pointer == lastAllocation);
        std::free(lastAllocation);
    }

    const std::u16string embeddedText(u"a\0b", 3);
    reject([&] { TransformWide(wide, &result, embeddedText); });
    reject([&] { Vtable(facet).transform(nullptr, wide, Characters(seven.data()), Characters(seven.data() + seven.size())); });
    require(allocations == before + 4);
    reject([&] { Vtable(facet).compare(wide, Characters(text.data() + 2), Characters(text.data()), Characters(text.data()), Characters(text.data() + 1)); });
    reject([&] { Vtable(facet).compare(wide, nullptr, Characters(text.data() + 1), Characters(text.data()), Characters(text.data() + 1)); });
    reject([&] { Vtable(facet).hash(wide, Characters(text.data() + 1), Characters(text.data())); });

    Vtable(facet).facet.retain(facet);
    require(facet->references == 1);
    require(Vtable(facet).facet.release(facet) == facet && facet->references == 0);
    const std::size_t freed = frees;
    Vtable(facet).facet.destroy(facet);
    require(frees == freed);
    Vtable(facet).facet.deleteObject(facet);
    require(frees == freed + 1 && lastFree == wide);
    require(allocations == before + 4);
}

}

int main() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(&allocate), reinterpret_cast<void*>(&release), reinterpret_cast<void*>(&allocateZeroed), reinterpret_cast<void*>(&reallocate), reinterpret_cast<void*>(&align), reinterpret_cast<void*>(&realign), reinterpret_cast<void*>(&posixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    const GuestLocale::Implementation* const* classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    require(_ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(nullptr, classic) == 1);
    GuestLocale::Facet existing{};
    GuestLocale::Facet* facet = &existing;
    require(_ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&facet, classic) == 1);
    require(facet == &existing && allocations == 0);

    GuestLocale::Implementation french = *_ZSt21_sceLibcClassicLocale_nid_postfix;
    french.name = "fr_FR";
    const GuestLocale::Implementation* frenchPointer = &french;
    facet = nullptr;
    reject([&] { _ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&facet, &frenchPointer); });
    require(facet == nullptr && allocations == 0);

    require(_ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&facet, classic) == 1);
    require(facet != nullptr && allocations == 1 && lastSize == sizeof(GuestLocale::CollateFacet) && lastAllocation == facet);
    const auto* collate = reinterpret_cast<const GuestLocale::CollateFacet*>(facet);
    require(facet->references == 0 && collate->collation == nullptr && collate->wideCollation == nullptr);

    require(Compare(collate, "abc", "abd") == -1);
    require(Compare(collate, "abd", "abc") == 1);
    require(Compare(collate, "a", "c") == -1);
    require(Compare(collate, "abc", "abc") == 0);
    require(Compare(collate, "ab", "abc") == -1);
    require(Compare(collate, "abc", "ab") == 1);
    require(Compare(collate, "", "") == 0);
    require(Compare(collate, "", "a") == -1);
    require(Compare(collate, "\x80", "a") == 1);
    require(Compare(collate, std::string("a\0", 2), "a") == 1);
    require(Compare(collate, std::string("a\0b", 3), std::string("a\0c", 3)) == -1);
    require(Vtable(facet).compare(collate, nullptr, nullptr, nullptr, nullptr) == 0);
    const char text[] = "abc";
    reject([&] { Vtable(facet).compare(collate, text + 2, text, text, text + 1); });
    reject([&] { Vtable(facet).compare(collate, nullptr, text, text, text + 1); });

    require(Hash(collate, "") == 0xcbf29ce484222325ull);
    require(Hash(collate, "a") == 0xaf63dc4c8601ec8cull);
    require(Hash(collate, "foobar") == 0x85944171f73967e8ull);
    require(Hash(collate, "\xff") == ((0xcbf29ce484222325ull ^ 0xffull) * 0x100000001b3ull));
    reject([&] { Vtable(facet).hash(collate, text + 1, text); });

    GuestLocale::String result;
    std::memset(&result, 0xcd, sizeof(result));
    const std::string shortText = "hello";
    require(Vtable(facet).transform(&result, collate, shortText.data(), shortText.data() + shortText.size()) == &result);
    require(result.reserved == 0xcdcdcdcdcdcdcdcdull && result.size == 5 && result.capacity == 15 && std::strcmp(result.buffer, "hello") == 0);
    require(allocations == 1);

    std::memset(&result, 0xcd, sizeof(result));
    require(Vtable(facet).transform(&result, collate, nullptr, nullptr) == &result);
    require(result.size == 0 && result.capacity == 15 && result.buffer[0] == 0);

    const std::string fifteen(15, 'q');
    require(Vtable(facet).transform(&result, collate, fifteen.data(), fifteen.data() + fifteen.size()) == &result);
    require(result.size == 15 && result.capacity == 15 && std::string(result.buffer) == fifteen && allocations == 1);

    const std::string longText = "collation of a longer text";
    require(Vtable(facet).transform(&result, collate, longText.data(), longText.data() + longText.size()) == &result);
    require(allocations == 2 && lastSize == 32 && result.pointer == lastAllocation);
    require(result.size == longText.size() && result.capacity == 31 && std::string(result.pointer) == longText);
    std::free(result.pointer);

    const std::string largestSmall(4079, 'z');
    require(Vtable(facet).transform(&result, collate, largestSmall.data(), largestSmall.data() + largestSmall.size()) == &result);
    require(allocations == 3 && lastSize == 4080 && result.pointer == lastAllocation && result.capacity == 4079 && std::string(result.pointer) == largestSmall);
    std::free(result.pointer);

    const std::string smallestLarge(4080, 'y');
    require(Vtable(facet).transform(&result, collate, smallestLarge.data(), smallestLarge.data() + smallestLarge.size()) == &result);
    const auto raw = reinterpret_cast<std::uintptr_t>(lastAllocation);
    const auto aligned = reinterpret_cast<std::uintptr_t>(result.pointer);
    require(allocations == 4 && lastSize == 4096 + 0x27);
    require(aligned % 0x20 == 0 && aligned >= raw + 8 && aligned <= raw + 0x27 && reinterpret_cast<void**>(result.pointer)[-1] == lastAllocation);
    require(result.size == 4080 && result.capacity == 4095 && std::string(result.pointer) == smallestLarge);
    std::free(lastAllocation);

    const std::string embedded("a\0b", 3);
    reject([&] { Vtable(facet).transform(&result, collate, embedded.data(), embedded.data() + embedded.size()); });
    reject([&] { Vtable(facet).transform(nullptr, collate, text, text + 1); });
    require(allocations == 4);

    Vtable(facet).facet.retain(facet);
    Vtable(facet).facet.retain(facet);
    require(facet->references == 2);
    require(Vtable(facet).facet.release(facet) == nullptr && facet->references == 1);
    require(Vtable(facet).facet.release(facet) == facet && facet->references == 0);
    CheckWideGetcat(classic, collate);

    Vtable(facet).facet.destroy(facet);
    require(frees == 1);
    Vtable(facet).facet.deleteObject(facet);
    require(frees == 2 && lastFree == collate);
    require(allocations == 8);
    return 0;
}
