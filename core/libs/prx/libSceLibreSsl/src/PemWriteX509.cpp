#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int LineLength = 64;
constexpr char Base64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Encode(const unsigned char* data, std::size_t length) {
    std::string text;
    text.reserve((length + 2) / 3 * 4);
    for (std::size_t offset = 0; offset < length; offset += 3) {
        const std::size_t remaining = length - offset;
        const std::uint32_t first = data[offset];
        const std::uint32_t second = remaining > 1 ? data[offset + 1] : 0;
        const std::uint32_t third = remaining > 2 ? data[offset + 2] : 0;
        const std::uint32_t value = (first << 16) | (second << 8) | third;
        text.push_back(Base64Alphabet[(value >> 18) & 0x3f]);
        text.push_back(Base64Alphabet[(value >> 12) & 0x3f]);
        text.push_back(remaining > 1 ? Base64Alphabet[(value >> 6) & 0x3f] : '=');
        text.push_back(remaining > 2 ? Base64Alphabet[value & 0x3f] : '=');
    }
    return text;
}

std::string Wrap(const std::string& text) {
    std::string wrapped;
    for (std::size_t offset = 0; offset < text.size(); offset += LineLength) {
        const std::size_t count = text.size() - offset < static_cast<std::size_t>(LineLength) ? text.size() - offset : static_cast<std::size_t>(LineLength);
        wrapped.append(text, offset, count);
        wrapped.push_back('\n');
    }

    return wrapped;
}

}

extern "C" {

int APS5_VABI PEM_write_bio_X509_nid_postfix(LibreSsl::Bio* bio, LibreSslX509* certificate) {
    if (!bio) throw std::invalid_argument("PEM_write_bio_X509: null BIO");
    if (!certificate) throw std::invalid_argument("PEM_write_bio_X509: null certificate");
    const auto* info = static_cast<const LibreSslX509CertInfo*>(certificate->certificateInfo);
    if (!info || !info->enc.enc || info->enc.len <= 0 || info->enc.modified)
        NotImplemented_nid_no_patch("PEM_write_bio_X509: a certificate without a cached encoding");
    std::string text("-----BEGIN CERTIFICATE-----\n");
    text += Wrap(Encode(info->enc.enc, static_cast<std::size_t>(info->enc.len)));
    text += "-----END CERTIFICATE-----\n";
    auto* buffer = static_cast<char*>(malloc_nid_postfix(text.size()));
    if (!buffer) return 0;
    std::memcpy(buffer, text.data(), text.size());
    const int result = LibreSslBioWrite_nid_no_patch(bio, buffer, static_cast<int>(text.size()));
    free_nid_postfix(buffer);
    return result == static_cast<int>(text.size()) ? 1 : 0;
}

}
