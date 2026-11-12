#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>
#include <string_view>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_sha512_256 check failed at line %u\n", location.line());
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

std::string Hex(const unsigned char* bytes, std::size_t size) {
    static constexpr char Digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < size; ++i) {
        text += Digits[bytes[i] >> 4];
        text += Digits[bytes[i] & 15];
    }
    return text;
}

std::string Digest(std::string_view message) {
    unsigned char digest[64];
    for (auto& byte : digest) byte = 0xA5;
    std::uint32_t size = 0;
    Require(EVP_Digest_nid_postfix(message.data(), message.size(), digest, &size, EVP_sha512_256_nid_postfix(), nullptr) == 1);
    Require(size == 32);
    for (std::size_t i = 32; i < sizeof(digest); ++i) Require(digest[i] == 0xA5);
    return Hex(digest, size);
}

std::string Streamed(std::string_view message, std::size_t chunk) {
    EvpMdContext* context = EVP_MD_CTX_create_nid_postfix();
    Require(context != nullptr);
    Require(EVP_DigestInit_ex_nid_postfix(context, EVP_sha512_256_nid_postfix(), nullptr) == 1);
    for (std::size_t offset = 0; offset < message.size(); offset += chunk)
        Require(EVP_DigestUpdate_nid_postfix(context, message.data() + offset, std::min(chunk, message.size() - offset)) == 1);
    unsigned char digest[32];
    std::uint32_t size = 0;
    Require(EVP_DigestFinal_ex_nid_postfix(context, digest, &size) == 1);
    Require(size == 32);
    EVP_MD_CTX_destroy_nid_postfix(context);
    return Hex(digest, size);
}

}

int main() {
    InitializeHeap();
    const auto* md = EVP_sha512_256_nid_postfix();
    Require(md == EVP_sha512_256_nid_postfix());
    Require(md->type == 1030 && md->pkeyType == 1026 && md->mdSize == 32 && md->flags == 0x8);
    Require(md->blockSize == 128 && md->contextSize == 224);
    Require(EVP_MD_size_nid_postfix(md) == 32);
    Require(Digest("") == "c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a");
    Require(Digest("abc") == "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23");
    const std::string twoBlocks = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    Require(Digest(twoBlocks) == "3928e184fb8690f840da3988121d31be65cb9d3ef83ee6146feac861e19b563a");
    const std::string thousand(1000, 'a');
    Require(Digest(thousand) == "40eb4a70d4d69815407a9e272f0101cd67e3d11262a4a0bfc087712749c7fb53");
    Require(Streamed(thousand, 7) == "40eb4a70d4d69815407a9e272f0101cd67e3d11262a4a0bfc087712749c7fb53");
}
