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

namespace {

std::size_t allocations = 0;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "LibcInternal messages: %s\n", message);
        std::abort();
    }
}

template<typename TAction>
void Reject(TAction action, const char* message) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, message);
}

void* APS5_VABI Allocate(std::size_t bytes) {
    ++allocations;
    return std::malloc(bytes);
}

void APS5_VABI Release(void* pointer) { std::free(pointer); }
void* APS5_VABI AllocateZeroed(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI Reallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI Align(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

struct Recorded {
    const GuestLocale::Facet* self = nullptr;
    const GuestLocale::String* name = nullptr;
    const GuestLocaleFacade* locale = nullptr;
    GuestLocale::String* result = nullptr;
    const GuestLocale::String* dfault = nullptr;
    std::int32_t catalog = 0;
    std::int32_t set = 0;
    std::int32_t message = 0;
    int opens = 0;
    int gets = 0;
    int closes = 0;
};

Recorded recorded;

void APS5_VABI UnusedFacetSlot(GuestLocale::Facet*) { std::abort(); }
GuestLocale::Facet* APS5_VABI UnusedReleaseSlot(GuestLocale::Facet*) { std::abort(); }

std::int32_t APS5_VABI RecordOpen(const GuestLocale::Facet* self, const GuestLocale::String* name, const GuestLocaleFacade* locale) {
    ++recorded.opens;
    recorded.self = self;
    recorded.name = name;
    recorded.locale = locale;
    return 0x1234;
}

GuestLocale::String* APS5_VABI RecordGet(GuestLocale::String* result, const GuestLocale::Facet* self, std::int32_t catalog, std::int32_t set, std::int32_t message, const GuestLocale::String* dfault) {
    ++recorded.gets;
    recorded.result = result;
    recorded.self = self;
    recorded.catalog = catalog;
    recorded.set = set;
    recorded.message = message;
    recorded.dfault = dfault;
    return result;
}

void APS5_VABI RecordClose(const GuestLocale::Facet* self, std::int32_t catalog) {
    ++recorded.closes;
    recorded.self = self;
    recorded.catalog = catalog;
}

GuestLocale::String MakeString(const std::string& text) {
    GuestLocale::String string{};
    GuestLocale::AssignString(&string, text.data(), text.size());
    return string;
}

std::string Text(const GuestLocale::String& string) {
    return std::string(GuestLocale::StringData(string), string.size);
}

void CheckPublicMembersDispatch() {
    const GuestLocale::MessagesVtable vtable{{UnusedFacetSlot, UnusedFacetSlot, UnusedFacetSlot, UnusedReleaseSlot}, RecordOpen, RecordGet, RecordClose};
    const GuestLocale::Facet facet{&vtable.facet, 0, 0};
    GuestLocale::String name = MakeString("catalog");
    GuestLocaleFacade locale{};

    Require(_ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix(&facet, &name, &locale) == 0x1234, "open must return do_open");
    Require(recorded.opens == 1 && recorded.self == &facet && recorded.name == &name && recorded.locale == &locale, "open must pass its arguments to do_open");

    GuestLocale::String dfault = MakeString("fallback");
    GuestLocale::String result{};
    Require(_ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix(&result, &facet, 7, -2, 99, &dfault) == &result, "get must return do_get");
    Require(recorded.gets == 1 && recorded.result == &result && recorded.self == &facet, "get must pass the result and facet to do_get");
    Require(recorded.catalog == 7 && recorded.set == -2 && recorded.message == 99 && recorded.dfault == &dfault, "get must pass catalog, set, message and default to do_get");

    _ZNKSt8messagesIcE5closeEi_nid_postfix(&facet, -1);
    Require(recorded.closes == 1 && recorded.self == &facet && recorded.catalog == -1, "close must pass the catalog to do_close");
    Require(recorded.opens == 1 && recorded.gets == 1, "each public member must call only its own virtual");

    const GuestLocale::MessagesVtable copying{{UnusedFacetSlot, UnusedFacetSlot, UnusedFacetSlot, UnusedReleaseSlot}, RecordOpen, _ZNKSt8messagesIcE6do_getEiiiRKSs_nid_postfix, RecordClose};
    const GuestLocale::Facet copyingFacet{&copying.facet, 0, 0};
    GuestLocale::String copied{};
    Require(_ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix(&copied, &copyingFacet, -1, 1, 1, &dfault) == &copied, "get through do_get must return the result");
    Require(Text(copied) == "fallback", "get through do_get must return the default message");
    GuestLocale::ReleaseStringStorage(copied);

    const GuestLocale::Facet missingVtable{nullptr, 0, 0};
    Reject([&] { _ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix(nullptr, &name, &locale); }, "open must reject a null facet");
    Reject([&] { _ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix(&missingVtable, &name, &locale); }, "open must reject a facet without a vtable");
    Reject([&] { _ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix(&result, nullptr, 0, 0, 0, &dfault); }, "get must reject a null facet");
    Reject([&] { _ZNKSt8messagesIcE5closeEi_nid_postfix(&missingVtable, 0); }, "close must reject a facet without a vtable");
    Require(recorded.opens == 1 && recorded.gets == 1 && recorded.closes == 1, "a rejected call must not reach a virtual");

    GuestLocale::ReleaseStringStorage(name);
    GuestLocale::ReleaseStringStorage(dfault);
}

void CheckDoGetCopiesDefault(const std::string& text, std::size_t capacity) {
    GuestLocale::String dfault = MakeString(text);
    const std::size_t before = allocations;
    GuestLocale::String result;
    std::memset(&result, 0xcd, sizeof(result));
    Require(_ZNKSt8messagesIcE6do_getEiiiRKSs_nid_postfix(&result, nullptr, -1, 3, 4, &dfault) == &result, "do_get must return its result");
    Require(result.reserved == 0xcdcdcdcdcdcdcdcdull, "do_get must leave the allocator word alone");
    Require(result.size == text.size() && result.capacity == capacity, "do_get must size the copy like a string copy");
    Require(Text(result) == text && GuestLocale::StringData(result)[text.size()] == 0, "do_get must copy every character and terminate the copy");
    Require(Text(dfault) == text, "do_get must leave the default message unchanged");
    if (text.size() > 15) {
        Require(GuestLocale::StringData(result) != GuestLocale::StringData(dfault), "do_get must not share the default storage");
        Require(allocations == before + 1, "a long copy must allocate once");
    } else {
        Require(GuestLocale::StringData(result) == result.buffer && allocations == before, "a short copy must stay inline");
    }
    GuestLocale::ReleaseStringStorage(result);
    GuestLocale::ReleaseStringStorage(dfault);
}

void CheckDoGet() {
    CheckDoGetCopiesDefault("", 15);
    CheckDoGetCopiesDefault("hello", 15);
    CheckDoGetCopiesDefault(std::string("a\0b", 3), 15);
    CheckDoGetCopiesDefault(std::string(15, 'x'), 15);
    CheckDoGetCopiesDefault(std::string(16, 'y'), 31);
    CheckDoGetCopiesDefault(std::string(40, 'z'), 47);
    CheckDoGetCopiesDefault(std::string(4095, 'q'), 4095);

    GuestLocale::String dfault = MakeString("text");
    GuestLocale::String result{};
    Reject([&] { _ZNKSt8messagesIcE6do_getEiiiRKSs_nid_postfix(&result, nullptr, 0, 0, 0, nullptr); }, "do_get must reject a null default message");
    Reject([&] { _ZNKSt8messagesIcE6do_getEiiiRKSs_nid_postfix(nullptr, nullptr, 0, 0, 0, &dfault); }, "do_get must reject a null result");
    GuestLocale::ReleaseStringStorage(dfault);
}

}

int main() {
    const std::array<void*, 10> api{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed), reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign), reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckPublicMembersDispatch();
    CheckDoGet();
    return 0;
}
