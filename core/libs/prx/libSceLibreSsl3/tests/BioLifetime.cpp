#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIO_vfree check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;
std::size_t freeCalls = 0;
std::size_t callocCalls = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* result = std::malloc(size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    ++freeCalls;
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    void* result = std::calloc(count, size);
    if (!result) return nullptr;
    allocations.emplace(result, count * size);
    ++callocCalls;
    return result;
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

struct CustomState {
    int createCalls = 0;
    int destroyCalls = 0;
    int callbackCalls = 0;
    int lastOperation = 0;
    int callbackVeto = 0;
};

int APS5_VABI CustomCreate_nid_no_patch(LibreSsl::Bio* bio) {
    if (!bio) return 0;
    bio->initialized = 1;
    bio->shutdown = 1;
    auto* state = static_cast<CustomState*>(bio->data);
    if (state) ++state->createCalls;
    return 1;
}

int APS5_VABI CustomDestroy_nid_no_patch(LibreSsl::Bio* bio) {
    if (!bio) return 0;
    auto* state = static_cast<CustomState*>(bio->data);
    if (state) ++state->destroyCalls;
    return 1;
}

std::int64_t APS5_VABI CustomCallback_nid_no_patch(LibreSsl::Bio* bio, int operation, const char* argp, int argi, std::int64_t argl, std::int64_t result) {
    (void)argp;
    (void)argi;
    (void)argl;
    auto* state = static_cast<CustomState*>(bio->data);
    if (state) {
        ++state->callbackCalls;
        state->lastOperation = operation;
        if (state->callbackVeto) return -7;
    }
    return result;
}

LibreSsl::BioMethod customMethod{
    0x480,
    "test_custom",
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    CustomCreate_nid_no_patch,
    CustomDestroy_nid_no_patch,
    nullptr
};

void TestNull() {
    errno = 123;
    BIO_vfree_nid_postfix(nullptr);
    Require(errno == 123);
}

void TestMemoryBio() {
    errno = 456;
    auto* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio != nullptr);
    Require(bio->references == 1);
    Require(allocations.size() == 2);
    BIO_vfree_nid_postfix(bio);
    Require(errno == 456);
    Require(allocations.empty());
}

void TestReferenceCount() {
    CustomState state{};
    auto* bio = BIO_new_nid_postfix(&customMethod);
    Require(bio != nullptr);
    bio->data = &state;
    bio->callback = CustomCallback_nid_no_patch;
    std::atomic_ref(bio->references).store(3);

    BIO_vfree_nid_postfix(bio);
    Require(bio->references == 2);
    Require(state.callbackCalls == 0);
    Require(state.destroyCalls == 0);

    BIO_vfree_nid_postfix(bio);
    Require(bio->references == 1);
    Require(state.callbackCalls == 0);
    Require(state.destroyCalls == 0);

    BIO_vfree_nid_postfix(bio);
    Require(state.callbackCalls == 1);
    Require(state.lastOperation == 1);
    Require(state.destroyCalls == 1);
    Require(allocations.empty());
}

void TestCallbackVeto() {
    CustomState state{};
    state.callbackVeto = 1;
    auto* bio = BIO_new_nid_postfix(&customMethod);
    Require(bio != nullptr);
    bio->data = &state;
    bio->callback = CustomCallback_nid_no_patch;

    BIO_vfree_nid_postfix(bio);
    Require(state.callbackCalls == 1);
    Require(state.lastOperation == 1);
    Require(state.destroyCalls == 0);
    Require(bio->references == 0);
    Require(!allocations.empty());

    state.callbackVeto = 0;
    bio->callback = nullptr;
    bio->references = 1;
    BIO_vfree_nid_postfix(bio);
    Require(state.destroyCalls == 1);
    Require(allocations.empty());
}

void TestChainedOwnership() {
    CustomState state1{};
    CustomState state2{};
    auto* bio1 = BIO_new_nid_postfix(&customMethod);
    auto* bio2 = BIO_new_nid_postfix(&customMethod);
    Require(bio1 != nullptr && bio2 != nullptr);
    bio1->data = &state1;
    bio2->data = &state2;
    bio1->next = bio2;
    bio2->previous = bio1;

    BIO_vfree_nid_postfix(bio1);
    Require(state1.destroyCalls == 1);
    Require(state2.destroyCalls == 0);
    Require(bio2->references == 1);

    BIO_vfree_nid_postfix(bio2);
    Require(state2.destroyCalls == 1);
    Require(allocations.empty());
}

} // namespace

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    TestNull();
    TestMemoryBio();
    TestReferenceCount();
    TestCallbackVeto();
    TestChainedOwnership();

    std::puts("LibreSSL3 BIO_vfree tests passed");
}
