#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

std::unordered_map<void*, std::size_t> allocations;
int failingCalloc = -1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_PKEY_CTX_new_id check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* result = std::malloc(size ? size : 1);
    Require(result != nullptr);
    Require(allocations.emplace(result, size).second);
    std::memset(result, 0xa5, size ? size : 1);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    if (failingCalloc == 0) {
        failingCalloc = -1;
        return nullptr;
    }
    if (failingCalloc > 0) --failingCalloc;
    auto* result = static_cast<unsigned char*>(Allocate_nid_no_patch(count * size));
    std::memset(result, 0, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected reallocation"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected realigned allocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }

constexpr std::uint64_t EvpCode(std::uint64_t reason) { return 0x06fff000ULL | reason; }

void ExpectError(std::uint64_t code) {
    Require(ERR_get_error_nid_postfix() == code);
    Require(ERR_get_error_nid_postfix() == 0);
}

template<class TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void* Engine() {
    static std::array<unsigned char, 64> engine {};
    std::memset(engine.data(), 0x5c, engine.size());
    return engine.data();
}

void RequireFreshContext(LibreSsl3PkeyContext* context, std::int32_t keyType, int padding) {
    Require(context != nullptr);
    Require(context->method == FindNativePkeyMethod_nid_no_patch(keyType));
    Require(context->key == nullptr);
    Require(context->peerKey == nullptr);
    Require(context->operation == LibreSsl3PkeyOperationUndefined);
    Require(context->applicationData == nullptr);
    Require(context->generationCallback == nullptr);
    Require(context->data != nullptr);
    auto* keyContext = static_cast<LibreSsl3RsaKeyContext*>(context->data);
    Require(context->generationInformation == static_cast<void*>(keyContext->generationTemporary));
    Require(context->generationInformationCount == 2);
    Require(keyContext->keyBits == 2048);
    Require(keyContext->padding == padding);
    Require(keyContext->saltLength == LibreSsl3RsaPssSaltLengthAuto);
    Require(allocations.size() == 2);
    Require(ERR_get_error_nid_postfix() == 0);
}

void RsaContext() {
    auto* context = EVP_PKEY_CTX_new_id_nid_postfix(LibreSsl3PkeyRsa, nullptr);
    RequireFreshContext(context, LibreSsl3PkeyRsa, LibreSsl3RsaPaddingPkcs1);
    int padding = 0;
    Require(EVP_PKEY_encrypt_init_nid_postfix(context) == 1);
    Require(context->operation == LibreSsl3PkeyOperationEncrypt);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, LibreSsl3PkeyOperationTypeCrypt,
        LibreSsl3PkeyControlGetRsaPadding, 0, &padding) == 1);
    Require(padding == LibreSsl3RsaPaddingPkcs1);
    std::array<unsigned char, 256> output {};
    std::size_t length = output.size();
    const unsigned char input[] {1, 2, 3};
    Require(Throws([&] { EVP_PKEY_encrypt_nid_postfix(context, output.data(), &length, input, sizeof(input)); }));
    EVP_PKEY_CTX_free_nid_postfix(context);
    Require(allocations.empty());
}

void RsaPssContext() {
    auto* context = EVP_PKEY_CTX_new_id_nid_postfix(LibreSsl3PkeyRsaPss, Engine());
    RequireFreshContext(context, LibreSsl3PkeyRsaPss, LibreSsl3RsaPaddingPss);
    Require(Throws([&] { EVP_PKEY_sign_init_nid_postfix(context); }));
    EVP_PKEY_CTX_free_nid_postfix(context);
    Require(allocations.empty());
}

void EngineIgnored() {
    auto* context = EVP_PKEY_CTX_new_id_nid_postfix(LibreSsl3PkeyRsa, Engine());
    RequireFreshContext(context, LibreSsl3PkeyRsa, LibreSsl3RsaPaddingPkcs1);
    EVP_PKEY_CTX_free_nid_postfix(context);
    Require(allocations.empty());
}

void UndefinedKeyType() {
    Require(EVP_PKEY_CTX_new_id_nid_postfix(-1, nullptr) == nullptr);
    Require(EVP_PKEY_CTX_new_id_nid_postfix(-1, Engine()) == nullptr);
    Require(ERR_get_error_nid_postfix() == 0);
    Require(allocations.empty());
}

void UnsupportedKeyTypes() {
    for (const int keyType : {0, 1, 5, 7, 115, 117, 407, 409, 854, 856, 893, 895, 911, 913, 949, 951, 953, 1021, 1023,
             1054, 1056, -2, INT_MIN, INT_MAX}) {
        Require(EVP_PKEY_CTX_new_id_nid_postfix(keyType, nullptr) == nullptr);
        ExpectError(EvpCode(156));
        Require(allocations.empty());
    }
}

void LibreSslMethodsWithoutNativeContext() {
    for (const int keyType : {894, 28, 116, 408, 952, 1022, 855, 1055, 950}) {
        Require(Throws([&] { EVP_PKEY_CTX_new_id_nid_postfix(keyType, nullptr); }));
        Require(ERR_get_error_nid_postfix() == 0);
        Require(allocations.empty());
    }
}

void ContextAllocationFailure() {
    failingCalloc = 0;
    Require(EVP_PKEY_CTX_new_id_nid_postfix(LibreSsl3PkeyRsa, nullptr) == nullptr);
    ExpectError(EvpCode(65));
    Require(allocations.empty());
}

void InitializationFailure() {
    for (const int keyType : {LibreSsl3PkeyRsa, LibreSsl3PkeyRsaPss}) {
        failingCalloc = 1;
        Require(EVP_PKEY_CTX_new_id_nid_postfix(keyType, nullptr) == nullptr);
        Require(failingCalloc == -1);
        Require(ERR_get_error_nid_postfix() == 0);
        Require(allocations.empty());
    }
}

} // namespace

int main() {
    std::array<void*, 10> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Calloc_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    RsaContext();
    RsaPssContext();
    EngineIgnored();
    UndefinedKeyType();
    UnsupportedKeyTypes();
    LibreSslMethodsWithoutNativeContext();
    ContextAllocationFailure();
    InitializationFailure();
    Require(allocations.empty());
    std::puts("LibreSSL3 EVP_PKEY_CTX_new_id checks passed");
    return 0;
}
