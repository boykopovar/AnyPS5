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

struct LibreSsl3Pkcs7Digest {
    Asn1String* version;
    LibreSsl3X509Algor* md;
    LibreSsl3Pkcs7* contents;
    Asn1String* digest;
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

struct LibreSsl3Pkcs7EncContent {
    Asn1Object_nid_no_patch* contentType;
    LibreSsl3X509Algor* algorithm;
    Asn1String* encData;
    const void* cipher;
};

struct LibreSsl3Pkcs7RecipInfo {
    Asn1String* version;
    LibreSsl3Pkcs7IssuerAndSerial* issuerAndSerial;
    LibreSsl3X509Algor* keyEncAlgor;
    Asn1String* encKey;
    void* cert;
};

struct LibreSsl3Pkcs7Envelope {
    Asn1String* version;
    LibreSsl3Stack* recipientInfo;
    LibreSsl3Pkcs7EncContent* encData;
};

struct LibreSsl3Pkcs7SignEnvelope {
    Asn1String* version;
    LibreSsl3Stack* mdAlgs;
    LibreSsl3Stack* cert;
    LibreSsl3Stack* crl;
    LibreSsl3Stack* signerInfo;
    LibreSsl3Pkcs7EncContent* encData;
    LibreSsl3Stack* recipientInfo;
};

struct LibreSsl3Pkcs7Encrypt {
    Asn1String* version;
    LibreSsl3Pkcs7EncContent* encData;
};

struct LibreSsl3X509Attribute {
    Asn1Object_nid_no_patch* object;
    LibreSsl3Stack* set;
};

static_assert(sizeof(LibreSsl3Pkcs7) == 40);
static_assert(offsetof(LibreSsl3Pkcs7, type) == 24);
static_assert(sizeof(LibreSsl3Pkcs7Signed) == 48);
static_assert(offsetof(LibreSsl3Pkcs7Signed, contents) == 40);
static_assert(sizeof(LibreSsl3Pkcs7Digest) == 32);
static_assert(offsetof(LibreSsl3Pkcs7Digest, md) == 8);
static_assert(offsetof(LibreSsl3Pkcs7Digest, contents) == 16);
static_assert(offsetof(LibreSsl3Pkcs7Digest, digest) == 24);
static_assert(sizeof(LibreSsl3Pkcs7IssuerAndSerial) == 16);
static_assert(sizeof(LibreSsl3Pkcs7SignerInfo) == 64);
static_assert(offsetof(LibreSsl3Pkcs7SignerInfo, unauthAttr) == 48);
static_assert(sizeof(LibreSsl3Pkcs7EncContent) == 32);
static_assert(offsetof(LibreSsl3Pkcs7EncContent, encData) == 16);
static_assert(sizeof(LibreSsl3Pkcs7RecipInfo) == 40);
static_assert(offsetof(LibreSsl3Pkcs7RecipInfo, encKey) == 24);
static_assert(sizeof(LibreSsl3Pkcs7Envelope) == 24);
static_assert(offsetof(LibreSsl3Pkcs7Envelope, encData) == 16);
static_assert(sizeof(LibreSsl3Pkcs7SignEnvelope) == 56);
static_assert(offsetof(LibreSsl3Pkcs7SignEnvelope, encData) == 40);
static_assert(sizeof(LibreSsl3Pkcs7Encrypt) == 16);
static_assert(offsetof(LibreSsl3Pkcs7Encrypt, encData) == 8);
static_assert(sizeof(LibreSsl3X509Attribute) == 16);

LibreSsl3Certificate* DecodeCertificate_nid_no_patch(std::span<const unsigned char>& input);
void FreeCertificate_nid_no_patch(LibreSsl3Certificate* certificate);
LibreSsl3X509Algor* DecodeAlgor_nid_no_patch(std::span<const unsigned char>& input);
void FreeAlgor_nid_no_patch(LibreSsl3X509Algor* algor);
LibreSslX509Name* DecodeName_nid_no_patch(std::span<const unsigned char> content);
void FreeName_nid_no_patch(LibreSslX509Name* name);
