#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

constexpr std::uint64_t MallocFailureCode = (11ull << 24) | (0xfffull << 12) | 65;

struct Object {
    std::int32_t type;
    LibreSslX509* certificate;
};

struct Stack {
    std::int32_t num;
    char** data;
    std::int32_t sorted;
    std::int32_t numAlloc;
    int (APS5_VABI* comp)(const void*, const void*);
};

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

void RequireEmptyStack(const void* pointer, bool comparator, const char* message) {
    const auto* stack = static_cast<const Stack*>(pointer);
    Require(stack && stack->num == 0 && stack->sorted == 0 && stack->numAlloc == 4 && stack->data, message);
    Require((stack->comp != nullptr) == comparator, message);
    for (int index = 0; index < 4; ++index) Require(stack->data[index] == nullptr, message);
}

char* GuestString(const char* text) {
    auto* copy = static_cast<char*>(malloc_nid_postfix(std::strlen(text) + 1));
    std::strcpy(copy, text);
    return copy;
}

void NewStore() {
    auto* store = X509_STORE_new_nid_postfix();
    Require(store, "store is allocated");
    Require(store->cache == 1 && store->references == 1 && store->exData == nullptr, "store fields start as LibreSSL sets them");
    RequireEmptyStack(store->objects, true, "object cache is an empty stack with a comparator");
    RequireEmptyStack(store->lookupMethods, false, "lookup methods are an empty stack without a comparator");
    for (void* callback : store->callbacks) Require(callback == nullptr, "callbacks start unset");
    const auto* param = store->param;
    Require(param && param->name == nullptr && param->checkTime == 0 && param->inheritanceFlags == 0 && param->flags == 0
        && param->purpose == 0 && param->trust == 0 && param->depth == -1 && param->policies == nullptr, "verify parameters start zeroed with depth -1");
    const auto* id = param->id;
    Require(id && id->hosts == nullptr && id->hostFlags == 0 && id->peerName == nullptr && id->email == nullptr
        && id->emailLength == 0 && id->ip == nullptr && id->ipLength == 0, "verify parameter identity starts zeroed");
    X509_STORE_free_nid_postfix(store);
}

void Flags() {
    auto* store = X509_STORE_new_nid_postfix();
    Require(X509_STORE_set_flags_nid_postfix(store, 0x1) == 1 && store->param->flags == 0x1, "flags are set");
    Require(X509_STORE_set_flags_nid_postfix(store, 0x8000) == 1 && store->param->flags == 0x8001, "flags accumulate");
    Require(X509_STORE_set_flags_nid_postfix(store, 0) == 1 && store->param->flags == 0x8001, "no flags change nothing");
    for (std::uint64_t policy : {0x100ull, 0x200ull, 0x400ull, 0x80ull}) {
        store->param->flags = 0;
        Require(X509_STORE_set_flags_nid_postfix(store, policy) == 1 && store->param->flags == (policy | 0x80), "a policy flag turns on the policy check");
    }
    store->param->flags = 0;
    Require(X509_STORE_set_flags_nid_postfix(store, 0x8000000000000000ull) == 1 && store->param->flags == 0x8000000000000000ull, "all 64 flag bits are kept");
    Require(store->param->inheritanceFlags == 0, "inheritance flags are left alone");
    RequireThrows([] { X509_STORE_set_flags_nid_postfix(nullptr, 1); }, "null store");
    auto* param = store->param;
    store->param = nullptr;
    RequireThrows([&] { X509_STORE_set_flags_nid_postfix(store, 1); }, "store without verify parameters");
    store->param = param;
    X509_STORE_free_nid_postfix(store);
}

void References() {
    X509_STORE_free_nid_postfix(nullptr);
    auto* store = X509_STORE_new_nid_postfix();
    store->references = 2;
    X509_STORE_free_nid_postfix(store);
    Require(store->references == 1 && store->param && store->param->depth == -1, "a shared store stays allocated");
    X509_STORE_free_nid_postfix(store);
}

void Identity() {
    auto* store = X509_STORE_new_nid_postfix();
    auto* id = store->param->id;
    id->peerName = GuestString("peer");
    id->email = GuestString("user@example.com");
    id->ip = reinterpret_cast<unsigned char*>(GuestString("ip"));
    auto* hosts = static_cast<Stack*>(malloc_nid_postfix(sizeof(Stack)));
    hosts->data = static_cast<char**>(malloc_nid_postfix(sizeof(char*) * 4));
    hosts->data[0] = GuestString("a.example");
    hosts->data[1] = nullptr;
    hosts->data[2] = GuestString("b.example");
    hosts->num = 3;
    hosts->numAlloc = 4;
    hosts->sorted = 0;
    hosts->comp = nullptr;
    id->hosts = hosts;
    store->exData = malloc_nid_postfix(sizeof(Stack));
    static_cast<Stack*>(store->exData)->data = nullptr;
    X509_STORE_free_nid_postfix(store);
}

void Unsupported() {
    auto* store = X509_STORE_new_nid_postfix();
    static_cast<Stack*>(store->objects)->num = 2;
    X509_STORE_free_nid_postfix(store);

    store = X509_STORE_new_nid_postfix();
    static_cast<Stack*>(store->objects)->data = nullptr;
    static_cast<Stack*>(store->objects)->num = 1;
    RequireThrows([&] { X509_STORE_free_nid_postfix(store); }, "null element array");
}

const char* Host(const LibreSslX509VerifyParam* param, int index) {
    const auto* hosts = static_cast<const Stack*>(param->id->hosts);
    return hosts && index < hosts->num ? hosts->data[index] : nullptr;
}

int HostCount(const LibreSslX509VerifyParam* param) {
    const auto* hosts = static_cast<const Stack*>(param->id->hosts);
    return hosts ? hosts->num : -1;
}

void Hosts() {
    auto* store = X509_STORE_new_nid_postfix();
    auto* param = store->param;
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(param, "example.com", 0) == 1, "a terminated name is set");
    Require(HostCount(param) == 1 && std::strcmp(Host(param, 0), "example.com") == 0, "the name is copied into a one-name list");
    const auto* hosts = static_cast<const Stack*>(param->id->hosts);
    Require(hosts->comp == nullptr && hosts->numAlloc == 4 && hosts->sorted == 0, "the host list is a plain stack");
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(param, "www.example.org.extra", 15) == 1, "a counted name is set");
    Require(HostCount(param) == 1 && std::strcmp(Host(param, 0), "www.example.org") == 0, "set replaces the list and copies the counted bytes");
    char unterminated[] = {'a', '.', 'b'};
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(param, unterminated, sizeof(unterminated)) == 1 && std::strcmp(Host(param, 0), "a.b") == 0,
        "a counted name needs no terminator");
    const char embedded[] = {'a', '\0', 'b'};
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(param, embedded, sizeof(embedded)) == 0, "an embedded NUL is refused");
    Require(HostCount(param) == 1 && std::strcmp(Host(param, 0), "a.b") == 0, "a refused name keeps the list");
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(param, "", 0) == 1 && param->id->hosts == nullptr, "an empty name clears the list");
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(param, "c.d", 0) == 1, "a name after clearing");
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(param, nullptr, 5) == 1 && param->id->hosts == nullptr, "a null name clears the list");
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(param, nullptr, 0) == 1 && param->id->hosts == nullptr, "clearing an empty list");
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(param, "last.example", 0) == 1, "a name that the store frees");
    RequireThrows([] { X509_VERIFY_PARAM_set1_host_nid_postfix(nullptr, "a", 0); }, "null verify parameters");
    auto* id = param->id;
    param->id = nullptr;
    RequireThrows([&] { X509_VERIFY_PARAM_set1_host_nid_postfix(param, "a", 0); }, "verify parameters without an identity");
    param->id = id;
    X509_STORE_free_nid_postfix(store);
}

Bytes Cat(std::initializer_list<Bytes> parts) {
    Bytes joined;
    for (const auto& part : parts) joined.insert(joined.end(), part.begin(), part.end());
    return joined;
}

Bytes Tlv(unsigned char tag, const Bytes& content) {
    Bytes encoded{tag, static_cast<unsigned char>(content.size())};
    encoded.insert(encoded.end(), content.begin(), content.end());
    return encoded;
}

Bytes Name(const std::string& commonName) {
    if (commonName.empty()) return Tlv(0x30, {});
    return Tlv(0x30, Tlv(0x31, Tlv(0x30, Cat({Tlv(0x06, {0x55, 0x04, 0x03}), Tlv(0x0c, Bytes(commonName.begin(), commonName.end()))}))));
}

LibreSslX509* Certificate(const std::string& subject, unsigned char serial = 1) {
    const Bytes algorithm = Tlv(0x30, Tlv(0x06, {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b}));
    const Bytes validity = Tlv(0x30, Cat({Tlv(0x17, Bytes(13, '0')), Tlv(0x17, Bytes(13, '0'))}));
    const Bytes key = Tlv(0x30, Cat({Tlv(0x30, Tlv(0x06, {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01})), Tlv(0x03, {0x00, 0x05})}));
    const Bytes info = Tlv(0x30, Cat({Tlv(0x02, {serial}), algorithm, Name("Issuer"), validity, Name(subject), key}));
    const Bytes encoded = Tlv(0x30, Cat({info, algorithm, Tlv(0x03, {0x00, 0x01})}));
    const unsigned char* in = encoded.data();
    LibreSslX509* certificate = d2i_X509_nid_postfix(nullptr, &in, static_cast<std::int64_t>(encoded.size()));
    Require(certificate && in == encoded.data() + encoded.size(), "a test certificate decodes");
    return certificate;
}

const Object* StoredAt(const LibreSslX509Store* store, int index) {
    return reinterpret_cast<const Object*>(static_cast<const Stack*>(store->objects)->data[index]);
}

void AddCertificates() {
    auto* store = X509_STORE_new_nid_postfix();
    auto* objects = static_cast<Stack*>(store->objects);
    LibreSslX509* b = Certificate("b");
    LibreSslX509* a = Certificate("a");
    LibreSslX509* longer = Certificate("aa");
    LibreSslX509* c = Certificate("c");
    LibreSslX509* empty = Certificate("");
    ERR_clear_error_nid_postfix();
    Require(X509_STORE_add_cert_nid_postfix(store, nullptr) == 0 && objects->num == 0 && ERR_peek_error_nid_postfix() == 0, "a null certificate is refused without an error");
    Require(X509_STORE_add_cert_nid_postfix(store, b) == 1 && b->references == 2, "an added certificate gains a reference");
    Require(objects->num == 1 && objects->sorted == 0 && StoredAt(store, 0)->type == 1 && StoredAt(store, 0)->certificate == b,
        "the store holds a certificate object for it");
    Require(X509_STORE_add_cert_nid_postfix(store, longer) == 1 && X509_STORE_add_cert_nid_postfix(store, a) == 1, "certificates with other subjects are added");
    Require(objects->num == 3 && objects->sorted == 0, "each push leaves the stack unsorted");
    Require(StoredAt(store, 0)->certificate == b && StoredAt(store, 1)->certificate == longer && StoredAt(store, 2)->certificate == a,
        "a shorter canonical encoding sorts first, and the new certificate goes last");
    Require(X509_STORE_add_cert_nid_postfix(store, c) == 1 && objects->num == 4 && objects->numAlloc == 8, "a fourth certificate grows the stack");
    Require(StoredAt(store, 0)->certificate == a && StoredAt(store, 1)->certificate == b && StoredAt(store, 2)->certificate == longer
        && StoredAt(store, 3)->certificate == c, "the lookup sorts the stored certificates before the push");
    LibreSslX509* last = Certificate("z");
    Require(X509_STORE_add_cert_nid_postfix(store, empty) == 1 && X509_STORE_add_cert_nid_postfix(store, last) == 1 && objects->num == 6, "two more certificates");
    Require(StoredAt(store, 0)->certificate == empty && StoredAt(store, 1)->certificate == a && StoredAt(store, 2)->certificate == b
        && StoredAt(store, 3)->certificate == c && StoredAt(store, 4)->certificate == longer && StoredAt(store, 5)->certificate == last,
        "an empty subject sorts first");
    Require(ERR_peek_error_nid_postfix() == 0, "adding raises no error");

    auto* shared = X509_STORE_new_nid_postfix();
    Require(X509_STORE_add_cert_nid_postfix(shared, a) == 1 && a->references == 3, "a certificate can sit in two stores");
    X509_STORE_free_nid_postfix(shared);
    Require(a->references == 2, "freeing a store drops its reference");
    X509_STORE_free_nid_postfix(store);
    for (auto* certificate : {a, b, c, longer, empty, last}) {
        Require(certificate->references == 1, "freeing the store returns each certificate to its caller");
        X509_free_nid_postfix(certificate);
    }
}

void SameSubject() {
    auto* store = X509_STORE_new_nid_postfix();
    LibreSslX509* first = Certificate("same", 1);
    LibreSslX509* second = Certificate("same", 2);
    const auto* objects = static_cast<const Stack*>(store->objects);
    Require(X509_STORE_add_cert_nid_postfix(store, first) == 1, "the first certificate is added");
    RequireThrows([&] { X509_STORE_add_cert_nid_postfix(store, second); }, "a second certificate with the subject needs X509_cmp");
    Require(second->references == 1 && first->references == 2 && objects->num == 1, "a throwing add gives back its reference");
    RequireThrows([&] { X509_STORE_add_cert_nid_postfix(store, first); }, "adding the same certificate again needs X509_cmp");
    Require(first->references == 2 && objects->num == 1, "a throwing add of a stored certificate keeps only the store's reference");
    LibreSslX509* modified = Certificate("other");
    modified->certificateInfo->subject->modified = 1;
    RequireThrows([&] { X509_STORE_add_cert_nid_postfix(store, modified); }, "a modified subject needs re-encoding");
    Require(modified->references == 1 && objects->num == 1, "a modified subject leaves the certificate and the store alone");
    RequireThrows([&] { X509_STORE_add_cert_nid_postfix(nullptr, first); }, "null store");
    Require(first->references == 2, "a null store takes no reference");
    modified->certificateInfo->subject->modified = 0;
    X509_STORE_free_nid_postfix(store);
    for (auto* certificate : {first, second, modified}) {
        Require(certificate->references == 1, "every certificate is back to its caller's reference");
        X509_free_nid_postfix(certificate);
    }
}

void Comparator() {
    auto* store = X509_STORE_new_nid_postfix();
    const auto compare = static_cast<Stack*>(store->objects)->comp;
    Object certificate{1, nullptr};
    Object crl{2, nullptr};
    Object other{3, nullptr};
    Object unknown{3, nullptr};
    const Object* left = &certificate;
    const Object* right = &crl;
    Require(compare(&left, &right) == -1 && compare(&right, &left) == 1, "objects of different types compare by type");
    left = &other;
    right = &unknown;
    Require(compare(&left, &right) == 0, "objects of an unknown type compare equal");
    left = &crl;
    right = &crl;
    RequireThrows([&] { compare(&left, &right); }, "CRLs compare by issuer");
    left = &certificate;
    right = &certificate;
    RequireThrows([&] { compare(&left, &right); }, "a certificate object without a certificate");
    X509_STORE_free_nid_postfix(store);

    store = X509_STORE_new_nid_postfix();
    auto* object = static_cast<Object*>(malloc_nid_postfix(sizeof(Object)));
    *object = {2, nullptr};
    static_cast<Stack*>(store->objects)->data[0] = reinterpret_cast<char*>(object);
    static_cast<Stack*>(store->objects)->num = 1;
    RequireThrows([&] { X509_STORE_free_nid_postfix(store); }, "freeing a stored CRL");
}

int allocations = 0;
int callocs = 0;
int frees = 0;
int reallocations = 0;
int failCallocAt = 0;
int failAllocationAt = 0;
int failReallocationAt = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (++allocations == failAllocationAt) return nullptr;
    return std::malloc(size);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) ++frees;
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    if (++callocs == failCallocAt) return nullptr;
    return std::calloc(count, size);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (++reallocations == failReallocationAt) return nullptr;
    return std::realloc(pointer, size);
}
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Allocator() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    auto* store = X509_STORE_new_nid_postfix();
    Require(store && allocations == 5 && callocs == 2 && frees == 0, "the store, two stacks and their arrays use malloc, the verify parameters calloc");
    X509_STORE_free_nid_postfix(store);
    Require(frees == 7, "free releases every block new allocated");
    for (int failing = 1; failing <= 2; ++failing) {
        allocations = callocs = frees = 0;
        failCallocAt = failing;
        Require(X509_STORE_new_nid_postfix() == nullptr, "a failed verify parameter allocation fails the store");
        Require(frees == allocations + failing - 1, "a failed store releases what it allocated");
    }
    failCallocAt = 0;

    store = X509_STORE_new_nid_postfix();
    allocations = callocs = frees = 0;
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(store->param, "a.example", 0) == 1 && allocations == 3 && frees == 0,
        "a first name allocates its copy, the stack and its array");
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(store->param, "b.example", 0) == 1 && allocations == 6 && frees == 3,
        "a new name frees the old list");
    allocations = frees = 0;
    failAllocationAt = 1;
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(store->param, "c.example", 0) == 0 && store->param->id->hosts == nullptr && frees == 3,
        "a failed copy leaves the list cleared");
    allocations = frees = 0;
    failAllocationAt = 2;
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(store->param, "d.example", 0) == 0 && store->param->id->hosts == nullptr && frees == 1,
        "a failed stack frees the copy");
    failAllocationAt = 0;
    X509_STORE_free_nid_postfix(store);

    store = X509_STORE_new_nid_postfix();
    std::array<LibreSslX509*, 4> certificates{Certificate("a"), Certificate("b"), Certificate("c"), Certificate("d")};
    ERR_clear_error_nid_postfix();
    allocations = frees = 0;
    failAllocationAt = 1;
    Require(X509_STORE_add_cert_nid_postfix(store, certificates[0]) == 0 && certificates[0]->references == 1 && frees == 0,
        "a failed object allocation leaves the certificate alone");
    Require(ERR_get_error_nid_postfix() == MallocFailureCode && ERR_get_error_nid_postfix() == 0, "a failed object allocation is a malloc failure");
    failAllocationAt = 0;
    for (int index = 0; index < 3; ++index) Require(X509_STORE_add_cert_nid_postfix(store, certificates[index]) == 1, "certificates fill the stack");
    allocations = frees = reallocations = 0;
    failReallocationAt = 1;
    Require(X509_STORE_add_cert_nid_postfix(store, certificates[3]) == 0 && certificates[3]->references == 1 && allocations == 1 && frees == 1,
        "a failed push frees the object and drops its reference");
    Require(static_cast<Stack*>(store->objects)->num == 3 && ERR_get_error_nid_postfix() == MallocFailureCode && ERR_get_error_nid_postfix() == 0,
        "a failed push keeps the stack and is a malloc failure");
    failReallocationAt = 0;
    Require(X509_STORE_add_cert_nid_postfix(store, certificates[3]) == 1, "the push succeeds once memory is back");
    allocations = frees = 0;
    RequireThrows([&] { X509_STORE_add_cert_nid_postfix(store, certificates[0]); }, "adding a stored certificate again throws");
    Require(allocations == 1 && frees == 1 && certificates[0]->references == 2, "a throwing add frees its object and drops its reference");
    X509_STORE_free_nid_postfix(store);
    for (auto* certificate : certificates) {
        Require(certificate->references == 1, "the store gave back every reference");
        X509_free_nid_postfix(certificate);
    }
}

} // namespace

int main(int argc, char**) {
    if (argc > 1) {
        Allocator();
        return 0;
    }
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    NewStore();
    Flags();
    References();
    Identity();
    Unsupported();
    Hosts();
    AddCertificates();
    SameSubject();
    Comparator();
}
