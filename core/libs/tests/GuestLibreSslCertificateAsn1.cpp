#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>

namespace {

constexpr std::uint64_t Error(int library, int function, int reason) {
    return (std::uint64_t{static_cast<unsigned>(library)} << 24)
        | (std::uint64_t{static_cast<unsigned>(function)} << 12)
        | static_cast<unsigned>(reason);
}

constexpr auto PreviousError = Error(1, 2, 3);
constexpr auto HeaderTooLong = Error(13, 0xfff, 123);
constexpr auto BadObjectHeader = Error(13, 0xfff, 102);
constexpr auto NestedAsn1Error = Error(13, 0xfff, 58);
constexpr auto SequenceNotConstructed = Error(13, 0xfff, 149);
constexpr auto Asn1TooLong = Error(13, 0xfff, 155);
constexpr auto WrongTag = Error(13, 0xfff, 168);
constexpr auto SslAsn1Error = Error(20, 0xfff, 13);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

void CheckFailure(const unsigned char* data, int length, std::initializer_list<std::uint64_t> expected) {
    LibreSslCtx context;
    std::memset(&context, 0xa5, sizeof(context));
    std::array<unsigned char, sizeof(context)> contextSnapshot{};
    std::memcpy(contextSnapshot.data(), &context, sizeof(context));
    std::array<unsigned char, 4> inputSnapshot{};
    if (data && length > 0) std::memcpy(inputSnapshot.data(), data, static_cast<std::size_t>(length));
    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(1, 2, 3);
    Require(SSL_CTX_use_certificate_ASN1_nid_postfix(&context, length, data) == 0,
        "an invalid outer certificate header returns zero");
    Require(std::memcmp(contextSnapshot.data(), &context, sizeof(context)) == 0,
        "header failures do not access or mutate the SSL context");
    if (data && length > 0)
        Require(std::memcmp(inputSnapshot.data(), data, static_cast<std::size_t>(length)) == 0,
            "header failures do not modify the DER buffer");
    Require(ERR_get_error_nid_postfix() == PreviousError, "header failure preserves existing errors first");
    for (const auto error : expected)
        Require(ERR_get_error_nid_postfix() == error, "header failure queues source ASN.1 and SSL errors in order");
    Require(ERR_get_error_nid_postfix() == 0, "header failure queues no extra numeric errors");
}

}

int main() {
    CheckFailure(nullptr, 0, {HeaderTooLong, BadObjectHeader, NestedAsn1Error, SslAsn1Error});
    constexpr unsigned char wrongTag[]{0x05, 0x00};
    CheckFailure(wrongTag, sizeof(wrongTag), {WrongTag, NestedAsn1Error, SslAsn1Error});
    CheckFailure(wrongTag, -1, {HeaderTooLong, BadObjectHeader, NestedAsn1Error, SslAsn1Error});
    constexpr unsigned char truncatedHeader[]{0x30};
    CheckFailure(truncatedHeader, sizeof(truncatedHeader), {HeaderTooLong, BadObjectHeader, NestedAsn1Error, SslAsn1Error});
    constexpr unsigned char truncatedContent[]{0x30, 0x02, 0x00};
    CheckFailure(truncatedContent, sizeof(truncatedContent), {Asn1TooLong, BadObjectHeader, NestedAsn1Error, SslAsn1Error});
    constexpr unsigned char primitiveSequence[]{0x10, 0x00};
    CheckFailure(primitiveSequence, sizeof(primitiveSequence), {SequenceNotConstructed, SslAsn1Error});
    constexpr unsigned char highTagWrong[]{0x3f, 0x01, 0x00};
    CheckFailure(highTagWrong, sizeof(highTagWrong), {WrongTag, NestedAsn1Error, SslAsn1Error});
    bool minimumLengthThrew = false;
    ERR_clear_error_nid_postfix();
    try {
        SSL_CTX_use_certificate_ASN1_nid_postfix(nullptr, INT_MIN, wrongTag);
    } catch (const std::exception&) {
        minimumLengthThrew = true;
    }
    Require(minimumLengthThrew && ERR_get_error_nid_postfix() == 0,
        "INT_MIN length stays unsupported where the source narrows its decremented bound");
    return 0;
}
