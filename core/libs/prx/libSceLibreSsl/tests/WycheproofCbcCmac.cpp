/* The test vectors are adapted for AnyPS5 from Project Wycheproof,
 * testvectors_v1/aes_cbc_pkcs5_test.json, aes_cmac_test.json at commit
 * 65ca494727a52944c204441c860b039e6786b99b
 * (https://github.com/C2SP/wycheproof/tree/65ca494727a52944c204441c860b039e6786b99b/testvectors_v1).
 * Altered: all 216 CBC and 311 CMAC tests are run; valid CBC tests must decrypt and encrypt to the
 * listed values and invalid ones (bad padding and empty ciphertexts) must fail in
 * EVP_DecryptFinal_ex with an error queued; valid CMAC tags must match, invalid ones must not, and
 * keys of an invalid size must be refused by CMAC_Init. The files have no acceptable tests.
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
#include "prx/libSceLibreSsl/include/Cmac.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>
#include "prx/libSceLibreSsl/tests/WycheproofCbcCmacVectors.hpp"

namespace {

void Require(bool condition, const char* message, int tcId = -1) {
    if (condition) return;
    std::fprintf(stderr, tcId >= 0 ? "failed: %s (tcId=%d)\n" : "failed: %s\n", message, tcId);
    std::abort();
}

int allocationsUntilFailure = -1;
std::vector<std::pair<void*, std::size_t>> allocations;

void* APS5_VABI Allocate(std::size_t size) {
    if (allocationsUntilFailure == 0) return nullptr;
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    auto* pointer = std::malloc(size);
    if (pointer != nullptr) allocations.emplace_back(pointer, size);
    return pointer;
}
void* APS5_VABI Reallocate(void*, std::size_t) { throw std::logic_error("unexpected reallocation"); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected realignment"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }

void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    if (allocationsUntilFailure == 0) return nullptr;
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    auto* pointer = std::calloc(count, size);
    if (pointer != nullptr) allocations.emplace_back(pointer, count * size);
    return pointer;
}

void APS5_VABI Free(void* pointer) {
    if (pointer == nullptr) return;
    const auto found = std::find_if(allocations.begin(), allocations.end(), [&](const auto& entry) { return entry.first == pointer; });
    Require(found != allocations.end(), "free a guest-owned allocation");
    const auto* bytes = static_cast<const unsigned char*>(pointer);
    Require(std::all_of(bytes, bytes + found->second, [](auto value) { return value == 0; }), "wipe contexts and key storage before freeing");
    allocations.erase(found);
    std::free(pointer);
}

void InitializeHeap() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

std::vector<unsigned char> Hex(std::string_view text) {
    Require(text.size() % 2 == 0, "hex fixture has complete bytes");
    std::vector<unsigned char> bytes;
    for (std::size_t index = 0; index < text.size(); index += 2) {
        const auto nibble = [](char value) { return value <= '9' ? value - '0' : value - 'a' + 10; };
        bytes.push_back(static_cast<unsigned char>((nibble(text[index]) << 4) | nibble(text[index + 1])));
    }
    return bytes;
}

const LibreSslCipher* Methods[3];

using CipherContext = std::unique_ptr<LibreSslEvpCipherCtx, void (APS5_VABI*)(LibreSslEvpCipherCtx*)>;

CipherContext NewCipherContext() {
    CipherContext context(EVP_CIPHER_CTX_new_nid_postfix(), EVP_CIPHER_CTX_free_nid_postfix);
    Require(context != nullptr, "allocate cipher context");
    return context;
}

using CmacContext = std::unique_ptr<LibreSslCmac, void (APS5_VABI*)(LibreSslCmac*)>;

CmacContext NewCmacContext() {
    CmacContext context(CMAC_CTX_new_nid_postfix(), CMAC_CTX_free_nid_postfix);
    Require(context != nullptr, "allocate CMAC context");
    return context;
}

std::array<unsigned char, 16> CmacFinal(LibreSslCmac* context) {
    std::array<unsigned char, 18> output;
    output.fill(0xa5);
    std::size_t length = 99;
    Require(CMAC_Final_nid_postfix(context, output.data() + 1, &length) == 1, "finalize CMAC");
    Require(length == 16 && output.front() == 0xa5 && output.back() == 0xa5, "tag size and output bounds");
    std::array<unsigned char, 16> result;
    std::copy_n(output.data() + 1, 16, result.data());
    return result;
}

void TestCbcDecrypt() {
    for (const auto& vector : LibreSslWycheproofCbcCmac::CbcVectors) {
        if (vector.expectedValid != 1) continue;

        const auto key = Hex(vector.keyHex);
        const auto iv = Hex(vector.ivHex);
        const auto ct = Hex(vector.ctHex);
        const auto expected = Hex(vector.msgHex);

        const auto* method = Methods[(vector.keyBits / 64) - 2];
        Require(method->keyLength == vector.keyBits / 8, "key length matches");

        auto context = NewCipherContext();
        Require(EVP_DecryptInit_ex_nid_postfix(context.get(), reinterpret_cast<const LibreSslEvpCipher*>(method), nullptr, key.data(), iv.data()) == 1, "decrypt init");

        std::vector<unsigned char> output(ct.size() + 16);
        int written = 0;
        Require(EVP_DecryptUpdate_nid_postfix(context.get(), output.data(), &written, ct.data(), static_cast<int>(ct.size())) == 1, "decrypt update");
        int finalWritten = 0;
        const int result = EVP_DecryptFinal_ex_nid_postfix(context.get(), output.data() + written, &finalWritten);
        Require(result == 1, "decrypt final succeeds for valid vector");
        written += finalWritten;
        output.resize(written);
        Require(output == expected, "decrypted plaintext matches");
    }
}

void TestCbcEncrypt() {
    for (const auto& vector : LibreSslWycheproofCbcCmac::CbcVectors) {
        if (vector.expectedValid != 1) continue;

        const auto key = Hex(vector.keyHex);
        const auto iv = Hex(vector.ivHex);
        const auto msg = Hex(vector.msgHex);
        const auto expected = Hex(vector.ctHex);

        const auto* method = Methods[(vector.keyBits / 64) - 2];
        Require(method->keyLength == vector.keyBits / 8, "key length matches");

        auto context = NewCipherContext();
        Require(EVP_EncryptInit_ex_nid_postfix(context.get(), reinterpret_cast<const LibreSslEvpCipher*>(method), nullptr, key.data(), iv.data()) == 1, "encrypt init");

        std::vector<unsigned char> output(msg.size() + 16);
        const unsigned char none = 0;
        int written = 0;
        Require(EVP_EncryptUpdate_nid_postfix(context.get(), output.data(), &written, msg.empty() ? &none : msg.data(), static_cast<int>(msg.size())) == 1, "encrypt update");
        int finalWritten = 0;
        Require(EVP_EncryptFinal_ex_nid_postfix(context.get(), output.data() + written, &finalWritten) == 1, "encrypt final");
        written += finalWritten;
        output.resize(written);
        Require(output == expected, "encrypted ciphertext matches");
    }
}

void TestCbcInvalidPadding() {
    for (const auto& vector : LibreSslWycheproofCbcCmac::CbcVectors) {
        if (vector.expectedValid != 0) continue;

        const auto key = Hex(vector.keyHex);
        const auto iv = Hex(vector.ivHex);
        const auto ct = Hex(vector.ctHex);

        const auto* method = Methods[(vector.keyBits / 64) - 2];
        Require(method->keyLength == vector.keyBits / 8, "key length matches");

        auto context = NewCipherContext();
        Require(EVP_DecryptInit_ex_nid_postfix(context.get(), reinterpret_cast<const LibreSslEvpCipher*>(method), nullptr, key.data(), iv.data()) == 1, "decrypt init");

        std::vector<unsigned char> output(ct.size() + 16);
        int written = 0;
        const unsigned char none = 0;
        Require(EVP_DecryptUpdate_nid_postfix(context.get(), output.data(), &written, ct.empty() ? &none : ct.data(), static_cast<int>(ct.size())) == 1, "decrypt update");
        int finalWritten = 0;
        ERR_clear_error_nid_postfix();
        const int result = EVP_DecryptFinal_ex_nid_postfix(context.get(), output.data() + written, &finalWritten);
        Require(result == 0, "decrypt final fails for invalid padding");
        Require(ERR_get_error_nid_postfix() != 0, "error queue set on padding failure");
    }
}

void TestCmac() {
    for (const auto& vector : LibreSslWycheproofCbcCmac::CmacVectors) {
        const auto key = Hex(vector.keyHex);
        const auto msg = Hex(vector.msgHex);
        const auto expected = Hex(vector.tagHex);

        const bool validKeyBits = vector.keyBits == 128 || vector.keyBits == 192 || vector.keyBits == 256;
        const auto* method = vector.keyBits == 192 ? Methods[1] : vector.keyBits == 256 ? Methods[2] : Methods[0];
        if (validKeyBits) Require(method->keyLength == vector.keyBits / 8, "key length matches");

        auto context = NewCmacContext();
        const unsigned char dummy = 0;
        const void* keyPtr = key.empty() ? &dummy : key.data();
        const int initResult = CMAC_Init_nid_postfix(context.get(), keyPtr, key.size(), method, nullptr);
        if (!validKeyBits) {
            if (initResult != 0 || vector.expectedValid != 0) {
                Require(false, "CMAC init rejects invalid key length", vector.tcId);
            }
            continue;
        }
        Require(initResult == 1, "CMAC init", vector.tcId);
        Require(CMAC_Update_nid_postfix(context.get(), msg.data(), msg.size()) == 1, "CMAC update");
        const auto actual = CmacFinal(context.get());

        if (vector.expectedValid == 1) {
            Require(expected.size() == actual.size() && std::equal(actual.begin(), actual.end(), expected.begin()), "CMAC tag matches for valid vector");
        } else {
            Require(!(expected.size() == actual.size() && std::equal(actual.begin(), actual.end(), expected.begin())), "CMAC tag differs for invalid vector");
        }
    }
}

}

int main() {
    InitializeHeap();
    Methods[0] = EVP_aes_128_cbc_nid_postfix();
    Methods[1] = EVP_aes_192_cbc_nid_postfix();
    Methods[2] = EVP_aes_256_cbc_nid_postfix();
    TestCbcDecrypt();
    TestCbcEncrypt();
    TestCbcInvalidPadding();
    TestCmac();
    Require(allocations.empty(), "all guest allocations released");
    std::puts("Wycheproof AES-CBC-PKCS5 and AES-CMAC tests passed");
}
