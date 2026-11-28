/* The test vectors are adapted for AnyPS5 from Project Wycheproof,
 * testvectors_v1/ecdsa_secp256r1_sha256_test.json, ecdsa_secp384r1_sha384_test.json at commit
 * 65ca494727a52944c204441c860b039e6786b99b
 * (https://github.com/C2SP/wycheproof/tree/65ca494727a52944c204441c860b039e6786b99b/testvectors_v1).
 * Altered: all 988 tests of both files are run through ECDSA_verify; valid tests must verify and
 * invalid tests must not. The files have no acceptable tests.
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
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "WycheproofEcdsaVectors.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "Wycheproof ECDSA check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t bytes) { return std::malloc(bytes ? bytes : 1); }
void APS5_VABI Release(void* block) { std::free(block); }
void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) { return std::calloc(count ? count : 1, bytes ? bytes : 1); }
void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

std::vector<unsigned char> Hex(const char* text) {
    std::vector<unsigned char> bytes;
    for (std::size_t index = 0; text[index] != '\0'; index += 2) {
        const char pair[3] = {text[index], text[index + 1], '\0'};
        bytes.push_back(static_cast<unsigned char>(std::strtoul(pair, nullptr, 16)));
    }
    return bytes;
}

void RunVectors() {
    for (const auto& v : LibreSsl3WycheproofEcdsa::Vectors) {
        ERR_clear_error_nid_postfix();

        EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(v.curveNid);
        Require(key != nullptr);

        auto pubBytes = Hex(v.publicHex);
        const unsigned char* pubCursor = pubBytes.data();
        EcKey* decodedKey = o2i_ECPublicKey_nid_postfix(&key, &pubCursor, static_cast<std::int64_t>(pubBytes.size()));
        Require(decodedKey == key);
        Require(pubCursor == pubBytes.data() + pubBytes.size());
        Require(EC_KEY_check_key_nid_postfix(key) == 1);

        auto digBytes = Hex(v.digestHex);
        auto sigBytes = Hex(v.signatureHex);

        int result = ECDSA_verify_nid_postfix(0, digBytes.data(), static_cast<int>(digBytes.size()), sigBytes.data(), static_cast<int>(sigBytes.size()), key);

        if (v.expectedVerify == 1) {
            Require(result == 1);
        } else {
            Require(result <= 0);
        }

        EC_KEY_free_nid_postfix(key);
        ERR_clear_error_nid_postfix();
    }
}

}

int main() {
    InitializeHeap();
    RunVectors();
    std::puts("Wycheproof ECDSA P-256/P-384 tests passed");
}
