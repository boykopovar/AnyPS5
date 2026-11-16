#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"
#include "prx/libSceNKWebKit/src/ParsedContentRangeAbi.hpp"
#include "prx/libSceNKWebKit/src/CacheControlAbi.hpp"

#include <cstddef>
#include <limits>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct StringSlot {
    WtfAbi::StringImpl* impl{nullptr};
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

struct CertificateInfo {
    int verificationError{0};
    WtfAbi::Vector<WtfAbi::Vector<unsigned char>> certificateChain;
    WtfAbi::Vector<unsigned char> privateKey;
    WtfAbi::String privateKeyPassword;
};

struct OptionalCertificateInfo {
    CertificateInfo value;
    bool engaged{false};
};

struct ResourceResponseBase {
    WtfAbi::URL url{};
    StringSlot mimeType{};
    long long expectedContentLength{0};
    StringSlot textEncodingName{};
    StringSlot httpStatusText{};
    StringSlot httpVersion{};
    ResponseHeaderStorage httpHeaderFields{};
    void* networkLoadMetrics{nullptr};
    OptionalCertificateInfo certificateInfo{};
    short httpStatusCode{0};
    unsigned char stateBits{1};
    double age{std::numeric_limits<double>::quiet_NaN()};
    double date{std::numeric_limits<double>::quiet_NaN()};
    double expires{std::numeric_limits<double>::quiet_NaN()};
    double lastModified{std::numeric_limits<double>::quiet_NaN()};
    ParsedContentRange contentRange{0, 0, ParsedContentRange::invalidLength};
    HttpAbi::CacheControlDirectives cacheControlDirectives{};
    unsigned char parsedBits{0};
    unsigned char taintBits{0};
    unsigned char typeBits{2};
};

struct ResourceResponse : ResourceResponseBase {};

static_assert(sizeof(StringSlot) == 8);
static_assert(sizeof(ResponseHeaderStorage) == 32);
static_assert(sizeof(ResponseOptionalTime) == 16);
static_assert(offsetof(ResponseOptionalTime, hasValue) == 0);
static_assert(offsetof(ResponseOptionalTime, seconds) == 8);
static_assert(!std::is_trivially_copy_constructible_v<ResponseOptionalTime>);
static_assert(!std::is_trivially_move_constructible_v<ResponseOptionalTime>);
static_assert(std::is_trivially_destructible_v<ResponseOptionalTime>);
static_assert(sizeof(CertificateInfo) == 48);
static_assert(alignof(CertificateInfo) == 8);
static_assert(offsetof(CertificateInfo, verificationError) == 0);
static_assert(offsetof(CertificateInfo, certificateChain) == 8);
static_assert(offsetof(CertificateInfo, privateKey) == 24);
static_assert(offsetof(CertificateInfo, privateKeyPassword) == 40);
static_assert(sizeof(OptionalCertificateInfo) == 56);
static_assert(offsetof(OptionalCertificateInfo, value) == 0);
static_assert(offsetof(OptionalCertificateInfo, engaged) == 48);
static_assert(offsetof(ResourceResponseBase, url) == 0);
static_assert(offsetof(ResourceResponseBase, mimeType) == 40);
static_assert(offsetof(ResourceResponseBase, expectedContentLength) == 48);
static_assert(offsetof(ResourceResponseBase, textEncodingName) == 56);
static_assert(offsetof(ResourceResponseBase, httpStatusText) == 64);
static_assert(offsetof(ResourceResponseBase, httpVersion) == 72);
static_assert(offsetof(ResourceResponseBase, httpHeaderFields) == 80);
static_assert(offsetof(ResourceResponseBase, networkLoadMetrics) == 112);
static_assert(offsetof(ResourceResponseBase, certificateInfo) == 120);
static_assert(offsetof(ResourceResponseBase, httpStatusCode) == 176);
static_assert(offsetof(ResourceResponseBase, stateBits) == 178);
static_assert(offsetof(ResourceResponseBase, age) == 184);
static_assert(offsetof(ResourceResponseBase, date) == 192);
static_assert(offsetof(ResourceResponseBase, expires) == 200);
static_assert(offsetof(ResourceResponseBase, lastModified) == 208);
static_assert(offsetof(ResourceResponseBase, contentRange) == 216);
static_assert(offsetof(ResourceResponseBase, cacheControlDirectives) == 240);
static_assert(offsetof(ResourceResponseBase, parsedBits) == 272);
static_assert(offsetof(ResourceResponseBase, taintBits) == 273);
static_assert(offsetof(ResourceResponseBase, typeBits) == 274);
static_assert(sizeof(ResourceResponseBase) == 280);
static_assert(alignof(ResourceResponseBase) == 8);
static_assert(sizeof(ResourceResponse) == 280);
static_assert(std::is_standard_layout_v<ResourceResponseBase>);

}
