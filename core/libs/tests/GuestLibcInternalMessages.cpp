#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/PendingExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

extern "C" {
extern std::uintptr_t _ZTVSt8messagesIcE_nid_postfix[9];
}

namespace {

using GuestLocale::Facet;
using GuestLocale::MessagesVtable;
using GuestLocale::String;

long live = 0;

void* APS5_VABI TestAllocate(std::size_t size) {
    ++live;
    return std::malloc(size);
}
void APS5_VABI TestFree(void* pointer) {
    if (pointer != nullptr) --live;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    ++live;
    return std::calloc(count, size);
}
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) {
    if (pointer == nullptr) ++live;
    return std::realloc(pointer, size);
}
void* APS5_VABI TestAlign(std::size_t, std::size_t size) {
    ++live;
    return std::malloc(size);
}
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) {
    if (pointer == nullptr) ++live;
    return std::realloc(pointer, size);
}
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    ++live;
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "LibcInternal messages: %s\n", message);
    std::abort();
}

template<typename Action>
bool Throws(Action action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

using GetExport = void* (APS5_VABI *)(void* result, const void* self, int catalog, int set, int message, const void* fallback);
using OpenExport = int (APS5_VABI *)(const void* self, const void* name, const void* locale);
using CloseExport = void (APS5_VABI *)(const void* self, int catalog);

struct Calls {
    int opens;
    int gets;
    int closes;
    const Facet* self;
    const String* name;
    const void* locale;
    String* result;
    const String* fallback;
    int catalog;
    int set;
    int message;
};

Calls calls{};

std::int32_t APS5_VABI RecordOpen(const Facet* self, const String* name, const void* locale) {
    ++calls.opens;
    calls.self = self;
    calls.name = name;
    calls.locale = locale;
    return 41;
}

String* APS5_VABI RecordGet(String* result, const Facet* self, std::int32_t catalog, std::int32_t set, std::int32_t message, const String* fallback) {
    ++calls.gets;
    calls.result = result;
    calls.self = self;
    calls.catalog = catalog;
    calls.set = set;
    calls.message = message;
    calls.fallback = fallback;
    return result;
}

void APS5_VABI RecordClose(const Facet* self, std::int32_t catalog) {
    ++calls.closes;
    calls.self = self;
    calls.catalog = catalog;
}

void CheckWrappersCallTheVirtuals() {
    struct Wrappers {
        GetExport get;
        OpenExport open;
        CloseExport close;
    };
    const std::array<Wrappers, 2> wrappers{{
        {_ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix, _ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix, sceLibcInternalPending4_005_nid_no_patch},
        {sceLibcInternalPending4_009_nid_no_patch, sceLibcInternalPending4_010_nid_no_patch, sceLibcInternalPending4_011_nid_no_patch},
    }};
    const MessagesVtable vtable{{nullptr, nullptr, nullptr, nullptr}, RecordOpen, RecordGet, RecordClose};
    const Facet facet{&vtable.facet, 0, 0};
    const MessagesVtable empty{};
    const Facet hollow{&empty.facet, 0, 0};
    const Facet bare{nullptr, 0, 0};
    String result{};
    const String fallback{};
    const String name{};
    const int locale = 0;
    for (const Wrappers& wrapper : wrappers) {
        calls = {};
        Require(wrapper.get(&result, &facet, 3, -4, INT_MAX, &fallback) == &result, "get returns what do_get returns");
        Require(calls.gets == 1 && calls.result == &result && calls.self == &facet, "get passes the result and the facet to do_get");
        Require(calls.catalog == 3 && calls.set == -4 && calls.message == INT_MAX && calls.fallback == &fallback, "get passes its arguments to do_get");
        Require(wrapper.open(&facet, &name, &locale) == 41, "open returns what do_open returns");
        Require(calls.opens == 1 && calls.self == &facet && calls.name == &name && calls.locale == &locale, "open passes its arguments to do_open");
        wrapper.close(&facet, -9);
        Require(calls.closes == 1 && calls.self == &facet && calls.catalog == -9, "close passes its arguments to do_close");
        Require(calls.gets == 1 && calls.opens == 1 && calls.closes == 1, "each wrapper calls only its own virtual");
        Require(Throws([&] { wrapper.get(&result, nullptr, 0, 0, 0, &fallback); }), "a null facet throws");
        Require(Throws([&] { wrapper.open(&bare, &name, &locale); }), "a facet without a vtable throws");
        Require(Throws([&] { wrapper.close(&hollow, 0); }), "an empty vtable slot throws");
        Require(calls.gets == 1 && calls.opens == 1 && calls.closes == 1, "a refused call reaches no virtual");
    }
}

struct Layout {
    std::size_t unit;
    std::size_t inlineCapacity;
    GetExport doGet;
};

String MakeInline(const Layout& layout, std::size_t size, unsigned char seed) {
    String string{};
    for (std::size_t index = 0; index < sizeof(string.buffer); ++index) string.buffer[index] = static_cast<char>(seed + index);
    std::memset(string.buffer + size * layout.unit, 0, layout.unit);
    string.size = size;
    string.capacity = layout.inlineCapacity;
    return string;
}

String MakeHeap(const Layout& layout, std::size_t size, std::size_t capacity, unsigned char seed) {
    String string{};
    const std::size_t bytes = (capacity + 1) * layout.unit;
    string.pointer = static_cast<char*>(GuestStringStorage::Allocate(bytes));
    for (std::size_t index = 0; index < bytes; ++index) string.pointer[index] = static_cast<char>(seed + index * 7);
    std::memset(string.pointer + size * layout.unit, 0, layout.unit);
    string.size = size;
    string.capacity = capacity;
    return string;
}

const char* Data(const Layout& layout, const String& string) {
    return string.capacity > layout.inlineCapacity ? string.pointer : string.buffer;
}

void Release(const Layout& layout, String& string) {
    if (string.capacity > layout.inlineCapacity) GuestStringStorage::Deallocate(string.pointer, (string.capacity + 1) * layout.unit);
    string = {};
}

String CopyOf(const Layout& layout, const String& source) {
    const std::array<std::array<int, 3>, 2> selectors{{{-1, 0, 0}, {INT_MIN, INT_MAX, 7}}};
    String first{};
    Require(layout.doGet(&first, nullptr, selectors[0][0], selectors[0][1], selectors[0][2], &source) == &first, "do_get returns its result");
    String second{};
    Require(layout.doGet(&second, nullptr, selectors[1][0], selectors[1][1], selectors[1][2], &source) == &second, "do_get returns its result");
    Require(first.size == second.size && first.capacity == second.capacity, "do_get ignores the catalog, set and message");
    Require(std::memcmp(Data(layout, first), Data(layout, second), (first.size + 1) * layout.unit) == 0, "do_get ignores the catalog, set and message");
    Release(layout, second);
    return first;
}

void CheckInlineCopy(const Layout& layout, const String& source, const char* message) {
    const long before = live;
    String copy = CopyOf(layout, source);
    Require(live == before, message);
    Require(copy.size == source.size && copy.capacity == layout.inlineCapacity, message);
    Require(std::memcmp(copy.buffer, Data(layout, source), sizeof(copy.buffer)) == 0, message);
}

void CheckHeapCopy(const Layout& layout, const String& source, std::size_t capacity, const char* message) {
    const long before = live;
    String copy = CopyOf(layout, source);
    Require(live == before + 1, message);
    Require(copy.size == source.size && copy.capacity == capacity && copy.pointer != source.pointer, message);
    Require(std::memcmp(copy.pointer, Data(layout, source), (source.size + 1) * layout.unit) == 0, message);
    if ((capacity + 1) * layout.unit >= GuestStringStorage::LargeAllocation)
        Require(reinterpret_cast<std::uintptr_t>(copy.pointer) % GuestStringStorage::AllocationAlignment == 0, message);
    Release(layout, copy);
}

void CheckDefaultCopies(const Layout& layout, std::size_t largeSize) {
    const std::size_t mask = layout.inlineCapacity;
    String shortInline = MakeInline(layout, layout.inlineCapacity / 2, 0x41);
    CheckInlineCopy(layout, shortInline, "a short default is copied with its whole buffer");
    String fullInline = MakeInline(layout, layout.inlineCapacity, 0x61);
    CheckInlineCopy(layout, fullInline, "a default that fills the buffer stays inline");
    String emptyInline = MakeInline(layout, 0, 0x30);
    CheckInlineCopy(layout, emptyInline, "an empty default is copied with its whole buffer");
    String shortHeap = MakeHeap(layout, 2, mask * 2 + 1, 0x11);
    CheckInlineCopy(layout, shortHeap, "a short default held on the heap is copied inline from its heap buffer");
    String firstHeap = MakeHeap(layout, layout.inlineCapacity + 1, layout.inlineCapacity * 4 + 3, 0x21);
    CheckHeapCopy(layout, firstHeap, (layout.inlineCapacity + 1) | mask, "a default one unit past the buffer gets the size rounded up by the mask");
    String nulls = MakeHeap(layout, layout.inlineCapacity + 5, layout.inlineCapacity * 6 + 5, 0x31);
    std::memset(nulls.pointer + 2 * layout.unit, 0, layout.unit);
    CheckHeapCopy(layout, nulls, (layout.inlineCapacity + 5) | mask, "an embedded null unit is copied");
    String large = MakeHeap(layout, largeSize, largeSize | mask, 0x51);
    CheckHeapCopy(layout, large, largeSize | mask, "a large default uses the aligned allocation");
    Release(layout, shortHeap);
    Release(layout, firstHeap);
    Release(layout, nulls);
    Release(layout, large);
    String result{};
    Require(Throws([&] { layout.doGet(&result, nullptr, 0, 0, 0, nullptr); }), "a null default throws");
    Require(Throws([&] { layout.doGet(nullptr, nullptr, 0, 0, 0, &shortInline); }), "a null result throws");
    String corrupt = shortInline;
    corrupt.capacity = mask * 2 + 1;
    corrupt.pointer = nullptr;
    Require(Throws([&] { layout.doGet(&result, nullptr, 0, 0, 0, &corrupt); }), "a heap default without storage throws");
}

void CheckExportedVtable() {
    const auto* vtable = reinterpret_cast<const MessagesVtable*>(&_ZTVSt8messagesIcE_nid_postfix[2]);
    const Facet facet{&vtable->facet, 0, 0};
    const String name{};
    const int locale = 0;
    Require(_ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix(&facet, &name, &locale) == -1, "open through the exported vtable returns -1");
    const Layout narrow{1, GuestLocale::StringInlineCapacity, sceLibcInternalPending4_006_nid_no_patch};
    String fallback = MakeHeap(narrow, 40, 47, 0x71);
    const long before = live;
    String result{};
    Require(_ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix(&result, &facet, -1, 1, 2, &fallback) == &result, "get through the exported vtable returns its result");
    Require(live == before + 1 && result.size == 40 && result.capacity == 47 && std::memcmp(result.pointer, fallback.pointer, 41) == 0, "get through the exported vtable copies the default");
    sceLibcInternalPending4_005_nid_no_patch(&facet, -1);
    Release(narrow, result);
    Release(narrow, fallback);
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const long before = live;
    CheckWrappersCallTheVirtuals();
    CheckDefaultCopies({1, GuestLocale::StringInlineCapacity, sceLibcInternalPending4_006_nid_no_patch}, 5000);
    CheckDefaultCopies({2, 7, sceLibcInternalPending4_012_nid_no_patch}, 3000);
    CheckExportedVtable();
    Require(live == before, "every copy is released");
    std::puts("libc internal messages: ok");
    return 0;
}
