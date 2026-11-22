#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceLibreSsl3/tests/WycheproofRsaPkcs1Vectors.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<void*, std::size_t> allocations;

void Require(bool condition) {
    if (condition) return;
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

constexpr std::uint64_t EvpCode(std::uint64_t reason) { return 0x06fff000ULL | reason; }
constexpr std::uint64_t RsaCode(std::uint64_t reason) { return 0x04fff000ULL | reason; }

void ExpectErrors(std::initializer_list<std::uint64_t> codes) {
    for (const auto code : codes) Require(ERR_get_error_nid_postfix() == code);
    Require(ERR_get_error_nid_postfix() == 0);
}

void* Pointer(const void* value) { return const_cast<void*>(value); }

std::vector<unsigned char> HexToBytes(const char* hex) {
    std::vector<unsigned char> bytes;
    for (std::size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
        unsigned char byte = 0;
        for (int j = 0; j < 2; ++j) {
            char c = hex[i + j];
            byte <<= 4;
            if (c >= '0' && c <= '9') byte |= c - '0';
            else if (c >= 'a' && c <= 'f') byte |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') byte |= c - 'A' + 10;
        }
        bytes.push_back(byte);
    }
    return bytes;
}

LibreSsl3Rsa* NewRsaKey(const char* nHex, const char* eHex) {
    auto* rsa = RSA_new_nid_disambig1_nid_postfix();
    Require(rsa != nullptr);
    auto nBytes = HexToBytes(nHex);
    auto eBytes = HexToBytes(eHex);
    auto* modulus = LibreSsl3::NewRsaBignum_nid_no_patch(nBytes.data(), nBytes.size());
    auto* exponent = LibreSsl3::NewRsaBignum_nid_no_patch(eBytes.data(), eBytes.size());
    Require(modulus != nullptr && exponent != nullptr);
    Require(RSA_set0_key_nid_postfix(rsa, modulus, exponent, nullptr) == 1);
    return rsa;
}

LibreSsl3EvpPkey* NewPkey(LibreSsl3Rsa* rsa) {
    auto* key = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(key != nullptr);
    key->type = LibreSsl3PkeyRsa;
    key->method = FindNativePkeyAsn1Method_nid_no_patch(LibreSsl3PkeyRsa);
    Require(key->method != nullptr);
    Require(EVP_PKEY_set1_RSA_nid_disambig1_nid_postfix(key, rsa) == 1);
    return key;
}

void TestVector(const LibreSsl3WycheproofRsaPkcs1::Vector& vec) {
    auto* rsa = NewRsaKey(vec.nHex, vec.eHex);
    auto* pkey = NewPkey(rsa);
    auto* context = EVP_PKEY_CTX_new_nid_postfix(pkey, nullptr);
    Require(context != nullptr);
    Require(EVP_PKEY_verify_init_nid_postfix(context) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1,
        LibreSsl3PkeyControlMessageDigest, 0, Pointer(EVP_sha256_nid_postfix())) == 1);
    Require(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1,
        LibreSsl3PkeyControlRsaPadding, LibreSsl3RsaPaddingPkcs1, nullptr) == 1);
    auto digestBytes = HexToBytes(vec.digestHex);
    auto sigBytes = HexToBytes(vec.signatureHex);
    int result = EVP_PKEY_verify_nid_postfix(context, sigBytes.data(), sigBytes.size(),
        digestBytes.data(), digestBytes.size());
    if (vec.expectedVerify == 1) {
        Require(result == 1);
    } else {
        Require(result <= 0);
    }
    EVP_PKEY_CTX_free_nid_postfix(context);
    EVP_PKEY_free_nid_disambig1_nid_postfix(pkey);
}

}

int main() {
    std::array<void*, 10> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch), reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    for (const auto& vec : LibreSsl3WycheproofRsaPkcs1::Vectors) {
        TestVector(vec);
    }
    return 0;
}
