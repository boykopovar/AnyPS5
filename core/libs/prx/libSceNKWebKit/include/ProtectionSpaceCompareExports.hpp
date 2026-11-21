#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct ByteVector {
    std::uint8_t* buffer;
    unsigned capacity;
    unsigned size;
};

struct CertificateChain {
    ByteVector* buffer;
    unsigned capacity;
    unsigned size;
};

struct CertificateInfo {
    int verificationError;
    CertificateChain certificateChain;
    ByteVector privateKey;
    NKWebKit::WtfAbi::String privateKeyPassword;
};

struct ProtectionSpace {
    NKWebKit::WtfAbi::String host;
    NKWebKit::WtfAbi::String realm;
    int port;
    std::uint8_t serverType;
    std::uint8_t authenticationScheme;
    bool isHashTableDeletedValue;
    CertificateInfo certificateInfo;
};

static_assert(sizeof(ByteVector) == 16);
static_assert(sizeof(CertificateChain) == 16);
static_assert(sizeof(CertificateInfo) == 48);
static_assert(offsetof(CertificateInfo, certificateChain) == 8);
static_assert(offsetof(CertificateInfo, privateKey) == 24);
static_assert(offsetof(CertificateInfo, privateKeyPassword) == 40);
static_assert(sizeof(ProtectionSpace) == 72);
static_assert(offsetof(ProtectionSpace, realm) == 8);
static_assert(offsetof(ProtectionSpace, port) == 16);
static_assert(offsetof(ProtectionSpace, serverType) == 20);
static_assert(offsetof(ProtectionSpace, authenticationScheme) == 21);
static_assert(offsetof(ProtectionSpace, isHashTableDeletedValue) == 22);
static_assert(offsetof(ProtectionSpace, certificateInfo) == 24);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

bool APS5_VABI _ZN7WebCore19ProtectionSpaceBase7compareERKNS_15ProtectionSpaceES3_(
    const NKWebKit::WebCoreAbi::ProtectionSpace* a, const NKWebKit::WebCoreAbi::ProtectionSpace* b);

}
