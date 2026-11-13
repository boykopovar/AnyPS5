#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "X509_digest check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (...) {
        threw = true;
    }
    Require(threw);
}

struct Encoding {
    unsigned char* data;
    std::int64_t length;
    std::int32_t modified;
    std::int32_t padding;
};

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    static_assert(sizeof(Encoding) == 24);
    LibreSsl3X509CertInfo info{};
    auto* encoding = reinterpret_cast<Encoding*>(reinterpret_cast<unsigned char*>(&info) + 80);
    unsigned char der[] {'a', 'b', 'c'};
    encoding->data = der;
    encoding->length = 3;
    LibreSsl3X509 certificate{};
    certificate.certInfo = &info;

    const std::array<std::uint8_t, 32> expected {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
    };
    std::array<unsigned char, 32> output{};
    std::uint32_t length = 0;
    Require(X509_digest_nid_postfix(&certificate, EVP_sha256_nid_postfix(), output.data(), &length) == 1);
    Require(length == output.size() && output == expected);

    std::array<unsigned char, 32> withoutLength{};
    Require(X509_digest_nid_postfix(&certificate, EVP_sha256_nid_postfix(), withoutLength.data(), nullptr) == 1);
    Require(withoutLength == expected);
    encoding->modified = 1;
    RequireThrows([&] { X509_digest_nid_postfix(&certificate, EVP_sha256_nid_postfix(), output.data(), &length); });
    encoding->modified = 0;

    RequireThrows([&] { X509_digest_nid_postfix(nullptr, EVP_sha256_nid_postfix(), output.data(), &length); });
    RequireThrows([&] { X509_digest_nid_postfix(&certificate, nullptr, output.data(), &length); });
    RequireThrows([&] { X509_digest_nid_postfix(&certificate, EVP_sha256_nid_postfix(), nullptr, &length); });
    encoding->data = nullptr;
    RequireThrows([&] { X509_digest_nid_postfix(&certificate, EVP_sha256_nid_postfix(), output.data(), &length); });

    std::puts("X509_digest tests passed");
}
