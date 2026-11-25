/* The test vectors are adapted for AnyPS5 from Project Wycheproof,
 * testvectors_v1/ecdh_secp256r1_test.json, ecdh_secp384r1_test.json at commit
 * 65ca494727a52944c204441c860b039e6786b99b
 * (https://github.com/C2SP/wycheproof/tree/65ca494727a52944c204441c860b039e6786b99b/testvectors_v1).
 * Altered: all 1659 tests of both files are derived; valid tests must give the listed shared secret
 * and invalid tests must fail with an error queued. The 460 acceptable tests (public keys with a
 * non-DER ASN.1 encoding, a compressed point or explicit curve parameters) may be rejected with an
 * error queued, but if the derivation succeeds it must give the listed shared secret.
 *
 * Copyright 2016-2026 The Wycheproof Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libSceLibreSsl3/include/PkeyState.hpp"
#include "prx/libSceLibreSsl3/include/RsaCrypto.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceLibreSsl3/tests/WycheproofEcdhVectors.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<void*, std::size_t> allocations;

void Require(bool condition, std::source_location location = std::source_location::current(), int tcId = 0) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 Wycheproof ECDH check failed at line %d (tcId=%d)\n",
        location.line(), tcId);
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
constexpr std::uint64_t EcCode(std::uint64_t reason) { return 0x10fff000ULL | reason; }

void ExpectErrors(std::initializer_list<std::uint64_t> codes) {
    for (const auto code : codes) Require(ERR_get_error_nid_postfix() == code);
    Require(ERR_get_error_nid_postfix() == 0);
}

std::vector<unsigned char> HexToBytes(const char* hex) {
    if (!hex || !*hex) return {};
    std::size_t len = std::strlen(hex);
    Require(len % 2 == 0);
    std::vector<unsigned char> bytes;
    bytes.reserve(len / 2);
    for (std::size_t i = 0; i < len; i += 2) {
        unsigned char high = 0, low = 0;
        char c = hex[i];
        if (c >= '0' && c <= '9') high = c - '0';
        else if (c >= 'a' && c <= 'f') high = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') high = c - 'A' + 10;
        else Require(false);
        c = hex[i + 1];
        if (c >= '0' && c <= '9') low = c - '0';
        else if (c >= 'a' && c <= 'f') low = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') low = c - 'A' + 10;
        else Require(false);
        bytes.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return bytes;
}

bool CompareBytes(const std::vector<unsigned char>& a, const unsigned char* b, std::size_t bLen) {
    return a.size() == bLen && std::equal(a.begin(), a.end(), b);
}

void RunVector(const LibreSsl3WycheproofEcdh::Vector& v) {
    std::vector<unsigned char> peerSpki = HexToBytes(v.peerSpkiHex);
    std::vector<unsigned char> privBytes = HexToBytes(v.privHex);
    std::vector<unsigned char> expectedShared = HexToBytes(v.sharedHex);

    const unsigned char* spkiCursor = peerSpki.data();
    LibreSsl3EvpPkey* peerKey = d2i_PUBKEY_nid_postfix(nullptr, &spkiCursor, static_cast<std::int64_t>(peerSpki.size()));

    EcKey* localEc = EC_KEY_new_by_curve_name_nid_postfix(v.curveNid);
    Require(localEc != nullptr && localEc->group != nullptr, std::source_location::current(), v.tcId);
    EC_KEY_set_asn1_flag_nid_postfix(localEc, 1);

    if (!EC_KEY_generate_key_nid_postfix(localEc)) {
        EC_KEY_free_nid_postfix(localEc);
        if (peerKey) EVP_PKEY_free_nid_disambig1_nid_postfix(peerKey);
        if (v.expectedValid == 1) Require(false, std::source_location::current(), v.tcId);
        ExpectErrors({EcCode(140)});
        return;
    }

    Require(mbedtls_mpi_read_binary(&localEc->privateKey->value, privBytes.data(), privBytes.size()) == 0, std::source_location::current(), v.tcId);
    Bignum* scalar = BN_bin2bn_nid_postfix(privBytes.data(), static_cast<int>(privBytes.size()), nullptr);
    Require(scalar != nullptr, std::source_location::current(), v.tcId);
    Require(EC_POINT_mul_nid_postfix(localEc->group, localEc->publicKey, scalar, nullptr, nullptr, nullptr) == 1, std::source_location::current(), v.tcId);
    BN_free_nid_postfix(scalar);

    LibreSsl3EvpPkey* localKey = EVP_PKEY_new_nid_disambig1_nid_postfix();
    Require(localKey != nullptr, std::source_location::current(), v.tcId);
    Require(EVP_PKEY_set1_EC_KEY_nid_postfix(localKey, reinterpret_cast<LibreSsl3EcKey*>(localEc)) == 1, std::source_location::current(), v.tcId);
    EC_KEY_free_nid_postfix(localEc);

    LibreSsl3PkeyContext* ctx = EVP_PKEY_CTX_new_nid_postfix(localKey, nullptr);
    Require(ctx != nullptr, std::source_location::current(), v.tcId);

    int deriveInit = EVP_PKEY_derive_init_nid_postfix(ctx);
    if (deriveInit != 1) {
        EVP_PKEY_CTX_free_nid_postfix(ctx);
        EVP_PKEY_free_nid_disambig1_nid_postfix(localKey);
        if (peerKey) EVP_PKEY_free_nid_disambig1_nid_postfix(peerKey);
        if (v.expectedValid == 1) Require(false, std::source_location::current(), v.tcId);
        ExpectErrors({EvpCode(151)});
        return;
    }

    int setPeer = 0;
    if (peerKey) {
        setPeer = EVP_PKEY_derive_set_peer_nid_postfix(ctx, peerKey);
    }

    if (v.isAcceptable == 1) {
        bool ok = peerKey != nullptr && setPeer == 1;
        std::size_t length = 0;
        if (ok) ok = EVP_PKEY_derive_nid_postfix(ctx, nullptr, &length) == 1;
        std::vector<unsigned char> shared;
        if (ok) {
            shared.resize(length);
            ok = EVP_PKEY_derive_nid_postfix(ctx, shared.data(), &length) == 1;
        }
        if (ok) {
            Require(CompareBytes(shared, expectedShared.data(), expectedShared.size()), std::source_location::current(), v.tcId);
        } else {
            Require(ERR_get_error_nid_postfix() != 0, std::source_location::current(), v.tcId);
            while (ERR_get_error_nid_postfix() != 0) {}
        }
    } else if (v.expectedValid == 1) {
        Require(setPeer == 1, std::source_location::current(), v.tcId);
        std::size_t length = 0;
        Require(EVP_PKEY_derive_nid_postfix(ctx, nullptr, &length) == 1, std::source_location::current(), v.tcId);
        Require(length == expectedShared.size(), std::source_location::current(), v.tcId);
        std::vector<unsigned char> shared(length);
        Require(EVP_PKEY_derive_nid_postfix(ctx, shared.data(), &length) == 1, std::source_location::current(), v.tcId);
        Require(length == expectedShared.size(), std::source_location::current(), v.tcId);
        Require(CompareBytes(shared, expectedShared.data(), expectedShared.size()), std::source_location::current(), v.tcId);
        Require(ERR_get_error_nid_postfix() == 0, std::source_location::current(), v.tcId);
    } else {
        if (setPeer == 1) {
            std::size_t length = 0;
            int lengthResult = EVP_PKEY_derive_nid_postfix(ctx, nullptr, &length);
            if (lengthResult == 1) {
                std::vector<unsigned char> shared(length);
                int deriveResult = EVP_PKEY_derive_nid_postfix(ctx, shared.data(), &length);
                Require(deriveResult <= 0, std::source_location::current(), v.tcId);
            } else {
                Require(lengthResult <= 0, std::source_location::current(), v.tcId);
            }
        } else {
            Require(setPeer <= 0, std::source_location::current(), v.tcId);
        }
        Require(ERR_get_error_nid_postfix() != 0, std::source_location::current(), v.tcId);
        while (ERR_get_error_nid_postfix() != 0) {}
    }

    EVP_PKEY_CTX_free_nid_postfix(ctx);
    EVP_PKEY_free_nid_disambig1_nid_postfix(localKey);
    if (peerKey) EVP_PKEY_free_nid_disambig1_nid_postfix(peerKey);
}

}

int main() {
    std::array<void*, 10> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Calloc_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    for (const auto& v : LibreSsl3WycheproofEcdh::Vectors) {
        RunVector(v);
    }

    Require(allocations.empty());
    std::puts("LibreSSL3 Wycheproof ECDH P-256/P-384 tests passed");
    return 0;
}
