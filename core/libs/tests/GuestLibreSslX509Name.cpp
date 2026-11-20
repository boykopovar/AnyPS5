#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <new>
#include <string>
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifndef _WIN32
static std::size_t hostAllocationLimit = SIZE_MAX;
void* operator new(std::size_t bytes) {
    if (bytes > hostAllocationLimit) {
        std::fprintf(stderr, "host allocation of %zu bytes exceeds the test cap\n", bytes);
        std::abort();
    }
    if (void* pointer = std::malloc(bytes ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    if (bytes > hostAllocationLimit) {
        std::fprintf(stderr, "host allocation of %zu bytes exceeds the test cap\n", bytes);
        std::abort();
    }
    return std::malloc(bytes ? bytes : 1);
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
#endif

template<typename TCall> static void Capped(TCall call) {
#ifndef _WIN32
    hostAllocationLimit = 1 << 20;
#endif
    call();
#ifndef _WIN32
    hostAllocationLimit = SIZE_MAX;
#endif
}

#ifndef _WIN32
class GuardedPage {
public:
    GuardedPage() {
        const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        mapping = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapping == MAP_FAILED || mprotect(static_cast<unsigned char*>(mapping) + page, page, PROT_NONE) != 0) std::abort();
        end = static_cast<unsigned char*>(mapping) + page;
        bytes = page * 2;
    }
    ~GuardedPage() { munmap(mapping, bytes); }
    unsigned char* Tail(std::size_t count) const { return end - count; }

private:
    void* mapping = nullptr;
    unsigned char* end = nullptr;
    std::size_t bytes = 0;
};
#endif

struct TestStack {
    int num;
    char** data;
    int sorted;
    int numAlloc;
    void* comp;
};

struct TestNameEntry {
    void* object;
    void* value;
    int set;
    int size;
};

struct TestName {
    TestStack* entries;
    int modified;
    void* bytes;
    unsigned char* canonEnc;
    int canonEnclen;
};

struct TestObject {
    const char* shortName;
    const char* longName;
    int nid;
    int length;
    const unsigned char* data;
    int flags;
};

static_assert(sizeof(TestStack) == 32 && offsetof(TestStack, data) == 8);
static_assert(sizeof(TestNameEntry) == 24 && offsetof(TestNameEntry, value) == 8);
static_assert(sizeof(TestName) == 40);

static_assert(sizeof(TestObject) == 40 && offsetof(TestObject, length) == 20 && offsetof(TestObject, data) == 24);

static const LibreSslX509Name* Name(const TestName* name) {
    return reinterpret_cast<const LibreSslX509Name*>(name);
}

static const LibreSslX509NameEntry* Entry(const TestNameEntry* entry) {
    return reinterpret_cast<const LibreSslX509NameEntry*>(entry);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL X509_NAME check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestGetEntry() {
    TestNameEntry first {};
    TestNameEntry second {};
    char* slots[] {reinterpret_cast<char*>(&first), reinterpret_cast<char*>(&second), nullptr};
    TestStack entries {2, slots, 0, 3, nullptr};
    TestName name {};
    Require(X509_NAME_get_entry_nid_postfix(nullptr, 0) == nullptr);
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), 0) == nullptr);
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), -1) == nullptr);
    name.entries = &entries;
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), 0) == Entry(&first));
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), 1) == Entry(&second));
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), 2) == nullptr);
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), -1) == nullptr);
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), INT_MIN) == nullptr);
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), INT_MAX) == nullptr);
    entries.num = 0;
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), 0) == nullptr);
    entries.num = 3;
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), 2) == nullptr);
    entries.data = nullptr;
    RequireThrows([&] { X509_NAME_get_entry_nid_postfix(Name(&name), 0); });
    Require(X509_NAME_get_entry_nid_postfix(Name(&name), 3) == nullptr);
}

static void TestEntryGetData() {
    int value = 0;
    TestNameEntry entry {};
    Require(X509_NAME_ENTRY_get_data_nid_postfix(nullptr) == nullptr);
    Require(X509_NAME_ENTRY_get_data_nid_postfix(Entry(&entry)) == nullptr);
    entry.value = &value;
    Require(X509_NAME_ENTRY_get_data_nid_postfix(Entry(&entry)) == &value);
    entry.object = &entry;
    entry.set = 7;
    Require(X509_NAME_ENTRY_get_data_nid_postfix(Entry(&entry)) == &value);
}

static const unsigned char CountryOid[] {0x55, 0x04, 0x06};
static const unsigned char OrganizationOid[] {0x55, 0x04, 0x0a};
static const unsigned char CommonNameOid[] {0x55, 0x04, 0x03};
static const unsigned char UnitOid[] {0x55, 0x04, 0x0b};
static const unsigned char EmailOid[] {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01};
static const unsigned char PrivateOid[] {0x2a, 0x03, 0x04};
static const unsigned char LocalityOid[] {0x55, 0x04, 0x07};
static const unsigned char UnstructuredNameOid[] {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x02};

struct DecodedName {
    TestObject objects[8];
    Asn1String values[8];
    LibreSslX509NameEntry entries[8];
    char* slots[8];
    LibreSslStack stack;
    LibreSslX509Name name;

    DecodedName() {
        const struct { const unsigned char* oid; int oidLength; const char* text; int textLength; } fields[] {
            {CountryOid, 3, "US", 2},
            {OrganizationOid, 3, "Example", 7},
            {CommonNameOid, 3, "first", 5},
            {UnitOid, 3, "Unit", 4},
            {CommonNameOid, 3, "second", 6},
            {EmailOid, 9, "a@b.c", 5},
            {PrivateOid, 3, "x", 1},
            {LocalityOid, 3, "a\0b", 3},
        };
        for (int index = 0; index < 8; ++index) {
            objects[index] = {nullptr, nullptr, 0, fields[index].oidLength, fields[index].oid, 0};
            values[index] = {fields[index].textLength, 12, reinterpret_cast<unsigned char*>(const_cast<char*>(fields[index].text)), 0};
            entries[index] = {&objects[index], &values[index], index, 0};
            slots[index] = reinterpret_cast<char*>(&entries[index]);
        }
        stack = {8, slots, 0, 8, nullptr};
        name = {&stack, 0, nullptr, nullptr, 0};
    }

    DecodedName(const DecodedName&) = delete;
};

static void TestIndexByNid() {
    DecodedName decoded;
    const auto* name = &decoded.name;
    const int commonName[][2] {{-1, 2}, {-5, 2}, {INT_MIN, 2}, {0, 2}, {2, 4}, {4, -1}, {7, -1}, {8, -1}, {100, -1}};
    for (const auto& [lastpos, index] : commonName) Require(X509_NAME_get_index_by_NID_nid_postfix(name, 13, lastpos) == index);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 14, -1) == 0);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 14, 0) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 48, -1) == 5);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 48, 4) == 5);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 48, 7) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 15, -1) == 7);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 15, 7) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 16, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 0, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(nullptr, 13, -1) == -1);
    decoded.objects[6].nid = 13;
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 13, 4) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 458, -1) == -1);
    decoded.objects[4] = {nullptr, nullptr, 0, 9, UnstructuredNameOid, 0};
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 48, -1) == 5);
    decoded.objects[6] = {nullptr, nullptr, 0, 0, nullptr, 0};
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 0, -1) == 6);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 0, 6) == -1);
    LibreSslStack empty {0, nullptr, 0, 0, nullptr};
    LibreSslX509Name emptyName {&empty, 0, nullptr, nullptr, 0};
    LibreSslX509Name noEntries {};
    Require(X509_NAME_get_index_by_NID_nid_postfix(&emptyName, 13, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(&noEntries, 13, -1) == -1);
    RequireThrows([&] { X509_NAME_get_index_by_NID_nid_postfix(name, 13, INT_MAX); });
    decoded.objects[2].data = nullptr;
    RequireThrows([&] { X509_NAME_get_index_by_NID_nid_postfix(name, 13, -1); });
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 14, -1) == 0);
    decoded.entries[1].object = nullptr;
    RequireThrows([&] { X509_NAME_get_index_by_NID_nid_postfix(name, 13, -1); });
    decoded.slots[1] = nullptr;
    RequireThrows([&] { X509_NAME_get_index_by_NID_nid_postfix(name, 13, -1); });
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 14, -1) == 0);
}

static bool BufferHolds(const char* buffer, const char* prefix, std::size_t length) {
    if (std::memcmp(buffer, prefix, length) != 0) return false;
    for (std::size_t index = length; index < 64; ++index) {
        if (buffer[index] != 0x7f) return false;
    }
    return true;
}

static void TestTextByNid() {
    DecodedName decoded;
    const auto* name = &decoded.name;
    char buffer[64];
    const struct { int len; int result; const char* prefix; std::size_t length; } cases[] {
        {64, 5, "first", 6}, {6, 5, "first", 6}, {5, 4, "firs", 5}, {3, 2, "fi", 3}, {2, 1, "f", 2}, {1, 0, "", 1},
        {0, -1, "", 0}, {-5, -6, "", 0},
    };
    for (const auto& item : cases) {
        std::memset(buffer, 0x7f, sizeof(buffer));
        Require(X509_NAME_get_text_by_NID_nid_postfix(name, 13, buffer, item.len) == item.result);
        Require(BufferHolds(buffer, item.prefix, item.length));
    }
    std::memset(buffer, 0x7f, sizeof(buffer));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 13, nullptr, 64) == 5);
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 13, nullptr, 0) == 5);
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 16, buffer, 64) == -1);
    Require(X509_NAME_get_text_by_NID_nid_postfix(nullptr, 13, buffer, 64) == -1);
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 16, buffer, INT_MIN) == -1);
    Require(BufferHolds(buffer, "", 0));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 15, buffer, 64) == 3);
    Require(BufferHolds(buffer, "a\0b", 4));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 48, buffer, 64) == 5);
    Require(BufferHolds(buffer, "a@b.c", 6));
    RequireThrows([&] { X509_NAME_get_text_by_NID_nid_postfix(name, 13, buffer, INT_MIN); });
    decoded.values[2].data = nullptr;
    RequireThrows([&] { X509_NAME_get_text_by_NID_nid_postfix(name, 13, buffer, 64); });
    std::memset(buffer, 0x7f, sizeof(buffer));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 13, buffer, 1) == 0);
    Require(BufferHolds(buffer, "", 1));
    decoded.entries[2].value = nullptr;
    RequireThrows([&] { X509_NAME_get_text_by_NID_nid_postfix(name, 13, buffer, 64); });
}

static const unsigned char RsaEncryptionOid[] {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
static const unsigned char SubjectAltNameOid[] {0x55, 0x1d, 0x11};
static const unsigned char Ed448phOid[] {0x2b, 0x65, 0x73};
static const int UnassignedNids[] {102, 118, 350, 511, 772, 774, 775};
static const int ObjectCount = 956;
static const std::uint64_t UnknownNidError = (8ull << 24) | (0xfffull << 12) | 101;

static bool IsUnassigned(int nid) {
    for (const int value : UnassignedNids) {
        if (value == nid) return true;
    }
    return false;
}

static bool NoError() {
    return ERR_get_error_nid_postfix() == 0;
}

static bool OneUnknownNidError() {
    return ERR_get_error_nid_postfix() == UnknownNidError && ERR_get_error_nid_postfix() == 0;
}

static void TestObjectTableNids() {
    OBJ_cleanup_nid_postfix();
    ERR_clear_error_nid_postfix();
    DecodedName decoded;
    const auto* name = &decoded.name;
    char buffer[64];
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 49, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 6, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 85, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 955, -1) == -1);
    decoded.objects[4] = {nullptr, nullptr, 0, 9, UnstructuredNameOid, 0};
    decoded.objects[3] = {nullptr, nullptr, 0, 9, RsaEncryptionOid, 0};
    decoded.objects[1] = {nullptr, nullptr, 0, 3, SubjectAltNameOid, 0};
    decoded.objects[7] = {nullptr, nullptr, 0, 3, Ed448phOid, 0};
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 49, -1) == 4);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 49, 3) == 4);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 49, 4) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 6, -1) == 3);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 85, -1) == 1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 955, -1) == 7);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 17, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 18, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 13, -1) == 2);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 13, 2) == -1);
    std::memset(buffer, 0x7f, sizeof(buffer));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 49, buffer, 64) == 6);
    Require(BufferHolds(buffer, "second", 7));
    std::memset(buffer, 0x7f, sizeof(buffer));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 6, buffer, 3) == 2);
    Require(BufferHolds(buffer, "Un", 3));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 85, nullptr, 0) == 7);
    std::memset(buffer, 0x7f, sizeof(buffer));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 955, buffer, 64) == 3);
    Require(BufferHolds(buffer, "a\0b", 4));
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 114, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 855, -1) == -1);
    decoded.objects[6] = {nullptr, nullptr, 0, 0, nullptr, 0};
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 114, -1) == 6);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 855, 5) == 6);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 855, 6) == -1);
    std::memset(buffer, 0x7f, sizeof(buffer));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, 114, buffer, 64) == 1);
    Require(BufferHolds(buffer, "x", 2));
    Require(NoError());
}

static void TestUnknownNids() {
    OBJ_cleanup_nid_postfix();
    ERR_clear_error_nid_postfix();
    DecodedName decoded;
    const auto* name = &decoded.name;
    LibreSslStack empty {0, nullptr, 0, 0, nullptr};
    LibreSslX509Name emptyName {&empty, 0, nullptr, nullptr, 0};
    char buffer[64];
    std::memset(buffer, 0x7f, sizeof(buffer));
    for (const int nid : UnassignedNids) {
        Require(X509_NAME_get_index_by_NID_nid_postfix(name, nid, -1) == -2);
        Require(OneUnknownNidError());
        Require(X509_NAME_get_index_by_NID_nid_postfix(nullptr, nid, INT_MAX) == -2);
        Require(OneUnknownNidError());
        Require(X509_NAME_get_text_by_NID_nid_postfix(name, nid, buffer, 64) == -1);
        Require(OneUnknownNidError());
        Require(X509_NAME_get_text_by_NID_nid_postfix(nullptr, nid, nullptr, INT_MIN) == -1);
        Require(OneUnknownNidError());
    }
    for (const int nid : {-1, INT_MIN, ObjectCount, ObjectCount + 1, INT_MAX}) {
        Require(X509_NAME_get_index_by_NID_nid_postfix(name, nid, -1) == -2);
        Require(X509_NAME_get_index_by_NID_nid_postfix(nullptr, nid, INT_MAX) == -2);
        Require(X509_NAME_get_text_by_NID_nid_postfix(name, nid, buffer, 64) == -1);
        Require(X509_NAME_get_text_by_NID_nid_postfix(nullptr, nid, nullptr, INT_MIN) == -1);
        Require(NoError());
    }
    Require(BufferHolds(buffer, "", 0));
    for (int nid = 0; nid < ObjectCount; ++nid) {
        Require(X509_NAME_get_index_by_NID_nid_postfix(&emptyName, nid, -1) == (IsUnassigned(nid) ? -2 : -1));
        Require(X509_NAME_get_text_by_NID_nid_postfix(&emptyName, nid, buffer, 64) == -1);
        Require((OBJ_nid2sn_nid_postfix(nid) == nullptr) == IsUnassigned(nid));
    }
    Require(BufferHolds(buffer, "", 0));
    ERR_clear_error_nid_postfix();
}

static void TestDynamicNid() {
    OBJ_cleanup_nid_postfix();
    ERR_clear_error_nid_postfix();
    const int nid = OBJ_create_nid_postfix("1.2.3.4", "testName", "Test Name");
    Require(nid >= ObjectCount);
    const int absent = OBJ_create_nid_postfix("1.2.3.5", nullptr, nullptr);
    Require(absent == nid + 1);
    DecodedName decoded;
    const auto* name = &decoded.name;
    char buffer[64];
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, nid, -1) == 6);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, nid, 5) == 6);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, nid, 6) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(nullptr, nid, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, absent, -1) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 13, -1) == 2);
    std::memset(buffer, 0x7f, sizeof(buffer));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, nid, buffer, 64) == 1);
    Require(BufferHolds(buffer, "x", 2));
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, nid, nullptr, 0) == 1);
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, absent, buffer, 64) == -1);
    Require(NoError());
    std::memset(buffer, 0x7f, sizeof(buffer));
    for (const int unknown : {absent + 1, -1, INT_MIN, INT_MAX}) {
        Require(X509_NAME_get_index_by_NID_nid_postfix(name, unknown, -1) == -2);
        Require(OneUnknownNidError());
        Require(X509_NAME_get_index_by_NID_nid_postfix(nullptr, unknown, INT_MAX) == -2);
        Require(OneUnknownNidError());
        Require(X509_NAME_get_text_by_NID_nid_postfix(name, unknown, buffer, 64) == -1);
        Require(OneUnknownNidError());
    }
    Require(BufferHolds(buffer, "", 0));
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 102, -1) == -2);
    Require(OneUnknownNidError());
    OBJ_cleanup_nid_postfix();
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, nid, -1) == -2);
    Require(X509_NAME_get_text_by_NID_nid_postfix(name, nid, buffer, 64) == -1);
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, -1, -1) == -2);
    Require(NoError());
    Require(X509_NAME_get_index_by_NID_nid_postfix(name, 13, -1) == 2);
    Require(BufferHolds(buffer, "", 0));
}

template<typename TCall> static void RejectsPreservingErrno(TCall call, std::source_location location = std::source_location::current()) {
    errno = 71;
    bool rejected = false;
    try { call(); } catch (const std::runtime_error&) { rejected = errno == 71; }
    Require(rejected, location);
}

static void TestGuestPointers() {
    auto* unmapped = reinterpret_cast<void*>(std::uintptr_t {0x10});
    char buffer[64];
    RejectsPreservingErrno([&] { X509_NAME_get_entry_nid_postfix(static_cast<const LibreSslX509Name*>(unmapped), 0); });
    RejectsPreservingErrno([&] { X509_NAME_ENTRY_get_data_nid_postfix(static_cast<const LibreSslX509NameEntry*>(unmapped)); });
    RejectsPreservingErrno([&] { X509_NAME_get_index_by_NID_nid_postfix(static_cast<const LibreSslX509Name*>(unmapped), 13, -1); });
    RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(static_cast<const LibreSslX509Name*>(unmapped), 13, buffer, 64); });
    {
        DecodedName decoded;
        decoded.name.entries = static_cast<LibreSslStack*>(unmapped);
        RejectsPreservingErrno([&] { X509_NAME_get_entry_nid_postfix(&decoded.name, 0); });
        RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, buffer, 64); });
    }
    {
        DecodedName decoded;
        decoded.stack.data = static_cast<char**>(unmapped);
        RejectsPreservingErrno([&] { X509_NAME_get_entry_nid_postfix(&decoded.name, 1); });
        RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, buffer, 64); });
    }
    {
        DecodedName decoded;
        decoded.slots[1] = static_cast<char*>(unmapped);
        Require(X509_NAME_get_entry_nid_postfix(&decoded.name, 1) == unmapped);
        RejectsPreservingErrno([&] { X509_NAME_get_index_by_NID_nid_postfix(&decoded.name, 13, -1); });
        Require(X509_NAME_get_index_by_NID_nid_postfix(&decoded.name, 14, -1) == 0);
    }
    {
        DecodedName decoded;
        decoded.entries[1].object = unmapped;
        RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, buffer, 64); });
    }
    {
        DecodedName decoded;
        decoded.objects[2].data = static_cast<const unsigned char*>(unmapped);
        RejectsPreservingErrno([&] { X509_NAME_get_index_by_NID_nid_postfix(&decoded.name, 13, -1); });
        Require(X509_NAME_get_index_by_NID_nid_postfix(&decoded.name, 14, -1) == 0);
    }
    {
        DecodedName decoded;
        decoded.entries[2].value = unmapped;
        Require(X509_NAME_ENTRY_get_data_nid_postfix(&decoded.entries[2]) == unmapped);
        RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, buffer, 64); });
        RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, nullptr, 0); });
    }
    {
        DecodedName decoded;
        decoded.values[2].data = static_cast<unsigned char*>(unmapped);
        std::memset(buffer, 0x7f, sizeof(buffer));
        RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, buffer, 64); });
        Require(BufferHolds(buffer, "", 0));
        Require(X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, nullptr, 64) == 5);
        Require(X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, buffer, 1) == 0);
        Require(BufferHolds(buffer, "", 1));
    }
    {
        DecodedName decoded;
        RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, static_cast<char*>(unmapped), 64); });
        RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, static_cast<char*>(unmapped), 1); });
        Require(X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, static_cast<char*>(unmapped), 0) == -1);
    }
    Require(NoError());
}

static void TestOversizedText() {
    char buffer[64];
    std::memset(buffer, 0x7f, sizeof(buffer));
    {
        DecodedName decoded;
        decoded.values[2].length = INT_MAX;
        decoded.values[2].data = reinterpret_cast<unsigned char*>(std::uintptr_t {0x10});
        Capped([&] { RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, buffer, INT_MAX); }); });
        Require(BufferHolds(buffer, "", 0));
#ifndef _WIN32
        GuardedPage page;
        std::memcpy(page.Tail(5), "first", 5);
        decoded.values[2].data = page.Tail(5);
        Capped([&] { RejectsPreservingErrno([&] { X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, buffer, INT_MAX); }); });
        Require(BufferHolds(buffer, "", 0));
        decoded.values[2].length = 5;
        Capped([&] { Require(X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, buffer, INT_MAX) == 5); });
        Require(std::memcmp(buffer, "first", 6) == 0);
#endif
    }
    {
        DecodedName decoded;
        std::string text(0x2345, 'x');
        for (std::size_t index = 0; index < text.size(); ++index) text[index] = static_cast<char>('a' + index % 26);
        decoded.values[2].length = static_cast<int>(text.size());
        decoded.values[2].data = reinterpret_cast<unsigned char*>(text.data());
        std::string out(text.size() + 2, '\x7f');
        Capped([&] { Require(X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, out.data(), static_cast<int>(out.size())) == static_cast<int>(text.size())); });
        Require(out.compare(0, text.size(), text) == 0 && out[text.size()] == '\0' && out[text.size() + 1] == '\x7f');
        Capped([&] { Require(X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, 13, out.data(), 0x1001) == 0x1000); });
        Require(out.compare(0, 0x1000, text, 0, 0x1000) == 0 && out[0x1000] == '\0');
    }
    Require(NoError());
}

int main() {
    TestGetEntry();
    TestEntryGetData();
    TestIndexByNid();
    TestTextByNid();
    TestObjectTableNids();
    TestUnknownNids();
    TestDynamicNid();
    TestGuestPointers();
    TestOversizedText();
    return 0;
}
