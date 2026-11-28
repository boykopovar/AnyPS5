#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

struct AuthorityKeyId {
    Asn1String* keyId;
    LibreSsl3Stack* issuer;
    Asn1String* serial;
};

struct BufMem {
    std::size_t length;
    char* data;
    std::size_t max;
};

struct alignas(8) Certificate {
    unsigned char bytes[224]{};
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_check_akid check failed at line %u\n", location.line());
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

struct Fixture {
    std::vector<unsigned char> keyBytes{1, 2, 3, 4};
    std::vector<unsigned char> serialBytes{0x01, 0x23};
    std::vector<unsigned char> canonBytes{0x31, 0x0b, 0x30, 0x09};
    std::vector<unsigned char> otherCanonBytes{0x31, 0x0b, 0x30, 0x0a};
    Asn1String skid{4, OctetString, keyBytes.data(), 0};
    Asn1String serial{2, Integer, serialBytes.data(), 0};
    BufMem encoding{13, nullptr, 0};
    LibreSslX509Name issuerName{nullptr, 0, &encoding, canonBytes.data(), 4};
    LibreSsl3CertificateInfo info{};
    Certificate certificate{};

    Fixture() {
        info.serialNumber = &serial;
        info.issuer = &issuerName;
        const LibreSsl3CertificateInfo* infoPointer = &info;
        std::memcpy(certificate.bytes, &infoPointer, sizeof(infoPointer));
        SetSkid(&skid);
    }

    void SetSkid(Asn1String* value) {
        std::memcpy(certificate.bytes + 96, &value, sizeof(value));
    }

    int Check(AuthorityKeyId* id) {
        return X509_check_akid_nid_postfix(certificate.bytes, id);
    }
};

struct Names {
    std::vector<LibreSslGeneralName> names;
    std::vector<char*> pointers;
    LibreSsl3Stack stack{};

    explicit Names(std::vector<LibreSslGeneralName> entries) : names(std::move(entries)) {
        for (auto& name : names) pointers.push_back(reinterpret_cast<char*>(&name));
        stack.num = static_cast<int>(pointers.size());
        stack.data = pointers.data();
        stack.numAlloc = stack.num;
    }
};

void TestNullAndEmpty() {
    void* const unreadable = reinterpret_cast<void*>(std::uintptr_t{1});
    Require(X509_check_akid_nid_postfix(nullptr, nullptr) == 0);
    Require(X509_check_akid_nid_postfix(unreadable, nullptr) == 0);
    errno = EDOM;
    RequireInvalidArgument([&] { X509_check_akid_nid_postfix(nullptr, unreadable); });
    Require(errno == EDOM);
    AuthorityKeyId empty{};
    Require(X509_check_akid_nid_postfix(nullptr, &empty) == 0);
    Require(X509_check_akid_nid_postfix(unreadable, &empty) == 0);
}

void TestKeyId() {
    Fixture fixture;
    std::vector<unsigned char> same{1, 2, 3, 4};
    std::vector<unsigned char> different{1, 2, 3, 5};
    Asn1String keyId{4, OctetString, same.data(), 0};
    AuthorityKeyId id{&keyId, nullptr, nullptr};
    Require(fixture.Check(&id) == 0);

    keyId.data = different.data();
    Require(fixture.Check(&id) == 30);
    keyId.data = same.data();
    keyId.length = 3;
    Require(fixture.Check(&id) == 30);
    keyId.length = 4;
    keyId.type = Integer;
    Require(fixture.Check(&id) == 30);

    fixture.SetSkid(nullptr);
    Require(fixture.Check(&id) == 0);

    Asn1String emptyKey{0, OctetString, nullptr, 0};
    Asn1String emptySkid{0, OctetString, nullptr, 0};
    fixture.SetSkid(&emptySkid);
    id.keyId = &emptyKey;
    Require(fixture.Check(&id) == 0);

    RequireInvalidArgument([&] { X509_check_akid_nid_postfix(nullptr, &id); });
}

void TestSerial() {
    Fixture fixture;
    std::vector<unsigned char> same{0x01, 0x23};
    std::vector<unsigned char> different{0x01, 0x24};
    Asn1String serial{2, Integer, same.data(), 0};
    AuthorityKeyId id{nullptr, nullptr, &serial};
    Require(fixture.Check(&id) == 0);

    serial.data = different.data();
    Require(fixture.Check(&id) == 31);
    serial.data = same.data();
    serial.type = NegativeInteger;
    Require(fixture.Check(&id) == 31);
    fixture.serial.type = NegativeInteger;
    Require(fixture.Check(&id) == 0);

    std::vector<unsigned char> otherKey{9};
    Asn1String keyId{1, OctetString, otherKey.data(), 0};
    serial.data = different.data();
    id.keyId = &keyId;
    Require(fixture.Check(&id) == 30);

    id.keyId = nullptr;
    fixture.info.serialNumber = nullptr;
    RequireInvalidArgument([&] { fixture.Check(&id); });
    RequireInvalidArgument([&] { X509_check_akid_nid_postfix(nullptr, &id); });
}

void TestIssuerName() {
    Fixture fixture;
    std::vector<unsigned char> sameCanon{0x31, 0x0b, 0x30, 0x09};
    BufMem encoding{13, nullptr, 0};
    LibreSslX509Name same{nullptr, 0, &encoding, sameCanon.data(), 4};
    LibreSslX509Name other{nullptr, 0, &encoding, fixture.otherCanonBytes.data(), 4};
    LibreSslX509Name shorter{nullptr, 0, &encoding, sameCanon.data(), 3};

    Names dnsOnly({{Dns, &other}});
    AuthorityKeyId id{nullptr, &dnsOnly.stack, nullptr};
    Require(fixture.Check(&id) == 0);

    Names matching({{Dns, &other}, {DirectoryName, &same}});
    id.issuer = &matching.stack;
    Require(fixture.Check(&id) == 0);

    Names mismatching({{DirectoryName, &other}});
    id.issuer = &mismatching.stack;
    Require(fixture.Check(&id) == 31);

    Names firstOnly({{DirectoryName, &same}, {DirectoryName, &other}});
    id.issuer = &firstOnly.stack;
    Require(fixture.Check(&id) == 0);

    Names lengthDiffers({{DirectoryName, &shorter}});
    id.issuer = &lengthDiffers.stack;
    Require(fixture.Check(&id) == 31);

    Names empty({});
    id.issuer = &empty.stack;
    Require(X509_check_akid_nid_postfix(nullptr, &id) == 0);

    LibreSslX509Name emptyCanon{nullptr, 0, &encoding, nullptr, 0};
    fixture.issuerName = emptyCanon;
    Names emptyMatch({{DirectoryName, &emptyCanon}});
    id.issuer = &emptyMatch.stack;
    Require(fixture.Check(&id) == 0);

    LibreSslX509Name noEncoding{nullptr, 0, nullptr, nullptr, 0};
    Names missingEncoding({{DirectoryName, &noEncoding}});
    id.issuer = &missingEncoding.stack;
    RequireInvalidArgument([&] { fixture.Check(&id); });

    LibreSslX509Name modified{nullptr, 1, &encoding, sameCanon.data(), 4};
    Names modifiedName({{DirectoryName, &modified}});
    id.issuer = &modifiedName.stack;
    RequireNotImplemented([&] { fixture.Check(&id); });

    Names nullEntry({});
    nullEntry.pointers.push_back(nullptr);
    nullEntry.stack.num = 1;
    nullEntry.stack.data = nullEntry.pointers.data();
    id.issuer = &nullEntry.stack;
    RequireInvalidArgument([&] { fixture.Check(&id); });
}

void TestOrder() {
    Fixture fixture;
    std::vector<unsigned char> different{0x01, 0x24};
    Asn1String serial{2, Integer, different.data(), 0};
    LibreSslX509Name modified{nullptr, 1, nullptr, nullptr, 0};
    Names modifiedName({{DirectoryName, &modified}});
    AuthorityKeyId id{nullptr, &modifiedName.stack, &serial};
    Require(fixture.Check(&id) == 31);
}

}

int main() {
    TestNullAndEmpty();
    TestKeyId();
    TestSerial();
    TestIssuerName();
    TestOrder();
    return 0;
}
