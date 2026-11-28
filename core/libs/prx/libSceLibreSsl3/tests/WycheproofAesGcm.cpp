/* The test vectors are adapted for AnyPS5 from Project Wycheproof, testvectors_v1/aes_gcm_test.json
 * at commit 65ca494727a52944c204441c860b039e6786b99b
 * (https://github.com/C2SP/wycheproof/tree/65ca494727a52944c204441c860b039e6786b99b/testvectors_v1).
 * Altered: all 316 tests of the file are run; valid tests must encrypt to the listed ciphertext and
 * tag and decrypt back to the message, and invalid tests (modified tags and unsupported IV sizes)
 * must fail. The file has no acceptable tests.
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

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "WycheproofAesGcmVectors.hpp"

constexpr std::size_t ContextBytes = 680;
constexpr int SetIvLength = 0x9;
constexpr int GetTag = 0x10;
constexpr int SetTag = 0x11;

static void* APS5_VABI GuestHeapAllocate(std::size_t size) { return std::malloc(size); }
static void APS5_VABI GuestHeapFree(void* pointer) { std::free(pointer); }
static void* APS5_VABI GuestHeapCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI GuestHeapReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI GuestHeapAlign(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
static void* APS5_VABI GuestHeapRealign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned reallocation"); }
static int APS5_VABI GuestHeapPosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }

static void InitializeHeap() {
    void* const api[10] {reinterpret_cast<void*>(GuestHeapAllocate), reinterpret_cast<void*>(GuestHeapFree), reinterpret_cast<void*>(GuestHeapCalloc),
        reinterpret_cast<void*>(GuestHeapReallocate), reinterpret_cast<void*>(GuestHeapAlign), reinterpret_cast<void*>(GuestHeapRealign),
        reinterpret_cast<void*>(GuestHeapPosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}

static void Require(bool condition, int tcId, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "Wycheproof AES GCM check failed: tcId %d at line %u\n", tcId, location.line());
    std::abort();
}

class Context {
public:
    Context() : context(EVP_CIPHER_CTX_new_nid_postfix()) {}
    ~Context() { EVP_CIPHER_CTX_free_nid_postfix(context); }
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    LibreSslEvpCipherCtx* get() const { return context; }

private:
    LibreSslEvpCipherCtx* context;
};

static std::vector<unsigned char> HexToBytes(const char* hex) {
    std::vector<unsigned char> bytes;
    for (; hex[0] && hex[1]; hex += 2) {
        auto nibble = [](char c) -> unsigned char {
            return c <= '9' ? static_cast<unsigned char>(c - '0') : static_cast<unsigned char>(c - 'a' + 10);
        };
        bytes.push_back(static_cast<unsigned char>(nibble(hex[0]) << 4 | nibble(hex[1])));
    }
    return bytes;
}

static const LibreSslEvpCipher* GetCipher(int keyBits) {
    switch (keyBits) {
    case 128: return EVP_aes_128_gcm_nid_postfix();
    case 192: return EVP_aes_192_gcm_nid_postfix();
    case 256: return EVP_aes_256_gcm_nid_postfix();
    default: return nullptr;
    }
}

static int EncryptTest(const LibreSsl3WycheproofAesGcm::Vector& v) {
    const auto* cipher = GetCipher(v.keyBits);
    if (!cipher) return 0;
    Context ctx;
    if (!ctx.get()) return 0;
    const auto key = HexToBytes(v.keyHex);
    const auto iv = HexToBytes(v.ivHex);
    const auto aad = HexToBytes(v.aadHex);
    const auto msg = HexToBytes(v.msgHex);
    const auto expectedCt = HexToBytes(v.ctHex);
    const auto expectedTag = HexToBytes(v.tagHex);
    const int ivLen = static_cast<int>(iv.size());
    if (EVP_EncryptInit_ex_nid_postfix(ctx.get(), cipher, nullptr, nullptr, nullptr) != 1) return 0;
    if (EVP_CIPHER_CTX_ctrl_nid_postfix(ctx.get(), SetIvLength, ivLen, nullptr) != 1) return 0;
    if (EVP_EncryptInit_ex_nid_postfix(ctx.get(), nullptr, nullptr, key.data(), ivLen > 0 ? iv.data() : nullptr) != 1) return 0;
    if (!aad.empty()) {
        int outl = 0;
        if (EVP_EncryptUpdate_nid_postfix(ctx.get(), nullptr, &outl, aad.data(), static_cast<int>(aad.size())) != 1) return 0;
    }
    std::vector<unsigned char> ct(msg.size() + 16);
    int outl = 0;
    if (!msg.empty()) {
        if (EVP_EncryptUpdate_nid_postfix(ctx.get(), ct.data(), &outl, msg.data(), static_cast<int>(msg.size())) != 1) return 0;
    }
    int finalOutl = 0;
    if (EVP_EncryptFinal_ex_nid_postfix(ctx.get(), ct.data() + outl, &finalOutl) != 1) return 0;
    outl += finalOutl;
    ct.resize(outl);
    if (ct != expectedCt) return 0;
    std::vector<unsigned char> tag(16);
    if (EVP_CIPHER_CTX_ctrl_nid_postfix(ctx.get(), GetTag, static_cast<int>(expectedTag.size()), tag.data()) != 1) return 0;
    if (std::vector<unsigned char>(tag.begin(), tag.begin() + expectedTag.size()) != expectedTag) return 0;
    return 1;
}

static int DecryptTest(const LibreSsl3WycheproofAesGcm::Vector& v) {
    const auto* cipher = GetCipher(v.keyBits);
    if (!cipher) return 0;
    Context ctx;
    if (!ctx.get()) return 0;
    const auto key = HexToBytes(v.keyHex);
    const auto iv = HexToBytes(v.ivHex);
    const auto aad = HexToBytes(v.aadHex);
    const auto ct = HexToBytes(v.ctHex);
    const auto tag = HexToBytes(v.tagHex);
    const int ivLen = static_cast<int>(iv.size());
    if (EVP_DecryptInit_ex_nid_postfix(ctx.get(), cipher, nullptr, nullptr, nullptr) != 1) return 0;
    if (EVP_CIPHER_CTX_ctrl_nid_postfix(ctx.get(), SetIvLength, ivLen, nullptr) != 1) return 0;
    if (EVP_DecryptInit_ex_nid_postfix(ctx.get(), nullptr, nullptr, key.data(), ivLen > 0 ? iv.data() : nullptr) != 1) return 0;
    if (!aad.empty()) {
        int outl = 0;
        if (EVP_DecryptUpdate_nid_postfix(ctx.get(), nullptr, &outl, aad.data(), static_cast<int>(aad.size())) != 1) return 0;
    }
    if (EVP_CIPHER_CTX_ctrl_nid_postfix(ctx.get(), SetTag, static_cast<int>(tag.size()), const_cast<unsigned char*>(tag.data())) != 1) return 0;
    std::vector<unsigned char> msg(ct.size());
    int outl = 0;
    if (!ct.empty()) {
        if (EVP_DecryptUpdate_nid_postfix(ctx.get(), msg.data(), &outl, ct.data(), static_cast<int>(ct.size())) != 1) return 0;
    }
    int finalOutl = 0;
    const int finalResult = EVP_DecryptFinal_ex_nid_postfix(ctx.get(), msg.data() + outl, &finalOutl);
    if (finalResult == 1 && v.expectedValid == 1) {
        Require(outl + finalOutl == static_cast<int>(ct.size()) && msg == HexToBytes(v.msgHex), v.tcId);
    }
    return finalResult;
}

int main() {
    InitializeHeap();
    for (const auto& v : LibreSsl3WycheproofAesGcm::Vectors) {
        const int encryptResult = EncryptTest(v);
        const int decryptResult = DecryptTest(v);
        const int expected = v.expectedValid;
        Require(encryptResult == expected && decryptResult == expected, v.tcId);
    }
    return 0;
}
