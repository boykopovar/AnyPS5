/* The test vectors are adapted for AnyPS5 from Project Wycheproof,
 * testvectors_v1/hmac_sha256_test.json, hmac_sha512_test.json, hkdf_sha256_test.json at commit
 * 65ca494727a52944c204441c860b039e6786b99b
 * (https://github.com/C2SP/wycheproof/tree/65ca494727a52944c204441c860b039e6786b99b/testvectors_v1).
 * Altered: all 348 HMAC and 86 HKDF tests are run; a valid tag or output key must match and an
 * invalid one must not. The files have no acceptable tests.
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
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

#include "WycheproofHmacHkdfVectors.hpp"

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "Wycheproof HMAC/HKDF check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) {
    return std::malloc(size);
}

void APS5_VABI Free(void* memory) {
    std::free(memory);
}

void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    return std::calloc(count, size);
}

void* APS5_VABI Reallocate(void* memory, std::size_t size) {
    return std::realloc(memory, size);
}

void* APS5_VABI Align(std::size_t, std::size_t size) {
    return std::malloc(size);
}

void* APS5_VABI Realign(void* memory, std::size_t size, std::size_t) {
    return std::realloc(memory, size);
}

int APS5_VABI PosixAlign(void** memory, std::size_t, std::size_t size) {
    *memory = std::malloc(size);
    return *memory ? 0 : 12;
}

constexpr unsigned char Guard = 0xa5;

std::vector<std::uint8_t> Bytes(const char* hex) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; hex[i]; i += 2) bytes.push_back(static_cast<std::uint8_t>(std::stoul(std::string(hex + i, 2), nullptr, 16)));
    return bytes;
}

const EvpMd* DigestFromBits(int shaBits) {
    if (shaBits == 256) return EVP_sha256_nid_postfix();
    return EVP_sha512_nid_postfix();
}

void CheckHmacVectors() {
    for (const auto& vector : LibreSsl3WycheproofHmacHkdf::HmacVectors) {
        const auto key = Bytes(vector.keyHex);
        const auto msg = Bytes(vector.msgHex);
        const auto tag = Bytes(vector.tagHex);
        const EvpMd* digest = DigestFromBits(vector.shaBits);
        const int digestSize = EVP_MD_size_nid_postfix(digest);
        const std::size_t tagBytes = static_cast<std::size_t>(vector.tagBytes);
        Require(tagBytes <= static_cast<std::size_t>(digestSize));

        std::vector<std::uint8_t> computed(digestSize);
        std::uint32_t computedLen = 0;
        const unsigned char* hmacResult = HMAC_nid_postfix(digest, key.data(), static_cast<int>(key.size()), msg.data(), msg.size(), computed.data(), &computedLen);
        Require(hmacResult != nullptr);
        Require(static_cast<std::size_t>(computedLen) == static_cast<std::size_t>(digestSize));

        const bool valid = std::memcmp(computed.data(), tag.data(), tagBytes) == 0;
        Require(valid == static_cast<bool>(vector.expectedValid));
    }
}

int DeriveHkdf(std::vector<std::uint8_t>& out, std::size_t length, const EvpMd* digest, const std::vector<std::uint8_t>& ikm, const std::vector<std::uint8_t>& salt, const std::vector<std::uint8_t>& info) {
    out.assign(length + 1, Guard);
    const int result = HKDF_nid_postfix(out.data(), length, digest, ikm.data(), ikm.size(), salt.empty() ? nullptr : salt.data(), salt.size(), info.empty() ? nullptr : info.data(), info.size());
    Require(out[length] == Guard);
    out.pop_back();
    return result;
}

void CheckHkdfVectors() {
    for (const auto& vector : LibreSsl3WycheproofHmacHkdf::HkdfVectors) {
        const auto ikm = Bytes(vector.ikmHex);
        const auto salt = Bytes(vector.saltHex);
        const auto info = Bytes(vector.infoHex);
        const auto okm = Bytes(vector.okmHex);
        const EvpMd* digest = EVP_sha256_nid_postfix();
        const int digestSize = EVP_MD_size_nid_postfix(digest);
        const std::size_t sizeBytes = static_cast<std::size_t>(vector.sizeBytes);
        const std::size_t limit = static_cast<std::size_t>(digestSize) * 255;

        std::vector<std::uint8_t> out;
        ERR_clear_error_nid_postfix();
        const int result = DeriveHkdf(out, sizeBytes, digest, ikm, salt, info);
        const bool valid = (result == 1 && out == okm);
        Require(valid == static_cast<bool>(vector.expectedValid));

        if (sizeBytes > limit) {
            Require(result == 0);
            Require(out.size() == 0 || okm.empty() || std::memcmp(out.data(), okm.data(), std::min(out.size(), okm.size())) == 0);
        }
    }
}

}

int main() {
    void* api[10] {reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
                   reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
                   reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    CheckHmacVectors();
    CheckHkdfVectors();
    std::puts("Wycheproof HMAC-SHA256/512 and HKDF-SHA256: ok");
    return 0;
}
