#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
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

static_assert(sizeof(TestStack) == 32 && offsetof(TestStack, data) == 8);
static_assert(sizeof(TestNameEntry) == 24 && offsetof(TestNameEntry, value) == 8);
static_assert(sizeof(TestName) == 40);

extern "C" {
TestNameEntry* APS5_VABI X509_NAME_get_entry_nid_postfix(const TestName*, int);
void* APS5_VABI X509_NAME_ENTRY_get_data_nid_postfix(const TestNameEntry*);
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
    Require(X509_NAME_get_entry_nid_postfix(&name, 0) == nullptr);
    Require(X509_NAME_get_entry_nid_postfix(&name, -1) == nullptr);
    name.entries = &entries;
    Require(X509_NAME_get_entry_nid_postfix(&name, 0) == &first);
    Require(X509_NAME_get_entry_nid_postfix(&name, 1) == &second);
    Require(X509_NAME_get_entry_nid_postfix(&name, 2) == nullptr);
    Require(X509_NAME_get_entry_nid_postfix(&name, -1) == nullptr);
    Require(X509_NAME_get_entry_nid_postfix(&name, INT_MIN) == nullptr);
    Require(X509_NAME_get_entry_nid_postfix(&name, INT_MAX) == nullptr);
    entries.num = 0;
    Require(X509_NAME_get_entry_nid_postfix(&name, 0) == nullptr);
    entries.num = 3;
    Require(X509_NAME_get_entry_nid_postfix(&name, 2) == nullptr);
    entries.data = nullptr;
    RequireThrows([&] { X509_NAME_get_entry_nid_postfix(&name, 0); });
    Require(X509_NAME_get_entry_nid_postfix(&name, 3) == nullptr);
}

static void TestEntryGetData() {
    int value = 0;
    TestNameEntry entry {};
    Require(X509_NAME_ENTRY_get_data_nid_postfix(nullptr) == nullptr);
    Require(X509_NAME_ENTRY_get_data_nid_postfix(&entry) == nullptr);
    entry.value = &value;
    Require(X509_NAME_ENTRY_get_data_nid_postfix(&entry) == &value);
    entry.object = &entry;
    entry.set = 7;
    Require(X509_NAME_ENTRY_get_data_nid_postfix(&entry) == &value);
}

int main() {
    TestGetEntry();
    TestEntryGetData();
    return 0;
}
