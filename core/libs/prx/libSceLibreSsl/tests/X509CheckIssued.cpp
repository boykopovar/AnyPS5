#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr int OctetString = 4;
constexpr int Integer = 2;
constexpr int NegativeInteger = 2 | 0x100;
constexpr int Dns = 2;
constexpr int DirectoryName = 4;
constexpr std::uint64_t KeyUsageFlag = 0x2;
constexpr std::uint64_t CachedFlag = 0x100;
constexpr std::uint64_t ProxyFlag = 0x400;
constexpr std::uint64_t DigitalSignature = 0x80;
constexpr std::uint64_t KeyCertSign = 0x4;

struct AuthorityKeyId {
    Asn1String* keyId;
    LibreSslStack* issuer;
    Asn1String* serial;
};

struct BufMem {
    std::size_t length;
    char* data;
    std::size_t max;
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL X509_check_issued check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireNotImplemented(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    } catch (...) {
    }
    Require(false, location);
}

template<typename TAction>
void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    } catch (...) {
    }
    Require(false, location);
}

struct Names {
    std::vector<LibreSslGeneralName> names;
    std::vector<char*> pointers;
    LibreSslStack stack{};

    explicit Names(std::vector<LibreSslGeneralName> entries) : names(std::move(entries)) {
        for (auto& name : names) pointers.push_back(reinterpret_cast<char*>(&name));
        stack.num = static_cast<int>(pointers.size());
        stack.data = pointers.data();
        stack.numAlloc = stack.num;
    }
};

struct Fixture {
    std::vector<unsigned char> rootCanon{0x31, 0x0b, 0x30, 0x09};
    std::vector<unsigned char> otherCanon{0x31, 0x0b, 0x30, 0x0a};
    std::vector<unsigned char> keyBytes{1, 2, 3, 4};
    std::vector<unsigned char> serialBytes{0x01, 0x23};
    BufMem encoding{13, nullptr, 0};
    LibreSslX509Name rootName{nullptr, 0, &encoding, rootCanon.data(), 4};
    LibreSslX509Name subjectIssuerName{nullptr, 0, &encoding, rootCanon.data(), 4};
    LibreSslX509Name grandparentName{nullptr, 0, &encoding, otherCanon.data(), 4};
    LibreSslX509Name leafName{nullptr, 0, &encoding, otherCanon.data(), 4};
    Asn1String skid{4, OctetString, keyBytes.data(), 0};
    Asn1String serial{2, Integer, serialBytes.data(), 0};
    LibreSslX509CertInfo issuerInfo{};
    LibreSslX509CertInfo subjectInfo{};
    LibreSslX509 issuer{};
    LibreSslX509 subject{};

    Fixture() {
        issuerInfo.serialNumber = &serial;
        issuerInfo.issuer = &grandparentName;
        issuerInfo.subject = &rootName;
        subjectInfo.issuer = &subjectIssuerName;
        subjectInfo.subject = &leafName;
        issuer.certificateInfo = &issuerInfo;
        subject.certificateInfo = &subjectInfo;
        issuer.extensionFlags = CachedFlag;
        subject.extensionFlags = CachedFlag;
        issuer.subjectKeyId = &skid;
    }

    int Check() {
        return X509_check_issued_nid_postfix(&issuer, &subject);
    }
};

void TestNames() {
    Fixture fixture;
    Require(fixture.Check() == 0);
    fixture.subjectIssuerName.canonEnc = fixture.otherCanon.data();
    Require(fixture.Check() == 29);
    fixture.subjectIssuerName.canonEnc = fixture.rootCanon.data();
    fixture.subjectIssuerName.canonEnclen = 3;
    Require(fixture.Check() == 29);

    Fixture empty;
    BufMem emptyEncoding{2, nullptr, 0};
    empty.rootName = {nullptr, 0, &emptyEncoding, nullptr, 0};
    empty.subjectIssuerName = {nullptr, 0, &emptyEncoding, nullptr, 0};
    Require(empty.Check() == 0);
    empty.subjectIssuerName.canonEnc = empty.rootCanon.data();
    empty.subjectIssuerName.canonEnclen = 4;
    Require(empty.Check() == 29);

    Fixture failed;
    BufMem oversized{0x80000000u, nullptr, 0};
    failed.rootName = {nullptr, 0, &oversized, nullptr, 0};
    failed.subjectIssuerName = {nullptr, 1, nullptr, nullptr, 0};
    Require(failed.Check() == 29);
}

void TestNameFailures() {
    Fixture modified;
    modified.rootName.modified = 1;
    RequireNotImplemented([&] { modified.Check(); });

    Fixture noEncoding;
    noEncoding.rootName = {nullptr, 0, nullptr, nullptr, 0};
    RequireInvalidArgument([&] { noEncoding.Check(); });

    Fixture nullCanon;
    nullCanon.rootName.canonEnc = nullptr;
    nullCanon.subjectIssuerName.canonEnc = nullptr;
    RequireInvalidArgument([&] { nullCanon.Check(); });

    Fixture negative;
    negative.rootName.canonEnclen = -1;
    negative.subjectIssuerName.canonEnclen = -1;
    RequireInvalidArgument([&] { negative.Check(); });

    Fixture nullName;
    nullName.issuerInfo.subject = nullptr;
    RequireInvalidArgument([&] { nullName.Check(); });
}

void TestCertificates() {
    Fixture fixture;
    RequireInvalidArgument([&] { X509_check_issued_nid_postfix(nullptr, &fixture.subject); });
    RequireInvalidArgument([&] { X509_check_issued_nid_postfix(&fixture.issuer, nullptr); });
    fixture.subject.certificateInfo = nullptr;
    RequireInvalidArgument([&] { fixture.Check(); });

    Fixture self;
    self.issuerInfo.issuer = &self.rootName;
    Require(X509_check_issued_nid_postfix(&self.issuer, &self.issuer) == 0);
}

void TestExtensionCache() {
    Fixture issuerUncached;
    issuerUncached.issuer.extensionFlags = 0;
    RequireNotImplemented([&] { issuerUncached.Check(); });

    Fixture subjectUncached;
    subjectUncached.subject.extensionFlags = KeyUsageFlag;
    RequireNotImplemented([&] { subjectUncached.Check(); });

    Fixture mismatch;
    mismatch.issuer.extensionFlags = 0;
    mismatch.subject.extensionFlags = 0;
    mismatch.subjectIssuerName.canonEnc = mismatch.otherCanon.data();
    Require(mismatch.Check() == 29);
}

void TestKeyUsage() {
    Fixture fixture;
    fixture.issuer.extensionFlags = CachedFlag | KeyUsageFlag;
    fixture.issuer.keyUsage = DigitalSignature;
    Require(fixture.Check() == 32);
    fixture.issuer.keyUsage = KeyCertSign;
    Require(fixture.Check() == 0);
    fixture.issuer.extensionFlags = CachedFlag;
    fixture.issuer.keyUsage = 0;
    Require(fixture.Check() == 0);

    Fixture proxy;
    proxy.subject.extensionFlags = CachedFlag | ProxyFlag;
    proxy.issuer.extensionFlags = CachedFlag | KeyUsageFlag;
    proxy.issuer.keyUsage = KeyCertSign;
    Require(proxy.Check() == 39);
    proxy.issuer.keyUsage = DigitalSignature;
    Require(proxy.Check() == 0);
}

void TestKeyId() {
    Fixture fixture;
    std::vector<unsigned char> same{1, 2, 3, 4};
    std::vector<unsigned char> different{1, 2, 3, 5};
    Asn1String keyId{4, OctetString, same.data(), 0};
    AuthorityKeyId id{&keyId, nullptr, nullptr};
    fixture.subject.authorityKeyId = &id;
    Require(fixture.Check() == 0);
    keyId.data = different.data();
    Require(fixture.Check() == 30);
    keyId.data = same.data();
    keyId.type = Integer;
    Require(fixture.Check() == 30);
    keyId.type = OctetString;
    keyId.length = 3;
    Require(fixture.Check() == 30);
    fixture.issuer.subjectKeyId = nullptr;
    Require(fixture.Check() == 0);

    Fixture usage;
    Asn1String otherKeyId{4, OctetString, different.data(), 0};
    AuthorityKeyId otherId{&otherKeyId, nullptr, nullptr};
    usage.subject.authorityKeyId = &otherId;
    usage.issuer.extensionFlags = CachedFlag | KeyUsageFlag;
    Require(usage.Check() == 30);
}

void TestSerial() {
    Fixture fixture;
    std::vector<unsigned char> same{0x01, 0x23};
    std::vector<unsigned char> different{0x01, 0x24};
    Asn1String serial{2, Integer, same.data(), 0};
    AuthorityKeyId id{nullptr, nullptr, &serial};
    fixture.subject.authorityKeyId = &id;
    Require(fixture.Check() == 0);
    serial.data = different.data();
    Require(fixture.Check() == 31);
    serial.data = same.data();
    serial.type = NegativeInteger;
    Require(fixture.Check() == 31);
    fixture.issuerInfo.serialNumber = nullptr;
    RequireInvalidArgument([&] { fixture.Check(); });
}

void TestIssuerName() {
    Fixture fixture;
    LibreSslX509Name matching{nullptr, 0, &fixture.encoding, fixture.otherCanon.data(), 4};
    LibreSslX509Name other{nullptr, 0, &fixture.encoding, fixture.rootCanon.data(), 4};
    Names first({{Dns, nullptr}, {DirectoryName, &matching}, {DirectoryName, &other}});
    AuthorityKeyId id{nullptr, &first.stack, nullptr};
    fixture.subject.authorityKeyId = &id;
    Require(fixture.Check() == 0);
    Names mismatched({{DirectoryName, &other}, {DirectoryName, &matching}});
    id.issuer = &mismatched.stack;
    Require(fixture.Check() == 31);
    Names noDirectory({{Dns, nullptr}});
    id.issuer = &noDirectory.stack;
    Require(fixture.Check() == 0);
    Names empty({});
    id.issuer = &empty.stack;
    Require(fixture.Check() == 0);
    Names nullEntry({{DirectoryName, &matching}});
    nullEntry.pointers[0] = nullptr;
    id.issuer = &nullEntry.stack;
    RequireInvalidArgument([&] { fixture.Check(); });
}

void TestOrder() {
    Fixture fixture;
    std::vector<unsigned char> different{9, 9, 9, 9};
    Asn1String keyId{4, OctetString, different.data(), 0};
    Asn1String serial{1, Integer, different.data(), 0};
    AuthorityKeyId id{&keyId, nullptr, &serial};
    fixture.subject.authorityKeyId = &id;
    Require(fixture.Check() == 30);
    keyId.data = fixture.keyBytes.data();
    Require(fixture.Check() == 31);
    fixture.subject.authorityKeyId = reinterpret_cast<void*>(std::uintptr_t{1});
    RequireInvalidArgument([&] { fixture.Check(); });
}

}

int main() {
    TestNames();
    TestNameFailures();
    TestCertificates();
    TestExtensionCache();
    TestKeyUsage();
    TestKeyId();
    TestSerial();
    TestIssuerName();
    TestOrder();
    return 0;
}
