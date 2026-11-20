#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {

struct AuthorityKeyId {
    Asn1String* keyId;
    LibreSslStack* issuer;
    Asn1String* serial;
};

std::unordered_map<void*, std::size_t> allocations;
std::unordered_set<void*> zeroed;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* result = std::malloc(size);
    if (result) allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    const auto found = allocations.find(pointer);
    if (found == allocations.end()) std::abort();
    if (zeroed.erase(pointer) != 0) {
        const auto* bytes = static_cast<const unsigned char*>(pointer);
        for (std::size_t index = 0; index < found->second; ++index) {
            if (bytes[index] != 0) std::abort();
        }
    }
    allocations.erase(found);
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    void* result = std::calloc(count, size);
    if (result) allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    const auto found = allocations.find(pointer);
    if (found == allocations.end()) std::abort();
    void* result = std::realloc(pointer, size);
    if (!result) return nullptr;
    allocations.erase(found);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509 item free check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireNoException(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (...) {
        Require(false, location);
    }
}

template<typename TAction>
void RequireNotImplemented(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, location);
}

template<typename TAction>
void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

Asn1String* MakeString(int type, std::initializer_list<unsigned char> bytes) {
    auto* string = static_cast<Asn1String*>(Callocate_nid_no_patch(1, sizeof(Asn1String)));
    Require(string != nullptr);
    string->length = static_cast<int>(bytes.size());
    string->type = type;
    if (!bytes.size()) return string;
    string->data = static_cast<unsigned char*>(Allocate_nid_no_patch(bytes.size()));
    Require(string->data != nullptr);
    std::memcpy(string->data, bytes.begin(), bytes.size());
    zeroed.insert(string->data);
    return string;
}

LibreSslStack* MakeIssuer() {
    auto* stack = static_cast<LibreSslStack*>(Callocate_nid_no_patch(1, sizeof(LibreSslStack)));
    auto* entries = static_cast<char**>(Callocate_nid_no_patch(1, sizeof(char*)));
    auto* name = static_cast<LibreSslGeneralName*>(Callocate_nid_no_patch(1, sizeof(LibreSslGeneralName)));
    Require(stack != nullptr && entries != nullptr && name != nullptr);
    name->type = 2;
    name->value = MakeString(22, {'c', 'a'});
    entries[0] = reinterpret_cast<char*>(name);
    stack->num = 1;
    stack->data = entries;
    stack->numAlloc = 1;
    return stack;
}

void TestNull() {
    RequireNoException([] { AUTHORITY_KEYID_free_nid_postfix(nullptr); });
    RequireNoException([] { X509_CRL_free_nid_postfix(nullptr); });
    RequireNoException([] { PKCS7_free_nid_postfix(nullptr); });
    void* const unreadable = reinterpret_cast<void*>(std::uintptr_t{1});
    Require(X509_check_akid_nid_postfix(nullptr, nullptr) == 0);
    Require(X509_check_akid_nid_postfix(unreadable, nullptr) == 0);
}

void TestNonNull() {
    void* const unreadable = reinterpret_cast<void*>(std::uintptr_t{1});
    errno = EDOM;
    RequireInvalidArgument([&] { AUTHORITY_KEYID_free_nid_postfix(unreadable); });
    Require(errno == EDOM);
    RequireNotImplemented([&] { PKCS7_free_nid_postfix(unreadable); });
}

void TestAuthorityKeyId() {
    auto* id = static_cast<AuthorityKeyId*>(Callocate_nid_no_patch(1, sizeof(AuthorityKeyId)));
    Require(id != nullptr);
    id->keyId = MakeString(4, {1, 2, 3});
    id->issuer = MakeIssuer();
    id->serial = MakeString(2, {4, 5});
    AUTHORITY_KEYID_free_nid_postfix(id);
    Require(allocations.empty());

    id = static_cast<AuthorityKeyId*>(Callocate_nid_no_patch(1, sizeof(AuthorityKeyId)));
    Require(id != nullptr);
    AUTHORITY_KEYID_free_nid_postfix(id);
    Require(allocations.empty());
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestNull();
    TestNonNull();
    TestAuthorityKeyId();
    std::puts("LibreSSL3 X509 item free tests passed");
}
