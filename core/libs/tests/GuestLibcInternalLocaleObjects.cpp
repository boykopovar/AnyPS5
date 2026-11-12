#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal locale objects: %s\n", message);
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

void CheckLocinfo() {
    alignas(16) std::array<std::byte, 80> bytes;
    bytes.fill(std::byte{0x5a});
    void* storage = bytes.data() + 8;
    sceLibcInternalLocinfoBaseConstructor_nid_no_patch(storage, "C");
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        Require(bytes[index] == (index >= 8 && index < 72 ? std::byte{} : std::byte{0x5a}), "the C2 constructor clears only the 64-byte object");
    }
    sceLibcInternalLocinfoBaseDestructor_nid_no_patch(storage);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        Require(bytes[index] == (index >= 8 && index < 72 ? std::byte{} : std::byte{0x5a}), "the D2 destructor leaves the storage untouched");
    }

    Require(Throws([&] { sceLibcInternalLocinfoBaseConstructor_nid_no_patch(storage, "unknown-locale"); }), "an unsupported locale name is rejected");
    Require(Throws([] { sceLibcInternalLocinfoBaseConstructor_nid_no_patch(nullptr, "C"); }), "a null object is rejected");
    Require(Throws([] { sceLibcInternalLocinfoBaseDestructor_nid_no_patch(nullptr); }), "destroying a null object is rejected");
}

void CheckLocimpCopy() {
    std::array<GuestLocale::Facet, 2> facets{};
    std::array<GuestLocale::Facet*, 3> slots{&facets[0], nullptr, &facets[1]};
    GuestLocale::Implementation source{};
    source.base.references = 7;
    source.facets = slots.data();
    source.facetCount = slots.size();
    source.category = 0x3f;
    source.transparent = true;
    source.name = "C";

    GuestLocale::Implementation expected{};
    GuestLocale::Implementation actual{};
    _ZNSt6locale7_LocimpC1ERKS0__nid_postfix(&expected, &source);
    sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(&actual, &source);

    Require(actual.base.vtable == expected.base.vtable && actual.base.vtable != nullptr, "the copy gets the same vtable as the C1 copy");
    Require(actual.base.references == 1, "the copy starts with one reference");
    Require(actual.facetCount == source.facetCount && actual.category == source.category, "the copy keeps the facet count and categories");
    Require(actual.transparent == source.transparent && actual.name == source.name, "the copy keeps the name and transparency");
    Require(actual.facets != nullptr && actual.facets != source.facets, "the copy owns a new facet vector");
    Require(std::memcmp(actual.facets, slots.data(), sizeof(slots)) == 0, "the copy shares the source facets");
    Require(source.base.references == 7 && source.facets == slots.data(), "the source is unchanged");

    GuestLocale::Implementation empty{};
    empty.name = "C";
    GuestLocale::Implementation emptyCopy{};
    sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(&emptyCopy, &empty);
    Require(emptyCopy.facetCount == 0 && emptyCopy.facets == nullptr, "a locale without facets copies no vector");

    Require(Throws([&] { sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(nullptr, &source); }), "a null object is rejected");
    Require(Throws([&] { sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(&emptyCopy, nullptr); }), "a null source is rejected");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckLocinfo();
    CheckLocimpCopy();

    std::puts("libc internal _Locinfo and _Locimp base-object checks passed");
    return 0;
}
