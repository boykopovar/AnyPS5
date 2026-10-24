#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslStack {
    int num;
    char** data;
    int sorted;
    int numAlloc;
    void* comp;
};

struct LibreSslX509NameEntry {
    void* object;
    void* value;
    int set;
    int size;
};

struct LibreSslX509Name {
    LibreSslStack* entries;
    int modified;
    void* bytes;
    unsigned char* canonEnc;
    int canonEnclen;
};

static_assert(sizeof(LibreSslStack) == 32);
static_assert(offsetof(LibreSslStack, data) == 8);
static_assert(sizeof(LibreSslX509NameEntry) == 24);
static_assert(offsetof(LibreSslX509NameEntry, value) == 8);
static_assert(sizeof(LibreSslX509Name) == 40);
static_assert(offsetof(LibreSslX509Name, entries) == 0);

extern "C" {

LibreSslX509NameEntry* APS5_VABI X509_NAME_get_entry_nid_postfix(const LibreSslX509Name* name, int loc) {
    if (!name || loc < 0 || !name->entries || name->entries->num <= loc) return nullptr;
    if (!name->entries->data) throw std::invalid_argument(std::string(__func__) + ": null entry array");
    return reinterpret_cast<LibreSslX509NameEntry*>(name->entries->data[loc]);
}

}
