#pragma once

#include "prx/libSceNKWebKit/src/ResourceResponseAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct ResourceResponseStatusLayout {
    struct Vector {
        void* buffer;
        std::uint32_t capacity;
        std::uint32_t size;
    };

    struct CertificateInfo {
        int verificationError;
        Vector certificateChain;
        Vector privateKey;
        WtfAbi::StringImpl* privateKeyPassword;
    };

    struct OptionalCertificateInfo {
        CertificateInfo value;
        bool hasValue;
    };

    ResourceResponseBase head;
    void* networkLoadMetrics;
    OptionalCertificateInfo certificateInfo;
    short httpStatusCode;
};

static_assert(sizeof(ResourceResponseBase) == 112);
static_assert(sizeof(ResourceResponseStatusLayout::Vector) == 16);
static_assert(sizeof(ResourceResponseStatusLayout::CertificateInfo) == 48);
static_assert(offsetof(ResourceResponseStatusLayout::CertificateInfo, verificationError) == 0);
static_assert(offsetof(ResourceResponseStatusLayout::CertificateInfo, certificateChain) == 8);
static_assert(offsetof(ResourceResponseStatusLayout::CertificateInfo, privateKey) == 24);
static_assert(offsetof(ResourceResponseStatusLayout::CertificateInfo, privateKeyPassword) == 40);
static_assert(sizeof(ResourceResponseStatusLayout::OptionalCertificateInfo) == 56);
static_assert(offsetof(ResourceResponseStatusLayout::OptionalCertificateInfo, hasValue) == 48);
static_assert(offsetof(ResourceResponseStatusLayout, head) == 0);
static_assert(offsetof(ResourceResponseStatusLayout, networkLoadMetrics) == 112);
static_assert(offsetof(ResourceResponseStatusLayout, certificateInfo) == 120);
static_assert(offsetof(ResourceResponseStatusLayout, httpStatusCode) == 176);
static_assert(std::is_standard_layout_v<ResourceResponseStatusLayout>);
static_assert(sizeof(ResourceResponseStatusLayout) == 184);

}  // namespace NKWebKit::WebCoreAbi
