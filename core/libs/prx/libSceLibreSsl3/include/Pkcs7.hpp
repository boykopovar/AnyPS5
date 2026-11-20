#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "prx/libSceLibreSsl3/include/Api.hpp"

struct LibreSsl3Pkcs7 {
    unsigned char* asn1;
    std::int64_t length;
    std::int32_t state;
    std::int32_t detached;
    Asn1Object_nid_no_patch* type;
    void* d;
};

struct LibreSsl3Pkcs7Signed {
    Asn1String* version;
    LibreSsl3Stack* mdAlgs;
    LibreSsl3Stack* cert;
    LibreSsl3Stack* crl;
    LibreSsl3Stack* signerInfo;
    LibreSsl3Pkcs7* contents;
};

struct LibreSsl3Pkcs7IssuerAndSerial {
    LibreSslX509Name* issuer;
    Asn1String* serial;
};

struct LibreSsl3Pkcs7SignerInfo {
    Asn1String* version;
    LibreSsl3Pkcs7IssuerAndSerial* issuerAndSerial;
    LibreSsl3X509Algor* digestAlg;
    LibreSsl3Stack* authAttr;
    LibreSsl3X509Algor* digestEncAlg;
    Asn1String* encDigest;
    LibreSsl3Stack* unauthAttr;
    void* pkey;
};

struct LibreSsl3X509Attribute {
    Asn1Object_nid_no_patch* object;
    LibreSsl3Stack* set;
};

static_assert(sizeof(LibreSsl3Pkcs7) == 40);
static_assert(offsetof(LibreSsl3Pkcs7, type) == 24);
static_assert(sizeof(LibreSsl3Pkcs7Signed) == 48);
static_assert(offsetof(LibreSsl3Pkcs7Signed, contents) == 40);
static_assert(sizeof(LibreSsl3Pkcs7IssuerAndSerial) == 16);
static_assert(sizeof(LibreSsl3Pkcs7SignerInfo) == 64);
static_assert(offsetof(LibreSsl3Pkcs7SignerInfo, unauthAttr) == 48);
static_assert(sizeof(LibreSsl3X509Attribute) == 16);

LibreSsl3Certificate* DecodeCertificate_nid_no_patch(std::span<const unsigned char>& input);
void FreeCertificate_nid_no_patch(LibreSsl3Certificate* certificate);
LibreSsl3X509Algor* DecodeAlgor_nid_no_patch(std::span<const unsigned char>& input);
void FreeAlgor_nid_no_patch(LibreSsl3X509Algor* algor);
LibreSslX509Name* DecodeName_nid_no_patch(std::span<const unsigned char> content);
void FreeName_nid_no_patch(LibreSslX509Name* name);
