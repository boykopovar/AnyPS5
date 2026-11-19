#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>

static_assert(sizeof(LibreSslX509VerifyParamId) == 56);
static_assert(offsetof(LibreSslX509VerifyParamId, hostFlags) == 8);
static_assert(offsetof(LibreSslX509VerifyParamId, peerName) == 16);
static_assert(offsetof(LibreSslX509VerifyParamId, ip) == 40);
static_assert(sizeof(LibreSslX509VerifyParam) == 64);
static_assert(offsetof(LibreSslX509VerifyParam, flags) == 24);
static_assert(offsetof(LibreSslX509VerifyParam, purpose) == 32);
static_assert(offsetof(LibreSslX509VerifyParam, depth) == 40);
static_assert(offsetof(LibreSslX509VerifyParam, policies) == 48);
static_assert(offsetof(LibreSslX509VerifyParam, id) == 56);
static_assert(sizeof(LibreSslX509Store) == 136);
static_assert(offsetof(LibreSslX509Store, objects) == 8);
static_assert(offsetof(LibreSslX509Store, lookupMethods) == 16);
static_assert(offsetof(LibreSslX509Store, param) == 24);
static_assert(offsetof(LibreSslX509Store, callbacks) == 32);
static_assert(offsetof(LibreSslX509Store, exData) == 120);
static_assert(offsetof(LibreSslX509Store, references) == 128);
static_assert(sizeof(LibreSslX509LookupMethod) == 80);
static_assert(offsetof(LibreSslX509LookupMethod, shutdown) == 32);
static_assert(offsetof(LibreSslX509LookupMethod, ctrl) == 40);
static_assert(sizeof(LibreSslX509Lookup) == 32);
static_assert(offsetof(LibreSslX509Lookup, method) == 8);
static_assert(offsetof(LibreSslX509Lookup, methodData) == 16);
static_assert(offsetof(LibreSslX509Lookup, storeContext) == 24);

namespace {

constexpr int MinimumNodes = 4;
constexpr int X509Library = 11;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int CertAlreadyInHashTable = 101;
constexpr std::uint64_t PolicyCheck = 0x80;
constexpr std::uint64_t PolicyMask = 0x80 | 0x100 | 0x200 | 0x400;

using Compare = int (APS5_VABI*)(const void*, const void*);

struct Stack {
    std::int32_t num;
    char** data;
    std::int32_t sorted;
    std::int32_t numAlloc;
    Compare comp;
};

static_assert(sizeof(Stack) == 32);
static_assert(offsetof(Stack, data) == 8);
static_assert(offsetof(Stack, numAlloc) == 20);
static_assert(offsetof(Stack, comp) == 24);

char** Elements(const Stack* stack, const char* function);

const LibreSslX509* ObjectCertificate(const LibreSslX509Object* object, const char* function) {
    if (!object) throw std::invalid_argument(std::string(function) + ": null store object");
    const auto* certificate = static_cast<const LibreSslX509*>(object->data);
    if (!certificate) throw std::invalid_argument(std::string(function) + ": null object certificate");
    return certificate;
}

const LibreSslX509CertInfo& ObjectCertificateInfo(const LibreSslX509Object* object, const char* function) {
    const auto* info = static_cast<const LibreSslX509CertInfo*>(ObjectCertificate(object, function)->certificateInfo);
    if (!info) throw std::invalid_argument(std::string(function) + ": null object certificate info");
    return *info;
}

const LibreSslX509Name& ObjectSubject(const LibreSslX509Object* object, const char* function) {
    const auto* name = static_cast<const LibreSslX509Name*>(ObjectCertificateInfo(object, function).subject);
    if (!name) throw std::invalid_argument(std::string(function) + ": null object subject name");
    if (name->canonEnclen < 0)
        throw std::invalid_argument(std::string(function) + ": negative canonical encoding length");
    if (!name->canonEnc && name->canonEnclen != 0)
        throw std::invalid_argument(std::string(function) + ": subject name without a canonical encoding");
    return *name;
}

int CompareObjectsImpl(const LibreSslX509Object* first, const LibreSslX509Object* second, const char* function) {
    if (first->type != second->type) return first->type < second->type ? -1 : 1;
    if (first->type != LibreSslX509LookupX509)
        throw std::invalid_argument(std::string(function) + ": unsupported object type");
    const auto& firstSubject = ObjectSubject(first, function);
    const auto& secondSubject = ObjectSubject(second, function);
    if (firstSubject.canonEnclen != secondSubject.canonEnclen)
        return firstSubject.canonEnclen < secondSubject.canonEnclen ? -1 : 1;
    if (firstSubject.canonEnclen == 0) return 0;
    return std::memcmp(firstSubject.canonEnc, secondSubject.canonEnc, static_cast<std::size_t>(firstSubject.canonEnclen));
}

bool SameCertificate(const LibreSslX509Object* first, const LibreSslX509Object* second, const char* function) {
    const auto& firstEncoding = ObjectCertificateInfo(first, function).enc;
    const auto& secondEncoding = ObjectCertificateInfo(second, function).enc;
    if (firstEncoding.modified != 0 || secondEncoding.modified != 0)
        throw std::invalid_argument(std::string(function) + ": modified certificate info");
    if (firstEncoding.len < 0) throw std::invalid_argument(std::string(function) + ": negative certificate info length");
    if (firstEncoding.len != secondEncoding.len) return false;
    if (firstEncoding.len == 0) return true;
    if (!firstEncoding.enc || !secondEncoding.enc)
        throw std::invalid_argument(std::string(function) + ": certificate info without an encoding");
    return std::memcmp(firstEncoding.enc, secondEncoding.enc, static_cast<std::size_t>(firstEncoding.len)) == 0;
}

int APS5_VABI CompareObjects(const void* first, const void* second) {
    return CompareObjectsImpl(*static_cast<const LibreSslX509Object* const*>(first),
        *static_cast<const LibreSslX509Object* const*>(second), "X509_STORE object comparison");
}

bool FindObject(Stack* stack, const LibreSslX509Object* object) {
    if (!stack || stack->num <= 0) return false;
    char** const elements = Elements(stack, __func__);
    for (int index = 0; index < stack->num; ++index) {
        const auto* element = reinterpret_cast<const LibreSslX509Object*>(elements[index]);
        if (CompareObjectsImpl(element, object, __func__) != 0) continue;
        if (SameCertificate(element, object, __func__)) return true;
    }
    return false;
}

int APS5_VABI LoadFile(LibreSslX509Lookup*, int, const char*, std::int64_t, char**) {
    NotImplemented_nid_no_patch("X509_LOOKUP_file: loading certificates and CRLs from a file");
    return 0;
}

LibreSslX509LookupMethod fileLookup{"Load file into cache", nullptr, nullptr, nullptr, nullptr,
    reinterpret_cast<void*>(LoadFile), nullptr, nullptr, nullptr, nullptr};

Stack* NewStack(Compare compare) {
    auto* stack = static_cast<Stack*>(malloc_nid_postfix(sizeof(Stack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(malloc_nid_postfix(sizeof(char*) * MinimumNodes));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    std::fill_n(stack->data, MinimumNodes, nullptr);
    stack->comp = compare;
    stack->numAlloc = MinimumNodes;
    stack->num = 0;
    stack->sorted = 0;
    return stack;
}

void FreeStack(Stack* stack) {
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

char** Elements(const Stack* stack, const char* function) {
    if (!stack->data) throw std::invalid_argument(std::string(function) + ": null element array");
    return stack->data;
}

bool Push(Stack* stack, void* element, const char* function) {
    if (!stack) return false;
    if (stack->numAlloc <= 0 || stack->num < 0) throw std::invalid_argument(std::string(function) + ": invalid stack size");
    if (stack->numAlloc <= stack->num + 1) {
        auto* grown = static_cast<char**>(realloc_nid_postfix(stack->data, static_cast<std::size_t>(stack->numAlloc) * 2 * sizeof(char*)));
        if (!grown) return false;
        stack->data = grown;
        stack->numAlloc *= 2;
    }
    stack->data[stack->num] = static_cast<char*>(element);
    ++stack->num;
    stack->sorted = 0;
    return true;
}

using LookupCallback = int (APS5_VABI*)(LibreSslX509Lookup*);
using LookupFree = void (APS5_VABI*)(LibreSslX509Lookup*);

void FreeLookup(LibreSslX509Lookup* lookup, bool shutdown, const char* function) {
    if (!lookup) NotImplemented_nid_no_patch(function);
    const auto* method = lookup->method;
    if (shutdown && method && method->shutdown) reinterpret_cast<LookupCallback>(method->shutdown)(lookup);
    if (method && method->free) reinterpret_cast<LookupFree>(method->free)(lookup);
    free_nid_postfix(lookup);
}

bool HoldsElements(const Stack* stack, const char* function) {
    if (!stack || stack->num <= 0) return false;
    char** const elements = Elements(stack, function);
    return std::any_of(elements, elements + stack->num, [](const char* element) { return element != nullptr; });
}

void FreeStrings(Stack* stack, const char* function) {
    if (!stack) return;
    if (stack->num > 0) {
        char** const elements = Elements(stack, function);
        std::for_each(elements, elements + stack->num, [](char* element) { free_nid_postfix(element); });
    }
    FreeStack(stack);
}

LibreSslX509VerifyParam* NewVerifyParam() {
    auto* param = static_cast<LibreSslX509VerifyParam*>(calloc_nid_postfix(1, sizeof(LibreSslX509VerifyParam)));
    if (!param) return nullptr;
    auto* id = static_cast<LibreSslX509VerifyParamId*>(calloc_nid_postfix(1, sizeof(LibreSslX509VerifyParamId)));
    if (!id) {
        free_nid_postfix(param);
        return nullptr;
    }
    param->id = id;
    param->depth = -1;
    return param;
}

void FreeVerifyParam(LibreSslX509VerifyParam* param, const char* function) {
    if (!param) return;
    auto* policies = static_cast<Stack*>(param->policies);
    if (HoldsElements(policies, function)) NotImplemented_nid_no_patch("X509_VERIFY_PARAM policy objects");
    FreeStack(policies);
    auto* id = param->id;
    if (!id) throw std::invalid_argument(std::string(function) + ": missing verify parameter identity");
    FreeStrings(static_cast<Stack*>(id->hosts), function);
    free_nid_postfix(id->peerName);
    free_nid_postfix(id->email);
    free_nid_postfix(id->ip);
    free_nid_postfix(id);
    free_nid_postfix(param);
}

bool ReadHostName(const char* name, std::size_t length, std::string& host) {
    constexpr std::uintptr_t Page = 4096;
    const bool counted = length != 0;
    auto address = reinterpret_cast<std::uintptr_t>(name);
    char chunk[Page];
    while (!counted || host.size() < length) {
        std::size_t size = Page - address % Page;
        if (counted) size = std::min<std::size_t>(size, length - host.size());
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(chunk, reinterpret_cast<const void*>(address), size);
        errno = savedErrno;
        if (!copied) throw std::invalid_argument("X509_VERIFY_PARAM_set1_host: unreadable host name");
        const auto* end = static_cast<const char*>(std::memchr(chunk, '\0', size));
        host.append(chunk, end ? static_cast<std::size_t>(end - chunk) : size);
        if (end) return !counted;
        address += size;
    }
    return true;
}

} // namespace

extern "C" {

LibreSslX509VerifyParam* LibreSslNewVerifyParam_nid_no_patch() {
    return NewVerifyParam();
}

void LibreSslFreeVerifyParam_nid_no_patch(LibreSslX509VerifyParam* param, const char* function) {
    FreeVerifyParam(param, function);
}

LibreSslX509Store* APS5_VABI X509_STORE_new_nid_postfix(void) {
    auto* store = static_cast<LibreSslX509Store*>(malloc_nid_postfix(sizeof(LibreSslX509Store)));
    if (!store) return nullptr;
    auto* objects = NewStack(CompareObjects);
    auto* lookupMethods = NewStack(nullptr);
    store->objects = objects;
    store->cache = 1;
    store->lookupMethods = lookupMethods;
    store->param = NewVerifyParam();
    if (!store->param) {
        FreeStack(lookupMethods);
        FreeStack(objects);
        free_nid_postfix(store);
        return nullptr;
    }
    std::fill(std::begin(store->callbacks), std::end(store->callbacks), nullptr);
    store->exData = nullptr;
    store->references = 1;
    return store;
}

void APS5_VABI X509_STORE_free_nid_postfix(LibreSslX509Store* store) {
    if (!store) return;
    if (std::atomic_ref<std::int32_t>(store->references).fetch_sub(1) - 1 > 0) return;
    auto* lookupMethods = static_cast<Stack*>(store->lookupMethods);
    if (lookupMethods && lookupMethods->num > 0) {
        char** const lookups = Elements(lookupMethods, __func__);
        for (int index = 0; index < lookupMethods->num; ++index)
            FreeLookup(reinterpret_cast<LibreSslX509Lookup*>(lookups[index]), true, __func__);
    }
    FreeStack(lookupMethods);
    auto* objects = static_cast<Stack*>(store->objects);
    if (HoldsElements(objects, __func__)) NotImplemented_nid_no_patch("X509_STORE_free: stored certificates and CRLs");
    FreeStack(objects);
    FreeStack(static_cast<Stack*>(store->exData));
    FreeVerifyParam(store->param, __func__);
    free_nid_postfix(store);
}

int APS5_VABI X509_STORE_set_flags_nid_postfix(LibreSslX509Store* store, std::uint64_t flags) {
    if (!store || !store->param) throw std::invalid_argument("X509_STORE_set_flags requires a store and verify parameters");
    store->param->flags |= flags;
    if (flags & PolicyMask) store->param->flags |= PolicyCheck;
    return 1;
}

LibreSslX509LookupMethod* APS5_VABI X509_LOOKUP_file_nid_postfix(void) {
    return &fileLookup;
}

LibreSslX509Lookup* APS5_VABI X509_STORE_add_lookup_nid_postfix(LibreSslX509Store* store, LibreSslX509LookupMethod* method) {
    if (!store) throw std::invalid_argument(std::string(__func__) + ": null store");
    auto* lookupMethods = static_cast<Stack*>(store->lookupMethods);
    if (lookupMethods && lookupMethods->num > 0) {
        char** const lookups = Elements(lookupMethods, __func__);
        for (int index = 0; index < lookupMethods->num; ++index) {
            auto* lookup = reinterpret_cast<LibreSslX509Lookup*>(lookups[index]);
            if (!lookup) throw std::invalid_argument(std::string(__func__) + ": listed lookup is null");
            if (lookup->method == method) return lookup;
        }
    }
    if (!method) throw std::invalid_argument(std::string(__func__) + ": null method");
    auto* lookup = static_cast<LibreSslX509Lookup*>(malloc_nid_postfix(sizeof(LibreSslX509Lookup)));
    if (!lookup) return nullptr;
    lookup->init = 0;
    lookup->skip = 0;
    lookup->method = method;
    lookup->methodData = nullptr;
    lookup->storeContext = nullptr;
    if (method->newItem && !reinterpret_cast<LookupCallback>(method->newItem)(lookup)) {
        free_nid_postfix(lookup);
        return nullptr;
    }
    lookup->storeContext = store;
    try {
        if (Push(lookupMethods, lookup, __func__)) return lookup;
    } catch (...) {
        FreeLookup(lookup, false, __func__);
        throw;
    }
    FreeLookup(lookup, false, __func__);
    return nullptr;
}

int APS5_VABI X509_VERIFY_PARAM_set1_host_nid_postfix(LibreSslX509VerifyParam* param, const char* name, std::size_t length) {
    if (!param || !param->id) throw std::invalid_argument("X509_VERIFY_PARAM_set1_host requires verify parameters");
    auto* id = param->id;
    std::string host;
    if (name && !ReadHostName(name, length, host)) return 0;
    if (id->hosts) {
        FreeStrings(static_cast<Stack*>(id->hosts), __func__);
        id->hosts = nullptr;
    }
    if (host.empty()) return 1;
    char* copy = strndup_nid_postfix(host.data(), host.size());
    if (!copy) return 0;
    if (!id->hosts && !(id->hosts = NewStack(nullptr))) {
        free_nid_postfix(copy);
        return 0;
    }
    auto* hosts = static_cast<Stack*>(id->hosts);
    if (!Push(hosts, copy, __func__)) {
        free_nid_postfix(copy);
        if (hosts->num == 0) {
            FreeStack(hosts);
            id->hosts = nullptr;
        }
        return 0;
    }
    return 1;
}

int APS5_VABI X509_STORE_add_cert_nid_postfix(LibreSslX509Store* store, LibreSslX509* certificate) {
    if (!certificate) return 0;
    if (!store) throw std::invalid_argument("X509_STORE_add_cert: null store");
    auto* objects = static_cast<Stack*>(store->objects);
    if (!objects) throw std::invalid_argument("X509_STORE_add_cert: null store objects");
    auto* object = static_cast<LibreSslX509Object*>(malloc_nid_postfix(sizeof(LibreSslX509Object)));
    if (!object) {
        LibreSslPutError_nid_no_patch(X509Library, AnyFunction, MallocFailure);
        return 0;
    }
    object->type = LibreSslX509LookupX509;
    object->padding = 0;
    object->data = certificate;
    std::atomic_ref<std::int32_t>(certificate->references).fetch_add(1, std::memory_order_relaxed);
    bool stored = false;
    try {
        if (FindObject(objects, object)) {
            LibreSslPutError_nid_no_patch(X509Library, AnyFunction, CertAlreadyInHashTable);
        } else if (Push(objects, object, __func__)) {
            stored = true;
        } else {
            LibreSslPutError_nid_no_patch(X509Library, AnyFunction, MallocFailure);
        }
    } catch (...) {
        std::atomic_ref<std::int32_t>(certificate->references).fetch_sub(1, std::memory_order_relaxed);
        free_nid_postfix(object);
        throw;
    }
    if (!stored) {
        std::atomic_ref<std::int32_t>(certificate->references).fetch_sub(1, std::memory_order_relaxed);
        free_nid_postfix(object);
        return 0;
    }
    return 1;
}

}
