#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct StringSlot {
    WtfAbi::StringImpl* impl;
};

using ResponseHeaderStorage = HttpAbi::HTTPHeaderMap;

struct ResponseOptionalTime {
    bool hasValue{};
    double seconds{};

    ResponseOptionalTime() = default;
    ResponseOptionalTime(bool engaged, double value) : hasValue(engaged), seconds(value) {}
    ResponseOptionalTime(const ResponseOptionalTime& other) : hasValue(other.hasValue), seconds(other.seconds) {}
    ResponseOptionalTime(ResponseOptionalTime&& other) noexcept : ResponseOptionalTime(other) {}
};

struct ByteVector {
    std::uint8_t* buffer;
    std::uint32_t capacity;
    std::uint32_t size;
};

struct CertificateChain {
    ByteVector* buffer;
    std::uint32_t capacity;
    std::uint32_t size;
};

struct CertificateInfo {
    int verificationError;
    CertificateChain certificateChain;
    ByteVector privateKey;
    StringSlot privateKeyPassword;
};

struct OptionalCertificateInfo {
    CertificateInfo value;
    bool hasValue;
};

struct ResourceResponseBase {
    WtfAbi::URL url;
    StringSlot mimeType;
    long long expectedContentLength;
    StringSlot textEncodingName;
    StringSlot httpStatusText;
    StringSlot httpVersion;
    ResponseHeaderStorage httpHeaderFields;
    void* networkLoadMetrics;
    OptionalCertificateInfo certificateInfo;
    short httpStatusCode;
};

static_assert(sizeof(StringSlot) == 8);
static_assert(sizeof(ResponseHeaderStorage) == 32);
static_assert(sizeof(ResponseOptionalTime) == 16);
static_assert(offsetof(ResponseOptionalTime, hasValue) == 0);
static_assert(offsetof(ResponseOptionalTime, seconds) == 8);
static_assert(!std::is_trivially_copy_constructible_v<ResponseOptionalTime>);
static_assert(!std::is_trivially_move_constructible_v<ResponseOptionalTime>);
static_assert(std::is_trivially_destructible_v<ResponseOptionalTime>);
static_assert(offsetof(ResourceResponseBase, url) == 0);
static_assert(offsetof(ResourceResponseBase, mimeType) == 40);
static_assert(offsetof(ResourceResponseBase, expectedContentLength) == 48);
static_assert(offsetof(ResourceResponseBase, textEncodingName) == 56);
static_assert(offsetof(ResourceResponseBase, httpStatusText) == 64);
static_assert(offsetof(ResourceResponseBase, httpVersion) == 72);
static_assert(offsetof(ResourceResponseBase, httpHeaderFields) == 80);
static_assert(sizeof(ByteVector) == 16);
static_assert(sizeof(CertificateChain) == 16);
static_assert(sizeof(CertificateInfo) == 48);
static_assert(offsetof(CertificateInfo, verificationError) == 0);
static_assert(offsetof(CertificateInfo, certificateChain) == 8);
static_assert(offsetof(CertificateInfo, privateKey) == 24);
static_assert(offsetof(CertificateInfo, privateKeyPassword) == 40);
static_assert(sizeof(OptionalCertificateInfo) == 56);
static_assert(offsetof(OptionalCertificateInfo, hasValue) == 48);
static_assert(offsetof(ResourceResponseBase, networkLoadMetrics) == 112);
static_assert(offsetof(ResourceResponseBase, certificateInfo) == 120);
static_assert(offsetof(ResourceResponseBase, httpStatusCode) == 176);

}
