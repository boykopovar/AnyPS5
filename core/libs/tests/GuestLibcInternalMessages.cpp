#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal messages: %s\n", message);
    std::exit(1);
}

using GuestLocale::Facet;
using GuestLocale::String;

struct TestMessagesVtable {
    GuestLocale::FacetVtable facet;
    int (APS5_VABI *open)(const Facet* self, const String* name, const void* locale);
    String* (APS5_VABI *get)(String* result, const Facet* self, int catalog, int set, int message, const String* fallback);
    void (APS5_VABI *close)(const Facet* self, int catalog);
};

static_assert(offsetof(TestMessagesVtable, open) == 4 * sizeof(void*));
static_assert(offsetof(TestMessagesVtable, get) == 5 * sizeof(void*));
static_assert(offsetof(TestMessagesVtable, close) == 6 * sizeof(void*));

constexpr int kDerived = 100;
constexpr unsigned char kUntouched = 0xa5;
constexpr std::uint64_t kWritten = 0x0123456789abcdefull;

struct Call {
    int opens;
    int gets;
    int closes;
    int table;
    const Facet* self;
    const String* name;
    const void* locale;
    String* result;
    int catalog;
    int set;
    int message;
    const String* fallback;
    bool resultUntouched;
};

Call g_call{};
int g_openResult = 0;

void APS5_VABI TestFacetNop(Facet*) {}
Facet* APS5_VABI TestFacetRelease(Facet*) { return nullptr; }

template<int TTable>
int APS5_VABI RecordOpen(const Facet* self, const String* name, const void* locale) {
    ++g_call.opens;
    g_call.table = TTable;
    g_call.self = self;
    g_call.name = name;
    g_call.locale = locale;
    return g_openResult;
}

template<int TTable>
String* APS5_VABI RecordGet(String* result, const Facet* self, int catalog, int set, int message, const String* fallback) {
    ++g_call.gets;
    g_call.table = TTable;
    g_call.self = self;
    g_call.result = result;
    g_call.catalog = catalog;
    g_call.set = set;
    g_call.message = message;
    g_call.fallback = fallback;
    g_call.resultUntouched = true;
    const auto* bytes = reinterpret_cast<const unsigned char*>(result);
    for (std::size_t index = 0; index < sizeof(String); ++index) {
        if (bytes[index] != kUntouched) g_call.resultUntouched = false;
    }
    std::memset(result, 0, sizeof(String));
    result->size = kWritten + static_cast<std::uint64_t>(TTable);
    return result;
}

template<int TTable>
void APS5_VABI RecordClose(const Facet*, int) {
    ++g_call.closes;
    g_call.table = TTable;
}

template<int TTable>
constexpr TestMessagesVtable RecordingVtable() {
    return TestMessagesVtable{{TestFacetNop, TestFacetNop, TestFacetNop, TestFacetRelease}, RecordOpen<TTable>, RecordGet<TTable>, RecordClose<TTable>};
}

const TestMessagesVtable g_base = RecordingVtable<0>();
const TestMessagesVtable g_derived = RecordingVtable<kDerived>();

void CheckOpen() {
    for (const TestMessagesVtable* vtable : {&g_base, &g_derived}) {
        for (const int answer : {-1, 0, 7, INT_MAX, INT_MIN}) {
            const int table = vtable == &g_derived ? kDerived : 0;
            Facet facet{&vtable->facet, 1, 0};
            String name{};
            alignas(8) unsigned char locale[16]{};
            g_call = Call{};
            g_call.table = -1;
            g_openResult = answer;
            const int result = _ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix(&facet, &name, locale);
            Require(g_call.opens == 1 && g_call.gets == 0 && g_call.closes == 0, "open calls do_open exactly once and no other member");
            Require(g_call.table == table, "open calls the do_open slot of the facet's own vtable");
            Require(g_call.self == &facet, "open passes the facet");
            Require(g_call.name == &name, "open passes the catalog name");
            Require(g_call.locale == locale, "open passes the locale");
            Require(result == answer, "open returns the catalog do_open returns");
            Require(facet.vtable == &vtable->facet && facet.references == 1, "open leaves the facet unchanged");
        }
    }
}

void CheckGet() {
    struct Arguments {
        int catalog;
        int set;
        int message;
    };
    for (const TestMessagesVtable* vtable : {&g_base, &g_derived}) {
        for (const Arguments arguments : {Arguments{-1, 0, 0}, Arguments{3, 5, 9}, Arguments{INT_MAX, INT_MIN, -7}, Arguments{INT_MIN, INT_MAX, 0x12345678}}) {
            const int table = vtable == &g_derived ? kDerived : 0;
            Facet facet{&vtable->facet, 1, 0};
            String fallback{};
            std::memcpy(fallback.buffer, "default", 8);
            fallback.size = 7;
            fallback.capacity = GuestLocale::StringInlineCapacity;
            const String before = fallback;
            String result;
            std::memset(&result, kUntouched, sizeof(result));
            g_call = Call{};
            g_call.table = -1;
            String* returned = _ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix(&result, &facet, arguments.catalog, arguments.set, arguments.message, &fallback);
            Require(g_call.gets == 1 && g_call.opens == 0 && g_call.closes == 0, "get calls do_get exactly once and no other member");
            Require(g_call.table == table, "get calls the do_get slot of the facet's own vtable");
            Require(g_call.self == &facet, "get passes the facet");
            Require(g_call.result == &result, "get passes its own result string to do_get");
            Require(g_call.resultUntouched, "get leaves the result string for do_get to construct");
            Require(g_call.catalog == arguments.catalog && g_call.set == arguments.set && g_call.message == arguments.message, "get passes the catalog, set and message");
            Require(g_call.fallback == &fallback, "get passes the default string");
            Require(returned == &result, "get returns the string do_get returns");
            Require(result.size == kWritten + static_cast<std::uint64_t>(table), "get keeps what do_get constructed");
            Require(std::memcmp(&fallback, &before, sizeof(fallback)) == 0, "get leaves the default string unchanged");
        }
    }
}

}

int main() {
    CheckOpen();
    CheckGet();
    std::puts("libc internal messages: ok");
    return 0;
}
