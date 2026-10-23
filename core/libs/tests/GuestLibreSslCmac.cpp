#include "prx/libSceLibreSsl/include/Cmac.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <mbedtls/cipher.h>
#include <mbedtls/cmac.h>
#include <mbedtls/aes.h>
#include <mbedtls/des.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifdef ANYPS5_CMAC_DYNAMIC
namespace {
decltype(&CMAC_CTX_new_nid_postfix) cmacNew;
decltype(&CMAC_CTX_free_nid_postfix) cmacFree;
decltype(&CMAC_Init_nid_postfix) cmacInit;
decltype(&CMAC_Update_nid_postfix) cmacUpdate;
decltype(&CMAC_Final_nid_postfix) cmacFinal;
decltype(&EVP_aes_128_cbc_nid_postfix) aes128Method;
decltype(&EVP_aes_192_cbc_nid_postfix) aes192Method;
decltype(&EVP_aes_256_cbc_nid_postfix) aes256Method;
decltype(&ERR_clear_error_nid_postfix) clearError;
decltype(&ERR_get_error_nid_postfix) getError;
decltype(&ApplicationHeapRegister_nid_no_patch) registerHeap;

void* Load(const std::string& path) {
#ifdef _WIN32
    auto* library = LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    auto* library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    if (library == nullptr) throw std::runtime_error("failed to load patched CMAC dependency: " + path);
    return library;
}

template<typename TFunction> void Resolve(TFunction& destination, void* library, const char* name) {
#ifdef _WIN32
    destination = reinterpret_cast<TFunction>(GetProcAddress(static_cast<HMODULE>(library), name));
#else
    destination = reinterpret_cast<TFunction>(dlsym(library, name));
#endif
    if (destination == nullptr) throw std::runtime_error(std::string("missing patched CMAC dependency: ") + name);
}

void InitializePatched(const char* directory) {
    const auto root = std::string(directory) + "/";
    auto* libc = Load(root + "libc.prx");
    auto* library = Load(root + "libSceLibreSsl.prx");
    Resolve(cmacNew, library, "mwJYST4Bb3U");
    Resolve(cmacFree, library, "cBNGAG1DkXc");
    Resolve(cmacInit, library, "LATrF+dIZYI");
    Resolve(cmacUpdate, library, "XpAJ1Ckg-q4");
    Resolve(cmacFinal, library, "6MM7ng9P+18");
    Resolve(aes128Method, library, "rV07jlO-+6Q");
    Resolve(aes192Method, library, "0X5DnA3olsk");
    Resolve(aes256Method, library, "J44dsZUcK4I");
    Resolve(clearError, library, "sieQ--PINWg");
    Resolve(getError, library, "DJg+qWX1rwY");
    Resolve(registerHeap, libc, "ApplicationHeapRegister_nid_no_patch");
}
}

#define CMAC_CTX_new_nid_postfix cmacNew
#define CMAC_CTX_free_nid_postfix cmacFree
#define CMAC_Init_nid_postfix cmacInit
#define CMAC_Update_nid_postfix cmacUpdate
#define CMAC_Final_nid_postfix cmacFinal
#define EVP_aes_128_cbc_nid_postfix aes128Method
#define EVP_aes_192_cbc_nid_postfix aes192Method
#define EVP_aes_256_cbc_nid_postfix aes256Method
#define ERR_clear_error_nid_postfix clearError
#define ERR_get_error_nid_postfix getError
#define ApplicationHeapRegister_nid_no_patch registerHeap
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
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
void* APS5_VABI Reallocate(void*, std::size_t) { throw std::logic_error("unexpected CMAC reallocation"); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::logic_error("unexpected CMAC aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected CMAC realignment"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected CMAC aligned allocation"); }

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

template<typename TAction> void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, "unsupported input must throw");
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

using CmacFree = void (APS5_VABI*)(LibreSslCmac*);
using Context = std::unique_ptr<LibreSslCmac, CmacFree>;

Context NewContext() {
    Context context(CMAC_CTX_new_nid_postfix(), CMAC_CTX_free_nid_postfix);
    Require(context != nullptr, "allocate CMAC context");
    return context;
}

std::array<unsigned char, 16> Final(LibreSslCmac* context) {
    std::array<unsigned char, 18> output;
    output.fill(0xa5);
    std::size_t length = 99;
    Require(CMAC_Final_nid_postfix(context, output.data() + 1, &length) == 1, "finalize CMAC");
    Require(length == 16 && output.front() == 0xa5 && output.back() == 0xa5, "tag size and output bounds");
    std::array<unsigned char, 16> result;
    std::copy_n(output.data() + 1, 16, result.data());
    return result;
}

const LibreSslCipher* Methods[3];

void RfcVectors() {
    const auto key = Hex("2b7e151628aed2a6abf7158809cf4f3c");
    const auto message = Hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
    const std::size_t lengths[] = {0, 16, 40, 64};
    const char* tags[] = {"bb1d6929e95937287fa37d129b756746", "070a16b46b4d4144f79bdd9dd04a287c", "dfa66747de9ae63030ca32611497c827", "51f0bebf7e3b9d92fc49741779363cfe"};
    auto context = NewContext();
    for (std::size_t vector = 0; vector < 4; ++vector) {
        const auto expected = Hex(tags[vector]);
        for (std::size_t split = 0; split <= lengths[vector]; ++split) {
            Require(CMAC_Init_nid_postfix(context.get(), key.data(), key.size(), Methods[0], nullptr) == 1, "RFC key initialization");
            Require(CMAC_Update_nid_postfix(context.get(), message.data(), split) == 1, "RFC first update");
            Require(CMAC_Update_nid_postfix(context.get(), nullptr, 0) == 1, "empty update");
            Require(CMAC_Update_nid_postfix(context.get(), message.data() + split, lengths[vector] - split) == 1, "RFC second update");
            std::size_t length = 99;
            Require(CMAC_Final_nid_postfix(context.get(), nullptr, &length) == 1 && length == 16, "length-only final");
            Require(CMAC_Final_nid_postfix(context.get(), nullptr, &length) == 1 && length == 16, "repeat length query");
            const auto actual = Final(context.get());
            if (!std::equal(actual.begin(), actual.end(), expected.begin())) {
                std::fprintf(stderr, "RFC vector %zu split %zu: ", vector, split);
                for (auto byte : actual) std::fprintf(stderr, "%02x", byte);
                std::fputc('\n', stderr);
            }
            Require(std::equal(actual.begin(), actual.end(), expected.begin()), "RFC 4493 tag");
        }
    }
}

void Differential() {
    const mbedtls_cipher_type_t types[] = {MBEDTLS_CIPHER_AES_128_ECB, MBEDTLS_CIPHER_AES_192_ECB, MBEDTLS_CIPHER_AES_256_ECB};
    std::array<unsigned char, 32> key;
    std::array<unsigned char, 257> message;
    for (std::size_t index = 0; index < key.size(); ++index) key[index] = static_cast<unsigned char>(index * 73 + 41);
    for (std::size_t index = 0; index < message.size(); ++index) message[index] = static_cast<unsigned char>(index * 197 + 17);
    auto context = NewContext();
    for (std::size_t method = 0; method < 3; ++method) {
        const auto* cipher = Methods[method];
        Require(cipher->nid == 419 + static_cast<int>(method) * 4 && cipher->blockSize == 16 && cipher->keyLength == 16 + static_cast<int>(method) * 8 && cipher->ivLength == 16 && cipher->flags == 0x5002, "AES descriptor metadata");
        const auto* reference = mbedtls_cipher_info_from_type(types[method]);
        Require(reference != nullptr, "reference cipher available");
        for (std::size_t length = 0; length <= message.size(); ++length) {
            std::array<unsigned char, 16> expected;
            Require(mbedtls_cipher_cmac(reference, key.data(), cipher->keyLength * 8, message.data(), length, expected.data()) == 0, "reference CMAC");
            for (const auto chunk : {1u, 7u, 15u, 16u, 17u, 64u, 257u}) {
                Require(CMAC_Init_nid_postfix(context.get(), key.data(), cipher->keyLength, cipher, nullptr) == 1, "differential initialization");
                for (std::size_t offset = 0; offset < length;) {
                    const auto take = std::min(static_cast<std::size_t>(chunk), length - offset);
                    Require(CMAC_Update_nid_postfix(context.get(), message.data() + offset, take) == 1, "differential update");
                    offset += take;
                }
                Require(Final(context.get()) == expected, "CMAC agrees with independent backend");
            }
        }
    }
}

void Lifecycle() {
    auto context = NewContext();
    const std::array<unsigned char, 32> key{};
    const std::array<unsigned char, 19> message{};
    std::array<unsigned char, 16> output;
    output.fill(0xa5);
    std::size_t length = 99;
    Require(CMAC_Update_nid_postfix(context.get(), nullptr, 1) == 0, "uninitialized update");
    Require(CMAC_Final_nid_postfix(context.get(), output.data(), &length) == 0 && length == 99 && output[0] == 0xa5, "uninitialized final preserves outputs");
    Require(CMAC_Final_nid_postfix(context.get(), nullptr, nullptr) == 0, "uninitialized final precedes null output length");
    Require(CMAC_Init_nid_postfix(context.get(), nullptr, 0, nullptr, nullptr) == 0, "uninitialized restart");
    Require(CMAC_Init_nid_postfix(context.get(), key.data(), 16, nullptr, nullptr) == 0, "key without cipher");
    Require(CMAC_Init_nid_postfix(context.get(), nullptr, 1, nullptr, nullptr) == 1, "nonzero key length without a key is not restart");
    Require(CMAC_Init_nid_postfix(context.get(), nullptr, 0, Methods[0], nullptr) == 1, "stage cipher selection");
    Require(CMAC_Update_nid_postfix(context.get(), message.data(), 1) == 0, "cipher-only context has no key");
    ERR_clear_error_nid_postfix();
    Require(CMAC_Init_nid_postfix(context.get(), key.data(), 15, nullptr, nullptr) == 0, "reject invalid key length");
    Require(ERR_get_error_nid_postfix() == 0x06fff082, "invalid key length publishes EVP error");
    Require(CMAC_Init_nid_postfix(context.get(), key.data(), 16, nullptr, nullptr) == 1, "finish staged initialization");
    Require(CMAC_Update_nid_postfix(context.get(), message.data(), message.size()) == 1, "first message");
    const auto expected = Final(context.get());
    const auto repeated = Final(context.get());
    Require(repeated != expected, "final advances the CBC state");
    Require(CMAC_Init_nid_postfix(context.get(), nullptr, 0, nullptr, nullptr) == 1, "restart reuses the key");
    Require(CMAC_Update_nid_postfix(context.get(), message.data(), message.size()) == 1 && Final(context.get()) == expected, "restart resets state");
    Require(CMAC_Init_nid_postfix(context.get(), key.data(), (std::size_t{1} << 32) + 16, nullptr, nullptr) == 1, "upstream key length narrows to int");
    Require(CMAC_Update_nid_postfix(context.get(), message.data(), message.size()) == 1 && Final(context.get()) == expected, "narrowed key length tag");
    Require(CMAC_Init_nid_postfix(context.get(), key.data(), 24, Methods[1], nullptr) == 1, "switch AES key size");
    Require(CMAC_Init_nid_postfix(context.get(), key.data(), 32, Methods[2], nullptr) == 1, "switch AES key size again");
    RequireThrows([&] { CMAC_Init_nid_postfix(nullptr, key.data(), 16, Methods[0], nullptr); });
    RequireThrows([&] { CMAC_Update_nid_postfix(nullptr, nullptr, 0); });
    RequireThrows([&] { CMAC_Final_nid_postfix(nullptr, output.data(), &length); });
    RequireThrows([&] { CMAC_Update_nid_postfix(context.get(), nullptr, 1); });
    RequireThrows([&] { CMAC_Final_nid_postfix(context.get(), output.data(), nullptr); });
    CMAC_CTX_free_nid_postfix(nullptr);
}

void AllocationFailures() {
    allocationsUntilFailure = 0;
    Require(CMAC_CTX_new_nid_postfix() == nullptr, "context allocation failure");
    allocationsUntilFailure = 1;
    auto context = NewContext();
    const std::array<unsigned char, 16> key{};
    ERR_clear_error_nid_postfix();
    Require(CMAC_Init_nid_postfix(context.get(), key.data(), key.size(), Methods[0], nullptr) == 0, "cipher allocation failure");
    Require(ERR_get_error_nid_postfix() == 0x06fff041, "cipher allocation failure publishes EVP error");
    allocationsUntilFailure = -1;
    Require(CMAC_Init_nid_postfix(context.get(), key.data(), key.size(), Methods[0], nullptr) == 1, "recover from failed allocation");
}

void CipherCallbacks() {
    std::array<unsigned char, 32> key{};
    std::array<unsigned char, 16> iv;
    for (std::size_t index = 0; index < iv.size(); ++index) iv[index] = static_cast<unsigned char>(index + 1);
    for (const auto* method : Methods) {
        for (std::size_t length = 0; length <= 48; ++length) {
            const auto padded = (length + 15) / 16 * 16;
            std::vector<unsigned char> input(padded);
            for (std::size_t index = 0; index < length; ++index) input[index] = static_cast<unsigned char>(index * 31 + 17);
            std::vector<unsigned char> expected(padded);
            auto referenceIv = iv;
            mbedtls_aes_context reference;
            mbedtls_aes_init(&reference);
            Require(mbedtls_aes_setkey_enc(&reference, key.data(), method->keyLength * 8) == 0, "reference AES key");
            Require(mbedtls_aes_crypt_cbc(&reference, MBEDTLS_AES_ENCRYPT, padded, referenceIv.data(), input.data(), expected.data()) == 0, "reference CBC");
            mbedtls_aes_free(&reference);
            std::vector<unsigned char> storage(method->contextSize);
            LibreSslCipherContext context{};
            context.cipher = method;
            context.cipherData = storage.data();
            context.keyLength = method->keyLength;
            for (const bool inPlace : {false, true}) {
                context.encrypt = 1;
                std::copy(iv.begin(), iv.end(), context.iv);
                Require(method->init(&context, key.data(), nullptr, 1) == 1, "AES descriptor encryption setup");
                auto output = input;
                output.push_back(0xa5);
                Require(method->cipher(&context, output.data(), inPlace ? output.data() : input.data(), length) == 1, "AES descriptor partial encryption");
                Require(output.back() == 0xa5 && std::equal(expected.begin(), expected.end(), output.begin()), "CBC partial encryption and bounds");
                Require(std::equal(referenceIv.begin(), referenceIv.end(), context.iv), "CBC encryption IV");
                context.encrypt = 0;
                std::copy(iv.begin(), iv.end(), context.iv);
                Require(method->init(&context, key.data(), nullptr, 0) == 1, "AES descriptor decryption setup");
                if (!inPlace) output.assign(padded + 1, 0xa5);
                const auto before = output;
                Require(method->cipher(&context, output.data(), inPlace ? output.data() : expected.data(), length) == 1, "AES descriptor partial decryption");
                Require(std::equal(input.begin(), input.begin() + length, output.begin()), "CBC decrypted prefix");
                Require(std::equal(before.begin() + length, before.end(), output.begin() + length), "partial decryption preserves trailing bytes");
                Require(std::equal(referenceIv.begin(), referenceIv.end(), context.iv), "CBC decryption IV");
            }
        }
    }
}

int APS5_VABI DesInit(LibreSslCipherContext* context, const unsigned char* key, const unsigned char*, int) {
    auto* des = static_cast<mbedtls_des3_context*>(context->cipherData);
    mbedtls_des3_init(des);
    return mbedtls_des3_set3key_enc(des, key) == 0;
}

int APS5_VABI DesCipher(LibreSslCipherContext* context, unsigned char* output, const unsigned char* input, std::size_t length) {
    return mbedtls_des3_crypt_cbc(static_cast<mbedtls_des3_context*>(context->cipherData), MBEDTLS_DES_ENCRYPT, length, context->iv, input, output) == 0;
}

void GuestDes() {
    const LibreSslCipher method{44, 8, 24, 8, 2, DesInit, DesCipher, nullptr, sizeof(mbedtls_des3_context), nullptr, nullptr, nullptr, nullptr};
    std::array<unsigned char, 24> key;
    std::array<unsigned char, 33> message;
    for (std::size_t index = 0; index < key.size(); ++index) key[index] = static_cast<unsigned char>(index * 31 + 7);
    for (std::size_t index = 0; index < message.size(); ++index) message[index] = static_cast<unsigned char>(index * 71 + 3);
    const auto* reference = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_DES_EDE3_ECB);
    Require(reference != nullptr, "reference 64-bit cipher");
    auto context = NewContext();
    for (std::size_t length = 0; length <= message.size(); ++length) {
        std::array<unsigned char, 8> expected;
        Require(mbedtls_cipher_cmac(reference, key.data(), 192, message.data(), length, expected.data()) == 0, "reference 64-bit CMAC");
        Require(CMAC_Init_nid_postfix(context.get(), key.data(), key.size(), &method, nullptr) == 1, "guest 64-bit cipher initialization");
        for (std::size_t index = 0; index < length; ++index) Require(CMAC_Update_nid_postfix(context.get(), message.data() + index, 1) == 1, "guest 64-bit cipher update");
        std::array<unsigned char, 10> actual;
        actual.fill(0xa5);
        std::size_t outputLength = 99;
        Require(CMAC_Final_nid_postfix(context.get(), actual.data() + 1, &outputLength) == 1, "guest 64-bit cipher final");
        Require(outputLength == 8 && actual.front() == 0xa5 && actual.back() == 0xa5 && std::equal(expected.begin(), expected.end(), actual.begin() + 1), "64-bit CMAC tag and output bounds");
    }
}

const LibreSslCipher* delegated;
int initCalls;
int cipherCalls;
int cleanupCalls;
int failAt;

int APS5_VABI GuestInit(LibreSslCipherContext* context, const unsigned char* key, const unsigned char* iv, int encrypt) {
    ++initCalls;
    Require(context->encrypt == 1 && context->keyLength == 16 && context->cipherData != nullptr, "guest callback context");
    return delegated->init(context, key, iv, encrypt);
}

int APS5_VABI GuestCipher(LibreSslCipherContext* context, unsigned char* output, const unsigned char* input, std::size_t length) {
    ++cipherCalls;
    if (failAt == cipherCalls) {
        std::memset(output, 0x5a, length);
        return 0;
    }
    return delegated->cipher(context, output, input, length);
}

int APS5_VABI GuestCleanup(LibreSslCipherContext*) {
    ++cleanupCalls;
    return 1;
}

int APS5_VABI GuestControl(LibreSslCipherContext* context, int command, int argument, void*) {
    if (command == 0) return 1;
    if (command == 1 && argument == 16) {
        context->keyLength = argument;
        return 1;
    }
    return -1;
}

void Callbacks() {
    delegated = Methods[0];
    auto method = *delegated;
    method.init = GuestInit;
    method.cipher = GuestCipher;
    method.cleanup = GuestCleanup;
    method.control = GuestControl;
    method.flags |= 0xc0;
    std::array<unsigned char, 16> key{};
    std::array<unsigned char, 33> message{};
    {
        auto context = NewContext();
        Require(CMAC_Init_nid_postfix(context.get(), key.data(), key.size(), &method, nullptr) == 1 && initCalls == 1, "guest initialization callback");
        Require(CMAC_Update_nid_postfix(context.get(), message.data(), message.size()) == 1 && cipherCalls == 3, "guest cipher callbacks");
        const auto tag = Final(context.get());
        Require(CMAC_Init_nid_postfix(context.get(), nullptr, 0, nullptr, nullptr) == 1, "guest callback restart");
        Require(CMAC_Update_nid_postfix(context.get(), message.data(), message.size()) == 1 && Final(context.get()) == tag, "guest-defined cipher matches builtin");
        failAt = cipherCalls + 1;
        std::array<unsigned char, 16> output;
        output.fill(0xa5);
        std::size_t length = 99;
        Require(CMAC_Final_nid_postfix(context.get(), output.data(), &length) == 0 && length == 16, "cipher failure propagates");
        Require(std::all_of(output.begin(), output.end(), [](auto value) { return value == 0; }), "failed final clears output");
        failAt = 0;
    }
    Require(cleanupCalls == 1, "guest cleanup callback");
    auto context = NewContext();
    method.blockSize = 32;
    ERR_clear_error_nid_postfix();
    Require(CMAC_Init_nid_postfix(context.get(), key.data(), key.size(), &method, nullptr) == 0, "reject unsupported block size");
    Require(ERR_get_error_nid_postfix() == 0x06fff088, "bad block length publishes EVP error");
}
}

int main(int argc, char** argv) {
#ifdef ANYPS5_CMAC_DYNAMIC
    Require(argc == 2, "patched test needs the library directory");
    InitializePatched(argv[1]);
#endif
    InitializeHeap();
    Methods[0] = EVP_aes_128_cbc_nid_postfix();
    Methods[1] = EVP_aes_192_cbc_nid_postfix();
    Methods[2] = EVP_aes_256_cbc_nid_postfix();
    RfcVectors();
    Differential();
    Lifecycle();
    Callbacks();
    AllocationFailures();
    CipherCallbacks();
    GuestDes();
    Require(allocations.empty(), "all guest allocations released");
    std::puts("CMAC vectors, streaming, lifecycle and callback checks passed");
}
