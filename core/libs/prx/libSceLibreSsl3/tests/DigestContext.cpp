#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "Testing.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr std::size_t ContextSize = 48;

const void* Digest(const void* digest, unsigned char fill) {
    alignas(8) std::array<unsigned char, ContextSize> context;
    std::memset(context.data(), fill, context.size());
    std::memcpy(context.data(), &digest, sizeof(digest));
    std::array<unsigned char, ContextSize> before = context;
    const void* result = EVP_MD_CTX_md_nid_postfix(context.data());
    Require(context == before);
    return result;
}

void TestDigest() {
    const std::array<std::int32_t, 4> sha256 {672, 668, 32, 0};
    const std::array<std::int32_t, 4> sha1 {64, 65, 20, 0};
    Require(Digest(sha256.data(), 0xA5) == sha256.data());
    Require(Digest(sha1.data(), 0x00) == sha1.data());
    Require(Digest(nullptr, 0xA5) == nullptr);
    Require(EVP_MD_CTX_md_nid_postfix(nullptr) == nullptr);
}

}

int main() {
    TestDigest();
    std::puts("LibreSSL3 EVP_MD_CTX tests passed");
}
