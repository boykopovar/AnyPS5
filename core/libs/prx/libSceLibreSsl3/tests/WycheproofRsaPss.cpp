#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyContext.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "WycheproofRsaPssVectors.hpp"

namespace {

std::unordered_map<void*, std::size_t> allocations;

void Require(bool condition, int tcId = -1, std::source_location location = std::source_location::current()) {
    if (condition) return;
    if (tcId >= 0) {
        std::fprintf(stderr, "Wycheproof RSA-PSS verify check failed for tcId %d at line %u\n", tcId, location.line());
    } else {
        std::fprintf(stderr, "Wycheproof RSA-PSS verify check failed at line %u\n", location.line());
    }
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
    auto* result = static_cast<unsigned char*>(Allocate_nid_no_patch(count * size));
    std::memset(result, 0, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected reallocation"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected realigned allocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }

void ExpectErrors(std::initializer_list<std::uint64_t> codes) {
    for (const auto code : codes) Require(ERR_get_error_nid_postfix() == code);
    Require(ERR_get_error_nid_postfix() == 0);
}

void* Pointer(const void* value) { return const_cast<void*>(value); }

std::vector<unsigned char> HexToBytes(const char* hex) {
    std::size_t len = std::strlen(hex);
    Require(len % 2 == 0);
    std::vector<unsigned char> bytes(len / 2);
    for (std::size_t i = 0; i < len; i += 2) {
        char hi = hex[i];
        char lo = hex[i + 1];
        unsigned char val = 0;
        if (hi >= '0' && hi <= '9') val = (hi - '0') << 4;
        else if (hi >= 'a' && hi <= 'f') val = (hi - 'a' + 10) << 4;
        else if (hi >= 'A' && hi <= 'F') val = (hi - 'A' + 10) << 4;
        else Require(false);
        if (lo >= '0' && lo <= '9') val |= (lo - '0');
        else if (lo >= 'a' && lo <= 'f') val |= (lo - 'a' + 10);
        else if (lo >= 'A' && lo <= 'F') val |= (lo - 'A' + 10);
        else Require(false);
        bytes[i / 2] = val;
    }
    return bytes;
}

LibreSsl3Bignum* NewBignumFromHex(const char* hex) {
    auto bytes = HexToBytes(hex);
    auto* bn = LibreSsl3::NewRsaBignum_nid_no_patch(bytes.data(), bytes.size());
    Require(bn != nullptr);
    return bn;
}

LibreSsl3EvpPkey* NewPublicKey(const char* nHex, const char* eHex) {
    auto* key = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(key != nullptr);
    key->type = LibreSsl3PkeyRsa;
    key->method = FindNativePkeyAsn1Method_nid_no_patch(LibreSsl3PkeyRsa);
    Require(key->method != nullptr);
    auto* rsa = RSA_new_nid_disambig1_nid_postfix();
    Require(rsa != nullptr);
    key->key = rsa;
    auto* n = NewBignumFromHex(nHex);
    auto* e = NewBignumFromHex(eHex);
    Require(RSA_set0_key_nid_postfix(rsa, n, e, nullptr) == 1);
    return key;
}

void RunVector(const LibreSsl3WycheproofRsaPss::Vector& v) {
    auto* key = NewPublicKey(v.nHex, v.eHex);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    Require(context != nullptr, v.tcId);

    Require(EVP_PKEY_verify_init_nid_postfix(context) == 1, v.tcId);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding,
        LibreSsl3RsaPaddingPss, nullptr) == 1, v.tcId);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlMessageDigest, 0,
        Pointer(EVP_sha256_nid_postfix())) == 1, v.tcId);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaMaskDigest, 0,
        Pointer(EVP_sha256_nid_postfix())) == 1, v.tcId);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPssSaltLength,
        v.saltLen, nullptr) == 1, v.tcId);

    auto digestBytes = HexToBytes(v.digestHex);
    auto signatureBytes = HexToBytes(v.signatureHex);

    int result = EVP_PKEY_verify_nid_postfix(context, signatureBytes.data(), signatureBytes.size(),
        digestBytes.data(), digestBytes.size());

    if (v.expectedVerify == 1) {
        Require(result == 1, v.tcId);
        Require(ERR_get_error_nid_postfix() == 0, v.tcId);
    } else {
        Require(result <= 0, v.tcId);
        ERR_clear_error_nid_postfix();
    }

    EVP_PKEY_CTX_free_nid_postfix(context);
    EVP_PKEY_free_nid_disambig1_nid_postfix(key);
}

}

int main() {
    std::array<void*, 10> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Calloc_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    for (const auto& v : LibreSsl3WycheproofRsaPss::Vectors) {
        RunVector(v);
    }

    Require(allocations.empty());
    std::puts("Wycheproof RSA-PSS verify tests passed");
}
