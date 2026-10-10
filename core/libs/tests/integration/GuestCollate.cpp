#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
std::size_t APS5_VABI _ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

std::size_t allocations = 0;
std::size_t frees = 0;
std::size_t lastSize = 0;
void* lastAllocation = nullptr;
void* lastFree = nullptr;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++allocations;
    lastSize = bytes;
    lastAllocation = std::malloc(bytes);
    return lastAllocation;
}

void APS5_VABI Release(void* pointer) {
    ++frees;
    lastFree = pointer;
    std::free(pointer);
}

void* APS5_VABI AllocateZeroed(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI Reallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI Align(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

const GuestLocale::Implementation* const* Classic() {
    return &_ZSt21_sceLibcClassicLocale_nid_postfix;
}

std::size_t Getcat(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    return _ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(facet, locale);
}

class HeapFixture {
public:
    HeapFixture() {
        const std::array<void*, 10> api{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release),
            reinterpret_cast<void*>(&AllocateZeroed), reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align),
            reinterpret_cast<void*>(&Realign), reinterpret_cast<void*>(&PosixAlign)};
        ApplicationHeapRegister_nid_no_patch(api.data());
        allocations = 0;
        frees = 0;
        lastSize = 0;
        lastAllocation = nullptr;
        lastFree = nullptr;
    }

    HeapFixture(const HeapFixture&) = delete;
    HeapFixture& operator=(const HeapFixture&) = delete;
};

const GuestLocale::CollateVtable& Vtable(const GuestLocale::Facet* facet) {
    return *reinterpret_cast<const GuestLocale::CollateVtable*>(facet->vtable);
}

class CollateFixture : public HeapFixture {
public:
    CollateFixture() {
        Require(Getcat(&facet, Classic()) == 1, "create the classic collate facet");
        Require(facet != nullptr, "collate facet allocated");
        collate = reinterpret_cast<const GuestLocale::CollateFacet*>(facet);
    }

    ~CollateFixture() {
        if (facet != nullptr) Vtable(facet).facet.deleteObject(facet);
    }

    CollateFixture(const CollateFixture&) = delete;
    CollateFixture& operator=(const CollateFixture&) = delete;

    const GuestLocale::CollateVtable& Functions() const { return Vtable(facet); }

    int Compare(const std::string& left, const std::string& right) const {
        return Functions().compare(collate, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
    }

    std::uint64_t Hash(const std::string& text) const {
        return static_cast<std::uint64_t>(Functions().hash(collate, text.data(), text.data() + text.size()));
    }

    GuestLocale::String* Transform(GuestLocale::String* result, const std::string& text) const {
        return Functions().transform(result, collate, text.data(), text.data() + text.size());
    }

    GuestLocale::Facet* facet = nullptr;
    const GuestLocale::CollateFacet* collate = nullptr;
};

struct FreeDeleter {
    void operator()(char* pointer) const { std::free(pointer); }
};

using HeapString = std::unique_ptr<char, FreeDeleter>;

GuestLocale::String GarbageString() {
    GuestLocale::String result;
    std::memset(&result, 0xcd, sizeof(result));
    return result;
}

template<typename TAction>
void RequireRejected(TAction action, const char* message) {
    try {
        action();
    } catch (const Testing::Failure&) {
        throw;
    } catch (const std::exception&) {
        return;
    }
    Testing::Fail(std::string(message) + ": did not throw");
}

const Case getcatWithoutFacet{"CollateGetcat_NullFacetPointer_ReturnsCollateCategory", [] {
    const HeapFixture heap;
    RequireEqual(Getcat(nullptr, Classic()), std::size_t{1}, "category");
}};

const Case getcatExisting{"CollateGetcat_ExistingFacet_KeepsFacetWithoutAllocating", [] {
    const HeapFixture heap;
    GuestLocale::Facet existing{};
    GuestLocale::Facet* facet = &existing;
    RequireEqual(Getcat(&facet, Classic()), std::size_t{1}, "category");
    RequireEqual(facet == &existing, true, "facet kept");
    RequireEqual(allocations, std::size_t{0}, "allocations");
}};

const Case getcatUnsupported{"CollateGetcat_UnsupportedLocale_ThrowsWithoutAllocating", [] {
    const HeapFixture heap;
    GuestLocale::Implementation french = *_ZSt21_sceLibcClassicLocale_nid_postfix;
    french.name = "fr_FR";
    const GuestLocale::Implementation* frenchPointer = &french;
    GuestLocale::Facet* facet = nullptr;
    RequireRejected([&] { Getcat(&facet, &frenchPointer); }, "fr_FR collate");
    RequireEqual(facet == nullptr, true, "no facet created");
    RequireEqual(allocations, std::size_t{0}, "allocations");
}};

const Case getcatCreates{"CollateGetcat_ClassicLocale_AllocatesUnreferencedFacet", [] {
    const CollateFixture fixture;
    RequireEqual(allocations, std::size_t{1}, "allocations");
    RequireEqual(lastSize, sizeof(GuestLocale::CollateFacet), "allocation size");
    RequireEqual(lastAllocation == fixture.facet, true, "facet storage from the guest heap");
    RequireEqual(fixture.facet->references, std::uint32_t{0}, "references");
    RequireEqual(fixture.collate->collation == nullptr, true, "collation");
    RequireEqual(fixture.collate->wideCollation == nullptr, true, "wide collation");
}};

const Case compare{"CollateCompare_ByteStrings_OrdersLexicographically", [] {
    const CollateFixture fixture;
    struct Example {
        std::string left;
        std::string right;
        int expected;
    };
    const Example examples[] = {
        {"abc", "abd", -1}, {"abd", "abc", 1}, {"a", "c", -1}, {"abc", "abc", 0}, {"ab", "abc", -1}, {"abc", "ab", 1},
        {"", "", 0}, {"", "a", -1}, {"\x80", "a", 1}, {std::string("a\0", 2), "a", 1},
        {std::string("a\0b", 3), std::string("a\0c", 3), -1},
    };
    for (const auto& example : examples) {
        RequireEqual(fixture.Compare(example.left, example.right), example.expected,
            "compare(\"" + example.left + "\", \"" + example.right + "\")");
    }
}};

const Case compareNull{"CollateCompare_EmptyNullRanges_ReturnsEqual", [] {
    const CollateFixture fixture;
    RequireEqual(fixture.Functions().compare(fixture.collate, nullptr, nullptr, nullptr, nullptr), 0, "result");
}};

const Case compareInvalid{"CollateCompare_InvalidRanges_Throws", [] {
    const CollateFixture fixture;
    const char text[] = "abc";
    RequireRejected([&] { fixture.Functions().compare(fixture.collate, text + 2, text, text, text + 1); }, "reversed range");
    RequireRejected([&] { fixture.Functions().compare(fixture.collate, nullptr, text, text, text + 1); }, "null start");
}};

const Case hash{"CollateHash_ByteStrings_UsesFnv1a", [] {
    const CollateFixture fixture;
    RequireEqual(fixture.Hash(""), std::uint64_t{0xcbf29ce484222325ull}, "empty");
    RequireEqual(fixture.Hash("a"), std::uint64_t{0xaf63dc4c8601ec8cull}, "a");
    RequireEqual(fixture.Hash("foobar"), std::uint64_t{0x85944171f73967e8ull}, "foobar");
    RequireEqual(fixture.Hash("\xff"), std::uint64_t{(0xcbf29ce484222325ull ^ 0xffull) * 0x100000001b3ull}, "0xff");
}};

const Case hashInvalid{"CollateHash_ReversedRange_Throws", [] {
    const CollateFixture fixture;
    const char text[] = "abc";
    RequireRejected([&] { fixture.Functions().hash(fixture.collate, text + 1, text); }, "reversed range");
}};

const Case transformShort{"CollateTransform_ShortText_UsesInlineBuffer", [] {
    const CollateFixture fixture;
    auto result = GarbageString();
    RequireEqual(fixture.Transform(&result, "hello") == &result, true, "returns result");
    RequireEqual(result.reserved, std::uint64_t{0xcdcdcdcdcdcdcdcdull}, "reserved untouched");
    RequireEqual(result.size, std::size_t{5}, "size");
    RequireEqual(result.capacity, std::size_t{15}, "capacity");
    RequireEqual(std::string_view(result.buffer), std::string_view("hello"), "text");
    RequireEqual(allocations, std::size_t{1}, "no allocation beyond the facet");
}};

const Case transformEmpty{"CollateTransform_EmptyNullRange_ProducesEmptyInlineString", [] {
    const CollateFixture fixture;
    auto result = GarbageString();
    RequireEqual(fixture.Functions().transform(&result, fixture.collate, nullptr, nullptr) == &result, true, "returns result");
    RequireEqual(result.size, std::size_t{0}, "size");
    RequireEqual(result.capacity, std::size_t{15}, "capacity");
    RequireEqual(result.buffer[0], '\0', "terminator");
}};

const Case transformFifteen{"CollateTransform_FifteenCharacters_StaysInline", [] {
    const CollateFixture fixture;
    auto result = GarbageString();
    const std::string fifteen(15, 'q');
    RequireEqual(fixture.Transform(&result, fifteen) == &result, true, "returns result");
    RequireEqual(result.size, std::size_t{15}, "size");
    RequireEqual(result.capacity, std::size_t{15}, "capacity");
    RequireEqual(std::string(result.buffer), fifteen, "text");
    RequireEqual(allocations, std::size_t{1}, "no allocation beyond the facet");
}};

const Case transformLong{"CollateTransform_LongText_AllocatesFromGuestHeap", [] {
    const CollateFixture fixture;
    auto result = GarbageString();
    const std::string longText = "collation of a longer text";
    RequireEqual(fixture.Transform(&result, longText) == &result, true, "returns result");
    const HeapString owned(result.pointer);
    RequireEqual(allocations, std::size_t{2}, "allocations");
    RequireEqual(lastSize, longText.size() + 1, "allocation size");
    RequireEqual(result.pointer == lastAllocation, true, "text storage from the guest heap");
    RequireEqual(result.size, longText.size(), "size");
    RequireEqual(result.capacity, longText.size(), "capacity");
    RequireEqual(std::string(result.pointer), longText, "text");
}};

const Case transformLargest{"CollateTransform_LargestSupportedText_AllocatesExactCapacity", [] {
    const CollateFixture fixture;
    auto result = GarbageString();
    const std::string largest(4094, 'z');
    RequireEqual(fixture.Transform(&result, largest) == &result, true, "returns result");
    const HeapString owned(result.pointer);
    RequireEqual(allocations, std::size_t{2}, "allocations");
    RequireEqual(lastSize, std::size_t{4095}, "allocation size");
    RequireEqual(result.capacity, std::size_t{4094}, "capacity");
    RequireEqual(std::string(result.pointer), largest, "text");
}};

const Case transformInvalid{"CollateTransform_InvalidInput_ThrowsWithoutAllocating", [] {
    const CollateFixture fixture;
    auto result = GarbageString();
    const char text[] = "abc";
    const std::string tooLong(4095, 'z');
    RequireRejected([&] { fixture.Transform(&result, tooLong); }, "4095 characters");
    const std::string embedded("a\0b", 3);
    RequireRejected([&] { fixture.Transform(&result, embedded); }, "embedded nul");
    RequireRejected([&] { fixture.Functions().transform(nullptr, fixture.collate, text, text + 1); }, "null result");
    RequireEqual(allocations, std::size_t{1}, "no allocation beyond the facet");
}};

const Case references{"CollateFacet_RetainAndRelease_TrackReferences", [] {
    const CollateFixture fixture;
    auto* facet = fixture.facet;
    fixture.Functions().facet.retain(facet);
    fixture.Functions().facet.retain(facet);
    RequireEqual(facet->references, std::uint32_t{2}, "after two retains");
    RequireEqual(fixture.Functions().facet.release(facet) == nullptr, true, "first release keeps the facet");
    RequireEqual(facet->references, std::uint32_t{1}, "after first release");
    RequireEqual(fixture.Functions().facet.release(facet) == facet, true, "last release returns the facet");
    RequireEqual(facet->references, std::uint32_t{0}, "after last release");
}};

const Case deletion{"CollateFacet_DeleteObject_FreesThroughGuestHeap", [] {
    CollateFixture fixture;
    auto* facet = fixture.facet;
    const auto& functions = fixture.Functions();
    functions.facet.destroy(facet);
    RequireEqual(frees, std::size_t{0}, "destroy does not free");
    fixture.facet = nullptr;
    functions.facet.deleteObject(facet);
    RequireEqual(frees, std::size_t{1}, "delete frees once");
    RequireEqual(lastFree == fixture.collate, true, "freed the facet");
}};

} // namespace
