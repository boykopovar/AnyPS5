#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_sha512_256 check failed at line %u\n", location.line());
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

constexpr unsigned char ExpectedAbc[32] {
    0x53, 0x04, 0x8e, 0x26, 0x81, 0x94, 0x1e, 0xf9, 0x9b, 0x2e, 0x29, 0xb7, 0x6b, 0x4c, 0x7d, 0xab,
    0xe4, 0xc2, 0xd0, 0xc6, 0x34, 0xfc, 0x6d, 0x46, 0xe0, 0xe2, 0xf1, 0x31, 0x07, 0xe7, 0xaf, 0x23,
};

constexpr unsigned char ExpectedEmpty[32] {
    0xc6, 0x72, 0xb8, 0xd1, 0xef, 0x56, 0xed, 0x28, 0xab, 0x87, 0xc3, 0x62, 0x2c, 0x51, 0x14, 0x06,
    0x9b, 0xdd, 0x3a, 0xd7, 0xb8, 0xf9, 0x73, 0x74, 0x98, 0xd0, 0xc0, 0x1e, 0xce, 0xf0, 0x96, 0x7a,
};

constexpr unsigned char ExpectedFox[32] {
    0xdd, 0x9d, 0x67, 0xb3, 0x71, 0x51, 0x9c, 0x33, 0x9e, 0xd8, 0xdb, 0xd2, 0x5a, 0xf9, 0x0e, 0x97,
    0x6a, 0x1e, 0xee, 0xfd, 0x4a, 0xd3, 0xd8, 0x89, 0x00, 0x5e, 0x53, 0x2f, 0xc5, 0xbe, 0xf0, 0x4d,
};

void CheckDescriptor() {
    const LibreSsl3::DigestMethod* digest = EVP_sha512_256_nid_postfix();
    Require(digest != nullptr);
    Require(digest->type == 1030);
    Require(digest->keyType == 1026);
    Require(digest->digestSize == 32);
    Require(digest->flags == 8);
    Require(digest->init != nullptr);
    Require(digest->update != nullptr);
    Require(digest->finalize != nullptr);
    Require(digest->blockSize == 128);
    Require(digest->contextSize == 8 + static_cast<std::int32_t>(sizeof(Sha512Context)));
    Require(EVP_sha512_256_nid_postfix() == digest);
    for (const char* name : {"sha512-256", "SHA512-256", "sha512-256WithRSAEncryption", "RSA-SHA512/256"})
        Require(EVP_get_digestbyname_nid_postfix(name) == digest);
}

void CheckStreamed(const char* data, std::size_t length, const unsigned char* expected, std::size_t chunk) {
    const LibreSsl3::DigestMethod* digest = EVP_sha512_256_nid_postfix();
    LibreSsl3::DigestContext* context = EVP_MD_CTX_new_nid_postfix();
    Require(context != nullptr);
    Require(EVP_DigestInit_nid_postfix(context, digest) == 1);
    for (std::size_t offset = 0; offset < length; offset += chunk) {
        const std::size_t piece = offset + chunk > length ? length - offset : chunk;
        Require(context->update(context, data + offset, piece) == 1);
    }
    unsigned char output[32];
    Require(context->digest->finalize(context, output) == 1);
    Require(std::memcmp(output, expected, sizeof(output)) == 0);
    EVP_MD_CTX_free_nid_postfix(context);
}

void CheckOneShot() {
    const unsigned char data[] = "The quick brown fox jumps over the lazy dog";
    unsigned char output[64];
    std::uint32_t length = 0;
    Require(EVP_Digest_nid_postfix(data, sizeof(data) - 1, output, &length, reinterpret_cast<const EvpMd*>(EVP_sha512_256_nid_postfix()), nullptr) == 1);
    Require(length == 32);
    Require(std::memcmp(output, ExpectedFox, sizeof(ExpectedFox)) == 0);
}

}

int main() {
    void* api[10] {reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
                   reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
                   reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    CheckDescriptor();
    CheckStreamed("abc", 3, ExpectedAbc, 3);
    CheckStreamed("abc", 3, ExpectedAbc, 1);
    CheckStreamed("", 0, ExpectedEmpty, 1);
    CheckOneShot();
    std::puts("LibreSSL3 EVP_sha512_256 tests passed");
    return 0;
}
