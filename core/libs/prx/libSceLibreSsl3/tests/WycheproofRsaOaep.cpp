#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyContext.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libSceLibreSsl3/tests/WycheproofRsaOaepVectors.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace {

std::size_t allocationCount = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* result = std::malloc(size ? size : 1);
    if (result) {
        ++allocationCount;
        std::memset(result, 0xa5, size ? size : 1);
    }
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) {
        --allocationCount;
        std::free(pointer);
    }
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    auto* result = static_cast<unsigned char*>(Allocate_nid_no_patch(count * size));
    if (result) {
        std::memset(result, 0, count * size);
    }
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) {
    return nullptr;
}

void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) {
    return nullptr;
}

void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) {
    return nullptr;
}

int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) {
    return -1;
}

constexpr std::uint64_t EvpCode(std::uint64_t reason) {
    return 0x06fff000ULL | reason;
}

constexpr std::uint64_t RsaCode(std::uint64_t reason) {
    return 0x04fff000ULL | reason;
}

void Require(bool condition, int line, int tcId = -1) {
    if (!condition) {
        std::fprintf(stderr, "Wycheproof RSA-OAEP check failed at line %d (tcId=%d)\n", line, tcId);
        std::fflush(stderr);
        std::abort();
    }
}

#define REQUIRE(cond, tcId) Require(cond, __LINE__, tcId)

void ExpectErrors(std::initializer_list<std::uint64_t> codes) {
    for (const auto code : codes) {
        REQUIRE(ERR_get_error_nid_postfix() == code, -1);
    }
    REQUIRE(ERR_get_error_nid_postfix() == 0, -1);
}

void* Pointer(const void* value) {
    return const_cast<void*>(value);
}

const EvpMd* GetDigest(int shaBits) {
    switch (shaBits) {
        case 1: return reinterpret_cast<const EvpMd*>(EVP_sha1_nid_postfix());
        case 224: return reinterpret_cast<const EvpMd*>(EVP_sha224_nid_postfix());
        case 256: return reinterpret_cast<const EvpMd*>(EVP_sha256_nid_postfix());
        case 384: return reinterpret_cast<const EvpMd*>(EVP_sha384_nid_postfix());
        case 512: return reinterpret_cast<const EvpMd*>(EVP_sha512_nid_postfix());
        default: return nullptr;
    }
}

std::vector<unsigned char> Hex(const char* hex) {
    std::vector<unsigned char> bytes;
    if (!hex) {
        return bytes;
    }
    std::size_t len = std::strlen(hex);
    REQUIRE(len % 2 == 0, -1);
    bytes.resize(len / 2);
    auto val = [](char c) -> unsigned char {
        if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<unsigned char>(c - 'A' + 10);
        return 0xff;
    };
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        unsigned char high = val(hex[i * 2]);
        unsigned char low = val(hex[i * 2 + 1]);
        REQUIRE(high != 0xff && low != 0xff, -1);
        bytes[i] = static_cast<unsigned char>((high << 4) | low);
    }
    return bytes;
}

LibreSsl3Rsa* BuildRsaKey(const std::vector<unsigned char>& nBytes, const std::vector<unsigned char>& eBytes,
    const std::vector<unsigned char>& dBytes) {
    auto* rsa = RSA_new_nid_disambig1_nid_postfix();
    REQUIRE(rsa != nullptr, -1);

    auto* modulus = LibreSsl3::NewRsaBignum_nid_no_patch(nBytes.data(), nBytes.size());
    auto* exponent = LibreSsl3::NewRsaBignum_nid_no_patch(eBytes.data(), eBytes.size());
    auto* privateExponent = LibreSsl3::NewRsaBignum_nid_no_patch(dBytes.data(), dBytes.size());

    if (!modulus || !exponent || !privateExponent || RSA_set0_key_nid_postfix(rsa, modulus, exponent, privateExponent) != 1) {
        if (modulus) BN_clear_free_nid_postfix(modulus);
        if (exponent) BN_clear_free_nid_postfix(exponent);
        if (privateExponent) BN_clear_free_nid_postfix(privateExponent);
        RSA_free_nid_disambig1_nid_postfix(rsa);
        REQUIRE(false, -1);
    }

    return rsa;
}

void RunVector(const LibreSsl3WycheproofRsaOaep::Vector& v) {
    auto nBytes = Hex(v.nHex);
    auto eBytes = Hex(v.eHex);
    auto dBytes = Hex(v.dHex);
    auto ctBytes = Hex(v.ctHex);
    std::vector<unsigned char> msgBytes;
    if (v.msgHex && v.msgHex[0] != '\0') {
        msgBytes = Hex(v.msgHex);
    }
    std::vector<unsigned char> labelBytes;
    if (v.labelHex && v.labelHex[0] != '\0') {
        labelBytes = Hex(v.labelHex);
    }
    REQUIRE(v.shaBits != 224 || GetDigest(224) != nullptr, v.tcId);
    REQUIRE(v.mgfShaBits != 224 || GetDigest(224) != nullptr, v.tcId);

    auto* rsa = BuildRsaKey(nBytes, eBytes, dBytes);

    auto* key = EVP_PKEY_new_nid_disambig1_nid_postfix();
    REQUIRE(key != nullptr, v.tcId);
    key->type = LibreSsl3PkeyRsa;
    key->method = FindNativePkeyAsn1Method_nid_no_patch(LibreSsl3PkeyRsa);
    REQUIRE(key->method != nullptr, v.tcId);
    key->key = rsa;

    auto* context = EVP_PKEY_CTX_new_nid_postfix(key, nullptr);
    REQUIRE(context != nullptr, v.tcId);

    REQUIRE(EVP_PKEY_decrypt_init_nid_postfix(context) == 1, v.tcId);
    REQUIRE(context->operation == LibreSsl3PkeyOperationDecrypt, v.tcId);

    REQUIRE(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaPadding, LibreSsl3RsaPaddingOaep, nullptr) == 1, v.tcId);

    auto* oaepDigest = GetDigest(v.shaBits);
    REQUIRE(oaepDigest != nullptr, v.tcId);
    REQUIRE(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaOaepDigest, 0, Pointer(oaepDigest)) == 1, v.tcId);

    auto* mgfDigest = GetDigest(v.mgfShaBits);
    REQUIRE(mgfDigest != nullptr, v.tcId);
    REQUIRE(EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaMaskDigest, 0, Pointer(mgfDigest)) == 1, v.tcId);

    if (!labelBytes.empty()) {
        auto* label = static_cast<unsigned char*>(malloc_nid_postfix(labelBytes.size()));
        REQUIRE(label != nullptr, v.tcId);
        std::memcpy(label, labelBytes.data(), labelBytes.size());
        if (EVP_PKEY_CTX_ctrl_nid_postfix(context, LibreSsl3PkeyRsa, -1, LibreSsl3PkeyControlRsaOaepLabel, static_cast<int>(labelBytes.size()), label) != 1) {
            free_nid_postfix(label);
            REQUIRE(false, v.tcId);
        }
    }

    std::vector<unsigned char> plain(nBytes.size());
    std::size_t plainLen = plain.size();

    int result = EVP_PKEY_decrypt_nid_postfix(context, plain.data(), &plainLen, ctBytes.data(), ctBytes.size());

    if (v.isAcceptable == 1) {
        if (result == 1) {
            REQUIRE(plainLen == msgBytes.size(), v.tcId);
            REQUIRE(std::memcmp(plain.data(), msgBytes.data(), msgBytes.size()) == 0, v.tcId);
        } else {
            REQUIRE(ERR_get_error_nid_postfix() != 0, v.tcId);
            ERR_clear_error_nid_postfix();
        }
    } else if (v.expectedDecrypt == 1) {
        REQUIRE(result == 1, v.tcId);
        REQUIRE(plainLen == msgBytes.size(), v.tcId);
        REQUIRE(std::memcmp(plain.data(), msgBytes.data(), msgBytes.size()) == 0, v.tcId);
    } else {
        REQUIRE(result <= 0, v.tcId);
        REQUIRE(ERR_get_error_nid_postfix() != 0, v.tcId);
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

    for (const auto& v : LibreSsl3WycheproofRsaOaep::Vectors) {
        RunVector(v);
    }

    REQUIRE(allocationCount == 0, -1);
    return 0;
}