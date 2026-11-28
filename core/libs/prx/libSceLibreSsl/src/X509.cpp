#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslX509CertInfo) == 104);
static_assert(offsetof(LibreSslX509CertInfo, serialNumber) == 8);
static_assert(offsetof(LibreSslX509CertInfo, issuer) == 24);
static_assert(offsetof(LibreSslX509CertInfo, subject) == 40);
static_assert(offsetof(LibreSslX509, certificateInfo) == 0);
static_assert(offsetof(LibreSslX509, exData) == 40);
static_assert(offsetof(LibreSslX509, extensionFlags) == 64);
static_assert(offsetof(LibreSslX509, keyUsage) == 72);
static_assert(offsetof(LibreSslX509, subjectKeyId) == 96);
static_assert(offsetof(LibreSslX509, authorityKeyId) == 104);
static_assert(sizeof(Asn1String) == 24 && offsetof(Asn1String, data) == 8);
static_assert(sizeof(LibreSslGeneralName) == 16 && offsetof(LibreSslGeneralName, value) == 8);
static_assert(offsetof(LibreSslStack, data) == 8);
static_assert(offsetof(LibreSslX509Name, modified) == 8 && offsetof(LibreSslX509Name, bytes) == 16);
static_assert(offsetof(LibreSslX509Name, canonEnc) == 24 && offsetof(LibreSslX509Name, canonEnclen) == 32);

namespace {

constexpr int VerifyOk = 0;
constexpr int SubjectIssuerMismatch = 29;
constexpr int SkidMismatch = 30;
constexpr int IssuerSerialMismatch = 31;
constexpr int KeyUsageNoCertSign = 32;
constexpr int KeyUsageNoDigitalSignature = 39;
constexpr std::uint64_t ExtensionKeyUsage = 0x2;
constexpr std::uint64_t ExtensionsCached = 0x100;
constexpr std::uint64_t ExtensionProxy = 0x400;
constexpr std::uint64_t UsageDigitalSignature = 0x80;
constexpr std::uint64_t UsageKeyCertSign = 0x4;
constexpr int DirectoryName = 4;

struct AuthorityKeyId {
    const Asn1String* keyId;
    const LibreSslStack* issuer;
    const Asn1String* serial;
};

static_assert(sizeof(AuthorityKeyId) == 24);

const LibreSslX509CertInfo& CertInfo(const LibreSslX509* certificate, const char* function) {
    if (!certificate) throw std::invalid_argument(std::string(function) + ": null certificate");
    if (!certificate->certificateInfo) throw std::invalid_argument(std::string(function) + ": null certificate info");
    return *static_cast<const LibreSslX509CertInfo*>(certificate->certificateInfo);
}

[[noreturn]] void CheckIssuedFail(const char* what) {
    throw std::invalid_argument(std::string("X509_check_issued: ") + what);
}

template<typename TValue> TValue Read(const void* source, const char* what) {
    if (!source) CheckIssuedFail(what);
    TValue value{};
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(value))) CheckIssuedFail(what);
    return value;
}

std::vector<unsigned char> ReadBytes(const unsigned char* source, int length, const char* what) {
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    if (length != 0) {
        if (!source || !GuestCopyIn_nid_no_patch(bytes.data(), source, bytes.size())) CheckIssuedFail(what);
    }
    return bytes;
}

bool StringsDiffer(const Asn1String* first, const Asn1String* second) {
    const auto left = Read<Asn1String>(first, "unreadable string");
    const auto right = Read<Asn1String>(second, "unreadable string");
    if (left.length != right.length) return true;
    if (left.length < 0) CheckIssuedFail("negative string length");
    if (ReadBytes(left.data, left.length, "unreadable string data") != ReadBytes(right.data, right.length, "unreadable string data")) return true;
    return left.type != right.type;
}

struct CanonicalName {
    bool failed = false;
    int length = 0;
    const unsigned char* bytes = nullptr;
};

CanonicalName Canonical(const void* name) {
    const auto fields = Read<LibreSslX509Name>(name, "unreadable name");
    if (fields.modified) NotImplemented_nid_no_patch("X509_check_issued: re-encoding a modified X509_NAME");
    if (!fields.canonEnc && static_cast<int>(Read<std::size_t>(fields.bytes, "unreadable name encoding")) < 0) return {true};
    return {false, fields.canonEnclen, fields.canonEnc};
}

bool NamesDiffer(const void* first, const void* second) {
    const CanonicalName left = Canonical(first);
    if (left.failed) return true;
    const CanonicalName right = Canonical(second);
    if (right.failed) return true;
    if (left.length != right.length) return true;
    if (left.length < 0) CheckIssuedFail("negative canonical length");
    return ReadBytes(left.bytes, left.length, "unreadable canonical encoding") != ReadBytes(right.bytes, right.length, "unreadable canonical encoding");
}

const void* FirstDirectoryName(const LibreSslStack* names) {
    const auto stack = Read<LibreSslStack>(names, "unreadable authority key issuer names");
    for (int index = 0; index < stack.num; ++index) {
        const auto* entry = Read<const LibreSslGeneralName*>(stack.data + index, "unreadable authority key issuer name array");
        const auto name = Read<LibreSslGeneralName>(entry, "unreadable general name");
        if (name.type == DirectoryName) return name.value;
    }
    return nullptr;
}

int CheckAuthorityKeyId(const LibreSslX509& issuer, const LibreSslX509CertInfo& issuerInfo, const void* authorityKeyId) {
    const auto id = Read<AuthorityKeyId>(authorityKeyId, "unreadable authority key identifier");
    if (id.keyId && issuer.subjectKeyId && StringsDiffer(id.keyId, issuer.subjectKeyId)) return SkidMismatch;
    if (id.serial && StringsDiffer(static_cast<const Asn1String*>(issuerInfo.serialNumber), id.serial)) return IssuerSerialMismatch;
    if (id.issuer) {
        const void* name = FirstDirectoryName(id.issuer);
        if (name && NamesDiffer(name, issuerInfo.issuer)) return IssuerSerialMismatch;
    }
    return VerifyOk;
}

bool RejectsKeyUsage(const LibreSslX509& certificate, std::uint64_t usage) {
    return (certificate.extensionFlags & ExtensionKeyUsage) != 0 && (certificate.keyUsage & usage) == 0;
}

}

extern "C" {

void* APS5_VABI X509_get_issuer_name_nid_postfix(const LibreSslX509* certificate) {
    return CertInfo(certificate, __func__).issuer;
}

void* APS5_VABI X509_get_subject_name_nid_postfix(const LibreSslX509* certificate) {
    return CertInfo(certificate, __func__).subject;
}

void* APS5_VABI X509_get_serialNumber_nid_postfix(const LibreSslX509* certificate) {
    return CertInfo(certificate, __func__).serialNumber;
}

int APS5_VABI X509_check_issued_nid_postfix(LibreSslX509* issuer, LibreSslX509* subject) {
    const auto issuerFields = Read<LibreSslX509>(issuer, "unreadable issuer");
    const auto issuerInfo = Read<LibreSslX509CertInfo>(issuerFields.certificateInfo, "unreadable issuer certificate info");
    const auto subjectFields = Read<LibreSslX509>(subject, "unreadable subject");
    const auto subjectInfo = Read<LibreSslX509CertInfo>(subjectFields.certificateInfo, "unreadable subject certificate info");
    if (NamesDiffer(issuerInfo.subject, subjectInfo.issuer)) return SubjectIssuerMismatch;
    if ((issuerFields.extensionFlags & ExtensionsCached) == 0) NotImplemented_nid_no_patch("X509_check_issued: caching the issuer's extensions");
    if ((subjectFields.extensionFlags & ExtensionsCached) == 0) NotImplemented_nid_no_patch("X509_check_issued: caching the subject's extensions");
    if (subjectFields.authorityKeyId) {
        const int result = CheckAuthorityKeyId(issuerFields, issuerInfo, subjectFields.authorityKeyId);
        if (result != VerifyOk) return result;
    }
    if (subjectFields.extensionFlags & ExtensionProxy) {
        if (RejectsKeyUsage(issuerFields, UsageDigitalSignature)) return KeyUsageNoDigitalSignature;
    } else if (RejectsKeyUsage(issuerFields, UsageKeyCertSign)) {
        return KeyUsageNoCertSign;
    }
    return VerifyOk;
}

}
