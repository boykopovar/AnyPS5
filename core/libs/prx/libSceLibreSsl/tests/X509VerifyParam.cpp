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

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
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

void RequireAddress(LibreSslX509VerifyParam* param, const char* text, std::initializer_list<unsigned char> expected) {
    Require(X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(param, text) == 1, text);
    Require(param->id->ipLength == expected.size() && std::memcmp(param->id->ip, expected.begin(), expected.size()) == 0, text);
}

void RequireRejected(LibreSslX509VerifyParam* param, const char* text) {
    const unsigned char* before = param->id->ip;
    const std::uint64_t length = param->id->ipLength;
    Require(X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(param, text) == 0, text);
    Require(param->id->ip == before && param->id->ipLength == length, "a rejected address keeps the previous one");
}

void HostFlags() {
    auto* store = X509_STORE_new_nid_postfix();
    auto* param = store->param;
    X509_VERIFY_PARAM_set_hostflags_nid_postfix(param, 0x4);
    Require(param->id->hostFlags == 0x4, "host flags are stored");
    X509_VERIFY_PARAM_set_hostflags_nid_postfix(param, 0xffffffffu);
    Require(param->id->hostFlags == 0xffffffffu, "host flags are replaced, not ORed");
    X509_VERIFY_PARAM_set_hostflags_nid_postfix(param, 0);
    Require(param->id->hostFlags == 0 && param->flags == 0 && param->inheritanceFlags == 0, "only the host flags change");
    RequireInvalidArgument([] { X509_VERIFY_PARAM_set_hostflags_nid_postfix(nullptr, 1); }, "null verify parameters");
    auto* id = param->id;
    param->id = nullptr;
    RequireInvalidArgument([&] { X509_VERIFY_PARAM_set_hostflags_nid_postfix(param, 1); }, "verify parameters without an identity");
    param->id = id;
    X509_STORE_free_nid_postfix(store);
}

void Ipv4() {
    auto* store = X509_STORE_new_nid_postfix();
    auto* param = store->param;
    RequireAddress(param, "192.168.0.1", {192, 168, 0, 1});
    RequireAddress(param, "0.0.0.0", {0, 0, 0, 0});
    RequireAddress(param, "255.255.255.255", {255, 255, 255, 255});
    RequireAddress(param, " 1. 2.3.4", {1, 2, 3, 4});
    RequireAddress(param, "+1.2.3.4x", {1, 2, 3, 4});
    RequireAddress(param, "010.1.1.1", {10, 1, 1, 1});
    for (const char* text : {"", "1.2.3", "256.0.0.0", "1.2.3.256", "-1.0.0.0", "1..2.3", "a.b.c.d", "1 .2.3.4"})
        RequireRejected(param, text);
    X509_STORE_free_nid_postfix(store);
}

void Ipv6() {
    auto* store = X509_STORE_new_nid_postfix();
    auto* param = store->param;
    RequireAddress(param, "::", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    RequireAddress(param, "::1", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});
    RequireAddress(param, "1::", {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    RequireAddress(param, "fe80::aB:1", {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xab, 0, 1});
    RequireAddress(param, "1:2:3:4:5:6:7:8", {0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8});
    RequireAddress(param, "::ffff:192.0.2.1", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 192, 0, 2, 1});
    RequireAddress(param, "1:2:3:4:5:6:10.0.0.1", {0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 10, 0, 0, 1});
    for (const char* text : {":::", "1:::2", "1::2::3", ":1", "1:", "::1:", "1:2:3:4:5:6:7", "1:2:3:4:5:6:7:8:9",
                             "1::2:3:4:5:6:7:8", "12345::", "g::", "1.2.3.4::", "1:2:3:4:5:6:7:1.2.3.4", "::1.2.3"})
        RequireRejected(param, text);
    X509_STORE_free_nid_postfix(store);
}

void Unsupported() {
    auto* store = X509_STORE_new_nid_postfix();
    RequireInvalidArgument([&] { X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(store->param, nullptr); }, "null address text");
    Require(X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(nullptr, "bad") == 0, "a rejected address does not read the verify parameters");
    RequireInvalidArgument([] { X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(nullptr, "1.2.3.4"); }, "null verify parameters");
    X509_STORE_free_nid_postfix(store);
}

int allocations = 0;
int frees = 0;
bool failNext = false;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failNext) {
        failNext = false;
        return nullptr;
    }
    ++allocations;
    return std::malloc(size);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) ++frees;
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
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
    auto* param = store->param;
    allocations = frees = 0;
    Require(X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(param, "10.0.0.1") == 1 && allocations == 1 && frees == 0, "the address is copied into a malloc'd block");
    Require(X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(param, "::1") == 1 && allocations == 2 && frees == 1, "a new address frees the previous one");
    unsigned char* kept = param->id->ip;
    failNext = true;
    Require(X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(param, "10.0.0.2") == 0, "a failed copy fails the call");
    Require(param->id->ip == kept && param->id->ipLength == 16 && frees == 1, "a failed copy keeps the previous address");
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
    HostFlags();
    Ipv4();
    Ipv6();
    Unsupported();
}
