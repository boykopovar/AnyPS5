#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

static_assert(sizeof(LibreSslX509StoreContext) == 240);
static_assert(offsetof(LibreSslX509StoreContext, param) == 40);
static_assert(offsetof(LibreSslX509StoreContext, callbacks) == 56);
static_assert(offsetof(LibreSslX509StoreContext, chain) == 160);
static_assert(offsetof(LibreSslX509StoreContext, tree) == 168);
static_assert(offsetof(LibreSslX509StoreContext, parent) == 224);
static_assert(offsetof(LibreSslX509StoreContext, exData) == 232);
static_assert(offsetof(LibreSslX509Store, references) == 128);
static_assert(std::atomic_ref<std::int32_t>::required_alignment <= alignof(std::int32_t));

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_STORE_CTX check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

std::unordered_map<void*, std::size_t> allocations;
int allocationsUntilFailure = -1;

bool FailAllocation() {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return true;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    return false;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (FailAllocation()) return nullptr;
    void* result = std::malloc(size);
    Require(result != nullptr);
    std::memset(result, 0xA5, size);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    if (FailAllocation()) return nullptr;
    void* result = std::calloc(count, size);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (FailAllocation()) return nullptr;
    if (pointer) Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

int Marker = 0;

void TestNewAndFree() {
    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    std::array<unsigned char, sizeof(LibreSslX509StoreContext)> zero{};
    Require(std::memcmp(context, zero.data(), zero.size()) == 0);
    X509_STORE_CTX_free_nid_postfix(context);
    X509_STORE_CTX_free_nid_postfix(nullptr);
}

void TestFreeBorrowedParam() {
    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    context->parent = context;
    context->param = &Marker;
    X509_STORE_CTX_free_nid_postfix(context);
}

template <typename Setter>
void CheckFreeThrows(Setter set) {
    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    set(*context);
    LibreSslX509StoreContext before;
    std::memcpy(&before, context, sizeof(before));
    Require(Throws([&] { X509_STORE_CTX_free_nid_postfix(context); }));
    Require(std::memcmp(&before, context, sizeof(before)) == 0);
    std::memset(context, 0, sizeof(*context));
    X509_STORE_CTX_free_nid_postfix(context);
}

void TestFreeUnsupported() {
    CheckFreeThrows([](LibreSslX509StoreContext& context) { context.callbacks[11] = &Marker; });
    CheckFreeThrows([](LibreSslX509StoreContext& context) { context.chain = &Marker; });
    CheckFreeThrows([](LibreSslX509StoreContext& context) { context.tree = &Marker; });
    CheckFreeThrows([](LibreSslX509StoreContext& context) { context.exData = &Marker; });
}

void TestGetParam() {
    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_get0_param_nid_postfix(context) == nullptr);
    context->parent = context;
    context->param = &Marker;
    Require(X509_STORE_CTX_get0_param_nid_postfix(context) == &Marker);
    X509_STORE_CTX_free_nid_postfix(context);
    Require(Throws([] { X509_STORE_CTX_get0_param_nid_postfix(nullptr); }));
}

void TestUpRef() {
    auto* store = X509_STORE_new_nid_postfix();
    Require(store != nullptr);
    Require(store->references == 1);
    Require(X509_STORE_up_ref_nid_postfix(store) == 1);
    Require(store->references == 2);
    Require(X509_STORE_up_ref_nid_postfix(store) == 1);
    Require(store->references == 3);
    X509_STORE_free_nid_postfix(store);
    X509_STORE_free_nid_postfix(store);
    Require(store->references == 1);
    X509_STORE_free_nid_postfix(store);

    LibreSslX509Store owned{};
    owned.references = 0;
    Require(X509_STORE_up_ref_nid_postfix(&owned) == 0);
    Require(owned.references == 1);
    owned.references = -5;
    Require(X509_STORE_up_ref_nid_postfix(&owned) == 0);
    Require(owned.references == -4);
    owned.references = std::numeric_limits<std::int32_t>::max();
    Require(Throws([&] { X509_STORE_up_ref_nid_postfix(&owned); }));
    Require(owned.references == std::numeric_limits<std::int32_t>::max());
    Require(Throws([] { X509_STORE_up_ref_nid_postfix(nullptr); }));
}

void TestUpRefRace() {
    constexpr int Threads = 8;
    constexpr int Increments = 20000;
    LibreSslX509Store shared{};
    shared.references = 1;
    std::atomic<bool> go{false};
    std::vector<std::thread> workers;
    for (int index = 0; index < Threads; ++index) {
        workers.emplace_back([&] {
            while (!go.load()) std::this_thread::yield();
            for (int count = 0; count < Increments; ++count) X509_STORE_up_ref_nid_postfix(&shared);
        });
    }
    go.store(true);
    for (auto& worker : workers) worker.join();
    Require(shared.references == 1 + Threads * Increments);
}


constexpr std::uint64_t MallocFailure = 0x0bfff041;
constexpr std::uint64_t TrustedFirst = 0x8000;

using VerifyErrorCallback = int (APS5_VABI*)(int, LibreSslX509StoreContext*);

struct Defaults {
    std::array<void*, 4> callbacks;
};

Defaults ReadDefaults() {
    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_init_nid_postfix(context, nullptr, nullptr, nullptr) == 1);
    Defaults defaults{};
    std::copy_n(context->callbacks, defaults.callbacks.size(), defaults.callbacks.begin());
    X509_STORE_CTX_free_nid_postfix(context);
    return defaults;
}

void RequireInitialised(const LibreSslX509StoreContext& context, LibreSslX509Store* store, void* leaf, void* untrusted, const Defaults& defaults) {
    Require(context.store == store && context.currentMethod == 0 && context.cert == leaf && context.untrusted == untrusted);
    Require(context.crls == nullptr && context.otherContext == nullptr);
    for (std::size_t index = 2; index < 4; ++index) Require(context.callbacks[index] == defaults.callbacks[index]);
    for (std::size_t index = 4; index < 12; ++index) Require(context.callbacks[index] == nullptr);
    Require(context.valid == 0 && context.lastUntrusted == 0 && context.chain == nullptr && context.tree == nullptr);
    Require(context.explicitPolicy == 0 && context.errorDepth == 0 && context.error == 65);
    Require(context.currentCert == nullptr && context.currentIssuer == nullptr && context.currentCrl == nullptr);
    Require(context.currentCrlScore == 0 && context.currentReasons == 0 && context.parent == nullptr && context.exData == nullptr);
}

bool IdIsEmpty(const LibreSslX509VerifyParamId& id) {
    return !id.hosts && id.hostFlags == 0 && !id.peerName && !id.email && id.emailLength == 0 && !id.ip && id.ipLength == 0;
}

LibreSslX509VerifyParam& Param(LibreSslX509StoreContext* context) {
    auto* param = static_cast<LibreSslX509VerifyParam*>(X509_STORE_CTX_get0_param_nid_postfix(context));
    Require(param != nullptr && param->id != nullptr);
    return *param;
}

void TestInitDefaults() {
    const auto defaults = ReadDefaults();
    for (std::size_t first = 0; first < 4; ++first) {
        Require(defaults.callbacks[first] != nullptr);
        for (std::size_t second = first + 1; second < 4; ++second) Require(defaults.callbacks[first] != defaults.callbacks[second]);
    }
    Require(ERR_get_error_nid_postfix() == 0);

    auto* store = X509_STORE_new_nid_postfix();
    Require(store != nullptr);
    const auto before = allocations.size();
    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    std::memset(context, 0xA5, sizeof(*context));
    Require(X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr) == 1);
    Require(allocations.size() == before + 3);
    Require(ERR_get_error_nid_postfix() == 0);
    RequireInitialised(*context, store, nullptr, nullptr, defaults);
    Require(context->callbacks[0] == defaults.callbacks[0] && context->callbacks[1] == defaults.callbacks[1]);
    auto verifyError = reinterpret_cast<VerifyErrorCallback>(context->callbacks[1]);
    Require(verifyError(1, context) == 1 && verifyError(0, context) == 0 && verifyError(-7, context) == -7);
    Require(X509_STORE_CTX_get_error_nid_postfix(context) == 65);
    Require(X509_STORE_CTX_get_ex_data_nid_postfix(context, 0) == nullptr);
    auto& param = Param(context);
    Require(&param != store->param && param.id != store->param->id);
    Require(param.name == nullptr && param.checkTime == 0 && param.inheritanceFlags == 0 && param.flags == TrustedFirst);
    Require(param.purpose == 0 && param.trust == 0 && param.depth == 100 && param.policies == nullptr && IdIsEmpty(*param.id));
    Require(store->references == 1 && store->param->depth == -1 && store->param->flags == 0 && IdIsEmpty(*store->param->id));
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(&param, "example.com", 0) == 1);
    Require(X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(&param, "192.0.2.1") == 1);
    X509_STORE_CTX_free_nid_postfix(context);
    Require(allocations.size() == before);
    X509_STORE_free_nid_postfix(store);
    Require(allocations.empty());
}

void TestInitWithoutStore() {
    const auto defaults = ReadDefaults();
    LibreSsl3X509 leaf{};
    LibreSslStack untrusted{};
    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    std::memset(context, 0x5A, sizeof(*context));
    Require(X509_STORE_CTX_init_nid_postfix(context, nullptr, &leaf, &untrusted) == 1);
    RequireInitialised(*context, nullptr, &leaf, &untrusted, defaults);
    Require(context->callbacks[0] == defaults.callbacks[0] && context->callbacks[1] == defaults.callbacks[1]);
    auto& param = Param(context);
    Require(param.inheritanceFlags == 0 && param.flags == TrustedFirst && param.depth == 100 && param.purpose == 0 && param.trust == 0);
    Require(param.checkTime == 0 && param.policies == nullptr && IdIsEmpty(*param.id));
    X509_STORE_CTX_free_nid_postfix(context);
    Require(allocations.empty());
    Require(Throws([] { X509_STORE_CTX_init_nid_postfix(nullptr, nullptr, nullptr, nullptr); }));
}

void TestStoreCallbacks() {
    const auto defaults = ReadDefaults();
    auto* store = X509_STORE_new_nid_postfix();
    Require(store != nullptr);
    int verify = 0;
    int verifyError = 0;
    int other = 0;
    store->callbacks[0] = &verify;
    store->callbacks[1] = &verifyError;
    for (std::size_t index = 2; index < 11; ++index) store->callbacks[index] = &other;
    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr) == 1);
    RequireInitialised(*context, store, nullptr, nullptr, defaults);
    Require(context->callbacks[0] == &verify && context->callbacks[1] == &verifyError);
    X509_STORE_CTX_free_nid_postfix(context);
    store->callbacks[0] = nullptr;
    context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr) == 1);
    Require(context->callbacks[0] == defaults.callbacks[0] && context->callbacks[1] == &verifyError);
    store->callbacks[0] = &verify;
    store->callbacks[1] = nullptr;
    X509_STORE_CTX_free_nid_postfix(context);
    context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr) == 1);
    Require(context->callbacks[0] == &verify && context->callbacks[1] == defaults.callbacks[1]);
    X509_STORE_CTX_free_nid_postfix(context);
    std::fill(std::begin(store->callbacks), std::end(store->callbacks), nullptr);
    X509_STORE_free_nid_postfix(store);
    Require(allocations.empty());
}

void TestInheritStoreParam() {
    auto* store = X509_STORE_new_nid_postfix();
    Require(store != nullptr);
    auto* source = store->param;
    source->purpose = 3;
    source->trust = 2;
    source->depth = 5;
    source->checkTime = 1234;
    Require(X509_STORE_set_flags_nid_postfix(store, 0x6) == 1);
    X509_VERIFY_PARAM_set_hostflags_nid_postfix(source, 0x10);
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(source, "a.example", 0) == 1);
    Require(X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(source, "192.0.2.1") == 1);
    const char email[] = "admin@example.com";
    source->id->email = static_cast<char*>(malloc_nid_postfix(sizeof(email) - 1));
    Require(source->id->email != nullptr);
    std::memcpy(source->id->email, email, sizeof(email) - 1);
    source->id->emailLength = sizeof(email) - 1;

    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr) == 1);
    Require(ERR_get_error_nid_postfix() == 0);
    auto& param = Param(context);
    Require(param.purpose == 3 && param.trust == 2 && param.depth == 5 && param.checkTime == 1234 && param.flags == (0x6 | TrustedFirst));
    Require(param.inheritanceFlags == 0 && param.policies == nullptr && param.name == nullptr);
    const auto& id = *param.id;
    Require(id.hostFlags == 0x10 && id.peerName == nullptr);
    const auto* hosts = static_cast<const LibreSslStack*>(id.hosts);
    Require(hosts != nullptr && hosts != source->id->hosts && sk_num_nid_postfix(hosts) == 1);
    const auto* host = static_cast<const char*>(sk_value_nid_postfix(hosts, 0));
    Require(host != sk_value_nid_postfix(static_cast<const LibreSslStack*>(source->id->hosts), 0) && std::strcmp(host, "a.example") == 0);
    Require(id.email != source->id->email && id.emailLength == sizeof(email) - 1 && std::memcmp(id.email, email, id.emailLength) == 0);
    const unsigned char address[] = {192, 0, 2, 1};
    Require(id.ip != source->id->ip && id.ipLength == 4 && std::memcmp(id.ip, address, 4) == 0);
    Require(source->purpose == 3 && source->depth == 5 && source->flags == 0x6 && source->id->emailLength == sizeof(email) - 1);
    X509_STORE_CTX_free_nid_postfix(context);

    source->flags = 0x4;
    context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr) == 1);
    Require(Param(context).checkTime == 0 && Param(context).flags == (0x4 | TrustedFirst));
    X509_STORE_CTX_free_nid_postfix(context);

    source->depth = -1;
    source->purpose = 0;
    source->inheritanceFlags = 0x8;
    context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr) == 1);
    Require(Param(context).trust == 0 && Param(context).flags == TrustedFirst && Param(context).depth == 100 && IdIsEmpty(*Param(context).id));
    X509_STORE_CTX_free_nid_postfix(context);

    source->inheritanceFlags = 0x2;
    context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr) == 1);
    Require(Param(context).depth == 100 && Param(context).trust == 2 && Param(context).id->hostFlags == 0x10 && Param(context).id->ipLength == 4);
    X509_STORE_CTX_free_nid_postfix(context);

    source->inheritanceFlags = 0;
    X509_STORE_free_nid_postfix(store);
    Require(allocations.empty());
}

void RequireFailedInit(LibreSslX509Store* store, bool paramAllocated) {
    const auto defaults = ReadDefaults();
    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr) == 0);
    Require(ERR_get_error_nid_postfix() == MallocFailure && ERR_get_error_nid_postfix() == 0);
    RequireInitialised(*context, store, nullptr, nullptr, defaults);
    Require((context->param != nullptr) == paramAllocated);
    X509_STORE_CTX_free_nid_postfix(context);
}

void TestInheritFailures() {
    auto* store = X509_STORE_new_nid_postfix();
    Require(store != nullptr);
    auto* source = store->param;
    auto& id = *source->id;
    source->inheritanceFlags = 0x2;
    RequireFailedInit(store, true);
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(source, "a.example", 0) == 1);
    id.email = strdup_nid_postfix("admin@example.com");
    Require(id.email != nullptr);
    RequireFailedInit(store, true);
    source->inheritanceFlags = 0;
    id.ip = static_cast<unsigned char*>(malloc_nid_postfix(3));
    Require(id.ip != nullptr);
    std::memset(id.ip, 1, 3);
    id.ipLength = 3;
    RequireFailedInit(store, true);
    id.ipLength = 0;
    RequireFailedInit(store, true);
    free_nid_postfix(id.ip);
    id.ip = nullptr;
    id.email[0] = '\0';
    RequireFailedInit(store, true);
    std::strcpy(id.email, "ab");
    id.emailLength = 3;
    RequireFailedInit(store, true);
    id.emailLength = 2;
    auto* context = X509_STORE_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr) == 1);
    Require(Param(context).id->emailLength == 2 && std::memcmp(Param(context).id->email, "ab", 2) == 0);
    X509_STORE_CTX_free_nid_postfix(context);
    X509_STORE_free_nid_postfix(store);
    Require(allocations.empty());
}

void TestAllocationFailures() {
    auto* store = X509_STORE_new_nid_postfix();
    Require(store != nullptr);
    Require(X509_VERIFY_PARAM_set1_host_nid_postfix(store->param, "a.example", 0) == 1);
    Require(X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(store->param, "192.0.2.1") == 1);
    store->param->id->email = strdup_nid_postfix("admin@example.com");
    Require(store->param->id->email != nullptr);
    const auto baseline = allocations.size();
    int failures = 0;
    for (int index = 0;; ++index) {
        auto* context = X509_STORE_CTX_new_nid_postfix();
        Require(context != nullptr);
        allocationsUntilFailure = index;
        const int result = X509_STORE_CTX_init_nid_postfix(context, store, nullptr, nullptr);
        const bool failed = allocationsUntilFailure < 0;
        allocationsUntilFailure = -1;
        if (!failed) {
            Require(result == 1 && ERR_get_error_nid_postfix() == 0);
            X509_STORE_CTX_free_nid_postfix(context);
            break;
        }
        ++failures;
        Require(result == 0 && ERR_get_error_nid_postfix() == MallocFailure && ERR_get_error_nid_postfix() == 0);
        Require(context->error == 65 && context->store == store && (context->param != nullptr) == (index >= 2));
        X509_STORE_CTX_free_nid_postfix(context);
        Require(allocations.size() == baseline);
    }
    Require(failures == 7);
    for (int index = 0; index < 2; ++index) {
        LibreSslX509StoreContext context{};
        allocationsUntilFailure = index;
        Require(X509_STORE_CTX_init_nid_postfix(&context, nullptr, nullptr, nullptr) == 0);
        Require(allocationsUntilFailure == -1 && context.param == nullptr && context.error == 65);
        Require(ERR_get_error_nid_postfix() == MallocFailure);
    }
    Require(allocations.size() == baseline);
    X509_STORE_free_nid_postfix(store);
    Require(allocations.empty());
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestNewAndFree();
    Require(allocations.empty());
    TestFreeBorrowedParam();
    TestFreeUnsupported();
    TestGetParam();
    TestUpRef();
    TestUpRefRace();
    TestInitDefaults();
    TestInitWithoutStore();
    TestStoreCallbacks();
    TestInheritStoreParam();
    TestInheritFailures();
    TestAllocationFailures();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 X509_STORE_CTX tests passed");
    return 0;
}
