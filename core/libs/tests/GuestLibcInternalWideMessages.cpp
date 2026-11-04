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
#include <exception>
#include <iterator>
#include <string>

namespace {

std::size_t allocations = 0;
std::size_t lastSize = 0;
void* lastAllocation = nullptr;

const GuestLocale::Facet* openSelf = nullptr;
const GuestLocale::String* openName = nullptr;
const GuestLocaleFacade* openLocale = nullptr;
GuestLocale::String* getResult = nullptr;
const GuestLocale::Facet* getSelf = nullptr;
int getCatalog = 0;
int getSet = 0;
int getMessage = 0;
const GuestLocale::String* getDefault = nullptr;
const GuestLocale::Facet* closeSelf = nullptr;
int closeCatalog = 0;
GuestLocale::String getReturn{};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal wide messages: %s\n", message);
    std::abort();
}

template<typename TAction>
void Reject(TAction action, const char* message) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, message);
}

void* APS5_VABI Allocate(std::size_t bytes) {
    ++allocations;
    lastSize = bytes;
    lastAllocation = std::malloc(bytes);
    return lastAllocation;
}

void APS5_VABI Release(void* pointer) { std::free(pointer); }
void* APS5_VABI AllocateZeroed(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI Reallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI Align(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

void APS5_VABI DestroyFacet(GuestLocale::Facet*) { std::abort(); }
void APS5_VABI RetainFacet(GuestLocale::Facet*) { std::abort(); }
GuestLocale::Facet* APS5_VABI ReleaseFacet(GuestLocale::Facet*) { std::abort(); }

std::int32_t APS5_VABI RecordOpen(const GuestLocale::Facet* self, const GuestLocale::String* name, const GuestLocaleFacade* locale) {
    openSelf = self;
    openName = name;
    openLocale = locale;
    return 41;
}

GuestLocale::String* APS5_VABI RecordGet(GuestLocale::String* result, const GuestLocale::Facet* self, std::int32_t catalog, std::int32_t set, std::int32_t message, const GuestLocale::String* dfault) {
    getResult = result;
    getSelf = self;
    getCatalog = catalog;
    getSet = set;
    getMessage = message;
    getDefault = dfault;
    return &getReturn;
}

void APS5_VABI RecordClose(const GuestLocale::Facet* self, std::int32_t catalog) {
    closeSelf = self;
    closeCatalog = catalog;
}

const GuestLocale::MessagesVtable g_recordingVtable{{DestroyFacet, DestroyFacet, RetainFacet, ReleaseFacet}, RecordOpen, RecordGet, RecordClose};
const GuestLocale::MessagesVtable g_doGetVtable{{DestroyFacet, DestroyFacet, RetainFacet, ReleaseFacet}, RecordOpen, _ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix, RecordClose};
constexpr char16_t kEmbeddedNullDefault[] = u"wide\0messageĀ with a nul!";
static_assert(std::size(kEmbeddedNullDefault) == 26);

GuestLocale::String WideString(std::u16string& text) {
    GuestLocale::String string{};
    string.reserved = 0x1122334455667788ull;
    string.size = text.size();
    if (text.size() <= 7) {
        std::memcpy(string.buffer, text.data(), text.size() * sizeof(char16_t));
        string.capacity = 7;
    } else {
        string.pointer = reinterpret_cast<char*>(text.data());
        string.capacity = text.size();
    }
    return string;
}

std::u16string Units(const GuestLocale::String& string) {
    const char* data = string.capacity > 7 ? string.pointer : string.buffer;
    return std::u16string(reinterpret_cast<const char16_t*>(data), string.size);
}

bool Terminated(const GuestLocale::String& string) {
    const char* data = string.capacity > 7 ? string.pointer : string.buffer;
    return reinterpret_cast<const char16_t*>(data)[string.size] == 0;
}

void CheckPublicMembersDispatch() {
    GuestLocale::Facet facet{&g_recordingVtable.facet, 0, 0};
    std::u16string text = u"name";
    GuestLocale::String name{};
    std::memcpy(name.buffer, "catalog", 8);
    name.size = 7;
    name.capacity = 15;
    GuestLocaleFacade facade{};
    const GuestLocaleFacade* locale = &facade;
    Require(_ZNKSt8messagesIwE4openERKSsRKSt6locale_nid_postfix(&facet, &name, locale) == 41, "open must return do_open's catalog");
    Require(openSelf == &facet && openName == &name && openLocale == locale, "open must pass its arguments to do_open");

    GuestLocale::String dfault = WideString(text);
    GuestLocale::String result{};
    Require(_ZNKSt8messagesIwE3getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(&result, &facet, 3, -4, 5, &dfault) == &getReturn, "get must return do_get's string");
    Require(getResult == &result && getSelf == &facet && getCatalog == 3 && getSet == -4 && getMessage == 5 && getDefault == &dfault, "get must pass its arguments to do_get");

    _ZNKSt8messagesIwE5closeEi_nid_postfix(&facet, -9);
    Require(closeSelf == &facet && closeCatalog == -9, "close must pass its arguments to do_close");

    GuestLocale::Facet unbound{nullptr, 0, 0};
    for (const GuestLocale::Facet* rejected : {static_cast<const GuestLocale::Facet*>(nullptr), static_cast<const GuestLocale::Facet*>(&unbound)}) {
        Reject([&] { _ZNKSt8messagesIwE4openERKSsRKSt6locale_nid_postfix(rejected, &name, locale); }, "open must reject a facet without a vtable");
        Reject([&] { _ZNKSt8messagesIwE3getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(&result, rejected, 0, 0, 0, &dfault); }, "get must reject a facet without a vtable");
        Reject([&] { _ZNKSt8messagesIwE5closeEi_nid_postfix(rejected, 0); }, "close must reject a facet without a vtable");
    }
}

void CheckDoGetCopiesDefault(const std::u16string& expected, std::size_t capacity, bool allocates) {
    std::u16string text = expected;
    GuestLocale::String dfault = WideString(text);
    const GuestLocale::String before = dfault;
    GuestLocale::Facet facet{&g_doGetVtable.facet, 0, 0};
    for (const int catalog : {-1, 0, 7}) {
        GuestLocale::String result;
        std::memset(&result, 0xcd, sizeof(result));
        const std::size_t allocated = allocations;
        Require(_ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(&result, &facet, catalog, catalog + 1, catalog - 1, &dfault) == &result, "do_get must return its result");
        Require(result.size == expected.size() && result.capacity == capacity, "do_get must size the copy of dfault");
        Require(Units(result) == expected && Terminated(result), "do_get must copy every unit of dfault");
        Require(allocations == allocated + (allocates ? 1 : 0), "do_get must allocate only a string longer than the inline buffer");
        if (allocates) {
            Require(result.pointer == lastAllocation && lastSize == (capacity + 1) * 2, "do_get must store a long copy in its own allocation");
            Require(result.pointer != dfault.pointer, "do_get must not share dfault's storage");
            std::free(result.pointer);
        }
        Require(std::memcmp(&dfault, &before, sizeof(dfault)) == 0 && text == expected, "do_get must leave dfault unchanged");
    }
}

void CheckDoGetThroughGet() {
    std::u16string text = u"fallback message";
    GuestLocale::String dfault = WideString(text);
    GuestLocale::Facet facet{&g_doGetVtable.facet, 0, 0};
    GuestLocale::String result{};
    Require(_ZNKSt8messagesIwE3getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(&result, &facet, -1, 1, 2, &dfault) == &result, "get must return do_get's result");
    Require(Units(result) == text && result.capacity == 23, "get must return dfault when no catalog is open");
    std::free(result.pointer);
}

void CheckDoGetRejections() {
    std::u16string text = u"abc";
    GuestLocale::String dfault = WideString(text);
    GuestLocale::Facet facet{&g_doGetVtable.facet, 0, 0};
    GuestLocale::String result{};
    Reject([&] { _ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(&result, &facet, 0, 0, 0, nullptr); }, "do_get must reject a null dfault");
    Reject([&] { _ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(nullptr, &facet, 0, 0, 0, &dfault); }, "do_get must reject a null result");
    GuestLocale::String oversized = dfault;
    oversized.size = 8;
    Reject([&] { _ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(&result, &facet, 0, 0, 0, &oversized); }, "do_get must reject a dfault larger than its capacity");
}

}

int main() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed), reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign), reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckPublicMembersDispatch();
    CheckDoGetCopiesDefault(u"", 7, false);
    CheckDoGetCopiesDefault(u"abc", 7, false);
    CheckDoGetCopiesDefault(u"seven!!", 7, false);
    CheckDoGetCopiesDefault(u"eight!!!", 15, true);
    CheckDoGetCopiesDefault(std::u16string(kEmbeddedNullDefault, std::size(kEmbeddedNullDefault) - 1), 31, true);
    CheckDoGetThroughGet();
    CheckDoGetRejections();
    return 0;
}
