#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

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

static void TestDynamicNid() {
    OBJ_cleanup_nid_postfix();
    ERR_clear_error_nid_postfix();
    const int nid = OBJ_create_nid_postfix("1.2.3.4", "testName", "Test Name");
    Require(nid >= 956);
    DecodedName decoded;
    bool indexAccepted = false;
    try {
        const int index = X509_NAME_get_index_by_NID_nid_postfix(&decoded.name, nid, -1);
        indexAccepted = index == 6;
        if (!indexAccepted) std::fprintf(stderr, "dynamic X509_NAME index: expected 6 or not implemented, got %d\n", index);
    } catch (const std::runtime_error& error) {
        Require(std::strstr(error.what(), "X509_NAME_get_index_by_NID") && std::strstr(error.what(), "not implemented"));
        indexAccepted = true;
    }
    bool textAccepted = false;
    char buffer[2] {};
    try {
        const int length = X509_NAME_get_text_by_NID_nid_postfix(&decoded.name, nid, buffer, sizeof(buffer));
        textAccepted = length == 1 && std::strcmp(buffer, "x") == 0;
        if (!textAccepted) std::fprintf(stderr, "dynamic X509_NAME text: expected x with length 1 or not implemented, got length %d\n", length);
    } catch (const std::runtime_error& error) {
        Require(std::strstr(error.what(), "X509_NAME_get_text_by_NID") && std::strstr(error.what(), "not implemented"));
        textAccepted = true;
    }
    OBJ_cleanup_nid_postfix();
    ERR_clear_error_nid_postfix();
    Require(indexAccepted);
    Require(textAccepted);
}

int main() {
    TestGetEntry();
    TestEntryGetData();
    TestIndexByNid();
    TestTextByNid();
    TestDynamicNid();
    return 0;
}
