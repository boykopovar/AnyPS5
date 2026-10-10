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
std::size_t APS5_VABI _ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale);
std::size_t APS5_VABI _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale);
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

const GuestLocale::WideCollateVtable& WideVtable(const GuestLocale::Facet* facet) {
    return *reinterpret_cast<const GuestLocale::WideCollateVtable*>(facet->vtable);
}

int Compare(const GuestLocale::CollateFacet* facet, const std::string& left, const std::string& right) {
    return Vtable(&facet->base).compare(facet, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
}

std::uint64_t Hash(const GuestLocale::CollateFacet* facet, const std::string& text) {
    return static_cast<std::uint64_t>(Vtable(&facet->base).hash(facet, text.data(), text.data() + text.size()));
}

using Wide = std::basic_string<char16_t>;

int Compare(const GuestLocale::CollateFacet* facet, const Wide& left, const Wide& right) {
    return WideVtable(&facet->base).compare(facet, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
}

std::uint64_t Hash(const GuestLocale::CollateFacet* facet, const Wide& text) {
    return static_cast<std::uint64_t>(WideVtable(&facet->base).hash(facet, text.data(), text.data() + text.size()));
}

const char16_t* Stored(const GuestLocale::String& text) {
    return reinterpret_cast<const char16_t*>(text.capacity == 14 ? text.buffer : text.pointer);
}

GuestLocale::String Transformed(const GuestLocale::CollateFacet* facet, const Wide& text) {
    GuestLocale::String result;
    std::memset(&result, 0xcd, sizeof(result));
    require(WideVtable(&facet->base).transform(&result, facet, text.data(), text.data() + text.size()) == &result);
    return result;
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
    require(allocations == 2 && lastSize == longText.size() + 1 && result.pointer == lastAllocation);
    require(result.size == longText.size() && result.capacity == longText.size() && std::string(result.pointer) == longText);
    std::free(result.pointer);

    const std::string largest(4094, 'z');
    require(Vtable(facet).transform(&result, collate, largest.data(), largest.data() + largest.size()) == &result);
    require(allocations == 3 && lastSize == 4095 && result.capacity == 4094 && std::string(result.pointer) == largest);
    std::free(result.pointer);

    const std::string tooLong(4095, 'z');
    reject([&] { Vtable(facet).transform(&result, collate, tooLong.data(), tooLong.data() + tooLong.size()); });
    const std::string embedded("a\0b", 3);
    reject([&] { Vtable(facet).transform(&result, collate, embedded.data(), embedded.data() + embedded.size()); });
    reject([&] { Vtable(facet).transform(nullptr, collate, text, text + 1); });
    require(allocations == 3);

    Vtable(facet).facet.retain(facet);
    Vtable(facet).facet.retain(facet);
    require(facet->references == 2);
    require(Vtable(facet).facet.release(facet) == nullptr && facet->references == 1);
    require(Vtable(facet).facet.release(facet) == facet && facet->references == 0);
    Vtable(facet).facet.destroy(facet);
    require(frees == 0);
    Vtable(facet).facet.deleteObject(facet);
    require(frees == 1 && lastFree == collate);

    require(_ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(nullptr, classic) == 1);
    GuestLocale::Facet cached{};
    GuestLocale::Facet* wideFacet = &cached;
    require(_ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&wideFacet, classic) == 1);
    require(wideFacet == &cached && allocations == 3);

    const GuestLocale::Implementation* const* named = &frenchPointer;
    GuestLocale::Facet* rejected = nullptr;
    reject([&] { _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&rejected, named); });
    reject([&] { _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&rejected, nullptr); });
    require(rejected == nullptr && allocations == 3);

    wideFacet = nullptr;
    require(_ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&wideFacet, classic) == 1);
    require(wideFacet != &cached && allocations == 4 && lastSize == sizeof(GuestLocale::CollateFacet) && lastAllocation == wideFacet);
    const auto* wideCollate = reinterpret_cast<const GuestLocale::CollateFacet*>(wideFacet);
    require(wideCollate->collation == nullptr && wideCollate->wideCollation == nullptr);
    require(WideVtable(wideFacet).facet.destroy == Vtable(facet).facet.destroy);
    require(reinterpret_cast<const void*>(WideVtable(wideFacet).compare) != reinterpret_cast<const void*>(Vtable(facet).compare));

    require(Compare(wideCollate, u"abc", u"abd") == -1);
    require(Compare(wideCollate, u"abd", u"abc") == 1);
    require(Compare(wideCollate, u"abc", u"abc") == 0);
    require(Compare(wideCollate, u"ab", u"abc") == -1);
    require(Compare(wideCollate, u"abc", u"ab") == 1);
    require(Compare(wideCollate, u"", u"") == 0);
    require(Compare(wideCollate, u"", u"a") == -1);
    require(Compare(wideCollate, Wide(1, u'\x0100'), u"\xfffe") == -1);
    require(Compare(wideCollate, u"\x00ff", u"\x0100") == -1);
    require(Compare(wideCollate, Wide(1, u'\x0100'), u"\x00ff") == 1);
    require(Compare(wideCollate, Wide(1, u'\x0100'), Wide(1, u'\x0101')) == -1);
    require(Compare(wideCollate, Wide(1, u'\0'), u"") == 1);
    require(Compare(wideCollate, Wide(1, u'\0'), Wide(1, u'\0')) == 0);
    require(WideVtable(wideFacet).compare(wideCollate, nullptr, nullptr, nullptr, nullptr) == 0);
    const Wide wideText = u"abc";
    reject([&] { WideVtable(wideFacet).compare(wideCollate, wideText.data() + 2, wideText.data(), wideText.data(), wideText.data() + 1); });
    reject([&] { WideVtable(wideFacet).compare(wideCollate, nullptr, wideText.data(), wideText.data(), wideText.data() + 1); });

std::uint64_t folded = 0xcbf29ce484222325ull;
    require(Hash(wideCollate, u"") == folded);
    for (const char letter : std::string("foobar")) {
        folded = (folded ^ static_cast<unsigned char>(letter)) * 0x100000001b3ull;
        folded = (folded ^ 0x00ull) * 0x100000001b3ull;
    }
    require(Hash(wideCollate, u"foobar") == folded);
    require(Hash(wideCollate, u"a") != Hash(wideCollate, u"b"));
    std::uint64_t lowByteFirst = 0xcbf29ce484222325ull;
    lowByteFirst = (lowByteFirst ^ 0xffull) * 0x100000001b3ull;
    lowByteFirst = (lowByteFirst ^ 0x00ull) * 0x100000001b3ull;
    require(Hash(wideCollate, Wide(1, u'\x00ff')) == lowByteFirst);
    std::uint64_t highByteFirst = 0xcbf29ce484222325ull;
    highByteFirst = (highByteFirst ^ 0x00ull) * 0x100000001b3ull;
    highByteFirst = (highByteFirst ^ 0xffull) * 0x100000001b3ull;
    require(Hash(wideCollate, Wide(1, u'\xff00')) == highByteFirst);
    require(Hash(wideCollate, u"ab") == Hash(wideCollate, u"ab"));
    reject([&] { WideVtable(wideFacet).hash(wideCollate, wideText.data() + 1, wideText.data()); });

    const GuestLocale::String shortWide = Transformed(wideCollate, u"hello");
    require(shortWide.size == 5 && shortWide.capacity == 14 && shortWide.reserved == 0xcdcdcdcdcdcdcdcdull);
    require(Wide(Stored(shortWide), shortWide.size) == u"hello" && allocations == 4);

    const GuestLocale::String emptyWide = Transformed(wideCollate, u"");
    require(emptyWide.size == 0 && emptyWide.capacity == 14 && Stored(emptyWide)[0] == 0 && allocations == 4);

    const Wide seven(7, u'q');
    const GuestLocale::String inlineWide = Transformed(wideCollate, seven);
    require(inlineWide.size == 7 && inlineWide.capacity == 14 && Wide(Stored(inlineWide), 7) == seven && allocations == 4);

    const Wide eight(8, u'q');
    const GuestLocale::String heapWide = Transformed(wideCollate, eight);
    require(heapWide.size == 8 && heapWide.capacity == 16 && Wide(Stored(heapWide), 8) == eight);
    require(allocations == 5 && lastSize == 18 && reinterpret_cast<const char16_t*>(lastAllocation)[8] == 0);
    std::free(heapWide.pointer);

    const Wide longest(2047, u'z');
    const Wide embeddedWide = Wide(u"a\0b", 3);
    GuestLocale::String scratch;
    reject([&] { WideVtable(wideFacet).transform(&scratch, wideCollate, longest.data(), longest.data() + longest.size()); });
    reject([&] { WideVtable(wideFacet).transform(&scratch, wideCollate, embeddedWide.data(), embeddedWide.data() + embeddedWide.size()); });
    reject([&] { WideVtable(wideFacet).transform(nullptr, wideCollate, wideText.data(), wideText.data() + 1); });
    require(allocations == 5);

    WideVtable(wideFacet).facet.deleteObject(wideFacet);
    require(frees == 2 && lastFree == wideCollate);
    return 0;
}
