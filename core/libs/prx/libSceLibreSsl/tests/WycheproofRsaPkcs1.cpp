/* The test vectors are adapted for AnyPS5 from Project Wycheproof,
 * testvectors_v1/rsa_signature_2048_sha224_test.json, rsa_signature_2048_sha256_test.json,
 * rsa_signature_2048_sha384_test.json, rsa_signature_2048_sha512_test.json at commit
 * 65ca494727a52944c204441c860b039e6786b99b
 * (https://github.com/C2SP/wycheproof/tree/65ca494727a52944c204441c860b039e6786b99b/testvectors_v1).
 * Altered: all 1034 tests of the four files are run through RSA_verify with the digest of the message.
 * Each test that RSA_verify answers must give the result and the error codes that RSA_verify gives in
 * LibreSSL 2.5.4; a test whose DigestInfo uses a BER form that is not implemented must throw.
 * LibreSSL 2.5.4 decodes the DigestInfo with its BER decoder and does not compare it with its DER
 * re-encoding, so it accepts 143 of the tests, among them every test that Project Wycheproof lists as
 * valid.
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

#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/tests/WycheproofRsaPkcs1Vectors.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void Require(bool condition, const char* message, const LibreSslWycheproofRsaPkcs1::Vector* vector = nullptr) {
    if (condition) return;
    if (vector)
        std::fprintf(stderr, "failed: %s (%s tcId=%d)\n", message, LibreSslWycheproofRsaPkcs1::Keys[vector->key].source, vector->tcId);
    else
        std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

std::vector<void*> allocations;

void* APS5_VABI Allocate(std::size_t size) {
    auto* pointer = std::malloc(size ? size : 1);
    if (pointer != nullptr) allocations.push_back(pointer);
    return pointer;
}

void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    auto* pointer = std::calloc(count ? count : 1, size ? size : 1);
    if (pointer != nullptr) allocations.push_back(pointer);
    return pointer;
}

void APS5_VABI Free(void* pointer) {
    if (pointer == nullptr) return;
    const auto found = std::find(allocations.begin(), allocations.end(), pointer);
    Require(found != allocations.end(), "free a guest-owned allocation");
    allocations.erase(found);
    std::free(pointer);
}

void* APS5_VABI Reallocate(void*, std::size_t) { throw std::logic_error("unexpected reallocation"); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected realignment"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }

void InitializeHeap() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

std::vector<unsigned char> Hex(std::string_view text) {
    Require(text.size() % 2 == 0, "hex fixture has complete bytes");
    const auto nibble = [](char value) { return value <= '9' ? value - '0' : value - 'a' + 10; };
    std::vector<unsigned char> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2)
        bytes.push_back(static_cast<unsigned char>(nibble(text[index]) << 4 | nibble(text[index + 1])));
    return bytes;
}

std::vector<LibreSslRsa*> LoadKeys() {
    std::vector<LibreSslRsa*> keys;
    for (const auto& key : LibreSslWycheproofRsaPkcs1::Keys) {
        const auto der = Hex(key.publicKeyDerHex);
        const unsigned char* cursor = der.data();
        auto* rsa = d2i_RSA_PUBKEY_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(der.size()));
        Require(rsa != nullptr, "d2i_RSA_PUBKEY reads the group's public key");
        Require(cursor == der.data() + der.size(), "d2i_RSA_PUBKEY consumes the whole key");
        Require(RSA_size_nid_postfix(rsa) == 256, "the group's key has 2048 bits");
        keys.push_back(rsa);
    }
    Require(ERR_get_error_nid_postfix() == 0, "loading the keys queues no error");
    return keys;
}

std::vector<std::uint64_t> ExpectedErrors(const std::array<std::uint64_t, 5>& codes) {
    std::vector<std::uint64_t> errors;
    for (const auto code : codes)
        if (code) errors.push_back(code);
    return errors;
}

std::vector<std::uint64_t> QueuedErrors() {
    std::vector<std::uint64_t> errors;
    for (std::uint64_t code = ERR_get_error_nid_postfix(); code != 0; code = ERR_get_error_nid_postfix()) errors.push_back(code);
    return errors;
}

bool UnimplementedBerForm(const std::runtime_error& error) {
    const std::string_view message = error.what();
    return message.starts_with("RSA_verify_nid_postfix: BER ") && message.ends_with(" not implemented");
}

void Verify(const std::vector<LibreSslRsa*>& keys) {
    for (const auto& vector : LibreSslWycheproofRsaPkcs1::Vectors) {
        if (vector.wycheproofResult == 1) Require(vector.expectedVerify == 1, "LibreSSL 2.5.4 accepts every valid test", &vector);
        const auto& key = LibreSslWycheproofRsaPkcs1::Keys[vector.key];
        const auto digest = Hex(vector.digestHex);
        const auto signature = Hex(vector.signatureHex);
        const unsigned char empty = 0;
        int result = 0;
        try {
            result = RSA_verify_nid_postfix(key.digestType, digest.data(), static_cast<unsigned int>(digest.size()),
                signature.empty() ? &empty : signature.data(), static_cast<unsigned int>(signature.size()), keys[vector.key]);
        } catch (const std::runtime_error& error) {
            Require(UnimplementedBerForm(error), "RSA_verify throws only for a BER form that is not implemented", &vector);
            Require(vector.wycheproofResult != 1, "RSA_verify answers every valid test", &vector);
            ERR_clear_error_nid_postfix();
            continue;
        }
        Require(result == vector.expectedVerify, "RSA_verify gives the result of LibreSSL 2.5.4", &vector);
        Require(QueuedErrors() == ExpectedErrors(vector.expectedErrors), "RSA_verify queues the errors of LibreSSL 2.5.4", &vector);
    }
}

}

int main() {
    InitializeHeap();
    ERR_clear_error_nid_postfix();
    const auto keys = LoadKeys();
    Verify(keys);
    for (auto* rsa : keys) RSA_free_nid_postfix(rsa);
    Require(allocations.empty(), "all guest allocations released");
    std::puts("Wycheproof RSA PKCS#1 v1.5 signature tests passed");
}
