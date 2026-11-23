#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int VerifyOk = 0;
constexpr int SkidMismatch = 30;
constexpr int IssuerSerialMismatch = 31;
constexpr int NegativeFlag = 0x100;
constexpr int DirectoryName = 4;
constexpr std::size_t SkidOffset = 96;

struct AuthorityKeyId {
    const Asn1String* keyId;
    const LibreSsl3Stack* issuer;
    const Asn1String* serial;
};

static_assert(sizeof(AuthorityKeyId) == 24);
static_assert(sizeof(Asn1String) == 24 && offsetof(Asn1String, data) == 8);
static_assert(sizeof(LibreSslGeneralName) == 16 && offsetof(LibreSslGeneralName, value) == 8);
static_assert(offsetof(LibreSslX509Name, modified) == 8 && offsetof(LibreSslX509Name, bytes) == 16);
static_assert(offsetof(LibreSslX509Name, canonEnc) == 24 && offsetof(LibreSslX509Name, canonEnclen) == 32);
static_assert(offsetof(LibreSsl3CertificateInfo, serialNumber) == 8 && offsetof(LibreSsl3CertificateInfo, issuer) == 24);

[[noreturn]] void Fail(const char* what) {
    throw std::invalid_argument(std::string("X509_check_akid: ") + what);
}

void ReadGuest(void* destination, const void* source, std::size_t size, const char* what) {
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, size);
    if (!copied) Fail(what);
}

template<typename Value> Value Read(const void* source, const char* what) {
    Value value{};
    ReadGuest(&value, source, sizeof(value), what);
    return value;
}

std::vector<unsigned char> ReadBytes(const void* source, int length, const char* what) {
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    if (length != 0) ReadGuest(bytes.data(), source, bytes.size(), what);
    return bytes;
}

bool StringsDiffer(const Asn1String* first, const Asn1String* second) {
    if (!first || !second) return true;
    const auto left = Read<Asn1String>(first, "unreadable string");
    const auto right = Read<Asn1String>(second, "unreadable string");
    if (left.length != right.length) return true;
    if (left.length < 0) Fail("negative string length");
    if (left.length != 0) {
        if (!left.data || !right.data) Fail("null string data");
        if (ReadBytes(left.data, left.length, "unreadable string data") != ReadBytes(right.data, right.length, "unreadable string data")) return true;
    }
    return left.type != right.type;
}

bool IntegersDiffer(const Asn1String* first, const Asn1String* second) {
    if (!first) Fail("null serial number");
    const int firstType = Read<Asn1String>(first, "unreadable serial number").type;
    const int secondType = Read<Asn1String>(second, "unreadable serial number").type;
    if ((firstType & NegativeFlag) != (secondType & NegativeFlag)) return true;
    return StringsDiffer(first, second);
}

struct CanonicalName {
    bool failed = false;
    int length = 0;
    const unsigned char* bytes = nullptr;
};

CanonicalName Canonical(const LibreSslX509Name* name) {
    if (!name) Fail("null name");
    const auto fields = Read<LibreSslX509Name>(name, "unreadable name");
    if (fields.modified) NotImplemented_nid_no_patch("X509_check_akid: re-encoding a modified X509_NAME");
    CanonicalName canonical{false, fields.canonEnclen, fields.canonEnc};
    if (!fields.canonEnc) {
        if (!fields.bytes) Fail("null name encoding");
        if (static_cast<int>(Read<std::size_t>(fields.bytes, "unreadable name encoding")) < 0) canonical.failed = true;
    }
    return canonical;
}

bool NamesDiffer(const LibreSslX509Name* first, const LibreSslX509Name* second) {
    const CanonicalName left = Canonical(first);
    if (left.failed) return true;
    const CanonicalName right = Canonical(second);
    if (right.failed) return true;
    if (left.length != right.length) return true;
    if (left.length < 0) Fail("negative canonical length");
    if (left.length != 0 && (!left.bytes || !right.bytes)) Fail("null canonical encoding");
    return ReadBytes(left.bytes, left.length, "unreadable canonical encoding") != ReadBytes(right.bytes, right.length, "unreadable canonical encoding");
}

const LibreSslX509Name* FirstDirectoryName(const LibreSsl3Stack* names) {
    const auto stack = Read<LibreSsl3Stack>(names, "unreadable issuer names");
    for (int index = 0; index < stack.num; ++index) {
        if (!stack.data) Fail("null issuer name array");
        const auto* entry = Read<const LibreSslGeneralName*>(stack.data + index, "unreadable issuer name array");
        if (!entry) Fail("null general name");
        const auto name = Read<LibreSslGeneralName>(entry, "unreadable general name");
        if (name.type == DirectoryName) return static_cast<const LibreSslX509Name*>(name.value);
    }
    return nullptr;
}

const LibreSsl3CertificateInfo* CertificateInfo(const LibreSsl3Certificate* certificate) {
    if (!certificate) Fail("null certificate");
    const auto* info = Read<const LibreSsl3CertificateInfo*>(&certificate->certInfo, "unreadable certificate");
    if (!info) Fail("null certificate info");
    return info;
}

} // namespace

extern "C" int APS5_VABI X509_check_akid_nid_postfix(void* issuer, void* authorityKeyId) {
    if (!authorityKeyId) return VerifyOk;
    const auto id = Read<AuthorityKeyId>(authorityKeyId, "unreadable authority key identifier");
    const auto* certificate = static_cast<const LibreSsl3Certificate*>(issuer);
    if (id.keyId) {
        if (!certificate) Fail("null certificate");
        const auto* skid = Read<const Asn1String*>(reinterpret_cast<const unsigned char*>(certificate) + SkidOffset, "unreadable certificate");
        if (skid && StringsDiffer(id.keyId, skid)) return SkidMismatch;
    }
    if (id.serial) {
        const auto* serial = Read<const Asn1String*>(&CertificateInfo(certificate)->serialNumber, "unreadable certificate info");
        if (IntegersDiffer(static_cast<const Asn1String*>(serial), id.serial)) return IssuerSerialMismatch;
    }
    if (id.issuer) {
        const LibreSslX509Name* name = FirstDirectoryName(id.issuer);
        if (name) {
            const auto* issuerName = Read<const LibreSslX509Name*>(&CertificateInfo(certificate)->issuer, "unreadable certificate info");
            if (NamesDiffer(name, issuerName)) return IssuerSerialMismatch;
        }
    }
    return VerifyOk;
}
