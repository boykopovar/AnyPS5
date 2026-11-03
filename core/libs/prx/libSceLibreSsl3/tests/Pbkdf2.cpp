#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 PBKDF2 check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::invalid_argument&) { return; } catch (const std::exception&) {}
    Require(false, location);
}

int allocationsBeforeFailure = -1;
int liveBlocks = 0;

bool FailAllocation() {
    if (allocationsBeforeFailure < 0) return false;
    return allocationsBeforeFailure-- == 0;
}

void* APS5_VABI Allocate(std::size_t bytes) {
    if (FailAllocation()) return nullptr;
    void* const block = std::malloc(bytes ? bytes : 1);
    if (block) ++liveBlocks;
    return block;
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (FailAllocation()) return nullptr;
    void* const block = std::calloc(count ? count : 1, bytes ? bytes : 1);
    if (block) ++liveBlocks;
    return block;
}

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

std::string Hex(const unsigned char* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < size; ++i) {
        text += digits[data[i] >> 4];
        text += digits[data[i] & 15];
    }
    return text;
}

const unsigned char* Bytes(const char* text) {
    return reinterpret_cast<const unsigned char*>(text);
}

constexpr std::size_t Guard = 8;
constexpr unsigned char Fill = 0xee;
constexpr std::uint64_t NullDigestError = 0x06fff09f;

bool Untouched(const std::vector<unsigned char>& buffer, std::size_t from) {
    for (std::size_t i = from; i < buffer.size(); ++i) if (buffer[i] != Fill) return false;
    return true;
}

std::string Derive(const char* password, int passwordLength, const unsigned char* salt, int saltLength, int iterations, const EvpMd* digest, int keyLength) {
    std::vector<unsigned char> output(static_cast<std::size_t>(keyLength) + Guard, Fill);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(password, passwordLength, salt, saltLength, iterations, digest, keyLength, output.data()) == 1);
    Require(Untouched(output, static_cast<std::size_t>(keyLength)));
    Require(ERR_get_error_nid_postfix() == 0);
    Require(liveBlocks == 0);
    return Hex(output.data(), static_cast<std::size_t>(keyLength));
}

void TestKnownAnswers() {
    const std::string passwdSalt = "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783";
    Require(Derive("passwd", 6, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), 64) == passwdSalt);
    Require(Derive("Password", 8, Bytes("NaCl"), 4, 80000, EVP_sha256_nid_postfix(), 64) == "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
    Require(Derive("password", 8, Bytes("salt"), 4, 3, EVP_md5_nid_postfix(), 40) == "f6acd4bda3e4d3d831a5f61da9ca9d5c3877566e979f4928778d81be4f2e9433a31043bbf3945c35");
    Require(Derive("pass\0word", 9, Bytes("sa\0lt"), 5, 5, EVP_sha384_nid_postfix(), 70) == "af10278310a96cefc4829ae970b4f9ac533947006b6283a30418cbe9af94738f3e72970aba21fd9dd520e87c8bb3351773b9559d5654e864b9be4b983678a515eee911219cce");
    std::string longPassword(200, '\0');
    for (std::size_t i = 0; i < longPassword.size(); ++i) longPassword[i] = static_cast<char>((i * 7 + 3) & 0xff);
    Require(Derive(longPassword.data(), 200, Bytes("NaCl"), 4, 2, EVP_sha512_nid_postfix(), 130) == "e24d18d3c5b15543f3a497c5953fe31ab29269be44dcb4beae98bd03fb4371e5773864c1b22ac9170110615e8ab58a0f08ed6e9d8dfc4655ed3b620d899642faaedfc7c2137a965f96c833c5f2176bf367cb37d531c3a67d87db41fefd14c279865ff7afc4adb05f3e1f8cbe801ee971f43c8947a8ad5c2e8fbd8b755df4aadb5b86");
    Require(Derive("password", 8, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), 20) == "120fb6cffcf8b32c43e7225256c4f837a86548c9");
}

void TestLengthsAndCounts() {
    const std::string passwdSalt = Derive("passwd", 6, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), 64);
    Require(Derive("passwd", -1, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), 64) == passwdSalt);
    Require(Derive("passwd", 6, Bytes("salt"), 4, 0, EVP_sha256_nid_postfix(), 64) == passwdSalt);
    Require(Derive("passwd", 6, Bytes("salt"), 4, -5, EVP_sha256_nid_postfix(), 64) == passwdSalt);
    Require(Derive("pass\0word", -1, Bytes("salt"), 4, 3, EVP_sha256_nid_postfix(), 32) == Derive("pass", 4, Bytes("salt"), 4, 3, EVP_sha256_nid_postfix(), 32));
    Require(Derive("", 0, Bytes("salt"), 4, 2, EVP_sha256_nid_postfix(), 32) == "62384466264daadc4144018c6bd864648272b34da8980d31521ffcce92ae003b");
    Require(Derive("", -1, Bytes("salt"), 4, 2, EVP_sha256_nid_postfix(), 32) == "62384466264daadc4144018c6bd864648272b34da8980d31521ffcce92ae003b");
    Require(Derive("password", 8, nullptr, 0, 2, EVP_sha256_nid_postfix(), 32) == "8bc2f9167a81cdcfad1235cd9047f1136271c1f978fcfcb35e22dbeafa4634f6");
    Require(Derive("password", 8, Bytes("ignored"), 0, 2, EVP_sha256_nid_postfix(), 32) == "8bc2f9167a81cdcfad1235cd9047f1136271c1f978fcfcb35e22dbeafa4634f6");
    std::vector<unsigned char> output(Guard, Fill);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix("passwd", 6, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), 0, output.data()) == 1);
    Require(Untouched(output, 0));
    Require(PKCS5_PBKDF2_HMAC_nid_postfix("passwd", 6, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), 0, nullptr) == 1);
    Require(ERR_get_error_nid_postfix() == 0);
}

void TestFailures() {
    std::vector<unsigned char> output(32 + Guard, Fill);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(nullptr, 6, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), 32, output.data()) == 0);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix(nullptr, -1, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), -1, nullptr) == 0);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix("passwd", -2, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), 32, output.data()) == 0);
    Require(Untouched(output, 0));
    Require(ERR_get_error_nid_postfix() == 0);
    Require(PKCS5_PBKDF2_HMAC_nid_postfix("passwd", 6, Bytes("salt"), 4, 1, nullptr, 32, output.data()) == 0);
    Require(Untouched(output, 0));
    Require(ERR_get_error_nid_postfix() == NullDigestError);
    Require(ERR_get_error_nid_postfix() == 0);
    RequireInvalidArgument([&] { PKCS5_PBKDF2_HMAC_nid_postfix("passwd", 6, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), -1, output.data()); });
    RequireInvalidArgument([&] { PKCS5_PBKDF2_HMAC_nid_postfix("passwd", 6, Bytes("salt"), 4, 1, EVP_sha256_nid_postfix(), 32, nullptr); });
    RequireInvalidArgument([&] { PKCS5_PBKDF2_HMAC_nid_postfix("passwd", 6, Bytes("salt"), -1, 1, EVP_sha256_nid_postfix(), 32, output.data()); });
    RequireInvalidArgument([&] { PKCS5_PBKDF2_HMAC_nid_postfix("passwd", 6, nullptr, 4, 1, EVP_sha256_nid_postfix(), 32, output.data()); });
    Require(Untouched(output, 0));
    Require(ERR_get_error_nid_postfix() == 0);
    Require(liveBlocks == 0);
}

void TestAllocationFailures() {
    const std::string expected = Derive("passwd", 6, Bytes("salt"), 4, 3, EVP_sha256_nid_postfix(), 64);
    bool failed = false;
    for (int allocation = 0; allocation < 16; ++allocation) {
        std::vector<unsigned char> output(64 + Guard, Fill);
        allocationsBeforeFailure = allocation;
        const int result = PKCS5_PBKDF2_HMAC_nid_postfix("passwd", 6, Bytes("salt"), 4, 3, EVP_sha256_nid_postfix(), 64, output.data());
        const bool reached = allocationsBeforeFailure < 0;
        allocationsBeforeFailure = -1;
        ERR_clear_error_nid_postfix();
        Require(liveBlocks == 0);
        if (reached) {
            Require(result == 0 && Untouched(output, 0));
            failed = true;
        } else {
            Require(result == 1 && Hex(output.data(), 64) == expected && Untouched(output, 64));
            break;
        }
    }
    Require(failed);
}

}

int main() {
    InitializeHeap();
    ERR_clear_error_nid_postfix();
    TestKnownAnswers();
    TestLengthsAndCounts();
    TestFailures();
    TestAllocationFailures();
    std::puts("libSceLibreSsl3 PBKDF2 tests passed");
}
