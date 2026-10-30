#include <cstddef>
#include <cstdint>

#include "prx/libSceLibreSsl/include/Api.hpp"

struct LibreSsl3EvpPkey {
    std::int32_t type;
    std::int32_t references;
    const void* method;
    void* key;
    std::int32_t saveParameters;
};

struct LibreSsl3EcKey {
    const void* method;
    std::int32_t version;
    void* group;
    void* publicKey;
    void* privateKey;
    std::uint32_t encodingFlags;
    std::int32_t conversionForm;
    std::int32_t references;
    std::int32_t flags;
    void* extraData;
};

struct LibreSsl3Rsa {
    std::int32_t pad;
    std::int64_t version;
    const void* method;
    void* n;
    void* e;
    void* d;
    void* p;
    void* q;
    void* dmp1;
    void* dmq1;
    void* iqmp;
    void* pss;
    void* extraData;
    std::int32_t references;
    std::int32_t flags;
    void* methodModN;
    void* methodModP;
    void* methodModQ;
    void* blinding;
    void* mtBlinding;
};

struct LibreSsl3X509Val {
    Asn1String* notBefore;
    Asn1String* notAfter;
};

struct LibreSsl3X509CertInfo {
    Asn1String* version;
    void* serialNumber;
    void* signature;
    void* issuer;
    LibreSsl3X509Val* validity;
    void* subject;
    void* key;
    void* issuerUID;
    void* subjectUID;
    LibreSslStack* extensions;
};

struct LibreSsl3X509 {
    LibreSsl3X509CertInfo* certInfo;
    void* sigAlg;
    void* signature;
    std::int32_t valid;
    std::int32_t references;
};

static_assert(sizeof(LibreSsl3EvpPkey) == 32);
static_assert(offsetof(LibreSsl3EvpPkey, references) == 4);
static_assert(sizeof(LibreSsl3EcKey) == 64);
static_assert(offsetof(LibreSsl3EcKey, references) == 48);
static_assert(sizeof(LibreSsl3Rsa) == 152);
static_assert(offsetof(LibreSsl3Rsa, references) == 104);
static_assert(sizeof(LibreSsl3X509Val) == 16);
static_assert(sizeof(LibreSsl3X509CertInfo) == 80);
static_assert(sizeof(LibreSsl3X509) == 32);
static_assert(offsetof(LibreSsl3X509, references) == 28);
static_assert(offsetof(LibreSsl3X509, certInfo) == 0);
static_assert(offsetof(LibreSsl3X509CertInfo, validity) == 32);
static_assert(offsetof(LibreSsl3X509CertInfo, extensions) == 72);
