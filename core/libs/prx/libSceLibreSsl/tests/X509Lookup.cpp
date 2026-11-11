#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>

namespace {

struct Stack {
    std::int32_t num;
    char** data;
    std::int32_t sorted;
    std::int32_t numAlloc;
    int (APS5_VABI* comp)(const void*, const void*);
};

using LookupControl = int (APS5_VABI*)(LibreSslX509Lookup*, int, const char*, std::int64_t, char**);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

template<typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    } catch (const std::exception&) {
    }
    Require(false, message);
}

Stack* Lookups(LibreSslX509Store* store) {
    return static_cast<Stack*>(store->lookupMethods);
}

void FileMethod() {
    auto* method = X509_LOOKUP_file_nid_postfix();
    Require(method && method == X509_LOOKUP_file_nid_postfix(), "the file method is one static object");
    Require(std::strcmp(method->name, "Load file into cache") == 0, "file method name");
    Require(!method->newItem && !method->free && !method->init && !method->shutdown, "the file method has no lifecycle callbacks");
    Require(!method->getBySubject && !method->getByIssuerSerial && !method->getByFingerprint && !method->getByAlias,
        "the file method has no lookup callbacks");
    Require(method->ctrl, "the file method has a control callback");
    auto control = reinterpret_cast<LookupControl>(method->ctrl);
    RequireThrows([&] { control(nullptr, 1, "certs.pem", 1, nullptr); }, "loading a file needs the certificate model");
}

void AddLookup() {
    auto* store = X509_STORE_new_nid_postfix();
    auto* method = X509_LOOKUP_file_nid_postfix();
    auto* lookup = X509_STORE_add_lookup_nid_postfix(store, method);
    Require(lookup && lookup->init == 0 && lookup->skip == 0 && lookup->method == method && lookup->methodData == nullptr
        && lookup->storeContext == store, "a new lookup starts as X509_LOOKUP_new leaves it, owned by the store");
    Require(Lookups(store)->num == 1 && Lookups(store)->data[0] == reinterpret_cast<char*>(lookup) && Lookups(store)->sorted == 0,
        "the lookup is pushed on the store's methods");
    Require(X509_STORE_add_lookup_nid_postfix(store, method) == lookup && Lookups(store)->num == 1, "the same method returns the same lookup");
    lookup->init = 1;
    Require(X509_STORE_add_lookup_nid_postfix(store, method) == lookup && lookup->init == 1, "an existing lookup is returned unchanged");

    std::array<LibreSslX509LookupMethod, 5> methods{};
    std::array<LibreSslX509Lookup*, 5> added{};
    for (std::size_t index = 0; index < methods.size(); ++index) {
        added[index] = X509_STORE_add_lookup_nid_postfix(store, &methods[index]);
        Require(added[index] && added[index]->method == &methods[index], "each method gets its own lookup");
    }
    Require(Lookups(store)->num == 6 && Lookups(store)->numAlloc == 8, "the stack doubles once four slots are not enough");
    for (std::size_t index = 0; index < methods.size(); ++index)
        Require(X509_STORE_add_lookup_nid_postfix(store, &methods[index]) == added[index], "lookups are found after growth");
    Require(Lookups(store)->num == 6, "finding a lookup adds nothing");
    X509_STORE_free_nid_postfix(store);
}

void Unsupported() {
    auto* store = X509_STORE_new_nid_postfix();
    RequireInvalidArgument([] { X509_STORE_add_lookup_nid_postfix(nullptr, X509_LOOKUP_file_nid_postfix()); }, "null store");
    RequireInvalidArgument([&] { X509_STORE_add_lookup_nid_postfix(store, nullptr); }, "null method");
    X509_STORE_free_nid_postfix(store);

    store = X509_STORE_new_nid_postfix();
    Lookups(store)->num = 1;
    RequireInvalidArgument([&] { X509_STORE_add_lookup_nid_postfix(store, X509_LOOKUP_file_nid_postfix()); }, "a null listed lookup");

    store = X509_STORE_new_nid_postfix();
    store->param->id = nullptr;
    RequireInvalidArgument([&] { X509_STORE_free_nid_postfix(store); }, "verify parameters without an identity");
}

struct Event {
    char kind;
    LibreSslX509Lookup* lookup;
    void* storeContext;
};

std::array<Event, 16> events{};
int eventCount = 0;
int newItemResult = 1;
int marker = 0;

void Record(char kind, LibreSslX509Lookup* lookup) {
    Require(eventCount < static_cast<int>(events.size()), "too many callbacks");
    events[static_cast<std::size_t>(eventCount++)] = {kind, lookup, lookup->storeContext};
}

int APS5_VABI NewItem(LibreSslX509Lookup* lookup) {
    Record('n', lookup);
    lookup->methodData = reinterpret_cast<char*>(&marker);
    return newItemResult;
}

void APS5_VABI FreeItem(LibreSslX509Lookup* lookup) {
    Record('f', lookup);
}

int APS5_VABI Shutdown(LibreSslX509Lookup* lookup) {
    Record('s', lookup);
    return 0;
}

void Callbacks() {
    LibreSslX509LookupMethod full{};
    full.newItem = reinterpret_cast<void*>(NewItem);
    full.free = reinterpret_cast<void*>(FreeItem);
    full.shutdown = reinterpret_cast<void*>(Shutdown);
    LibreSslX509LookupMethod freeOnly{};
    freeOnly.free = reinterpret_cast<void*>(FreeItem);

    auto* store = X509_STORE_new_nid_postfix();
    eventCount = 0;
    auto* lookup = X509_STORE_add_lookup_nid_postfix(store, &full);
    Require(lookup && eventCount == 1 && events[0].kind == 'n' && events[0].lookup == lookup && events[0].storeContext == nullptr,
        "new_item runs once, before the store is set");
    Require(lookup->storeContext == store && lookup->methodData == reinterpret_cast<char*>(&marker) && lookup->init == 0,
        "the lookup keeps what new_item stored and then gets its store");
    Require(X509_STORE_add_lookup_nid_postfix(store, &full) == lookup && eventCount == 1, "finding a lookup calls nothing");
    auto* second = X509_STORE_add_lookup_nid_postfix(store, &freeOnly);
    Require(second && eventCount == 1, "a method without new_item calls nothing");

    eventCount = 0;
    X509_STORE_free_nid_postfix(store);
    Require(eventCount == 3, "freeing the store calls shutdown and free");
    Require(events[0].kind == 's' && events[0].lookup == lookup && events[1].kind == 'f' && events[1].lookup == lookup,
        "shutdown and then free for the first lookup, though shutdown returns 0");
    Require(events[2].kind == 'f' && events[2].lookup == second, "only free for a method without shutdown");

    store = X509_STORE_new_nid_postfix();
    newItemResult = 0;
    eventCount = 0;
    Require(X509_STORE_add_lookup_nid_postfix(store, &full) == nullptr, "a failed new_item fails the call");
    Require(eventCount == 1 && events[0].kind == 'n' && Lookups(store)->num == 0, "the refused lookup is not pushed and free is not called");
    newItemResult = 1;
    eventCount = 0;
    X509_STORE_free_nid_postfix(store);
    Require(eventCount == 0, "the refused lookup is not freed again");
}

int allocations = 0;
int frees = 0;
int reallocations = 0;
bool failRealloc = false;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    ++allocations;
    return std::malloc(size);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) ++frees;
    std::free(pointer);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    ++reallocations;
    if (failRealloc) return nullptr;
    return std::realloc(pointer, size);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Allocator() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    std::array<LibreSslX509LookupMethod, 4> methods{};

    auto* store = X509_STORE_new_nid_postfix();
    allocations = frees = reallocations = 0;
    for (auto& method : methods) X509_STORE_add_lookup_nid_postfix(store, &method);
    Require(allocations == 4 && reallocations == 1 && frees == 0, "four lookups are malloc'd and the fourth push grows the array");
    X509_STORE_free_nid_postfix(store);
    Require(frees == 4 + 7, "freeing the store frees its lookups");

    store = X509_STORE_new_nid_postfix();
    for (std::size_t index = 0; index < 3; ++index) X509_STORE_add_lookup_nid_postfix(store, &methods[index]);
    allocations = frees = 0;
    failRealloc = true;
    Require(X509_STORE_add_lookup_nid_postfix(store, &methods[3]) == nullptr, "a failed growth fails the call");
    Require(allocations == 1 && frees == 1 && Lookups(store)->num == 3 && Lookups(store)->numAlloc == 4, "a failed growth frees the new lookup and keeps the stack");
    failRealloc = false;
    X509_STORE_free_nid_postfix(store);

    store = X509_STORE_new_nid_postfix();
    for (std::size_t index = 0; index < 3; ++index) X509_STORE_add_lookup_nid_postfix(store, &methods[index]);
    LibreSslX509LookupMethod freeOnly{};
    freeOnly.free = reinterpret_cast<void*>(FreeItem);
    allocations = frees = eventCount = 0;
    failRealloc = true;
    Require(X509_STORE_add_lookup_nid_postfix(store, &freeOnly) == nullptr, "a failed growth fails the call");
    Require(allocations == 1 && frees == 1 && eventCount == 1 && events[0].kind == 'f' && events[0].storeContext == store,
        "a failed push frees the new lookup through the free callback");
    failRealloc = false;
    X509_STORE_free_nid_postfix(store);

    LibreSslX509LookupMethod refusing{};
    refusing.newItem = reinterpret_cast<void*>(NewItem);
    refusing.free = reinterpret_cast<void*>(FreeItem);
    store = X509_STORE_new_nid_postfix();
    newItemResult = 0;
    allocations = frees = eventCount = 0;
    Require(X509_STORE_add_lookup_nid_postfix(store, &refusing) == nullptr, "a failed new_item fails the call");
    Require(allocations == 1 && frees == 1 && eventCount == 1 && events[0].kind == 'n', "a failed new_item frees the lookup without the free callback");
    newItemResult = 1;
    X509_STORE_free_nid_postfix(store);

    store = X509_STORE_new_nid_postfix();
    allocations = frees = 0;
    RequireThrows([&] { X509_STORE_add_lookup_nid_postfix(store, nullptr); }, "null method");
    Require(allocations == 0 && frees == 0 && Lookups(store)->num == 0, "a refused method allocates nothing");
    X509_STORE_free_nid_postfix(store);

    store = X509_STORE_new_nid_postfix();
    auto* lookupMethods = Lookups(store);
    store->lookupMethods = nullptr;
    allocations = frees = 0;
    Require(X509_STORE_add_lookup_nid_postfix(store, &methods[0]) == nullptr && allocations == 1 && frees == 1,
        "a store without a method stack frees the new lookup");
    store->lookupMethods = lookupMethods;
    X509_STORE_free_nid_postfix(store);
}

} // namespace

int main(int argc, char**) {
    if (argc > 1) {
        Allocator();
        return 0;
    }
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    FileMethod();
    AddLookup();
    Unsupported();
    Callbacks();
}
