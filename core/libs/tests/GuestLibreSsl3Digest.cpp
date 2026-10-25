#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 digest check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static std::string Hex(const unsigned char* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < size; ++i) {
        text += digits[data[i] >> 4];
        text += digits[data[i] & 15];
    }
    return text;
}

int main() {
    const auto* abc = reinterpret_cast<const unsigned char*>("abc");
    const std::string million(1000000, 'a');
    const auto* many = reinterpret_cast<const unsigned char*>(million.data());
    unsigned char digest[24];
    std::memset(digest, 0xee, sizeof(digest));
    Require(MD5_nid_postfix(abc, 3, digest) == digest && Hex(digest, 16) == "900150983cd24fb0d6963f7d28e17f72" && digest[16] == 0xee);
    Require(MD5_nid_postfix(nullptr, 0, digest) == digest && Hex(digest, 16) == "d41d8cd98f00b204e9800998ecf8427e");
    Require(MD5_nid_postfix(many, million.size(), digest) == digest && Hex(digest, 16) == "7707d6ae4e027c70eea2a935c2296f21");
    std::memset(digest, 0xee, sizeof(digest));
    Require(SHA1_nid_postfix(abc, 3, digest) == digest && Hex(digest, 20) == "a9993e364706816aba3e25717850c26c9cd0d89d" && digest[20] == 0xee);
    Require(SHA1_nid_postfix(nullptr, 0, digest) == digest && Hex(digest, 20) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    Require(SHA1_nid_postfix(many, million.size(), digest) == digest && Hex(digest, 20) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    RequireThrows([&] { MD5_nid_postfix(abc, 3, nullptr); });
    RequireThrows([&] { SHA1_nid_postfix(abc, 3, nullptr); });
    RequireThrows([&] { SHA1_nid_postfix(nullptr, 1, digest); });

    std::puts("libSceLibreSsl3 digest tests passed");
}
