#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

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

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* result = std::malloc(size);
    Require(result != nullptr);
    std::memset(result, 0xA5, size);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
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
    CheckFreeThrows([](LibreSslX509StoreContext& context) { context.param = &Marker; });
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
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 X509_STORE_CTX tests passed");
    return 0;
}
