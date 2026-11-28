#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

std::unordered_map<void*, std::size_t> allocations;
int allocationsUntilFailure = -1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509V3_EXT_d2i check failed at line %u\n", location.line());
    std::abort();
}

template <typename Callable>
void RequireInvalidArgument(Callable call, std::source_location location = std::source_location::current()) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

template <typename Callable>
void RequireNotImplemented(Callable call, std::source_location location = std::source_location::current()) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, location);
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    void* result = std::malloc(size == 0 ? 1 : size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    void* result = std::calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    if (!pointer) return Allocate_nid_no_patch(size);
    Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size == 0 ? 1 : size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

constexpr std::uint64_t Error(int reason) {
    return 0x0dfff000u | static_cast<std::uint64_t>(reason);
}

void RequireQueue(std::initializer_list<int> reasons) {
    for (const int reason : reasons) Require(ERR_get_error_nid_postfix() == Error(reason));
    Require(ERR_get_error_nid_postfix() == 0);
}

void DrainErrors() {
    while (ERR_get_error_nid_postfix() != 0) {}
}

struct Fixture {
    LibreSslAsn1Object object{};
    Asn1String encoded{};
    LibreSsl3X509Extension extension{};
    std::vector<unsigned char> der;

    Fixture(int nid, std::initializer_list<unsigned char> bytes) : der(bytes) {
        object.nid = nid;
        encoded.length = static_cast<std::int32_t>(der.size());
        encoded.type = 4;
        encoded.data = der.data();
        extension.object = &object;
        extension.critical = 0;
        extension.value = &encoded;
    }

    Fixture(int nid, const unsigned char* oid, std::int32_t oidLength, std::initializer_list<unsigned char> bytes)
        : der(bytes) {
        object.nid = nid;
        object.length = oidLength;
        object.data = oid;
        encoded.length = static_cast<std::int32_t>(der.size());
        encoded.type = 4;
        encoded.data = der.data();
        extension.object = &object;
        extension.critical = 0;
        extension.value = &encoded;
    }
};

void FreeString(Asn1String* value) {
    if (!value) return;
    free_nid_postfix(value->data);
    free_nid_postfix(value);
}

void FreeInteger(Asn1String* value) {
    ASN1_INTEGER_free_nid_postfix(value);
}

void FreeOctetString(Asn1String* value) {
    ASN1_OCTET_STRING_free_nid_postfix(value);
}

void CheckKeyUsage(Fixture& fixture, std::int32_t length, std::int64_t flags, unsigned char first) {
    auto* value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&fixture.extension));
    Require(value != nullptr);
    Require(value->length == length && value->type == 3 && value->flags == flags);
    if (length > 0) Require(value->data[0] == first);
    FreeString(value);
}

void TestBitStrings() {
    Fixture keyUsage(83, {0x03, 0x02, 0x05, 0xa0});
    CheckKeyUsage(keyUsage, 1, 0x0d, 0xa0);
    Fixture empty(83, {0x03, 0x01, 0x00});
    CheckKeyUsage(empty, 0, 0x08, 0);
    Fixture allBits(83, {0x03, 0x02, 0x00, 0xff});
    CheckKeyUsage(allBits, 1, 0x08, 0xff);
    Fixture trailing(83, {0x03, 0x02, 0x05, 0xa0, 0x5a});
    CheckKeyUsage(trailing, 1, 0x0d, 0xa0);
    Fixture nsCertType(71, {0x03, 0x02, 0x02, 0xc0});
    CheckKeyUsage(nsCertType, 1, 0x0a, 0xc0);
}

void TestOctetStrings() {
    Fixture subjectKeyIdentifier(82, {0x04, 0x03, 0x01, 0x02, 0x03});
    auto* value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&subjectKeyIdentifier.extension));
    Require(value != nullptr);
    Require(value->length == 3 && value->type == 4 && value->flags == 0);
    Require(value->data[0] == 1 && value->data[1] == 2 && value->data[2] == 3 && value->data[3] == 0);
    FreeOctetString(value);
    Fixture empty(82, {0x04, 0x00});
    value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&empty.extension));
    Require(value != nullptr && value->length == 0 && value->type == 4);
    Require(value->data != nullptr && value->data[0] == 0);
    FreeOctetString(value);
    Fixture longPadded(82, {0x04, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03});
    value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&longPadded.extension));
    Require(value != nullptr && value->length == 3 && value->type == 4 && value->flags == 0);
    Require(value->data[0] == 1 && value->data[1] == 2 && value->data[2] == 3 && value->data[3] == 0);
    FreeOctetString(value);
    RequireQueue({});
}

void TestIntegers() {
    Fixture crlNumber(88, {0x02, 0x01, 0x2a});
    auto* value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&crlNumber.extension));
    Require(value != nullptr && value->length == 1 && value->type == 2 && value->data[0] == 0x2a);
    FreeInteger(value);
    Fixture negative(88, {0x02, 0x01, 0xd6});
    value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&negative.extension));
    Require(value != nullptr && value->length == 1 && value->type == 0x102 && value->data[0] == 0x2a);
    FreeInteger(value);
    Fixture padded(88, {0x02, 0x02, 0x00, 0x80});
    value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&padded.extension));
    Require(value != nullptr && value->length == 1 && value->type == 2 && value->data[0] == 0x80);
    FreeInteger(value);
    Fixture delta(140, {0x02, 0x01, 0x07});
    value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&delta.extension));
    Require(value != nullptr && value->length == 1 && value->data[0] == 7);
    FreeInteger(value);
    Fixture inhibit(748, {0x02, 0x01, 0x00});
    value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&inhibit.extension));
    Require(value != nullptr && value->length == 1 && value->type == 2 && value->data[0] == 0);
    FreeInteger(value);
}

void TestEnumerated() {
    Fixture reason(141, {0x0a, 0x01, 0x01});
    auto* value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&reason.extension));
    Require(value != nullptr && value->length == 1 && value->type == 10 && value->data[0] == 1);
    FreeInteger(value);
    Fixture negative(141, {0x0a, 0x01, 0xd6});
    value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&negative.extension));
    Require(value != nullptr && value->length == 1 && value->type == 0x10a && value->data[0] == 0x2a);
    FreeInteger(value);
    Fixture empty(141, {0x0a, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&empty.extension) == nullptr);
    RequireQueue({62});
}

constexpr unsigned char ValidTime[] = {'2', '0', '2', '4', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', 'Z'};

void TestGeneralizedTime() {
    Fixture invalidityDate(142, {0x18, 0x0f, '2', '0', '2', '4', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', 'Z'});
    auto* value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&invalidityDate.extension));
    Require(value != nullptr && value->length == 15 && value->type == 24 && value->flags == 0);
    Require(std::memcmp(value->data, ValidTime, sizeof(ValidTime)) == 0 && value->data[15] == 0);
    FreeString(value);
    Fixture archiveCutoff(370, {0x18, 0x0f, '2', '0', '2', '4', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', 'Z'});
    value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&archiveCutoff.extension));
    Require(value != nullptr && value->length == 15 && value->type == 24);
    FreeString(value);
    for (const auto bytes : std::initializer_list<std::initializer_list<unsigned char>>{
             {0x18, 0x0f, '2', '0', '2', '4', '1', '3', '0', '2', '0', '3', '0', '4', '0', '5', 'Z'},
             {0x18, 0x0f, '2', '0', '2', '4', '0', '2', '3', '0', '0', '3', '0', '4', '0', '5', 'Z'},
             {0x18, 0x0f, '2', '0', '2', '4', '0', '2', '0', '3', '2', '4', '0', '4', '0', '5', 'Z'},
             {0x18, 0x0e, '2', '0', '2', '4', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5'},
             {0x18, 0x0f, '2', '0', '2', '4', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '+'}}) {
        Fixture invalid(142, bytes);
        Require(X509V3_EXT_d2i_nid_postfix(&invalid.extension) == nullptr);
        RequireQueue({132});
    }
}

void TestIa5Strings() {
    Fixture comment(78, {0x16, 0x03, 'a', 'b', 'c'});
    auto* value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&comment.extension));
    Require(value != nullptr && value->length == 3 && value->type == 22 && value->flags == 0);
    Require(std::memcmp(value->data, "abc", 3) == 0 && value->data[3] == 0);
    FreeString(value);
    Fixture baseUrl(72, {0x16, 0x01, 'x'});
    value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&baseUrl.extension));
    Require(value != nullptr && value->length == 1 && value->type == 22);
    FreeString(value);
    Fixture wrongTag(78, {0x13, 0x03, 'a', 'b', 'c'});
    Require(X509V3_EXT_d2i_nid_postfix(&wrongTag.extension) == nullptr);
    RequireQueue({168, 58});
}

void TestObjects() {
    Fixture holdInstruction(430, {0x06, 0x06, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d});
    auto* object = static_cast<Asn1Object_nid_no_patch*>(X509V3_EXT_d2i_nid_postfix(&holdInstruction.extension));
    Require(object != nullptr && object->length == 6 && object->nid == 0 && object->flags == 0x09);
    Require(object->shortName == nullptr && object->longName == nullptr);
    Require(std::memcmp(object->data, holdInstruction.der.data() + 2, 6) == 0);
    Require(OBJ_obj2nid_nid_postfix(reinterpret_cast<const LibreSslAsn1Object*>(object)) == 1);
    ASN1_OBJECT_free_nid_postfix(reinterpret_cast<Asn1Object_nid_no_patch*>(object));
    for (const auto bytes : std::initializer_list<std::initializer_list<unsigned char>>{
             {0x06, 0x00},
             {0x06, 0x02, 0x80, 0x01},
             {0x06, 0x01, 0x80}}) {
        Fixture invalid(430, bytes);
        Require(X509V3_EXT_d2i_nid_postfix(&invalid.extension) == nullptr);
        RequireQueue({216});
    }
}

void TestOcspNonce() {
    Fixture nonce(366, {0x04, 0x02, 0xaa, 0xbb});
    auto* value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&nonce.extension));
    Require(value != nullptr && value->length == 4 && value->type == 4);
    Require(std::memcmp(value->data, nonce.der.data(), 4) == 0 && value->data[4] == 0);
    FreeOctetString(value);
}

struct AuthorityKeyId {
    Asn1String* keyId;
    LibreSslStack* issuer;
    Asn1String* serial;
};

void FreeAuthorityKeyId(AuthorityKeyId* id) {
    AUTHORITY_KEYID_free_nid_postfix(id);
}

void TestAuthorityKeyId() {
    Fixture keyIdOnly(90, {0x30, 0x06, 0x80, 0x04, 0x01, 0x02, 0x03, 0x04});
    auto* id = static_cast<AuthorityKeyId*>(X509V3_EXT_d2i_nid_postfix(&keyIdOnly.extension));
    Require(id != nullptr && id->issuer == nullptr && id->serial == nullptr);
    Require(id->keyId != nullptr && id->keyId->length == 4 && id->keyId->type == 4 && id->keyId->flags == 0);
    Require(std::memcmp(id->keyId->data, keyIdOnly.der.data() + 4, 4) == 0 && id->keyId->data[4] == 0);
    FreeAuthorityKeyId(id);
    Fixture keyIdAndSerial(90, {0x30, 0x09, 0x80, 0x02, 0xaa, 0xbb, 0x82, 0x03, 0x01, 0x00, 0x01});
    id = static_cast<AuthorityKeyId*>(X509V3_EXT_d2i_nid_postfix(&keyIdAndSerial.extension));
    Require(id != nullptr && id->keyId != nullptr && id->keyId->length == 2 && id->issuer == nullptr);
    Require(id->serial != nullptr && id->serial->type == 2 && ASN1_INTEGER_get_nid_postfix(id->serial) == 0x10001);
    FreeAuthorityKeyId(id);
    Fixture serialOnly(90, {0x30, 0x03, 0x82, 0x01, 0xfb});
    id = static_cast<AuthorityKeyId*>(X509V3_EXT_d2i_nid_postfix(&serialOnly.extension));
    Require(id != nullptr && id->keyId == nullptr && id->issuer == nullptr);
    Require(id->serial != nullptr && id->serial->type == 0x102 && ASN1_INTEGER_get_nid_postfix(id->serial) == -5);
    FreeAuthorityKeyId(id);
    Fixture empty(90, {0x30, 0x00, 0x05, 0x00});
    id = static_cast<AuthorityKeyId*>(X509V3_EXT_d2i_nid_postfix(&empty.extension));
    Require(id != nullptr && id->keyId == nullptr && id->issuer == nullptr && id->serial == nullptr);
    FreeAuthorityKeyId(id);
    RequireQueue({});
    for (const auto bytes : std::initializer_list<std::initializer_list<unsigned char>>{
             {0x30, 0x08, 0xa1, 0x06, 0x82, 0x02, 'a', 'b', 0xa0, 0x00},
             {0x30, 0x80, 0x80, 0x01, 0x01, 0x00, 0x00},
             {0x30, 0x05, 0xa0, 0x03, 0x04, 0x01, 0x01}}) {
        Fixture unsupported(90, bytes);
        RequireNotImplemented([&] { X509V3_EXT_d2i_nid_postfix(&unsupported.extension); });
        Require(allocations.empty());
    }
    DrainErrors();
}

void TestAuthorityKeyIssuer() {
    Fixture mixed(90, {
        0x30, 0x2d, 0x80, 0x01, 0x01, 0xa1, 0x25,
        0x82, 0x02, 'a', 'b',
        0xa4, 0x0f, 0x30, 0x0d, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x02, 'C', 'A',
        0x87, 0x04, 192, 0, 2, 1, 0x88, 0x02, 0x2a, 0x03, 0x81, 0x01, 'x', 0x86, 0x01, 'y',
        0x82, 0x01, 0x2a,
    });
    auto* id = static_cast<AuthorityKeyId*>(X509V3_EXT_d2i_nid_postfix(&mixed.extension));
    Require(id && id->keyId && id->serial && id->issuer);
    Require(ASN1_INTEGER_get_nid_postfix(id->serial) == 42);
    Require(sk_num_nid_postfix(id->issuer) == 6);
    const auto issuerName = [&id](int index, int type) {
        auto* name = static_cast<LibreSslGeneralName*>(sk_value_nid_postfix(id->issuer, index));
        Require(name && name->type == type && name->value);
        return name->value;
    };
    const auto* dns = static_cast<const Asn1String*>(issuerName(0, 2));
    Require(dns->type == 22 && dns->length == 2 && std::memcmp(dns->data, "ab", 2) == 0);
    const auto* directory = static_cast<const LibreSslX509Name*>(issuerName(1, 4));
    Require(!directory->modified && directory->canonEnc && directory->canonEnclen > 0);
    Require(sk_num_nid_postfix(directory->entries) == 1);
    const auto* commonName = static_cast<const LibreSslX509NameEntry*>(sk_value_nid_postfix(directory->entries, 0));
    Require(commonName && commonName->value);
    const auto* value = static_cast<const Asn1String*>(commonName->value);
    Require(value->type == 12 && value->length == 2 && std::memcmp(value->data, "CA", 2) == 0);
    const auto* ip = static_cast<const Asn1String*>(issuerName(2, 7));
    const unsigned char ipBytes[] = {192, 0, 2, 1};
    Require(ip->type == 4 && ip->length == 4 && std::memcmp(ip->data, ipBytes, sizeof(ipBytes)) == 0);
    const auto* oid = static_cast<const LibreSslAsn1Object*>(issuerName(3, 8));
    Require(oid->length == 2 && oid->data[0] == 0x2a && oid->data[1] == 0x03);
    const auto* email = static_cast<const Asn1String*>(issuerName(4, 1));
    const auto* uri = static_cast<const Asn1String*>(issuerName(5, 6));
    Require(email->type == 22 && email->length == 1 && email->data[0] == 'x');
    Require(uri->type == 22 && uri->length == 1 && uri->data[0] == 'y');
    FreeAuthorityKeyId(id);
    Require(allocations.empty());
    RequireQueue({});

    Fixture constructedEmptyDns(90, {0x30, 0x04, 0xa1, 0x02, 0xa2, 0x00});
    id = static_cast<AuthorityKeyId*>(X509V3_EXT_d2i_nid_postfix(&constructedEmptyDns.extension));
    Require(id != nullptr && id->keyId == nullptr && id->serial == nullptr && id->issuer != nullptr);
    Require(sk_num_nid_postfix(id->issuer) == 1);
    const auto* emptyDns = static_cast<const Asn1String*>(issuerName(0, 2));
    Require(emptyDns->type == 22 && emptyDns->length == 0 && emptyDns->data[0] == 0);
    FreeAuthorityKeyId(id);
    Require(allocations.empty());
    RequireQueue({});

    for (const auto bytes : std::initializer_list<std::initializer_list<unsigned char>>{
             {0x30, 0x09, 0xa1, 0x07, 0xa2, 0x05, 0x04, 0x03, 'a', '.', 'b'},
             {0x30, 0x0b, 0xa1, 0x09, 0xa2, 0x80, 0x04, 0x03, 'a', '.', 'b', 0x00, 0x00}}) {
        Fixture constructedDns(90, bytes);
        id = static_cast<AuthorityKeyId*>(X509V3_EXT_d2i_nid_postfix(&constructedDns.extension));
        Require(id != nullptr && id->issuer != nullptr && sk_num_nid_postfix(id->issuer) == 1);
        const auto* constructedValue = static_cast<const Asn1String*>(issuerName(0, 2));
        Require(constructedValue->type == 22 && constructedValue->length == 3 &&
            std::memcmp(constructedValue->data, "a.b", 3) == 0 && constructedValue->data[3] == 0);
        FreeAuthorityKeyId(id);
        Require(allocations.empty());
        RequireQueue({});
    }

    const auto checkMalformedConstructed = [](std::initializer_list<unsigned char> bytes,
                                              std::initializer_list<int> queue) {
        Fixture malformedConstructed(90, bytes);
        Require(X509V3_EXT_d2i_nid_postfix(&malformedConstructed.extension) == nullptr);
        Require(allocations.empty());
        RequireQueue(queue);
    };
    checkMalformedConstructed({0x30, 0x06, 0xa1, 0x04, 0xa2, 0x02, 0x00, 0x00}, {159, 58, 58, 58});
    checkMalformedConstructed({0x30, 0x04, 0xa1, 0x02, 0xa2, 0x80}, {137, 58, 58, 58});

    for (const auto bytes : std::initializer_list<std::initializer_list<unsigned char>>{
             {0x30, 0x02, 0x81, 0x00},
             {0x30, 0x06, 0xa1, 0x04, 0x82, 0x03, 'a', 'b'},
             {0x30, 0x04, 0xa1, 0x02, 0x84, 0x00},
             {0x30, 0x08, 0xa1, 0x06, 0xa4, 0x04, 0x30, 0x00, 0x05, 0x00},
             {0x30, 0x04, 0xa1, 0x02, 0x89, 0x00},
             {0x30, 0x04, 0xa1, 0x02, 0x88, 0x00},
             {0x30, 0x04, 0xa1, 0x00, 0xa1, 0x00}}) {
        Fixture malformed(90, bytes);
        Require(X509V3_EXT_d2i_nid_postfix(&malformed.extension) == nullptr);
        Require(allocations.empty());
        Require(ERR_get_error_nid_postfix() != 0);
        DrainErrors();
    }
    for (int allocation = 0; allocation < 64; ++allocation) {
        allocationsUntilFailure = allocation;
        id = static_cast<AuthorityKeyId*>(X509V3_EXT_d2i_nid_postfix(&mixed.extension));
        if (id) {
            Require(allocationsUntilFailure == 0);
            allocationsUntilFailure = -1;
            FreeAuthorityKeyId(id);
            Require(allocations.empty());
            return;
        }
        Require(allocationsUntilFailure == -1);
        Require(allocations.empty());
        DrainErrors();
    }
    Require(false);
}

void TestMalformedAuthorityKeyId() {
    struct Case {
        std::initializer_list<unsigned char> bytes;
        std::initializer_list<int> queue;
    };
    for (const Case& item : std::initializer_list<Case>{
             {{0x31, 0x00}, {168, 58}},
             {{0x10, 0x00}, {149}},
             {{0x30, 0x05, 0x80, 0x01}, {155, 58}},
             {{0x30, 0x03, 0x80, 0x05, 0x01}, {155, 58}},
             {{0x30, 0x03, 0x83, 0x01, 0x00}, {168, 58}},
             {{0x30, 0x03, 0x04, 0x01, 0x00}, {168, 58}},
             {{0x30, 0x06, 0x82, 0x01, 0x01, 0x80, 0x01, 0x01}, {148}},
             {{0x30, 0x06, 0x80, 0x01, 0x01, 0x80, 0x01, 0x02}, {168, 58}},
             {{0x30, 0x05, 0xa2, 0x03, 0x02, 0x01, 0x01}, {231}},
             {{0x30, 0x02, 0x80, 0x80}, {102, 58}},
             {{0x30, 0x01, 0x1f}, {102, 58}}}) {
        Fixture malformed(90, item.bytes);
        Require(X509V3_EXT_d2i_nid_postfix(&malformed.extension) == nullptr);
        Require(allocations.empty());
        RequireQueue(item.queue);
    }
    Fixture emptySerial(90, {0x30, 0x05, 0x80, 0x01, 0x01, 0x82, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&emptySerial.extension) == nullptr);
    Require(allocations.empty());
    Require(ERR_get_error_nid_postfix() != 0);
    DrainErrors();
}

using Bytes = std::vector<unsigned char>;

struct MalformedValue {
    Bytes bytes;
    std::vector<int> queue;
};

struct BasicConstraintsValue {
    int ca;
    Asn1String* pathLength;
};

struct UsagePeriodValue {
    Asn1String* notBefore;
    Asn1String* notAfter;
};

void SetValue(Fixture& fixture, const Bytes& bytes) {
    fixture.der = bytes;
    fixture.encoded.length = static_cast<std::int32_t>(fixture.der.size());
    fixture.encoded.data = fixture.der.data();
}

void* DecodeValue(int nid, const Bytes& bytes) {
    Fixture fixture(nid, {});
    SetValue(fixture, bytes);
    return X509V3_EXT_d2i_nid_postfix(&fixture.extension);
}

Bytes Joined(std::initializer_list<Bytes> parts) {
    Bytes joined;
    for (const Bytes& part : parts) joined.insert(joined.end(), part.begin(), part.end());
    return joined;
}

Bytes Element(unsigned char identifier, const Bytes& content) {
    Require(content.size() < 0x80);
    Bytes element{identifier, static_cast<unsigned char>(content.size())};
    element.insert(element.end(), content.begin(), content.end());
    return element;
}

std::vector<int> DrainReasons() {
    std::vector<int> reasons;
    while (const std::uint64_t code = ERR_get_error_nid_postfix()) {
        Require((code & ~std::uint64_t{0xfff}) == Error(0));
        reasons.push_back(static_cast<int>(code & 0xfff));
    }
    return reasons;
}

void RequireMalformed(int nid, const std::vector<MalformedValue>& values) {
    for (const MalformedValue& value : values) {
        Require(DecodeValue(nid, value.bytes) == nullptr);
        Require(allocations.empty());
        Require(DrainReasons() == value.queue);
    }
}

void RequireUnmodelled(int nid, const std::vector<Bytes>& values) {
    for (const Bytes& bytes : values) {
        Fixture fixture(nid, {});
        SetValue(fixture, bytes);
        RequireNotImplemented([&] { X509V3_EXT_d2i_nid_postfix(&fixture.extension); });
        Require(allocations.empty());
        DrainErrors();
    }
}

std::vector<std::vector<int>> AllocationFailures(int nid, const Bytes& bytes, void (*release)(void*)) {
    Fixture fixture(nid, {});
    SetValue(fixture, bytes);
    std::vector<std::vector<int>> queues;
    for (int step = 0; step < 64; ++step) {
        allocationsUntilFailure = step;
        void* value = X509V3_EXT_d2i_nid_postfix(&fixture.extension);
        allocationsUntilFailure = -1;
        if (value) {
            RequireQueue({});
            release(value);
            Require(allocations.empty());
            return queues;
        }
        Require(allocations.empty());
        queues.push_back(DrainReasons());
    }
    Require(false);
    return queues;
}

void FreeBasicConstraints(void* value) {
    auto* constraints = static_cast<BasicConstraintsValue*>(value);
    FreeInteger(constraints->pathLength);
    free_nid_postfix(constraints);
}

void FreeExtendedKeyUsage(void* value) {
    sk_pop_free_nid_postfix(static_cast<LibreSslStack*>(value), +[](void* pointer) APS5_VABI {
        ASN1_OBJECT_free_nid_postfix(static_cast<Asn1Object_nid_no_patch*>(pointer));
    });
}

void FreeUsagePeriod(void* value) {
    auto* period = static_cast<UsagePeriodValue*>(value);
    FreeOctetString(period->notBefore);
    FreeOctetString(period->notAfter);
    free_nid_postfix(period);
}

BasicConstraintsValue* DecodeBasicConstraints(const Bytes& bytes) {
    auto* constraints = static_cast<BasicConstraintsValue*>(DecodeValue(87, bytes));
    Require(constraints != nullptr);
    RequireQueue({});
    return constraints;
}

void RequireBasicConstraints(const Bytes& bytes, int ca) {
    BasicConstraintsValue* constraints = DecodeBasicConstraints(bytes);
    Require(constraints->ca == ca && constraints->pathLength == nullptr);
    Require(allocations.size() == 1);
    FreeBasicConstraints(constraints);
    Require(allocations.empty());
}

void RequirePathLength(const Bytes& bytes, int ca, std::int32_t type, std::int64_t pathLength) {
    BasicConstraintsValue* constraints = DecodeBasicConstraints(bytes);
    Require(constraints->ca == ca && constraints->pathLength != nullptr);
    Require(constraints->pathLength->type == type && constraints->pathLength->flags == 0);
    Require(ASN1_INTEGER_get_nid_postfix(constraints->pathLength) == pathLength);
    Require(allocations.size() == 3);
    FreeBasicConstraints(constraints);
    Require(allocations.empty());
}

void TestBasicConstraints() {
    static_assert(sizeof(BasicConstraintsValue) == 16 && offsetof(BasicConstraintsValue, pathLength) == 8);
    RequireBasicConstraints({0x30, 0x00}, 0);
    RequireBasicConstraints({0x30, 0x00, 0x5a}, 0);
    RequireBasicConstraints({0x30, 0x81, 0x00}, 0);
    RequireBasicConstraints({0x30, 0x03, 0x01, 0x01, 0xff, 0x5a, 0x5a}, 255);
    RequireBasicConstraints({0x30, 0x81, 0x03, 0x01, 0x01, 0xff}, 255);
    RequireBasicConstraints({0x30, 0x04, 0x01, 0x81, 0x01, 0xff}, 255);
    for (int byte = 0; byte < 256; ++byte)
        RequireBasicConstraints({0x30, 0x03, 0x01, 0x01, static_cast<unsigned char>(byte)}, byte);
    RequirePathLength({0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x03}, 255, 2, 3);
    RequirePathLength({0x30, 0x06, 0x01, 0x01, 0x00, 0x02, 0x01, 0x00}, 0, 2, 0);
    RequirePathLength({0x30, 0x03, 0x02, 0x01, 0x00}, 0, 2, 0);
    RequirePathLength({0x30, 0x03, 0x02, 0x01, 0x7f, 0x5a}, 0, 2, 127);
    RequirePathLength({0x30, 0x03, 0x02, 0x01, 0xfb}, 0, 0x102, -5);
    RequirePathLength({0x30, 0x07, 0x01, 0x01, 0x01, 0x02, 0x02, 0x01, 0x00}, 1, 2, 256);

    BasicConstraintsValue* large = DecodeBasicConstraints(
        {0x30, 0x0e, 0x01, 0x01, 0xff, 0x02, 0x09, 0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88});
    Require(large->ca == 255 && large->pathLength != nullptr);
    Require(large->pathLength->type == 2 && large->pathLength->length == 8);
    const unsigned char magnitude[] = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88};
    Require(std::memcmp(large->pathLength->data, magnitude, sizeof(magnitude)) == 0);
    FreeBasicConstraints(large);
    Require(allocations.empty());

    RequireMalformed(87, {
        {{}, {102, 58}},
        {{0x30}, {102, 58}},
        {{0x30, 0xff}, {102, 58}},
        {{0x31, 0x00}, {168, 58}},
        {{0x70, 0x00}, {168, 58}},
        {{0x10, 0x00}, {149}},
        {{0x10, 0x80}, {102, 58}},
        {{0x30, 0x05, 0x01, 0x01}, {155, 58}},
        {{0x30, 0x02, 0x00, 0x00}, {159}},
        {{0x30, 0x03, 0x00, 0x00, 0x00}, {159}},
        {{0x30, 0x05, 0x01, 0x01, 0xff, 0x00, 0x00}, {159}},
        {{0x30, 0x01, 0x00}, {168, 58, 58}},
        {{0x30, 0x02, 0x05, 0x00}, {168, 58, 58}},
        {{0x30, 0x03, 0x81, 0x01, 0xff}, {168, 58, 58}},
        {{0x30, 0x01, 0x1f}, {102, 58, 58}},
        {{0x30, 0x01, 0x01}, {102, 58, 58}},
        {{0x30, 0x02, 0x01, 0x80}, {102, 58, 58}},
        {{0x30, 0x02, 0x01, 0x05}, {155, 58, 58}},
        {{0x30, 0x03, 0x21, 0x01, 0xff}, {231, 58}},
        {{0x30, 0x04, 0x21, 0x80, 0x00, 0x00}, {231, 58}},
        {{0x30, 0x02, 0x01, 0x00}, {106, 58}},
        {{0x30, 0x04, 0x01, 0x02, 0xff, 0xff}, {106, 58}},
        {{0x30, 0x06, 0x01, 0x01, 0xff, 0x01, 0x01, 0xff}, {168, 58, 58}},
        {{0x30, 0x06, 0x02, 0x01, 0x01, 0x01, 0x01, 0xff}, {148}},
        {{0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x02}, {148}},
        {{0x30, 0x08, 0x01, 0x01, 0xff, 0x02, 0x01, 0x03, 0x05, 0x00}, {148}},
        {{0x30, 0x08, 0x01, 0x01, 0xff, 0x02, 0x01, 0x03, 0x00, 0x00}, {148}},
        {{0x30, 0x05, 0x01, 0x01, 0xff, 0x02, 0x00}, {62, 58}},
        {{0x30, 0x07, 0x01, 0x01, 0xff, 0x02, 0x02, 0x00, 0x01}, {62, 58}},
        {{0x30, 0x05, 0x01, 0x01, 0xff, 0x22, 0x00}, {231, 58}},
        {{0x30, 0x05, 0x01, 0x01, 0xff, 0x02, 0x05}, {155, 58, 58}},
        {{0x30, 0x04, 0x01, 0x01, 0xff, 0x02}, {102, 58, 58}},
        {{0x30, 0x04, 0x01, 0x01, 0xff, 0x1f}, {102, 58, 58}},
    });
    RequireUnmodelled(87, {{0x30, 0x80, 0x00, 0x00}, {0x30, 0x80, 0x01, 0x01, 0xff, 0x00, 0x00}});

    const std::vector<std::vector<int>> failures =
        AllocationFailures(87, {0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x03}, FreeBasicConstraints);
    Require((failures == std::vector<std::vector<int>>{{65, 58}, {58}, {65, 65, 58}}));
    Require(AllocationFailures(87, {0x30, 0x00}, FreeBasicConstraints) == std::vector<std::vector<int>>{{65, 58}});
}

void RequireUsage(const LibreSslStack* usages, int index, const Bytes& encoding, int nid) {
    const auto* object = static_cast<const Asn1Object_nid_no_patch*>(sk_value_nid_postfix(usages, index));
    Require(object != nullptr && object->length == static_cast<int>(encoding.size()));
    Require(std::memcmp(object->data, encoding.data(), encoding.size()) == 0);
    Require(object->nid == 0 && object->flags == 0x09 && object->shortName == nullptr && object->longName == nullptr);
    if (nid >= 0) Require(OBJ_obj2nid_nid_postfix(reinterpret_cast<const LibreSslAsn1Object*>(object)) == nid);
}

LibreSslStack* DecodeExtendedKeyUsage(const Bytes& bytes, int count) {
    auto* usages = static_cast<LibreSslStack*>(DecodeValue(126, bytes));
    Require(usages != nullptr);
    RequireQueue({});
    Require(sk_num_nid_postfix(usages) == count && usages->sorted == 0 && usages->comp == nullptr);
    Require(usages->data != nullptr && usages->numAlloc > count);
    Require(allocations.size() == 2 + 2 * static_cast<std::size_t>(count));
    return usages;
}

void TestExtendedKeyUsage() {
    const Bytes serverAuth{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01};
    const Bytes clientAuth{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02};
    const Bytes codeSigning{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x03};
    const Bytes privateUsage{0x2a, 0x03, 0x04, 0x05};

    for (const Bytes& empty : {Bytes{0x30, 0x00}, Bytes{0x10, 0x00}, Bytes{0x30, 0x00, 0x5a}, Bytes{0x30, 0x81, 0x00}}) {
        LibreSslStack* usages = DecodeExtendedKeyUsage(empty, 0);
        FreeExtendedKeyUsage(usages);
        Require(allocations.empty());
    }
    for (const int identifier : {0x30, 0x10}) {
        LibreSslStack* usages = DecodeExtendedKeyUsage(
            Joined({Element(static_cast<unsigned char>(identifier), Element(0x06, privateUsage)), {0x5a}}), 1);
        RequireUsage(usages, 0, privateUsage, 0);
        FreeExtendedKeyUsage(usages);
        Require(allocations.empty());
    }

    const Bytes certificateValue{0x30, 0x24, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01, 0x06,
        0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05,
        0x07, 0x03, 0x03, 0x06, 0x04, 0x2a, 0x03, 0x04, 0x05};
    LibreSslStack* usages = DecodeExtendedKeyUsage(certificateValue, 4);
    RequireUsage(usages, 0, serverAuth, 129);
    RequireUsage(usages, 1, clientAuth, 130);
    RequireUsage(usages, 2, codeSigning, 131);
    RequireUsage(usages, 3, privateUsage, 0);
    Require(sk_value_nid_postfix(usages, 4) == nullptr);
    FreeExtendedKeyUsage(usages);
    Require(allocations.empty());

    Bytes content;
    for (unsigned char last = 1; last <= 9; ++last) {
        const Bytes element = Element(0x06, {0x2a, 0x03, last});
        content.insert(content.end(), element.begin(), element.end());
    }
    usages = DecodeExtendedKeyUsage(Element(0x30, content), 9);
    for (unsigned char last = 1; last <= 9; ++last) RequireUsage(usages, last - 1, {0x2a, 0x03, last}, -1);
    FreeExtendedKeyUsage(usages);
    Require(allocations.empty());

    RequireMalformed(126, {
        {{}, {102, 58}},
        {{0x30}, {102, 58}},
        {{0x31, 0x00}, {168, 58}},
        {{0x10, 0x80}, {102, 58}},
        {{0x30, 0x05, 0x06, 0x02}, {155, 58}},
        {{0x30, 0x02, 0x00, 0x00}, {159}},
        {{0x30, 0x06, 0x06, 0x02, 0x2a, 0x03, 0x00, 0x00}, {159}},
        {{0x30, 0x02, 0x05, 0x00}, {168, 58, 58}},
        {{0x30, 0x04, 0x86, 0x02, 0x2a, 0x03}, {168, 58, 58}},
        {{0x30, 0x06, 0x06, 0x02, 0x2a, 0x03, 0x04, 0x00}, {168, 58, 58}},
        {{0x30, 0x01, 0x06}, {102, 58, 58}},
        {{0x30, 0x01, 0x1f}, {102, 58, 58}},
        {{0x30, 0x02, 0x06, 0x80}, {102, 58, 58}},
        {{0x30, 0x02, 0x06, 0x05}, {155, 58, 58}},
        {{0x30, 0x05, 0x06, 0x02, 0x2a, 0x03, 0x00}, {168, 58, 58}},
        {{0x30, 0x02, 0x26, 0x00}, {231, 58}},
        {{0x30, 0x04, 0x26, 0x80, 0x00, 0x00}, {231, 58}},
        {{0x30, 0x02, 0x06, 0x00}, {216, 58}},
        {{0x30, 0x03, 0x06, 0x01, 0x80}, {216, 58}},
        {{0x30, 0x04, 0x06, 0x02, 0x2a, 0x83}, {216, 58}},
        {{0x30, 0x08, 0x06, 0x02, 0x2a, 0x03, 0x06, 0x02, 0x80, 0x01}, {216, 58}},
    });
    RequireUnmodelled(126, {{0x30, 0x80, 0x00, 0x00}, {0x30, 0x80, 0x06, 0x02, 0x2a, 0x03, 0x00, 0x00}});

    const std::vector<std::vector<int>> failures = AllocationFailures(126, certificateValue, FreeExtendedKeyUsage);
    int stackFailures = 0;
    int dataFailures = 0;
    int objectFailures = 0;
    for (const std::vector<int>& queue : failures) {
        if (queue == std::vector<int>{65}) ++stackFailures;
        else if (queue == std::vector<int>{58}) ++dataFailures;
        else if (queue == std::vector<int>{65, 58}) ++objectFailures;
        else Require(false);
    }
    Require(stackFailures >= 3 && dataFailures == 4 && objectFailures == 4);
    Require(failures.front() == std::vector<int>{65} && failures.back() == std::vector<int>{65});

    LibreSsl3X509Extension* built = X509V3_EXT_conf_nid_nid_postfix(nullptr, nullptr, 126, "serverAuth,clientAuth");
    Require(built != nullptr);
    usages = static_cast<LibreSslStack*>(X509V3_EXT_d2i_nid_postfix(built));
    Require(usages != nullptr && sk_num_nid_postfix(usages) == 2);
    RequireUsage(usages, 0, serverAuth, 129);
    RequireUsage(usages, 1, clientAuth, 130);
    FreeExtendedKeyUsage(usages);
    X509_EXTENSION_free_nid_postfix(built);
    Require(allocations.empty());
    RequireQueue({});
}

void RequireTime(const Asn1String* time, const Bytes& expected) {
    Require(time != nullptr && time->type == 24 && time->flags == 0);
    Require(time->length == static_cast<std::int32_t>(expected.size()));
    Require(std::memcmp(time->data, expected.data(), expected.size()) == 0 && time->data[expected.size()] == 0);
}

UsagePeriodValue* DecodeUsagePeriod(const Bytes& bytes) {
    auto* period = static_cast<UsagePeriodValue*>(DecodeValue(84, bytes));
    Require(period != nullptr);
    RequireQueue({});
    return period;
}

void TestPrivateKeyUsagePeriod() {
    static_assert(sizeof(UsagePeriodValue) == 16 && offsetof(UsagePeriodValue, notAfter) == 8);
    const Bytes start(ValidTime, ValidTime + sizeof(ValidTime));
    const Bytes end{'2', '0', '2', '5', '1', '2', '3', '1', '2', '3', '5', '9', '5', '9', 'Z'};
    const Bytes badMonth{'2', '0', '2', '4', '1', '3', '0', '2', '0', '3', '0', '4', '0', '5', 'Z'};
    const Bytes shortTime(ValidTime, ValidTime + sizeof(ValidTime) - 1);
    const Bytes notBefore = Element(0x80, start);
    const Bytes notAfter = Element(0x81, end);

    for (const Bytes& empty : {Bytes{0x30, 0x00}, Bytes{0x30, 0x00, 0x5a}, Bytes{0x30, 0x81, 0x00}}) {
        UsagePeriodValue* period = DecodeUsagePeriod(empty);
        Require(period->notBefore == nullptr && period->notAfter == nullptr && allocations.size() == 1);
        FreeUsagePeriod(period);
        Require(allocations.empty());
    }
    UsagePeriodValue* period = DecodeUsagePeriod(Joined({Element(0x30, Joined({notBefore, notAfter})), {0x5a}}));
    RequireTime(period->notBefore, start);
    RequireTime(period->notAfter, end);
    Require(allocations.size() == 5);
    FreeUsagePeriod(period);
    Require(allocations.empty());
    period = DecodeUsagePeriod(Element(0x30, notBefore));
    RequireTime(period->notBefore, start);
    Require(period->notAfter == nullptr && allocations.size() == 3);
    FreeUsagePeriod(period);
    period = DecodeUsagePeriod(Element(0x30, notAfter));
    RequireTime(period->notAfter, end);
    Require(period->notBefore == nullptr && allocations.size() == 3);
    FreeUsagePeriod(period);
    Require(allocations.empty());

    RequireMalformed(84, {
        {{}, {102, 58}},
        {{0x31, 0x00}, {168, 58}},
        {{0x10, 0x00}, {149}},
        {{0x30, 0x05, 0x80, 0x0f}, {155, 58}},
        {{0x30, 0x02, 0x00, 0x00}, {159}},
        {Element(0x30, Joined({notBefore, {0x00, 0x00}})), {159}},
        {Element(0x30, Joined({notAfter, notBefore})), {148}},
        {Element(0x30, Joined({notBefore, notAfter, {0x05, 0x00}})), {148}},
        {Element(0x30, Joined({notAfter, notAfter})), {148}},
        {Element(0x30, Joined({notBefore, notBefore})), {168, 58, 58}},
        {Element(0x30, Element(0x18, start)), {168, 58, 58}},
        {Element(0x30, Joined({notBefore, Element(0x18, end)})), {168, 58, 58}},
        {Element(0x30, Element(0x82, start)), {168, 58, 58}},
        {{0x30, 0x02, 0x05, 0x00}, {168, 58, 58}},
        {Element(0x30, Element(0x80, badMonth)), {132, 58}},
        {Element(0x30, Element(0x81, badMonth)), {132, 58}},
        {Element(0x30, Joined({notBefore, Element(0x81, shortTime)})), {132, 58}},
        {{0x30, 0x02, 0x80, 0x00}, {132, 58}},
        {{0x30, 0x01, 0x80}, {102, 58, 58}},
        {{0x30, 0x01, 0x9f}, {102, 58, 58}},
        {{0x30, 0x02, 0x80, 0x80}, {102, 58, 58}},
        {{0x30, 0x02, 0x81, 0x05}, {155, 58, 58}},
        {Element(0x30, Joined({notBefore, {0x81}})), {102, 58, 58}},
    });
    RequireUnmodelled(84, {
        {0x30, 0x80, 0x00, 0x00},
        {0x30, 0x02, 0xa0, 0x00},
        {0x30, 0x02, 0xa1, 0x00},
        Element(0x30, Joined({notBefore, {0xa1, 0x80, 0x00, 0x00}})),
    });

    const Bytes both = Element(0x30, Joined({notBefore, notAfter}));
    Require((AllocationFailures(84, both, FreeUsagePeriod) ==
        std::vector<std::vector<int>>{{65, 58}, {65, 65, 58}, {65, 58}, {65, 65, 58}, {65, 58}}));
}

void TestStructuredRoundTrips() {
    LibreSsl3X509Extension* built = X509V3_EXT_conf_nid_nid_postfix(nullptr, nullptr, 87, "DER:30060101FF020103");
    Require(built != nullptr);
    auto* constraints = static_cast<BasicConstraintsValue*>(X509V3_EXT_d2i_nid_postfix(built));
    Require(constraints != nullptr && constraints->ca == 255 && constraints->pathLength != nullptr);
    Require(ASN1_INTEGER_get_nid_postfix(constraints->pathLength) == 3);
    FreeBasicConstraints(constraints);
    X509_EXTENSION_free_nid_postfix(built);

    built = X509V3_EXT_conf_nid_nid_postfix(nullptr, nullptr, 84,
        "critical,DER:3022800F32303234303130323033303430355A810F32303235313233313233353935395A");
    Require(built != nullptr);
    auto* period = static_cast<UsagePeriodValue*>(X509V3_EXT_d2i_nid_postfix(built));
    Require(period != nullptr);
    RequireTime(period->notBefore, Bytes(ValidTime, ValidTime + sizeof(ValidTime)));
    RequireTime(period->notAfter, {'2', '0', '2', '5', '1', '2', '3', '1', '2', '3', '5', '9', '5', '9', 'Z'});
    FreeUsagePeriod(period);
    X509_EXTENSION_free_nid_postfix(built);
    Require(allocations.empty());
    RequireQueue({});
}

void TestNidResolution() {
    static unsigned char keyUsageOid[] = {0x55, 0x1d, 0x0f};
    Fixture resolved(0, keyUsageOid, 3, {0x03, 0x02, 0x05, 0xa0});
    auto* value = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&resolved.extension));
    Require(value != nullptr && value->type == 3 && value->length == 1 && value->data[0] == 0xa0);
    FreeString(value);
    static unsigned char privateOid[] = {0x2a, 0x03, 0x04, 0x05};
    Fixture unknown(0, privateOid, 4, {0x03, 0x02, 0x05, 0xa0});
    Require(X509V3_EXT_d2i_nid_postfix(&unknown.extension) == nullptr);
    RequireQueue({});
}

Bytes FromHex(std::string_view hex) {
    Bytes bytes;
    for (std::size_t index = 0; index + 1 < hex.size(); index += 2)
        bytes.push_back(static_cast<unsigned char>(std::stoul(std::string(hex.substr(index, 2)), nullptr, 16)));
    return bytes;
}

void FreeGeneralNames(void* value) {
    GENERAL_NAMES_free_nid_postfix(static_cast<LibreSslStack*>(value));
}

LibreSslStack* DecodeGeneralNames(int nid, const Bytes& bytes) {
    auto* names = static_cast<LibreSslStack*>(DecodeValue(nid, bytes));
    Require(names != nullptr);
    RequireQueue({});
    return names;
}

const LibreSslGeneralName* GeneralNameAt(const LibreSslStack* names, int index, int type) {
    const auto* name = static_cast<const LibreSslGeneralName*>(sk_value_nid_postfix(names, index));
    Require(name != nullptr && name->type == type && name->value != nullptr);
    return name;
}

void RequireString(const void* value, std::int32_t type, const Bytes& data) {
    const auto* string = static_cast<const Asn1String*>(value);
    Require(string->type == type && string->length == static_cast<std::int32_t>(data.size()));
    if (!data.empty()) Require(std::memcmp(string->data, data.data(), data.size()) == 0);
    Require(string->data[data.size()] == 0);
}

std::vector<std::vector<int>> DistinctQueues(std::vector<std::vector<int>> queues) {
    std::sort(queues.begin(), queues.end());
    queues.erase(std::unique(queues.begin(), queues.end()), queues.end());
    return queues;
}

void TestGeneralNames() {
    for (const Bytes& bytes : {FromHex("3000"), FromHex("30005a"), FromHex("1000")}) {
        LibreSslStack* names = DecodeGeneralNames(85, bytes);
        Require(sk_num_nid_postfix(names) == 0);
        FreeGeneralNames(names);
        Require(allocations.empty());
    }

    LibreSslStack* names = DecodeGeneralNames(85, FromHex("3014810178820261628601798704c000020188022a03"));
    Require(sk_num_nid_postfix(names) == 5);
    RequireString(GeneralNameAt(names, 0, 1)->value, 22, {'x'});
    RequireString(GeneralNameAt(names, 1, 2)->value, 22, {'a', 'b'});
    RequireString(GeneralNameAt(names, 2, 6)->value, 22, {'y'});
    RequireString(GeneralNameAt(names, 3, 7)->value, 4, {192, 0, 2, 1});
    const auto* object = static_cast<const Asn1Object_nid_no_patch*>(GeneralNameAt(names, 4, 8)->value);
    Require(object->length == 2 && object->flags == 0x09 && std::memcmp(object->data, "\x2a\x03", 2) == 0);
    FreeGeneralNames(names);
    Require(allocations.empty());

    names = DecodeGeneralNames(86, FromHex("3003820161"));
    RequireString(GeneralNameAt(names, 0, 2)->value, 22, {'a'});
    FreeGeneralNames(names);
    Require(allocations.empty());

    names = DecodeGeneralNames(85, FromHex("30088100870086008200"));
    Require(sk_num_nid_postfix(names) == 4);
    RequireString(GeneralNameAt(names, 0, 1)->value, 22, {});
    RequireString(GeneralNameAt(names, 1, 7)->value, 4, {});
    RequireString(GeneralNameAt(names, 2, 6)->value, 22, {});
    RequireString(GeneralNameAt(names, 3, 2)->value, 22, {});
    FreeGeneralNames(names);
    Require(allocations.empty());

    names = DecodeGeneralNames(85, FromHex("3002a300"));
    RequireString(GeneralNameAt(names, 0, 3)->value, 16, {0xa3, 0x00});
    FreeGeneralNames(names);
    names = DecodeGeneralNames(85, FromHex("3003a38100"));
    RequireString(GeneralNameAt(names, 0, 3)->value, 16, {0xa3, 0x81, 0x00});
    FreeGeneralNames(names);
    names = DecodeGeneralNames(85, FromHex("3004a3020500"));
    RequireString(GeneralNameAt(names, 0, 3)->value, 16, {0xa3, 0x02, 0x05, 0x00});
    FreeGeneralNames(names);
    Require(allocations.empty());

    const Bytes directory = FromHex("3011a40f300d310b300906035504030c024341");
    names = DecodeGeneralNames(85, directory);
    const auto* name = static_cast<const LibreSslX509Name*>(GeneralNameAt(names, 0, 4)->value);
    Require(sk_num_nid_postfix(name->entries) == 1);
    const auto* entry = static_cast<const LibreSslX509NameEntry*>(sk_value_nid_postfix(name->entries, 0));
    Require(entry != nullptr && entry->value != nullptr);
    RequireString(entry->value, 12, {'C', 'A'});
    FreeGeneralNames(names);
    Require(allocations.empty());

    names = DecodeGeneralNames(85, FromHex("3011a40f300d310b3009060355040313024341"));
    const auto* printable = static_cast<const LibreSslX509Name*>(GeneralNameAt(names, 0, 4)->value);
    entry = static_cast<const LibreSslX509NameEntry*>(sk_value_nid_postfix(printable->entries, 0));
    RequireString(entry->value, 19, {'C', 'A'});
    FreeGeneralNames(names);
    Require(allocations.empty());

    names = DecodeGeneralNames(85, FromHex("3004a4023000"));
    name = static_cast<const LibreSslX509Name*>(GeneralNameAt(names, 0, 4)->value);
    Require(sk_num_nid_postfix(name->entries) == 0);
    FreeGeneralNames(names);
    names = DecodeGeneralNames(85, FromHex("3004a4021000"));
    name = static_cast<const LibreSslX509Name*>(GeneralNameAt(names, 0, 4)->value);
    Require(sk_num_nid_postfix(name->entries) == 0);
    FreeGeneralNames(names);
    Require(allocations.empty());

    names = DecodeGeneralNames(85, FromHex("30138100a40f300d310b300906035504030c024341"));
    Require(sk_num_nid_postfix(names) == 2);
    RequireString(GeneralNameAt(names, 0, 1)->value, 22, {});
    FreeGeneralNames(names);
    Require(allocations.empty());

    names = DecodeGeneralNames(85, FromHex("3002a200"));
    Require(sk_num_nid_postfix(names) == 1);
    RequireString(GeneralNameAt(names, 0, 2)->value, 22, {});
    FreeGeneralNames(names);
    for (const std::string_view hex : {"3007a2050403612e62", "300ba20924040402612e040162",
             "3009a2800403612e620000"}) {
        names = DecodeGeneralNames(85, FromHex(hex));
        Require(sk_num_nid_postfix(names) == 1);
        RequireString(GeneralNameAt(names, 0, 2)->value, 22, {'a', '.', 'b'});
        FreeGeneralNames(names);
    }
    names = DecodeGeneralNames(85, FromHex("3007a2800000820178"));
    Require(sk_num_nid_postfix(names) == 2);
    RequireString(GeneralNameAt(names, 0, 2)->value, 22, {});
    RequireString(GeneralNameAt(names, 1, 2)->value, 22, {'x'});
    FreeGeneralNames(names);
    for (const std::string_view hex : {"3008a203040161820162", "3008a206240304026162"}) {
        names = DecodeGeneralNames(85, FromHex(hex));
        Require(sk_num_nid_postfix(names) == 1);
        RequireString(GeneralNameAt(names, 0, 2)->value, 22, {'a', 'b'});
        FreeGeneralNames(names);
    }
    names = DecodeGeneralNames(85, FromHex("3004a1028000"));
    Require(sk_num_nid_postfix(names) == 1);
    RequireString(GeneralNameAt(names, 0, 1)->value, 22, {});
    FreeGeneralNames(names);
    names = DecodeGeneralNames(85, FromHex("3004a7028000"));
    Require(sk_num_nid_postfix(names) == 1);
    RequireString(GeneralNameAt(names, 0, 7)->value, 4, {});
    FreeGeneralNames(names);
    names = DecodeGeneralNames(85, FromHex("300ea20c240a24082406240424020400"));
    Require(sk_num_nid_postfix(names) == 1);
    RequireString(GeneralNameAt(names, 0, 2)->value, 22, {});
    FreeGeneralNames(names);
    names = DecodeGeneralNames(85, FromHex("3089000000000000000003820161"));
    Require(sk_num_nid_postfix(names) == 1);
    RequireString(GeneralNameAt(names, 0, 2)->value, 22, {'a'});
    FreeGeneralNames(names);
    Require(allocations.empty());

    RequireMalformed(85, {
        {{}, {102, 58}},
        {FromHex("30ff"), {102, 58}},
        {FromHex("3100"), {168, 58}},
        {FromHex("0000"), {168, 58}},
        {FromHex("30020000"), {159}},
        {FromHex("3005820261"), {155, 58}},
        {FromHex("30011f"), {102, 58, 58, 58, 58}},
        {FromHex("30029f80"), {102, 58, 58, 58, 58}},
        {FromHex("30028900"), {143, 58}},
        {FromHex("3003020100"), {143, 58}},
        {FromHex("30039f810100"), {143, 58}},
        {FromHex("30023000"), {143, 58}},
        {FromHex("30020500"), {143, 58}},
        {FromHex("30028a00"), {143, 58}},
        {FromHex("300281ff"), {102, 58, 58, 58, 58}},
        {FromHex("30028180"), {102, 58, 58, 58, 58}},
        {FromHex("30028201"), {155, 58, 58, 58, 58}},
        {FromHex("3004a2020000"), {159, 58, 58, 58}},
        {FromHex("3002a280"), {137, 58, 58, 58}},
        {FromHex("3005a203040561"), {155, 58, 58, 58, 58}},
        {FromHex("300da2802404040261620401630000"), {159, 58, 58, 58}},
        {FromHex("3003a2011f"), {102, 58, 58, 58, 58}},
        {FromHex("3010a20e240c240a24082406240424020400"), {197, 58, 58, 58}},
        {FromHex("3003830161"), {156, 58, 58, 58}},
        {FromHex("30028800"), {216, 58, 58, 58}},
        {FromHex("300488028001"), {216, 58, 58, 58}},
        {FromHex("3005a803060101"), {231, 58, 58, 58}},
        {FromHex("3003800100"), {149, 58, 58, 58}},
        {FromHex("3003850100"), {149, 58, 58, 58}},
        {FromHex("30028400"), {120, 58, 58}},
        {FromHex("3002a400"), {102, 58, 58, 58, 58, 58}},
        {FromHex("3012a410300d310b300906035504030c02434100"), {148, 58, 58}},
        {FromHex("3013a411300d310b300906035504030c0243410500"), {148, 58, 58}},
        {FromHex("3004a4023100"), {168, 58, 58, 58, 58, 58}},
        {FromHex("3003a40100"), {168, 58, 58, 58, 58, 58}},
        {FromHex("3004a4023005"), {155, 58, 58, 58, 58, 58}},
        {FromHex("3002a405"), {155, 58, 58, 58}},
        {FromHex("30138100840f300d310b300906035504030c024341"), {120, 58, 58}},
    });

    for (const auto& [hex, reason] : {std::pair<std::string_view, int>{"3006a40430023000", 168},
             {"300ba409300731053003060100", 121}, {"3009a40730053103020100", 168}}) {
        Require(DecodeValue(85, FromHex(hex)) == nullptr);
        Require(allocations.empty());
        const std::vector<int> queue = DrainReasons();
        Require(!queue.empty() && queue.front() == reason);
    }

    RequireUnmodelled(85, {
        FromHex("300f810178a00a06032a0304a003160178"),
        FromHex("3008a506a1040c026162"),
        FromHex("3004a3800000"),
        FromHex("3004a4800000"),
        FromHex("30800000"),
    });

    Require(DistinctQueues(AllocationFailures(85, FromHex("3003820161"), FreeGeneralNames)) ==
        std::vector<std::vector<int>>{{65}, {65, 58, 58}, {65, 58, 58, 58}, {65, 65, 58, 58, 58}});
    Require(DistinctQueues(AllocationFailures(85, FromHex("3014810178820261628601798704c000020188022a03"), FreeGeneralNames)) ==
        std::vector<std::vector<int>>{{58, 58, 58}, {65}, {65, 58, 58}, {65, 58, 58, 58}, {65, 65, 58, 58, 58}});
    Require(DistinctQueues(AllocationFailures(85, FromHex("300488022a03"), FreeGeneralNames)) ==
        std::vector<std::vector<int>>{{58, 58, 58}, {65}, {65, 58, 58}, {65, 58, 58, 58}});
    Require(DistinctQueues(AllocationFailures(85, FromHex("3002a300"), FreeGeneralNames)) ==
        std::vector<std::vector<int>>{{65}, {65, 58, 58}, {65, 58, 58, 58}, {65, 65, 58, 58, 58}});
    const std::vector<std::vector<int>> directoryFailures = AllocationFailures(85, directory, FreeGeneralNames);
    Require(!directoryFailures.empty());
    for (const std::vector<int>& queue : directoryFailures) Require(!queue.empty());
}

struct AccessDescription {
    Asn1Object_nid_no_patch* method;
    LibreSslGeneralName* location;
};

void FreeAuthorityInfoAccess(void* value) {
    sk_pop_free_nid_postfix(static_cast<LibreSslStack*>(value), +[](void* pointer) APS5_VABI {
        auto* description = static_cast<AccessDescription*>(pointer);
        if (!description) return;
        GENERAL_NAME_free_nid_postfix(description->location);
        ASN1_OBJECT_free_nid_postfix(description->method);
        free_nid_postfix(description);
    });
}

LibreSslStack* DecodeAuthorityInfoAccess(int nid, const Bytes& bytes) {
    auto* info = static_cast<LibreSslStack*>(DecodeValue(nid, bytes));
    Require(info != nullptr);
    RequireQueue({});
    return info;
}

const AccessDescription* AccessDescriptionAt(const LibreSslStack* info, int index, const Bytes& methodEncoding, int methodNid,
    int locationType) {
    const auto* description = static_cast<const AccessDescription*>(sk_value_nid_postfix(info, index));
    Require(description != nullptr && description->method != nullptr && description->location != nullptr);
    Require(description->method->length == static_cast<int>(methodEncoding.size()));
    Require(std::memcmp(description->method->data, methodEncoding.data(), methodEncoding.size()) == 0);
    Require(description->method->nid == 0 && description->method->flags == 0x09);
    Require(OBJ_obj2nid_nid_postfix(reinterpret_cast<const LibreSslAsn1Object*>(description->method)) == methodNid);
    Require(description->location->type == locationType && description->location->value != nullptr);
    return description;
}

void TestAuthorityInfoAccess() {
    static_assert(sizeof(AccessDescription) == 16 && offsetof(AccessDescription, location) == 8);
    for (const Bytes& empty : {FromHex("3000"), FromHex("30005a"), FromHex("1000"), FromHex("308100")}) {
        LibreSslStack* info = DecodeAuthorityInfoAccess(177, empty);
        Require(sk_num_nid_postfix(info) == 0);
        FreeAuthorityInfoAccess(info);
        Require(allocations.empty());
    }
    {
        LibreSslStack* info = DecodeAuthorityInfoAccess(398, FromHex("3000"));
        Require(sk_num_nid_postfix(info) == 0);
        FreeAuthorityInfoAccess(info);
        Require(allocations.empty());
    }

    const Bytes ocspMethod{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01};
    const Bytes caIssuersMethod{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x02};
    const Bytes caRepositoryMethod{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x05};
    const Bytes privateMethod{0x2a, 0x03};

    const Bytes singleAia = FromHex("3010300e06082b0601050507300186026162");
    LibreSslStack* info = DecodeAuthorityInfoAccess(177, singleAia);
    Require(sk_num_nid_postfix(info) == 1);
    RequireString(AccessDescriptionAt(info, 0, ocspMethod, 178, 6)->location->value, 22, {'a', 'b'});
    FreeAuthorityInfoAccess(info);
    Require(allocations.empty());

    const Bytes singleSia = FromHex("3010300e06082b0601050507300586026364");
    info = DecodeAuthorityInfoAccess(398, singleSia);
    Require(sk_num_nid_postfix(info) == 1);
    RequireString(AccessDescriptionAt(info, 0, caRepositoryMethod, 785, 6)->location->value, 22, {'c', 'd'});
    FreeAuthorityInfoAccess(info);
    Require(allocations.empty());

    const Bytes multiAia = FromHex(
        "303a300f06082b060105050730028603757269300906022a038203646e73300a06022a038704c0000201"
        "300806022a0388022a03300606022a03a300");
    info = DecodeAuthorityInfoAccess(177, multiAia);
    Require(sk_num_nid_postfix(info) == 5);
    RequireString(AccessDescriptionAt(info, 0, caIssuersMethod, 179, 6)->location->value, 22, {'u', 'r', 'i'});
    RequireString(AccessDescriptionAt(info, 1, privateMethod, 0, 2)->location->value, 22, {'d', 'n', 's'});
    RequireString(AccessDescriptionAt(info, 2, privateMethod, 0, 7)->location->value, 4, {192, 0, 2, 1});
    const auto* rid = static_cast<const Asn1Object_nid_no_patch*>(
        AccessDescriptionAt(info, 3, privateMethod, 0, 8)->location->value);
    Require(rid->length == 2 && rid->flags == 0x09 && std::memcmp(rid->data, "\x2a\x03", 2) == 0);
    RequireString(AccessDescriptionAt(info, 4, privateMethod, 0, 3)->location->value, 16, {0xa3, 0x00});
    FreeAuthorityInfoAccess(info);
    Require(allocations.empty());

    const Bytes dirAia = FromHex("3017301506022a03a40f300d310b300906035504030c024341");
    info = DecodeAuthorityInfoAccess(177, dirAia);
    Require(sk_num_nid_postfix(info) == 1);
    const auto* dirName = static_cast<const LibreSslX509Name*>(
        AccessDescriptionAt(info, 0, privateMethod, 0, 4)->location->value);
    Require(sk_num_nid_postfix(dirName->entries) == 1);
    const auto* entry = static_cast<const LibreSslX509NameEntry*>(sk_value_nid_postfix(dirName->entries, 0));
    Require(entry != nullptr && entry->value != nullptr);
    RequireString(entry->value, 12, {'C', 'A'});
    FreeAuthorityInfoAccess(info);
    Require(allocations.empty());

    for (const int nid : {177, 398}) {
        info = DecodeAuthorityInfoAccess(nid, FromHex("300a300806022a03a1028000"));
        Require(sk_num_nid_postfix(info) == 1);
        RequireString(AccessDescriptionAt(info, 0, privateMethod, 0, 1)->location->value, 22, {});
        FreeAuthorityInfoAccess(info);
        Require(allocations.empty());
    }

    info = DecodeAuthorityInfoAccess(177, FromHex("301a300e06082b0601050507300186026162300806022a03a1028000"));
    Require(sk_num_nid_postfix(info) == 2);
    RequireString(AccessDescriptionAt(info, 0, ocspMethod, 178, 6)->location->value, 22, {'a', 'b'});
    RequireString(AccessDescriptionAt(info, 1, privateMethod, 0, 1)->location->value, 22, {});
    FreeAuthorityInfoAccess(info);
    Require(allocations.empty());

    for (const std::string_view hex : {
             "3017301506082b06010505073001a609240404026162040163",
             "3017301506082b06010505073001a680040261620401630000"}) {
        info = DecodeAuthorityInfoAccess(177, FromHex(hex));
        Require(sk_num_nid_postfix(info) == 1);
        RequireString(AccessDescriptionAt(info, 0, ocspMethod, 178, 6)->location->value, 22, {'a', 'b', 'c'});
        FreeAuthorityInfoAccess(info);
        Require(allocations.empty());
    }

    info = DecodeAuthorityInfoAccess(177, FromHex("3089000000000000000010300e06082b0601050507300186026162"));
    Require(sk_num_nid_postfix(info) == 1);
    RequireString(AccessDescriptionAt(info, 0, ocspMethod, 178, 6)->location->value, 22, {'a', 'b'});
    FreeAuthorityInfoAccess(info);
    Require(allocations.empty());

    static unsigned char aiaOid[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01};
    Fixture resolvedAia(0, aiaOid, sizeof(aiaOid), {0x30, 0x10, 0x30, 0x0e, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07,
        0x30, 0x01, 0x86, 0x02, 'a', 'b'});
    info = static_cast<LibreSslStack*>(X509V3_EXT_d2i_nid_postfix(&resolvedAia.extension));
    Require(info != nullptr && sk_num_nid_postfix(info) == 1);
    RequireString(AccessDescriptionAt(info, 0, ocspMethod, 178, 6)->location->value, 22, {'a', 'b'});
    FreeAuthorityInfoAccess(info);

    static unsigned char siaOid[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x0b};
    Fixture resolvedSia(0, siaOid, sizeof(siaOid), {0x30, 0x10, 0x30, 0x0e, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07,
        0x30, 0x05, 0x86, 0x02, 'c', 'd'});
    info = static_cast<LibreSslStack*>(X509V3_EXT_d2i_nid_postfix(&resolvedSia.extension));
    Require(info != nullptr && sk_num_nid_postfix(info) == 1);
    RequireString(AccessDescriptionAt(info, 0, caRepositoryMethod, 785, 6)->location->value, 22, {'c', 'd'});
    FreeAuthorityInfoAccess(info);

    struct ConfCase {
        int nid;
        const char* value;
        Bytes method;
        int methodNid;
        Bytes expected;
    };
    for (const ConfCase& item : {
             ConfCase{177, "DER:3010300e06082b0601050507300186026162", ocspMethod, 178, {'a', 'b'}},
             ConfCase{398, "DER:3010300e06082b0601050507300586026364", caRepositoryMethod, 785, {'c', 'd'}}}) {
        LibreSsl3X509Extension* built = X509V3_EXT_conf_nid_nid_postfix(nullptr, nullptr, item.nid, item.value);
        Require(built != nullptr);
        info = static_cast<LibreSslStack*>(X509V3_EXT_d2i_nid_postfix(built));
        Require(info != nullptr && sk_num_nid_postfix(info) == 1);
        RequireString(AccessDescriptionAt(info, 0, item.method, item.methodNid, 6)->location->value, 22, item.expected);
        FreeAuthorityInfoAccess(info);
        X509_EXTENSION_free_nid_postfix(built);
    }
    Require(allocations.empty());
    RequireQueue({});

    const std::vector<MalformedValue> malformed{
        {{}, {102, 58}},
        {FromHex("30"), {102, 58}},
        {FromHex("3100"), {168, 58}},
        {FromHex("0000"), {168, 58}},
        {FromHex("1080"), {102, 58}},
        {FromHex("30053000"), {155, 58}},
        {FromHex("30020000"), {159}},
        {FromHex("30023100"), {168, 58, 58}},
        {FromHex("30020500"), {168, 58, 58}},
        {FromHex("30021000"), {149, 58}},
        {FromHex("300130"), {102, 58, 58}},
        {FromHex("30011f"), {102, 58, 58}},
        {FromHex("300230ff"), {102, 58, 58}},
        {FromHex("30023005"), {155, 58, 58}},
        {FromHex("30023000"), {121, 58}},
        {FromHex("300430020000"), {159, 58}},
        {FromHex("3008300606042b060105"), {121, 58}},
        {FromHex("300a300806042b0601050000"), {159, 58}},
        {FromHex("3012300e06082b06010505073001860261620000"), {159}},
        {FromHex("300430020500"), {168, 58, 58, 58}},
        {FromHex("300430028600"), {168, 58, 58, 58}},
        {FromHex("3003300106"), {102, 58, 58, 58}},
        {FromHex("300330011f"), {102, 58, 58, 58}},
        {FromHex("300430020680"), {102, 58, 58, 58}},
        {FromHex("3004300206ff"), {102, 58, 58, 58}},
        {FromHex("300430020605"), {155, 58, 58, 58}},
        {FromHex("300430022600"), {231, 58, 58}},
        {FromHex("300430020600"), {216, 58, 58}},
        {FromHex("3006300406028001"), {216, 58, 58}},
        {FromHex("30053003060180"), {216, 58, 58}},
        {FromHex("3006300406022a83"), {216, 58, 58}},
        {FromHex("300a300806042b0601058900"), {143, 58, 58}},
        {FromHex("3009300706042b0601051f"), {102, 58, 58, 58, 58, 58}},
        {FromHex("300a300806042b0601058201"), {155, 58, 58, 58, 58, 58}},
        {FromHex("300b300906042b060105830161"), {156, 58, 58, 58, 58}},
        {FromHex("300a300806042b0601058400"), {120, 58, 58, 58}},
        {FromHex("300a300806042b060105a400"), {102, 58, 58, 58, 58, 58, 58}},
        {FromHex("301a301806042b060105a410300d310b300906035504030c02434100"), {148, 58, 58, 58}},
        {FromHex("300a300806042b0601058800"), {216, 58, 58, 58, 58}},
        {FromHex("300d300b06042b060105a803060101"), {231, 58, 58, 58, 58}},
        {FromHex("300b300906042b060105800100"), {149, 58, 58, 58, 58}},
        {FromHex("300b300906042b060105850100"), {149, 58, 58, 58, 58}},
        {FromHex("300d300b06042b0601058601790500"), {148, 58}},
        {FromHex("300d300b06042b0601058601790000"), {148, 58}},
        {FromHex("3018300e06082b0601050507300186026162300606022a038900"), {143, 58, 58}},
    };
    RequireMalformed(177, malformed);
    RequireMalformed(398, malformed);

    const std::vector<Bytes> unmodelled{
        FromHex("30800000"),
        FromHex("30023080"),
        FromHex("300430800000"),
        FromHex("3012301006022a03a00a06032a0304a003160178"),
        FromHex("300e300c06022a03a506a1040c026162"),
        FromHex("300a300806022a03a3800000"),
    };
    RequireUnmodelled(177, unmodelled);
    RequireUnmodelled(398, unmodelled);

    Require(DistinctQueues(AllocationFailures(177, FromHex("3000"), FreeAuthorityInfoAccess)) ==
        std::vector<std::vector<int>>{{65}});
    Require(DistinctQueues(AllocationFailures(177, singleAia, FreeAuthorityInfoAccess)) ==
        std::vector<std::vector<int>>{{58, 58}, {65}, {65, 58, 58}, {65, 58, 58, 58}, {65, 58, 58, 58, 58},
            {65, 65, 58, 58, 58, 58}});
    Require(DistinctQueues(AllocationFailures(177, multiAia, FreeAuthorityInfoAccess)) ==
        std::vector<std::vector<int>>{{58, 58}, {58, 58, 58, 58}, {65}, {65, 58, 58}, {65, 58, 58, 58},
            {65, 58, 58, 58, 58}, {65, 65, 58, 58, 58, 58}});
    const std::vector<std::vector<int>> directoryFailures = AllocationFailures(177, dirAia, FreeAuthorityInfoAccess);
    Require(!directoryFailures.empty());
    for (const std::vector<int>& queue : directoryFailures) Require(!queue.empty());
}

struct DistPointName {
    int type;
    union {
        LibreSslStack* fullname;
        LibreSslStack* relativename;
    } name;
    LibreSslX509Name* dpname;
};

struct DistPoint {
    DistPointName* distpoint;
    Asn1String* reasons;
    LibreSslStack* crlIssuer;
    int dpReasons;
};

void FreeNameEntry(LibreSslX509NameEntry* entry) {
    if (!entry) return;
    ASN1_OBJECT_free_nid_postfix(static_cast<Asn1Object_nid_no_patch*>(entry->object));
    ASN1_OCTET_STRING_free_nid_postfix(static_cast<Asn1String*>(entry->value));
    free_nid_postfix(entry);
}

void FreeRelativeName(LibreSslStack* entries) {
    if (!entries) return;
    for (int index = 0; index < entries->num; ++index)
        FreeNameEntry(reinterpret_cast<LibreSslX509NameEntry*>(entries->data[index]));
    sk_free_nid_postfix(entries);
}

void FreeCrlDistPoints(void* value) {
    sk_pop_free_nid_postfix(static_cast<LibreSslStack*>(value), +[](void* pointer) APS5_VABI {
        auto* point = static_cast<DistPoint*>(pointer);
        if (!point) return;
        if (point->distpoint) {
            if (point->distpoint->type == 0) GENERAL_NAMES_free_nid_postfix(point->distpoint->name.fullname);
            else if (point->distpoint->type == 1) FreeRelativeName(point->distpoint->name.relativename);
            free_nid_postfix(point->distpoint);
        }
        ASN1_OCTET_STRING_free_nid_postfix(point->reasons);
        GENERAL_NAMES_free_nid_postfix(point->crlIssuer);
        free_nid_postfix(point);
    });
}

LibreSslStack* DecodeCrlDistPoints(int nid, const Bytes& bytes) {
    auto* points = static_cast<LibreSslStack*>(DecodeValue(nid, bytes));
    Require(points != nullptr);
    RequireQueue({});
    return points;
}

const DistPoint* DistPointAt(const LibreSslStack* points, int index) {
    const auto* point = static_cast<const DistPoint*>(sk_value_nid_postfix(points, index));
    Require(point != nullptr && point->dpReasons == 0);
    return point;
}

const LibreSslX509NameEntry* NameEntryAt(const LibreSslStack* entries, int index, const Bytes& oidEncoding, int oidNid,
    std::int32_t valueType, const Bytes& valueData, std::int64_t valueFlags = 0) {
    const auto* entry = static_cast<const LibreSslX509NameEntry*>(sk_value_nid_postfix(entries, index));
    Require(entry != nullptr && entry->object != nullptr && entry->value != nullptr);
    Require(entry->set == 0 && entry->size == 0);
    const auto* object = static_cast<const Asn1Object_nid_no_patch*>(entry->object);
    Require(object->length == static_cast<int>(oidEncoding.size()));
    Require(std::memcmp(object->data, oidEncoding.data(), oidEncoding.size()) == 0);
    Require(object->nid == 0 && object->flags == 0x09);
    Require(OBJ_obj2nid_nid_postfix(reinterpret_cast<const LibreSslAsn1Object*>(object)) == oidNid);
    const auto* string = static_cast<const Asn1String*>(entry->value);
    Require(string->type == valueType && string->flags == valueFlags);
    Require(string->length == static_cast<std::int32_t>(valueData.size()));
    if (!valueData.empty()) Require(std::memcmp(string->data, valueData.data(), valueData.size()) == 0);
    return entry;
}

void TestCrlDistPoints() {
    static_assert(sizeof(DistPointName) == 24 && offsetof(DistPointName, name) == 8 && offsetof(DistPointName, dpname) == 16);
    static_assert(sizeof(DistPoint) == 32 && offsetof(DistPoint, reasons) == 8 && offsetof(DistPoint, crlIssuer) == 16 &&
        offsetof(DistPoint, dpReasons) == 24);

    for (const int nid : {103, 857}) {
        for (const Bytes& empty : {FromHex("3000"), FromHex("1000"), FromHex("308100"), FromHex("30005a")}) {
            LibreSslStack* points = DecodeCrlDistPoints(nid, empty);
            Require(sk_num_nid_postfix(points) == 0);
            FreeCrlDistPoints(points);
            Require(allocations.empty());
        }
    }

    {
        LibreSslStack* points = DecodeCrlDistPoints(103, FromHex("30023000"));
        Require(sk_num_nid_postfix(points) == 1);
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint == nullptr && dp->reasons == nullptr && dp->crlIssuer == nullptr);
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }

    const Bytes singleFullname = FromHex("300c300aa008a006860474657374");
    for (const int nid : {103, 857}) {
        LibreSslStack* points = DecodeCrlDistPoints(nid, singleFullname);
        Require(sk_num_nid_postfix(points) == 1);
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 0 && dp->distpoint->dpname == nullptr);
        Require(dp->reasons == nullptr && dp->crlIssuer == nullptr);
        Require(sk_num_nid_postfix(dp->distpoint->name.fullname) == 1);
        RequireString(GeneralNameAt(dp->distpoint->name.fullname, 0, 6)->value, 22, {'t', 'e', 's', 't'});
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }

    for (const Bytes& emptyFullname : {FromHex("30063004a002a000"), FromHex("30063004a0028000")}) {
        LibreSslStack* points = DecodeCrlDistPoints(103, emptyFullname);
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 0 && dp->distpoint->dpname == nullptr);
        Require(sk_num_nid_postfix(dp->distpoint->name.fullname) == 0);
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }
    {
        LibreSslStack* points = DecodeCrlDistPoints(103, FromHex("300a3008a006800486026162"));
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 0 && dp->distpoint->dpname == nullptr);
        Require(sk_num_nid_postfix(dp->distpoint->name.fullname) == 1);
        RequireString(GeneralNameAt(dp->distpoint->name.fullname, 0, 6)->value, 22, {'a', 'b'});
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }

    const Bytes cnOid{0x55, 0x04, 0x03};
    const Bytes ouOid{0x55, 0x04, 0x0b};
    const Bytes privateOid{0x2a, 0x03};
    const Bytes singleRelative = FromHex("3011300fa00da10b3009060355040313024341");
    {
        LibreSslStack* points = DecodeCrlDistPoints(103, singleRelative);
        Require(sk_num_nid_postfix(points) == 1);
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 1 && dp->distpoint->dpname == nullptr);
        Require(dp->reasons == nullptr && dp->crlIssuer == nullptr);
        Require(sk_num_nid_postfix(dp->distpoint->name.relativename) == 1);
        NameEntryAt(dp->distpoint->name.relativename, 0, cnOid, 13, 19, {'C', 'A'});
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }
    for (const Bytes& emptyRelative : {FromHex("30063004a002a100"), FromHex("30063004a0028100")}) {
        LibreSslStack* points = DecodeCrlDistPoints(103, emptyRelative);
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 1 && dp->distpoint->dpname == nullptr);
        Require(sk_num_nid_postfix(dp->distpoint->name.relativename) == 0);
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }
    {
        LibreSslStack* points = DecodeCrlDistPoints(103, FromHex("300f300da00b8109300706022a03130141"));
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 1 && dp->distpoint->dpname == nullptr);
        Require(sk_num_nid_postfix(dp->distpoint->name.relativename) == 1);
        NameEntryAt(dp->distpoint->name.relativename, 0, privateOid, 0, 19, {'A'});
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }
    {
        LibreSslStack* points = DecodeCrlDistPoints(103, FromHex(
            "30323030a02ea12c300906035504030c024341300806022a0316026162300906022a03030305a000300a06022a031e0400410042"));
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 1 && dp->distpoint->dpname == nullptr);
        const LibreSslStack* entries = dp->distpoint->name.relativename;
        Require(sk_num_nid_postfix(entries) == 4);
        NameEntryAt(entries, 0, cnOid, 13, 12, {'C', 'A'});
        NameEntryAt(entries, 1, privateOid, 0, 22, {'a', 'b'});
        NameEntryAt(entries, 2, privateOid, 0, 3, {0xa0, 0x00}, 13);
        NameEntryAt(entries, 3, privateOid, 0, 30, {0x00, 'A', 0x00, 'B'});
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }
    {
        LibreSslStack* points = DecodeCrlDistPoints(103,
            FromHex("301a3018a016a114300a06022a031c0400000041300606022a033000"));
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 1 && dp->distpoint->dpname == nullptr);
        Require(dp->reasons == nullptr && dp->crlIssuer == nullptr);
        const LibreSslStack* entries = dp->distpoint->name.relativename;
        Require(sk_num_nid_postfix(entries) == 2);
        NameEntryAt(entries, 0, privateOid, 0, 28, {0x00, 0x00, 0x00, 'A'});
        NameEntryAt(entries, 1, privateOid, 0, 16, {0x30, 0x00});
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }
    for (const auto& [hex, valueType, data] : {
             std::tuple<std::string_view, std::int32_t, Bytes>{"300f300da00ba109300706022a030a0101", 10, {0x01}},
             {"300f300da00ba109300706022a030a01fe", 0x10a, {0x02}},
             {"300f300da00ba109300706022a030d0101", 13, {0x01}},
             {"300f300da00ba109300706022a03070141", 7, {'A'}},
         }) {
        LibreSslStack* points = DecodeCrlDistPoints(103, FromHex(hex));
        Require(sk_num_nid_postfix(points) == 1);
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 1 && dp->distpoint->dpname == nullptr);
        Require(dp->reasons == nullptr && dp->crlIssuer == nullptr);
        const LibreSslStack* entries = dp->distpoint->name.relativename;
        Require(sk_num_nid_postfix(entries) == 1);
        NameEntryAt(entries, 0, privateOid, 0, valueType, data);
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }
    {
        LibreSslStack* points = DecodeCrlDistPoints(
            103, FromHex("301c301aa018a11630090603550403130243413009060355040b13024954"));
        Require(sk_num_nid_postfix(points) == 1);
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 1 && dp->distpoint->dpname == nullptr);
        Require(dp->reasons == nullptr && dp->crlIssuer == nullptr);
        const LibreSslStack* entries = dp->distpoint->name.relativename;
        Require(sk_num_nid_postfix(entries) == 2);
        NameEntryAt(entries, 0, cnOid, 13, 19, {'C', 'A'});
        NameEntryAt(entries, 1, ouOid, 18, 19, {'I', 'T'});
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }

    {
        LibreSslStack* points = DecodeCrlDistPoints(103, FromHex("30063004810205a0"));
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint == nullptr && dp->reasons != nullptr && dp->crlIssuer == nullptr);
        Require(dp->reasons->length == 1 && dp->reasons->type == 3 && dp->reasons->flags == 13);
        Require(dp->reasons->data[0] == 0xa0);
        FreeCrlDistPoints(points);

        points = DecodeCrlDistPoints(103, FromHex("30053003810100"));
        dp = DistPointAt(points, 0);
        Require(dp->reasons != nullptr && dp->reasons->length == 0 && dp->reasons->type == 3 && dp->reasons->flags == 8);
        FreeCrlDistPoints(points);

        points = DecodeCrlDistPoints(103, FromHex("30063004810207ff"));
        dp = DistPointAt(points, 0);
        Require(dp->reasons != nullptr && dp->reasons->length == 1 && dp->reasons->type == 3 && dp->reasons->flags == 15);
        Require(dp->reasons->data[0] == 0x80);
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }

    {
        LibreSslStack* points = DecodeCrlDistPoints(103, FromHex("300c300aa2088202616286026364"));
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint == nullptr && dp->reasons == nullptr && dp->crlIssuer != nullptr);
        Require(sk_num_nid_postfix(dp->crlIssuer) == 2);
        RequireString(GeneralNameAt(dp->crlIssuer, 0, 2)->value, 22, {'a', 'b'});
        RequireString(GeneralNameAt(dp->crlIssuer, 1, 6)->value, 22, {'c', 'd'});
        FreeCrlDistPoints(points);

        for (const Bytes& emptyIssuer : {FromHex("30043002a200"), FromHex("300430028200")}) {
            points = DecodeCrlDistPoints(103, emptyIssuer);
            dp = DistPointAt(points, 0);
            Require(dp->crlIssuer != nullptr && sk_num_nid_postfix(dp->crlIssuer) == 0);
            FreeCrlDistPoints(points);
        }

        points = DecodeCrlDistPoints(103, FromHex("3006300482028600"));
        dp = DistPointAt(points, 0);
        Require(dp->crlIssuer != nullptr && sk_num_nid_postfix(dp->crlIssuer) == 1);
        RequireString(GeneralNameAt(dp->crlIssuer, 0, 6)->value, 22, {});
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }

    for (const int nid : {103, 857}) {
        LibreSslStack* points = DecodeCrlDistPoints(nid, FromHex("30083006a204a1028000"));
        Require(sk_num_nid_postfix(points) == 1);
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint == nullptr && dp->reasons == nullptr && dp->crlIssuer != nullptr);
        Require(sk_num_nid_postfix(dp->crlIssuer) == 1);
        RequireString(GeneralNameAt(dp->crlIssuer, 0, 1)->value, 22, {});
        FreeCrlDistPoints(points);

        for (const auto& [hex, type, expected] : {
                 std::tuple<std::string_view, int, Bytes>{
                     "3011300fa00da00ba609240404026162040163", 6, {'a', 'b', 'c'}},
                 {"3011300fa00da00ba680040261620401630000", 6, {'a', 'b', 'c'}},
                 {"3011300fa00da00ba20924040402612e040162", 2, {'a', '.', 'b'}},
                 {"300f300da00ba009a2800403612e620000", 2, {'a', '.', 'b'}}}) {
            points = DecodeCrlDistPoints(nid, FromHex(hex));
            Require(sk_num_nid_postfix(points) == 1);
            dp = DistPointAt(points, 0);
            Require(dp->distpoint != nullptr && dp->distpoint->type == 0 && dp->distpoint->dpname == nullptr);
            Require(dp->reasons == nullptr && dp->crlIssuer == nullptr);
            Require(sk_num_nid_postfix(dp->distpoint->name.fullname) == 1);
            RequireString(GeneralNameAt(dp->distpoint->name.fullname, 0, type)->value, 22, expected);
            FreeCrlDistPoints(points);
        }

        for (const auto& [hex, type, expected] : {
                 std::tuple<std::string_view, int, Bytes>{
                     "300f300da20ba609240404026162040163", 6, {'a', 'b', 'c'}},
                 {"300f300da20ba680040261620401630000", 6, {'a', 'b', 'c'}},
                 {"300f300da20ba20924040402612e040162", 2, {'a', '.', 'b'}},
                 {"300d300ba209a2800403612e620000", 2, {'a', '.', 'b'}}}) {
            points = DecodeCrlDistPoints(nid, FromHex(hex));
            Require(sk_num_nid_postfix(points) == 1);
            dp = DistPointAt(points, 0);
            Require(dp->distpoint == nullptr && dp->reasons == nullptr && dp->crlIssuer != nullptr);
            Require(sk_num_nid_postfix(dp->crlIssuer) == 1);
            RequireString(GeneralNameAt(dp->crlIssuer, 0, type)->value, 22, expected);
            FreeCrlDistPoints(points);
        }

        points = DecodeCrlDistPoints(nid, FromHex("308900000000000000000c300aa008a006860474657374"));
        Require(sk_num_nid_postfix(points) == 1);
        dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->distpoint->type == 0 && dp->distpoint->dpname == nullptr);
        Require(dp->reasons == nullptr && dp->crlIssuer == nullptr);
        Require(sk_num_nid_postfix(dp->distpoint->name.fullname) == 1);
        RequireString(GeneralNameAt(dp->distpoint->name.fullname, 0, 6)->value, 22, {'t', 'e', 's', 't'});
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }

    {
        LibreSslStack* points = DecodeCrlDistPoints(103, FromHex("30123010a00aa008860668747470733a810205a0"));
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->reasons != nullptr && dp->crlIssuer == nullptr);
        RequireString(GeneralNameAt(dp->distpoint->name.fullname, 0, 6)->value, 22, {'h', 't', 't', 'p', 's', ':'});
        Require(dp->reasons->length == 1 && dp->reasons->flags == 13 && dp->reasons->data[0] == 0xa0);
        FreeCrlDistPoints(points);

        points = DecodeCrlDistPoints(103, FromHex("30143012a00aa008860668747470733aa20482026162"));
        dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->reasons == nullptr && dp->crlIssuer != nullptr);
        RequireString(GeneralNameAt(dp->distpoint->name.fullname, 0, 6)->value, 22, {'h', 't', 't', 'p', 's', ':'});
        RequireString(GeneralNameAt(dp->crlIssuer, 0, 2)->value, 22, {'a', 'b'});
        FreeCrlDistPoints(points);
    }

    const Bytes reasonsAndIssuer = FromHex("300c300a810205a0a20482026162");
    {
        LibreSslStack* points = DecodeCrlDistPoints(103, reasonsAndIssuer);
        const DistPoint* dp = DistPointAt(points, 0);
        Require(dp->distpoint == nullptr && dp->reasons != nullptr && dp->crlIssuer != nullptr);
        Require(dp->reasons->length == 1 && dp->reasons->flags == 13 && dp->reasons->data[0] == 0xa0);
        RequireString(GeneralNameAt(dp->crlIssuer, 0, 2)->value, 22, {'a', 'b'});
        FreeCrlDistPoints(points);

        points = DecodeCrlDistPoints(103, FromHex("30183016a00aa008860668747470733a810205a0a20482026162"));
        dp = DistPointAt(points, 0);
        Require(dp->distpoint != nullptr && dp->reasons != nullptr && dp->crlIssuer != nullptr);
        Require(dp->distpoint->type == 0 && dp->distpoint->dpname == nullptr);
        RequireString(GeneralNameAt(dp->distpoint->name.fullname, 0, 6)->value, 22, {'h', 't', 't', 'p', 's', ':'});
        Require(dp->reasons->length == 1 && dp->reasons->flags == 13 && dp->reasons->data[0] == 0xa0);
        RequireString(GeneralNameAt(dp->crlIssuer, 0, 2)->value, 22, {'a', 'b'});
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }

    const Bytes fivePoints = FromHex(
        "30323008a006a004860261313008a006a004860261323008a006a004860261333008a006a004860261343008a006a00486026135");
    {
        LibreSslStack* points = DecodeCrlDistPoints(103, fivePoints);
        Require(sk_num_nid_postfix(points) == 5);
        for (int index = 0; index < 5; ++index) {
            const DistPoint* dp = DistPointAt(points, index);
            Require(dp->distpoint != nullptr && dp->distpoint->type == 0 && dp->distpoint->dpname == nullptr);
            Require(dp->reasons == nullptr && dp->crlIssuer == nullptr);
            Require(sk_num_nid_postfix(dp->distpoint->name.fullname) == 1);
            RequireString(GeneralNameAt(dp->distpoint->name.fullname, 0, 6)->value, 22,
                {'a', static_cast<unsigned char>('1' + index)});
        }
        FreeCrlDistPoints(points);
        Require(allocations.empty());
    }

    static unsigned char crldpOid[] = {0x55, 0x1d, 0x1f};
    Fixture resolvedCrldp(0, crldpOid, sizeof(crldpOid), {0x30, 0x0c, 0x30, 0x0a, 0xa0, 0x08, 0xa0, 0x06, 0x86, 0x04, 't', 'e', 's', 't'});
    LibreSslStack* points = static_cast<LibreSslStack*>(X509V3_EXT_d2i_nid_postfix(&resolvedCrldp.extension));
    Require(points != nullptr && sk_num_nid_postfix(points) == 1);
    RequireString(GeneralNameAt(DistPointAt(points, 0)->distpoint->name.fullname, 0, 6)->value, 22, {'t', 'e', 's', 't'});
    FreeCrlDistPoints(points);

    static unsigned char freshestOid[] = {0x55, 0x1d, 0x2e};
    Fixture resolvedFreshest(0, freshestOid, sizeof(freshestOid), {0x30, 0x0c, 0x30, 0x0a, 0xa0, 0x08, 0xa0, 0x06, 0x86, 0x04, 't', 'e', 's', 't'});
    points = static_cast<LibreSslStack*>(X509V3_EXT_d2i_nid_postfix(&resolvedFreshest.extension));
    Require(points != nullptr && sk_num_nid_postfix(points) == 1);
    RequireString(GeneralNameAt(DistPointAt(points, 0)->distpoint->name.fullname, 0, 6)->value, 22, {'t', 'e', 's', 't'});
    FreeCrlDistPoints(points);

    for (const int nid : {103, 857}) {
        LibreSsl3X509Extension* built =
            X509V3_EXT_conf_nid_nid_postfix(nullptr, nullptr, nid, "DER:300c300aa008a006860474657374");
        Require(built != nullptr);
        points = static_cast<LibreSslStack*>(X509V3_EXT_d2i_nid_postfix(built));
        Require(points != nullptr && sk_num_nid_postfix(points) == 1);
        RequireString(GeneralNameAt(DistPointAt(points, 0)->distpoint->name.fullname, 0, 6)->value, 22, {'t', 'e', 's', 't'});
        FreeCrlDistPoints(points);
        X509_EXTENSION_free_nid_postfix(built);
    }
    Require(allocations.empty());
    RequireQueue({});

    const std::vector<MalformedValue> malformed{
        {{}, {102, 58}},
        {FromHex("30"), {102, 58}},
        {FromHex("3100"), {168, 58}},
        {FromHex("0000"), {168, 58}},
        {FromHex("1080"), {102, 58}},
        {FromHex("30053000"), {155, 58}},
        {FromHex("30020000"), {159}},
        {FromHex("30023100"), {168, 58, 58}},
        {FromHex("30020500"), {168, 58, 58}},
        {FromHex("30021000"), {149, 58}},
        {FromHex("300130"), {102, 58, 58}},
        {FromHex("30011f"), {102, 58, 58}},
        {FromHex("300230ff"), {102, 58, 58}},
        {FromHex("30023005"), {155, 58, 58}},
        {FromHex("300430020000"), {159, 58}},
        {FromHex("300430000000"), {159}},
        {FromHex("3006300230000000"), {168, 58, 58}},
        {FromHex("300430028000"), {120, 58}},
        {FromHex("3003300180"), {102, 58, 58}},
        {FromHex("300330019f"), {102, 58, 58}},
        {FromHex("300430028080"), {102, 58, 58}},
        {FromHex("30043002a005"), {155, 58, 58}},
        {FromHex("30043002a000"), {102, 58, 58, 58, 58, 58}},
        {FromHex("30063004a0028200"), {143, 58, 58, 58}},
        {FromHex("30063004a0023000"), {143, 58, 58, 58}},
        {FromHex("300c300aa008a004860261620500"), {148, 58}},
        {FromHex("300c300aa008a004860261620000"), {148, 58}},
        {FromHex("300c300aa006a004860261620000"), {159, 58}},
        {FromHex("30083006a004a0020000"), {159, 58, 58, 58, 58}},
        {FromHex("30083006a004a0028900"), {143, 58, 58, 58, 58, 58}},
        {FromHex("30083006a004a0028205"), {155, 58, 58, 58, 58, 58, 58, 58, 58}},
        {FromHex("30093007a005a003830161"), {156, 58, 58, 58, 58, 58, 58, 58}},
        {FromHex("30053003a00181"), {102, 58, 58, 58, 58, 58}},
        {FromHex("30063004a0028180"), {102, 58, 58, 58, 58, 58}},
        {FromHex("30063004a0028105"), {155, 58, 58, 58, 58, 58}},
        {FromHex("30083006a004a1020000"), {159, 58, 58, 58, 58}},
        {FromHex("30083006a004a1023100"), {168, 58, 58, 58, 58, 58, 58}},
        {FromHex("30083006a004a1021000"), {149, 58, 58, 58, 58, 58}},
        {FromHex("30073005a003a10130"), {102, 58, 58, 58, 58, 58, 58}},
        {FromHex("30083006a004a1023005"), {155, 58, 58, 58, 58, 58, 58}},
        {FromHex("30083006a004a1023000"), {121, 58, 58, 58, 58, 58}},
        {FromHex("300a3008a006a10430020000"), {159, 58, 58, 58, 58, 58}},
        {FromHex("300c300aa008a106300406022a03"), {121, 58, 58, 58, 58, 58}},
        {FromHex("300e300ca00aa108300606022a030000"), {159, 58, 58, 58, 58, 58}},
        {FromHex("300a3008a006a10430020500"), {168, 58, 58, 58, 58, 58, 58, 58}},
        {FromHex("300a3008a006a10430022600"), {231, 58, 58, 58, 58, 58, 58}},
        {FromHex("300a3008a006a10430020600"), {216, 58, 58, 58, 58, 58, 58}},
        {FromHex("300c300aa008a106300406028001"), {216, 58, 58, 58, 58, 58, 58}},
        {FromHex("300e300ca00aa108300606022a038000"), {139, 58, 58, 58, 58, 58, 58}},
        {FromHex("300e300ca00aa108300606022a030500"), {140, 58, 58, 58, 58, 58, 58}},
        {FromHex("300e300ca00aa108300606022a031000"), {156, 58, 58, 58, 58, 58, 58}},
        {FromHex("300e300ca00aa108300606022a030300"), {152, 58, 58, 58, 58, 58, 58}},
        {FromHex("3010300ea00ca10a300806022a0303020800"), {220, 58, 58, 58, 58, 58, 58}},
        {FromHex("300f300da00ba109300706022a031e0141"), {214, 58, 58, 58, 58, 58, 58}},
        {FromHex("3011300fa00da10b300906022a031c03414243"), {215, 58, 58, 58, 58, 58, 58}},
        {FromHex("300f300da00ba109300706022a032d0101"), {231, 58, 58, 58, 58, 58, 58}},
        {FromHex("300f300da00ba109300706022a032a0101"), {231, 58, 58, 58, 58, 58, 58}},
        {FromHex("300e300ca00aa108300606022a030a00"), {62, 58, 58, 58, 58, 58, 58}},
        {FromHex("3011300fa00da10b300906022a031301410500"), {148, 58, 58, 58, 58, 58}},
        {FromHex("3011300fa00da10b300906022a031301410000"), {148, 58, 58, 58, 58, 58}},
        {FromHex("300e300ca00aa108300606022a031305"), {155, 58, 58, 58, 58, 58, 58, 58}},
        {FromHex("300c300aa008a106300406022a0313"), {121, 58, 58, 58, 58, 58}},
        {FromHex("300430028100"), {152, 58, 58}},
        {FromHex("3006300481020800"), {220, 58, 58}},
        {FromHex("3003300181"), {102, 58, 58, 58}},
        {FromHex("300430028180"), {102, 58, 58, 58}},
        {FromHex("300430028105"), {155, 58, 58, 58}},
        {FromHex("30083006810205a00000"), {159, 58}},
        {FromHex("300e300ca008a0068604746573740000"), {159, 58}},
        {FromHex("30083006810205a08000"), {168, 58, 58}},
        {FromHex("30083006810205a08100"), {168, 58, 58}},
        {FromHex("300430028300"), {168, 58, 58}},
        {FromHex("300430020500"), {168, 58, 58}},
        {FromHex("30063004a2020000"), {159, 58}},
        {FromHex("30063004a2028900"), {143, 58, 58}},
        {FromHex("30073005a200800100"), {148, 58}},
        {FromHex("30073005a200810100"), {148, 58}},
        {FromHex("30063004a2000000"), {148, 58}},
        {FromHex("30063004a2000500"), {148, 58}},
        {FromHex("30063004a200a200"), {148, 58}},
    };
    RequireMalformed(103, malformed);
    RequireMalformed(857, malformed);

    const std::vector<Bytes> unmodelled{
        FromHex("30800000"),
        FromHex("300430800000"),
        FromHex("30043002a080"),
        FromHex("30063004a0800000"),
        FromHex("30083006a004a0800000"),
        FromHex("30083006a004a1800000"),
        FromHex("30083006a004a1023080"),
        FromHex("3010300ea00ca10a300806022a0330800000"),
        FromHex("30133011a00fa10d300b06022a032c050403616263"),
        FromHex("30093007a105030300ff00"),
        FromHex("30063004a2800000"),
        FromHex("30123010a00ea00ca00a06032a0304a003160178"),
        FromHex("30103008a006a004860261623004a2800000"),
        FromHex("301d301ba019a117300806022a0313024341300b06022a032c050403616263"),
    };
    RequireUnmodelled(103, unmodelled);
    RequireUnmodelled(857, unmodelled);

    Require(DistinctQueues(AllocationFailures(103, FromHex("3000"), FreeCrlDistPoints)) ==
        std::vector<std::vector<int>>{{65}});
    Require(DistinctQueues(AllocationFailures(103, singleFullname, FreeCrlDistPoints)) ==
        std::vector<std::vector<int>>{
            {65}, {65, 58, 58}, {65, 58, 58, 58, 58}, {65, 58, 58, 58, 58, 58, 58},
            {65, 58, 58, 58, 58, 58, 58, 58}, {65, 65, 58, 58, 58, 58, 58, 58, 58}});

    const std::vector<std::vector<int>> relativeFailures =
        AllocationFailures(103, singleRelative, FreeCrlDistPoints);
    Require(!relativeFailures.empty());
    for (const std::vector<int>& queue : relativeFailures) Require(!queue.empty());

    const std::vector<std::vector<int>> reasonsIssuerFailures =
        AllocationFailures(103, reasonsAndIssuer, FreeCrlDistPoints);
    Require(!reasonsIssuerFailures.empty());
    for (const std::vector<int>& queue : reasonsIssuerFailures) Require(!queue.empty());

    const std::vector<std::vector<int>> fivePointFailures =
        AllocationFailures(103, fivePoints, FreeCrlDistPoints);
    Require(fivePointFailures.front() == std::vector<int>{65});
    Require(DistinctQueues(fivePointFailures) ==
        std::vector<std::vector<int>>{
            {65}, {65, 58, 58}, {65, 58, 58, 58, 58}, {65, 58, 58, 58, 58, 58, 58},
            {65, 58, 58, 58, 58, 58, 58, 58}, {65, 65, 58, 58, 58, 58, 58, 58, 58}});
    Require(std::count(fivePointFailures.begin(), fivePointFailures.end(), std::vector<int>{65}) >= 3);
    Require(std::find(fivePointFailures.begin() + 3, fivePointFailures.end(), std::vector<int>{65}) !=
        fivePointFailures.end());
    for (const std::vector<int>& queue : fivePointFailures) Require(!queue.empty());
}

struct NoticeRef {
    Asn1String* organization;
    LibreSslStack* noticeNos;
};

struct UserNotice {
    NoticeRef* noticeRef;
    Asn1String* expText;
};

struct PolicyQualInfo {
    Asn1Object_nid_no_patch* qualId;
    union {
        Asn1String* cpsUri;
        UserNotice* userNotice;
        Asn1Type* other;
    } d;
};

struct PolicyInfo {
    Asn1Object_nid_no_patch* policyId;
    LibreSslStack* qualifiers;
};

void FreeNoticeNos(LibreSslStack* noticeNos) {
    if (!noticeNos) return;
    for (int index = 0; index < noticeNos->num; ++index)
        ASN1_INTEGER_free_nid_postfix(reinterpret_cast<Asn1String*>(noticeNos->data[index]));
    sk_free_nid_postfix(noticeNos);
}

void FreeNoticeRef(NoticeRef* noticeRef) {
    if (!noticeRef) return;
    ASN1_OCTET_STRING_free_nid_postfix(noticeRef->organization);
    FreeNoticeNos(noticeRef->noticeNos);
    free_nid_postfix(noticeRef);
}

void FreeUserNotice(UserNotice* userNotice) {
    if (!userNotice) return;
    FreeNoticeRef(userNotice->noticeRef);
    ASN1_OCTET_STRING_free_nid_postfix(userNotice->expText);
    free_nid_postfix(userNotice);
}

void FreePolicyQualInfo(PolicyQualInfo* qual) {
    if (!qual) return;
    if (qual->qualId) {
        const int nid = OBJ_obj2nid_nid_postfix(reinterpret_cast<const LibreSslAsn1Object*>(qual->qualId));
        if (nid == 164) ASN1_OCTET_STRING_free_nid_postfix(qual->d.cpsUri);
        else if (nid == 165) FreeUserNotice(qual->d.userNotice);
        else ASN1_TYPE_free_nid_postfix(qual->d.other);
        ASN1_OBJECT_free_nid_postfix(qual->qualId);
    }
    free_nid_postfix(qual);
}

void FreeCertificatePolicies(void* value) {
    sk_pop_free_nid_postfix(static_cast<LibreSslStack*>(value), +[](void* pointer) APS5_VABI {
        auto* info = static_cast<PolicyInfo*>(pointer);
        if (!info) return;
        ASN1_OBJECT_free_nid_postfix(info->policyId);
        if (info->qualifiers) {
            for (int index = 0; index < info->qualifiers->num; ++index)
                FreePolicyQualInfo(reinterpret_cast<PolicyQualInfo*>(info->qualifiers->data[index]));
            sk_free_nid_postfix(info->qualifiers);
        }
        free_nid_postfix(info);
    });
}

LibreSslStack* DecodeCertificatePolicies(const Bytes& bytes) {
    auto* policies = static_cast<LibreSslStack*>(DecodeValue(89, bytes));
    Require(policies != nullptr);
    RequireQueue({});
    return policies;
}

void RequireObject(const Asn1Object_nid_no_patch* object, const Bytes& encoding, int nid) {
    Require(object != nullptr && object->length == static_cast<int>(encoding.size()));
    Require(std::memcmp(object->data, encoding.data(), encoding.size()) == 0);
    Require(object->nid == 0 && object->flags == 0x09);
    Require(OBJ_obj2nid_nid_postfix(reinterpret_cast<const LibreSslAsn1Object*>(object)) == nid);
}

const PolicyInfo* PolicyInfoAt(const LibreSslStack* policies, int index, const Bytes& oidEncoding) {
    const auto* info = static_cast<const PolicyInfo*>(sk_value_nid_postfix(policies, index));
    Require(info != nullptr);
    RequireObject(info->policyId, oidEncoding, 0);
    return info;
}

const PolicyQualInfo* PolicyQualInfoAt(const LibreSslStack* qualifiers, int index, const Bytes& oidEncoding, int nid) {
    const auto* qual = static_cast<const PolicyQualInfo*>(sk_value_nid_postfix(qualifiers, index));
    Require(qual != nullptr);
    RequireObject(qual->qualId, oidEncoding, nid);
    return qual;
}

void RequireExactString(const void* value, std::int32_t type, const Bytes& data, std::int64_t flags = 0) {
    const auto* string = static_cast<const Asn1String*>(value);
    Require(string != nullptr && string->type == type && string->flags == flags);
    Require(string->length == static_cast<std::int32_t>(data.size()));
    if (!data.empty()) Require(std::memcmp(string->data, data.data(), data.size()) == 0);
}

void TestCertificatePolicies() {
    static_assert(sizeof(NoticeRef) == 16 && offsetof(NoticeRef, organization) == 0 && offsetof(NoticeRef, noticeNos) == 8);
    static_assert(sizeof(UserNotice) == 16 && offsetof(UserNotice, noticeRef) == 0 && offsetof(UserNotice, expText) == 8);
    static_assert(sizeof(PolicyQualInfo) == 16 && offsetof(PolicyQualInfo, qualId) == 0 && offsetof(PolicyQualInfo, d) == 8);
    static_assert(sizeof(PolicyInfo) == 16 && offsetof(PolicyInfo, policyId) == 0 && offsetof(PolicyInfo, qualifiers) == 8);
    static_assert(sizeof(Asn1Type) == 16 && offsetof(Asn1Type, value) == 8);

    const Bytes pidA{0x2a, 0x03};
    const Bytes pidB{0x2a, 0x04};
    const Bytes pidC{0x2a, 0x05};
    const Bytes cpsOid{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x01};
    const Bytes unoticeOid{0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x02};

    for (const Bytes& empty : {FromHex("3000"), FromHex("1000"), FromHex("30005a"), FromHex("308100"),
             FromHex("3089000000000000000000")}) {
        LibreSslStack* policies = DecodeCertificatePolicies(empty);
        Require(sk_num_nid_postfix(policies) == 0);
        FreeCertificatePolicies(policies);
        Require(allocations.empty());
    }

    {
        LibreSslStack* policies = DecodeCertificatePolicies(FromHex("3006300406022a03"));
        Require(sk_num_nid_postfix(policies) == 1);
        Require(PolicyInfoAt(policies, 0, pidA)->qualifiers == nullptr);
        FreeCertificatePolicies(policies);

        for (const Bytes& emptyQuals : {FromHex("3008300606022a033000"), FromHex("3008300606022a031000")}) {
            policies = DecodeCertificatePolicies(emptyQuals);
            Require(sk_num_nid_postfix(policies) == 1);
            const PolicyInfo* info = PolicyInfoAt(policies, 0, pidA);
            Require(info->qualifiers != nullptr && sk_num_nid_postfix(info->qualifiers) == 0);
            FreeCertificatePolicies(policies);
        }

        policies = DecodeCertificatePolicies(FromHex("301c300406022a03301406022a04300e300c06082b060105050702011600"));
        Require(sk_num_nid_postfix(policies) == 2);
        Require(PolicyInfoAt(policies, 0, pidA)->qualifiers == nullptr);
        const PolicyInfo* second = PolicyInfoAt(policies, 1, pidB);
        Require(sk_num_nid_postfix(second->qualifiers) == 1);
        RequireString(PolicyQualInfoAt(second->qualifiers, 0, cpsOid, 164)->d.cpsUri, 22, {});
        FreeCertificatePolicies(policies);
        Require(allocations.empty());
    }

    for (const auto& [hex, expected] : {
             std::pair<std::string_view, Bytes>{"3016301406022a03300e300c06082b060105050702011600", {}},
             {"301e301c06022a033016301406082b060105050702011608687474703a2f2f61", {'h', 't', 't', 'p', ':', '/', '/', 'a'}},
             {"3020301e06022a033018301606082b06010505070201360a04026162040463646566", {'a', 'b', 'c', 'd', 'e', 'f'}},
             {"3020301e06022a033018301606082b06010505070201368004026162040263640000", {'a', 'b', 'c', 'd'}},
             {"3019301706022a033011300f06082b060105050702013603800161", {'a'}}}) {
        LibreSslStack* policies = DecodeCertificatePolicies(FromHex(hex));
        const PolicyInfo* info = PolicyInfoAt(policies, 0, pidA);
        Require(sk_num_nid_postfix(info->qualifiers) == 1);
        RequireString(PolicyQualInfoAt(info->qualifiers, 0, cpsOid, 164)->d.cpsUri, 22, expected);
        FreeCertificatePolicies(policies);
        Require(allocations.empty());
    }

    {
        LibreSslStack* policies = DecodeCertificatePolicies(FromHex("3016301406022a03300e300c06082b060105050702023000"));
        const UserNotice* notice =
            PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, unoticeOid, 165)->d.userNotice;
        Require(notice != nullptr && notice->noticeRef == nullptr && notice->expText == nullptr);
        FreeCertificatePolicies(policies);

        for (const auto& [hex, type, expected] : {
                 std::tuple<std::string_view, std::int32_t, Bytes>{
                     "3019301706022a033011300f06082b060105050702023003160161", 22, {'a'}},
                 {"3019301706022a033011300f06082b0601050507020230031a0162", 26, {'b'}},
                 {"3019301706022a033011300f06082b0601050507020230030c0163", 12, {'c'}},
                 {"301c301a06022a033014301206082b0601050507020230061e0400410042", 30, {0x00, 'A', 0x00, 'B'}},
                 {"301f301d06022a033017301506082b0601050507020230092c0704026162040163", 12, {'a', 'b', 'c'}},
                 {"3020301e06022a033018301606082b06010505070202300a3e080402004104020042", 30, {0x00, 'A', 0x00, 'B'}},
                 {"3021301f06022a033019301706082b06010505070202300b2c80040261620401630000", 12, {'a', 'b', 'c'}}}) {
            policies = DecodeCertificatePolicies(FromHex(hex));
            notice = PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, unoticeOid, 165)->d.userNotice;
            Require(notice != nullptr && notice->noticeRef == nullptr);
            RequireString(notice->expText, type, expected);
            FreeCertificatePolicies(policies);
        }

        for (const std::string_view hex : {"301d301b06022a033015301306082b060105050702023007300516016f3000",
                 "301d301b06022a033015301306082b060105050702023007300516016f1000"}) {
            policies = DecodeCertificatePolicies(FromHex(hex));
            notice = PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, unoticeOid, 165)->d.userNotice;
            Require(notice != nullptr && notice->noticeRef != nullptr && notice->expText == nullptr);
            RequireString(notice->noticeRef->organization, 22, {'o'});
            Require(notice->noticeRef->noticeNos != nullptr && sk_num_nid_postfix(notice->noticeRef->noticeNos) == 0);
            FreeCertificatePolicies(policies);
        }

        policies = DecodeCertificatePolicies(
            FromHex("3027302506022a03301f301d06082b060105050702023011300f1a026f7230090201000201070201ff"));
        notice = PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, unoticeOid, 165)->d.userNotice;
        Require(notice != nullptr && notice->noticeRef != nullptr && notice->expText == nullptr);
        RequireString(notice->noticeRef->organization, 26, {'o', 'r'});
        Require(sk_num_nid_postfix(notice->noticeRef->noticeNos) == 3);
        const auto* n0 = static_cast<const Asn1String*>(sk_value_nid_postfix(notice->noticeRef->noticeNos, 0));
        const auto* n1 = static_cast<const Asn1String*>(sk_value_nid_postfix(notice->noticeRef->noticeNos, 1));
        const auto* n2 = static_cast<const Asn1String*>(sk_value_nid_postfix(notice->noticeRef->noticeNos, 2));
        Require(n0->type == 2 && n0->length == 1 && n0->data[0] == 0);
        Require(n1->type == 2 && n1->length == 1 && n1->data[0] == 7);
        Require(n2->type == 0x102 && n2->length == 1 && n2->data[0] == 1);
        FreeCertificatePolicies(policies);

        policies = DecodeCertificatePolicies(
            FromHex("3025302306022a03301d301b06082b06010505070202300f300d2c8004024f4b00003003020101"));
        notice = PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, unoticeOid, 165)->d.userNotice;
        Require(notice != nullptr && notice->noticeRef != nullptr && notice->expText == nullptr);
        RequireString(notice->noticeRef->organization, 12, {'O', 'K'});
        Require(sk_num_nid_postfix(notice->noticeRef->noticeNos) == 1);
        Require(ASN1_INTEGER_get_nid_postfix(
                    static_cast<const Asn1String*>(sk_value_nid_postfix(notice->noticeRef->noticeNos, 0))) == 1);
        FreeCertificatePolicies(policies);

        policies = DecodeCertificatePolicies(
            FromHex("302f302d06022a033027302506082b060105050702023019300d0c024f4b3007020101020201001e0800480065006c006f"));
        notice = PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, unoticeOid, 165)->d.userNotice;
        Require(notice != nullptr && notice->noticeRef != nullptr && notice->expText != nullptr);
        RequireString(notice->noticeRef->organization, 12, {'O', 'K'});
        Require(sk_num_nid_postfix(notice->noticeRef->noticeNos) == 2);
        Require(ASN1_INTEGER_get_nid_postfix(
                    static_cast<const Asn1String*>(sk_value_nid_postfix(notice->noticeRef->noticeNos, 0))) == 1);
        Require(ASN1_INTEGER_get_nid_postfix(
                    static_cast<const Asn1String*>(sk_value_nid_postfix(notice->noticeRef->noticeNos, 1))) == 256);
        RequireString(notice->expText, 30, {0x00, 'H', 0x00, 'e', 0x00, 'l', 0x00, 'o'});
        FreeCertificatePolicies(policies);
        Require(allocations.empty());
    }

    {
        LibreSslStack* policies = DecodeCertificatePolicies(FromHex("3010300e06022a033008300606022a040500"));
        const Asn1Type* other = PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, pidB, 0)->d.other;
        Require(other != nullptr && other->type == 5 && other->value.pointer == nullptr);
        FreeCertificatePolicies(policies);

        for (const auto& [hex, expectedBool] : {
                 std::pair<std::string_view, std::int32_t>{"3011300f06022a033009300706022a040101ff", 255},
                 {"3011300f06022a033009300706022a04010100", 0}}) {
            policies = DecodeCertificatePolicies(FromHex(hex));
            other = PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, pidB, 0)->d.other;
            Require(other != nullptr && other->type == 1 && other->value.boolean == expectedBool);
            FreeCertificatePolicies(policies);
        }

        policies = DecodeCertificatePolicies(FromHex("3012301006022a03300a300806022a0406022a03"));
        other = PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, pidB, 0)->d.other;
        Require(other != nullptr && other->type == 6);
        RequireObject(static_cast<const Asn1Object_nid_no_patch*>(other->value.pointer), pidA, 0);
        FreeCertificatePolicies(policies);

        for (const auto& [hex, anyType, strType, data, flags] : {
                 std::tuple<std::string_view, std::int32_t, std::int32_t, Bytes, std::int64_t>{
                     "3011300f06022a033009300706022a040201fe", 2, 0x102, {0x02}, 0},
                 {"3012301006022a03300a300806022a0402020100", 2, 2, {0x01, 0x00}, 0},
                 {"3012301006022a03300a300806022a04030205a0", 3, 3, {0xa0}, 13},
                 {"3011300f06022a033009300706022a04030100", 3, 3, {}, 8},
                 {"3012301006022a03300a300806022a0404020102", 4, 4, {0x01, 0x02}, 0},
                 {"3011300f06022a033009300706022a040a01fe", 10, 0x10a, {0x02}, 0},
                 {"3012301006022a03300a300806022a040c026162", 12, 12, {'a', 'b'}, 0},
                 {"3013301106022a03300b300906022a043003020101", 16, 16, {0x30, 0x03, 0x02, 0x01, 0x01}, 0},
                 {"3013301106022a03300b300906022a043103020101", 17, 17, {0x31, 0x03, 0x02, 0x01, 0x01}, 0},
                 {"3012301006022a03300a300806022a0480026162", -3, -3, {0x80, 0x02, 'a', 'b'}, 0},
                 {"3014301206022a03300c300a06022a04a00404026162", -3, -3, {0xa0, 0x04, 0x04, 0x02, 'a', 'b'}, 0},
                 {"3016301406022a03300e300c06022a042406040161040162", 4, 4, {'a', 'b'}, 0},
                 {"3018301606022a033010300e06022a0424800401610401620000", 4, 4, {'a', 'b'}, 0},
                 {"3012301006022a03300a300806022a0430800000", 16, 16, {0x30, 0x80, 0x00, 0x00}, 0},
                 {"301a301806022a033012301006022a04308030800000300205000000", 16, 16,
                     {0x30, 0x80, 0x30, 0x80, 0x00, 0x00, 0x30, 0x02, 0x05, 0x00, 0x00, 0x00}, 0},
                 {"3012301006022a03300a300806022a04a0800000", -3, -3, {0xa0, 0x80, 0x00, 0x00}, 0},
                 {"3011300f06022a033009300706022a04000161", 0, 0, {'a'}, 0},
                 {"3011300f06022a033009300706022a04400161", -3, -3, {0x40, 0x01, 'a'}, 0},
                 {"3011300f06022a033009300706022a04c00161", -3, -3, {0xc0, 0x01, 'a'}, 0},
                 {"3018301606022a033010300e06022a042306030200aa030200bb", 3, 3, {0xaa, 0x00, 0xbb}, 8},
                 {"301f301d06022a033017301506022a04180f32303234303130323033303430355a", 24, 24,
                     {'2', '0', '2', '4', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', 'Z'}, 0},
                 {"301d301b06022a033015301306022a04170d3234303130323033303430355a", 23, 23,
                     {'2', '4', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', 'Z'}, 0},
                 {"301d301b06022a033015301306022a04170d3936303232393030303030305a", 23, 23,
                     {'9', '6', '0', '2', '2', '9', '0', '0', '0', '0', '0', '0', 'Z'}, 0},
                 {"301d301b06022a033015301306022a04170d3030303232393030303030305a", 23, 23,
                     {'0', '0', '0', '2', '2', '9', '0', '0', '0', '0', '0', '0', 'Z'}, 0},
                 {"3014301206022a03300c300a06022a041e0400410042", 30, 30, {0x00, 'A', 0x00, 'B'}, 0},
                 {"3014301206022a03300c300a06022a041c0400000041", 28, 28, {0x00, 0x00, 0x00, 'A'}, 0},
                 {"3013301106022a03300b300906022a041f81000161", 128, 128, {'a'}, 0}}) {
            policies = DecodeCertificatePolicies(FromHex(hex));
            other = PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, pidB, 0)->d.other;
            Require(other != nullptr && other->type == anyType);
            RequireExactString(other->value.pointer, strType, data, flags);
            FreeCertificatePolicies(policies);
        }
        Require(allocations.empty());
    }

    const Bytes complexMulti = FromHex(
        "304a304206022a03303c300e06082b0601050507020116026869301c06082b060105050702023010300a0c024f4b3004020201001a026e6f"
        "300c06022a043006040261620500300406022a05");
    {
        LibreSslStack* policies = DecodeCertificatePolicies(complexMulti);
        Require(sk_num_nid_postfix(policies) == 2);
        const PolicyInfo* first = PolicyInfoAt(policies, 0, pidA);
        Require(sk_num_nid_postfix(first->qualifiers) == 3);
        RequireString(PolicyQualInfoAt(first->qualifiers, 0, cpsOid, 164)->d.cpsUri, 22, {'h', 'i'});
        const UserNotice* notice = PolicyQualInfoAt(first->qualifiers, 1, unoticeOid, 165)->d.userNotice;
        Require(notice != nullptr && notice->noticeRef != nullptr);
        RequireString(notice->noticeRef->organization, 12, {'O', 'K'});
        Require(sk_num_nid_postfix(notice->noticeRef->noticeNos) == 1);
        Require(ASN1_INTEGER_get_nid_postfix(
                    static_cast<const Asn1String*>(sk_value_nid_postfix(notice->noticeRef->noticeNos, 0))) == 256);
        RequireString(notice->expText, 26, {'n', 'o'});
        const Asn1Type* other = PolicyQualInfoAt(first->qualifiers, 2, pidB, 0)->d.other;
        Require(other != nullptr && other->type == 16);
        RequireExactString(other->value.pointer, 16, {0x30, 0x06, 0x04, 0x02, 'a', 'b', 0x05, 0x00});
        Require(PolicyInfoAt(policies, 1, pidC)->qualifiers == nullptr);
        FreeCertificatePolicies(policies);
        Require(allocations.empty());
    }

    static unsigned char cpolsOid[] = {0x55, 0x1d, 0x20};
    Fixture resolvedCpols(0, cpolsOid, sizeof(cpolsOid), {0x30, 0x06, 0x30, 0x04, 0x06, 0x02, 0x2a, 0x03});
    LibreSslStack* policies = static_cast<LibreSslStack*>(X509V3_EXT_d2i_nid_postfix(&resolvedCpols.extension));
    Require(policies != nullptr && sk_num_nid_postfix(policies) == 1);
    Require(PolicyInfoAt(policies, 0, pidA)->qualifiers == nullptr);
    FreeCertificatePolicies(policies);

    LibreSsl3X509Extension* built =
        X509V3_EXT_conf_nid_nid_postfix(nullptr, nullptr, 89, "DER:3016301406022a03300e300c06082b060105050702011600");
    Require(built != nullptr);
    policies = static_cast<LibreSslStack*>(X509V3_EXT_d2i_nid_postfix(built));
    Require(policies != nullptr && sk_num_nid_postfix(policies) == 1);
    RequireString(PolicyQualInfoAt(PolicyInfoAt(policies, 0, pidA)->qualifiers, 0, cpsOid, 164)->d.cpsUri, 22, {});
    FreeCertificatePolicies(policies);
    X509_EXTENSION_free_nid_postfix(built);
    Require(allocations.empty());
    RequireQueue({});

    const std::vector<MalformedValue> malformed{
        {{}, {102, 58}},
        {FromHex("30"), {102, 58}},
        {FromHex("3100"), {168, 58}},
        {FromHex("0500"), {168, 58}},
        {FromHex("30053000"), {155, 58}},
        {FromHex("30020000"), {159}},
        {FromHex("30023100"), {168, 58, 58}},
        {FromHex("30021000"), {149, 58}},
        {FromHex("300130"), {102, 58, 58}},
        {FromHex("30023005"), {155, 58, 58}},
        {FromHex("30023000"), {121, 58}},
        {FromHex("300430020000"), {159, 58}},
        {FromHex("300430020500"), {168, 58, 58, 58}},
        {FromHex("300430022600"), {231, 58, 58}},
        {FromHex("300430020600"), {216, 58, 58}},
        {FromHex("3006300406028001"), {216, 58, 58}},
        {FromHex("3003300106"), {102, 58, 58, 58}},
        {FromHex("300430020605"), {155, 58, 58, 58}},
        {FromHex("3008300606022a030500"), {168, 58, 58}},
        {FromHex("3008300606022a030000"), {159, 58}},
        {FromHex("300a300806022a0330000500"), {148, 58}},
        {FromHex("300a300806022a0330000000"), {148, 58}},
        {FromHex("300a300806022a0330023100"), {168, 58, 58, 58}},
        {FromHex("300a300806022a0330021000"), {149, 58, 58}},
        {FromHex("300a300806022a0330023000"), {121, 58, 58}},
        {FromHex("300a300806022a0330020000"), {159, 58}},
        {FromHex("300c300a06022a03300430020000"), {159, 58, 58}},
        {FromHex("3014301206022a03300c300a06082b06010505070201"), {121, 58, 58}},
        {FromHex("3014301206022a03300c300a06082b06010505070202"), {121, 58, 58}},
        {FromHex("300e300c06022a033006300406022a04"), {121, 58, 58}},
        {FromHex("300c300a06022a03300430020500"), {168, 58, 58, 58, 58}},
        {FromHex("300c300a06022a03300430022600"), {231, 58, 58, 58}},
        {FromHex("300c300a06022a03300430020600"), {216, 58, 58, 58}},
        {FromHex("3010300e06022a0330083006060280010500"), {216, 58, 58, 58}},
        {FromHex("3010300e06022a033008300606022a040000"), {159, 58, 58}},
        {FromHex("3016301406022a03300e300c06082b060105050702010c00"), {168, 58, 58, 58, 58}},
        {FromHex("3015301306022a03300d300b06082b0601050507020116"), {102, 58, 58, 58, 58}},
        {FromHex("3016301406022a03300e300c06082b060105050702011605"), {155, 58, 58, 58, 58}},
        {FromHex("3024302206022a03301c301a06082b06010505070201360e360c360a36083606360436020400"), {197, 58, 58, 58}},
        {FromHex("3018301606022a033010300e06082b0601050507020136020000"), {159, 58, 58, 58}},
        {FromHex("3019301706022a033011300f06082b060105050702013680040161"), {137, 58, 58, 58}},
        {FromHex("3018301606022a033010300e06082b0601050507020116000500"), {148, 58, 58}},
        {FromHex("3016301406022a03300e300c06082b060105050702020500"), {168, 58, 58, 58, 58}},
        {FromHex("3016301406022a03300e300c06082b060105050702021000"), {149, 58, 58, 58}},
        {FromHex("3018301606022a033010300e06082b0601050507020230000500"), {148, 58, 58}},
        {FromHex("3018301606022a033010300e06082b0601050507020230020000"), {159, 58, 58, 58}},
        {FromHex("3017301506022a03300f300d06082b0601050507020230011f"), {102, 58, 58, 58, 58, 58}},
        {FromHex("3018301606022a033010300e06082b0601050507020230023005"), {155, 58, 58, 58, 58, 58}},
        {FromHex("3019301706022a033011300f06082b060105050702023003130161"), {140, 58, 58, 58, 58}},
        {FromHex("3019301706022a033011300f06082b060105050702023003140161"), {140, 58, 58, 58, 58}},
        {FromHex("301c301a06022a033014301206082b0601050507020230061c0400000041"), {140, 58, 58, 58, 58}},
        {FromHex("3019301706022a033011300f06082b060105050702023003040161"), {140, 58, 58, 58, 58}},
        {FromHex("3019301706022a033011300f06082b060105050702023003800161"), {139, 58, 58, 58, 58}},
        {FromHex("3019301706022a033011300f06082b0601050507020230031e0141"), {214, 58, 58, 58, 58}},
        {FromHex("301b301906022a033013301106082b0601050507020230053e03040141"), {214, 58, 58, 58, 58}},
        {FromHex("301c301a06022a033014301206082b060105050702023006160161160162"), {148, 58, 58, 58}},
        {FromHex("301b301906022a033013301106082b0601050507020230051601610000"), {148, 58, 58, 58}},
        {FromHex("3017301506022a03300f300d06082b06010505070202300116"), {102, 58, 58, 58, 58, 58}},
        {FromHex("3019301706022a033011300f06082b060105050702023003160561"), {155, 58, 58, 58, 58, 58}},
        {FromHex("3018301606022a033010300e06082b0601050507020230021000"), {149, 58, 58, 58, 58}},
        {FromHex("3018301606022a033010300e06082b0601050507020230023000"), {121, 58, 58, 58, 58}},
        {FromHex("301a301806022a033012301006082b06010505070202300430020000"), {159, 58, 58, 58, 58}},
        {FromHex("301d301b06022a033015301306082b06010505070202300730051301613000"), {140, 58, 58, 58, 58, 58}},
        {FromHex("301d301b06022a033015301306082b06010505070202300730051401613000"), {140, 58, 58, 58, 58, 58}},
        {FromHex("301d301b06022a033015301306082b06010505070202300730058001613000"), {139, 58, 58, 58, 58, 58}},
        {FromHex("301d301b06022a033015301306082b06010505070202300730051e01413000"), {214, 58, 58, 58, 58, 58}},
        {FromHex("301b301906022a033013301106082b060105050702023005300316016f"), {121, 58, 58, 58, 58}},
        {FromHex("301d301b06022a033015301306082b060105050702023007300516016f0500"), {168, 58, 58, 58, 58, 58}},
        {FromHex("301f301d06022a033017301506082b060105050702023009300716016f30020500"), {168, 58, 58, 58, 58, 58, 58}},
        {FromHex("301f301d06022a033017301506082b060105050702023009300716016f30022200"), {231, 58, 58, 58, 58, 58}},
        {FromHex("301f301d06022a033017301506082b060105050702023009300716016f30020200"), {62, 58, 58, 58, 58, 58}},
        {FromHex("3021301f06022a033019301706082b06010505070202300b300916016f300402020001"), {62, 58, 58, 58, 58, 58}},
        {FromHex("301f301d06022a033017301506082b060105050702023009300716016f30000500"), {148, 58, 58, 58, 58}},
        {FromHex("3022302006022a03301a301806082b06010505070202300c300516016f30001303616263"), {140, 58, 58, 58, 58}},
        {FromHex("301f301d06022a033017301506082b060105050702023009300516016f30000000"), {159, 58, 58, 58}},
        {FromHex("301f301d06022a033017301506082b060105050702023009300716016f30020000"), {159, 58, 58, 58, 58}},
        {FromHex("3031302f06022a033029302706082b06010505070202301b300d0c024f4b3007020101020201001e0800480065006c006f0500"),
            {148, 58, 58, 58}},
        {FromHex("3011300f06022a033009300706022a04050100"), {144, 58, 58, 58}},
        {FromHex("3010300e06022a033008300606022a042500"), {231, 58, 58, 58}},
        {FromHex("3012301006022a03300a300806022a0401020001"), {106, 58, 58, 58}},
        {FromHex("3011300f06022a033009300706022a042101ff"), {231, 58, 58, 58}},
        {FromHex("3010300e06022a033008300606022a040200"), {62, 58, 58, 58}},
        {FromHex("3011300f06022a033009300706022a04220101"), {231, 58, 58, 58}},
        {FromHex("3010300e06022a033008300606022a040300"), {152, 58, 58, 58}},
        {FromHex("3012301006022a03300a300806022a0403020800"), {220, 58, 58, 58}},
        {FromHex("3012301006022a03300a300806022a0406028001"), {216, 58, 58, 58}},
        {FromHex("3010300e06022a033008300606022a042600"), {231, 58, 58, 58}},
        {FromHex("3011300f06022a033009300706022a04290100"), {231, 58, 58, 58}},
        {FromHex("3010300e06022a033008300606022a042a00"), {231, 58, 58, 58}},
        {FromHex("3011300f06022a033009300706022a042d0101"), {231, 58, 58, 58}},
        {FromHex("3011300f06022a033009300706022a042e0101"), {231, 58, 58, 58}},
        {FromHex("3010300e06022a033008300606022a041000"), {156, 58, 58, 58}},
        {FromHex("3010300e06022a033008300606022a041100"), {156, 58, 58, 58}},
        {FromHex("3011300f06022a033009300706022a041e0141"), {214, 58, 58, 58}},
        {FromHex("3013301106022a03300b300906022a041c03414243"), {215, 58, 58, 58}},
        {FromHex("3013301106022a03300b300906022a041803616263"), {132, 58, 58, 58}},
        {FromHex("301f301d06022a033017301506022a04180f31393030303232393030303030305a"), {132, 58, 58, 58}},
        {FromHex("3013301106022a03300b300906022a041703616263"), {132, 58, 58, 58}},
        {FromHex("301d301b06022a033015301306022a04170d3939303232393030303030305a"), {132, 58, 58, 58}},
        {FromHex("3010300e06022a033008300606022a043080"), {137, 58, 58, 58}},
        {FromHex("3011300f06022a033009300706022a04308004"), {102, 58, 58, 58, 58}},
        {FromHex("3014301206022a03300c300a06022a04308030800000"), {137, 58, 58, 58}},
        {FromHex("300f300d06022a033007300506022a0404"), {102, 58, 58, 58, 58}},
        {FromHex("3011300f06022a033009300706022a04040561"), {155, 58, 58, 58, 58}},
        {FromHex("3012301006022a03300a300806022a0424020000"), {159, 58, 58, 58}},
        {FromHex("3013301106022a03300b300906022a042480040161"), {137, 58, 58, 58}},
        {FromHex("301e301c06022a033016301406022a04240e240c240a24082406240424020400"), {197, 58, 58, 58}},
        {FromHex("3012301006022a03300a300806022a0405000500"), {148, 58, 58}},
    };
    RequireMalformed(89, malformed);

    const std::vector<Bytes> unmodelled{
        FromHex("30800000"),
        FromHex("300430800000"),
        FromHex("300a300806022a0330800000"),
        FromHex("300c300a06022a03300430800000"),
        FromHex("3016301406022a03300e300c06082b0601050507020230800000"),
        FromHex("3018301606022a033010300e06082b06010505070202300230800000"),
        FromHex("301f301d06022a033017301506082b060105050702023009300716016f30800000"),
    };
    RequireUnmodelled(89, unmodelled);

    Require(DistinctQueues(AllocationFailures(89, FromHex("3000"), FreeCertificatePolicies)) ==
        std::vector<std::vector<int>>{{65}});

    const std::vector<std::vector<int>> complexFailures =
        AllocationFailures(89, complexMulti, FreeCertificatePolicies);
    Require(!complexFailures.empty());
    Require(complexFailures.front() == std::vector<int>{65});
    int mallocQueueCount = 0;
    for (const std::vector<int>& queue : complexFailures) {
        Require(!queue.empty());
        if (std::find(queue.begin(), queue.end(), 65) != queue.end()) ++mallocQueueCount;
    }
    Require(mallocQueueCount >= 20);

    const std::vector<std::vector<int>> otherOidFailures =
        AllocationFailures(89, FromHex("3012301006022a03300a300806022a0406022a03"), FreeCertificatePolicies);
    Require(!otherOidFailures.empty());
    for (const std::vector<int>& queue : otherOidFailures) Require(!queue.empty());

    const std::vector<std::vector<int>> otherIntFailures =
        AllocationFailures(89, FromHex("3012301006022a03300a300806022a0402020100"), FreeCertificatePolicies);
    Require(!otherIntFailures.empty());
    for (const std::vector<int>& queue : otherIntFailures) Require(!queue.empty());
}

void TestUnsupported() {
    Fixture serviceLocator(371, {0x30, 0x00});
    RequireNotImplemented([&] { X509V3_EXT_d2i_nid_postfix(&serviceLocator.extension); });
}

void TestUnknownExtensions() {
    static unsigned char privateOid[] = {0x2a, 0x03, 0x04, 0x05};
    Fixture undef(0, privateOid, 4, {0x03, 0x02, 0x05, 0xa0});
    Require(X509V3_EXT_d2i_nid_postfix(&undef.extension) == nullptr);
    Fixture nullObject(0, {0x03, 0x02, 0x05, 0xa0});
    nullObject.extension.object = nullptr;
    Require(X509V3_EXT_d2i_nid_postfix(&nullObject.extension) == nullptr);
    RequireQueue({});
}

void TestMethodTable() {
    for (const int nid : {143, 369, 663}) {
        Fixture removed(nid, {0x05, 0x00});
        Require(X509V3_EXT_d2i_nid_postfix(&removed.extension) == nullptr);
        RequireQueue({});
        Fixture malformed(nid, {0xff});
        Require(X509V3_EXT_d2i_nid_postfix(&malformed.extension) == nullptr);
        RequireQueue({});
    }
    for (const int nid : {1018, 1021}) {
        Fixture list(nid, {0x04, 0x02, 0x00, 0x00});
        RequireNotImplemented([&] { X509V3_EXT_d2i_nid_postfix(&list.extension); });
        Fixture empty(nid, {});
        RequireNotImplemented([&] { X509V3_EXT_d2i_nid_postfix(&empty.extension); });
    }
    RequireQueue({});
    void* const present = reinterpret_cast<void*>(std::uintptr_t{1});
    Fixture poison(1019, {0x05, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&poison.extension) == present);
    Fixture trailing(1019, {0x05, 0x00, 0x5a});
    Require(X509V3_EXT_d2i_nid_postfix(&trailing.extension) == present);
    Fixture longForm(1019, {0x05, 0x81, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&longForm.extension) == present);
    RequireQueue({});
    Fixture content(1019, {0x05, 0x01, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&content.extension) == nullptr);
    RequireQueue({144});
    Fixture empty(1019, {});
    Require(X509V3_EXT_d2i_nid_postfix(&empty.extension) == nullptr);
    RequireQueue({102, 58});
    Fixture wrongTag(1019, {0x04, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&wrongTag.extension) == nullptr);
    RequireQueue({168, 58});
    Fixture contextTag(1019, {0x85, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&contextTag.extension) == nullptr);
    RequireQueue({168, 58});
    Fixture truncated(1019, {0x05, 0x02, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&truncated.extension) == nullptr);
    RequireQueue({155, 58});
    Fixture constructed(1019, {0x25, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&constructed.extension) == nullptr);
    RequireQueue({231});
    Fixture constructedIndefinite(1019, {0x25, 0x80, 0x00, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&constructedIndefinite.extension) == nullptr);
    RequireQueue({231});
    Fixture indefinite(1019, {0x05, 0x80});
    Require(X509V3_EXT_d2i_nid_postfix(&indefinite.extension) == nullptr);
    RequireQueue({102, 58});
    Fixture badLength(1019, {0x05, 0xff});
    Require(X509V3_EXT_d2i_nid_postfix(&badLength.extension) == nullptr);
    RequireQueue({102, 58});
    Fixture negativeLength(1019, {0x05, 0x00});
    negativeLength.encoded.length = -1;
    Require(X509V3_EXT_d2i_nid_postfix(&negativeLength.extension) == nullptr);
    RequireQueue({});
    Fixture keyUsage(83, {0x03, 0x02, 0x05, 0xa0});
    CheckKeyUsage(keyUsage, 1, 0x0d, 0xa0);
    Fixture crlNumber(88, {0x02, 0x01, 0x05});
    auto* number = static_cast<Asn1String*>(X509V3_EXT_d2i_nid_postfix(&crlNumber.extension));
    Require(number != nullptr && number->length == 1 && number->data[0] == 0x05);
    FreeInteger(number);
    RequireQueue({});
}

void TestFaultingArguments() {
    RequireInvalidArgument([] { X509V3_EXT_d2i_nid_postfix(nullptr); });
    Fixture nullValue(83, {0x03, 0x02, 0x05, 0xa0});
    nullValue.extension.value = nullptr;
    RequireInvalidArgument([&] { X509V3_EXT_d2i_nid_postfix(&nullValue.extension); });
    Fixture nullData(83, {0x03, 0x02, 0x05, 0xa0});
    nullData.encoded.data = nullptr;
    RequireInvalidArgument([&] { X509V3_EXT_d2i_nid_postfix(&nullData.extension); });
    Fixture nullObjectData(83, {0x03, 0x02, 0x05, 0xa0});
    nullObjectData.object.data = nullptr;
    nullObjectData.object.length = 3;
    RequireInvalidArgument([&] { X509V3_EXT_d2i_nid_postfix(&nullObjectData.extension); });
    Fixture negativeLength(83, {0x03, 0x02, 0x05, 0xa0});
    negativeLength.encoded.length = -1;
    Require(X509V3_EXT_d2i_nid_postfix(&negativeLength.extension) == nullptr);
    RequireQueue({});
}

void TestMalformed() {
    Fixture empty(83, {});
    Require(X509V3_EXT_d2i_nid_postfix(&empty.extension) == nullptr);
    RequireQueue({102, 58});
    Fixture wrongTag(83, {0x04, 0x02, 0x05, 0xa0});
    Require(X509V3_EXT_d2i_nid_postfix(&wrongTag.extension) == nullptr);
    RequireQueue({168, 58});
    Fixture truncated(83, {0x03, 0x05, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&truncated.extension) == nullptr);
    RequireQueue({155, 58});
    Fixture constructed(83, {0x23, 0x03, 0x00, 0x00, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&constructed.extension) == nullptr);
    RequireQueue({231});
    Fixture indefinite(82, {0x04, 0x80, 0x00, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&indefinite.extension) == nullptr);
    RequireQueue({102, 58});
    Fixture noBits(83, {0x03, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&noBits.extension) == nullptr);
    RequireQueue({152});
    Fixture tooManyBits(83, {0x03, 0x02, 0x08, 0x00});
    Require(X509V3_EXT_d2i_nid_postfix(&tooManyBits.extension) == nullptr);
    RequireQueue({220});
    Fixture badLength(83, {0x03, 0xff});
    Require(X509V3_EXT_d2i_nid_postfix(&badLength.extension) == nullptr);
    RequireQueue({102, 58});
    Fixture reservedLength(82, {0x04, 0xff, 0x01});
    Require(X509V3_EXT_d2i_nid_postfix(&reservedLength.extension) == nullptr);
    RequireQueue({102, 58});
    Fixture overflowLength(82, {0x04, 0x89, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    Require(X509V3_EXT_d2i_nid_postfix(&overflowLength.extension) == nullptr);
    RequireQueue({102, 58});
    Fixture shortIdentifier(83, {0x1f});
    Require(X509V3_EXT_d2i_nid_postfix(&shortIdentifier.extension) == nullptr);
    RequireQueue({102, 58});
}

void TestAllocationFailures() {
    Fixture keyUsage(83, {0x03, 0x02, 0x05, 0xa0});
    allocationsUntilFailure = 0;
    Require(X509V3_EXT_d2i_nid_postfix(&keyUsage.extension) == nullptr);
    Require(allocations.empty());
    RequireQueue({65, 65});
    Fixture keyUsageSecond(83, {0x03, 0x02, 0x05, 0xa0});
    allocationsUntilFailure = 1;
    Require(X509V3_EXT_d2i_nid_postfix(&keyUsageSecond.extension) == nullptr);
    Require(allocations.empty());
    RequireQueue({65});
    Fixture integer(88, {0x02, 0x01, 0x2a});
    allocationsUntilFailure = 0;
    Require(X509V3_EXT_d2i_nid_postfix(&integer.extension) == nullptr);
    Require(allocations.empty());
    RequireQueue({});
    Fixture object(430, {0x06, 0x02, 0x2a, 0x03});
    allocationsUntilFailure = 0;
    Require(X509V3_EXT_d2i_nid_postfix(&object.extension) == nullptr);
    Require(allocations.empty());
    RequireQueue({});
    Fixture objectSecond(430, {0x06, 0x02, 0x2a, 0x03});
    allocationsUntilFailure = 1;
    Require(X509V3_EXT_d2i_nid_postfix(&objectSecond.extension) == nullptr);
    Require(allocations.empty());
    RequireQueue({65});
    Fixture authorityKeyId(90, {0x30, 0x06, 0x80, 0x01, 0x01, 0x82, 0x01, 0x01});
    allocationsUntilFailure = 0;
    Require(X509V3_EXT_d2i_nid_postfix(&authorityKeyId.extension) == nullptr);
    Require(allocations.empty());
    RequireQueue({65});
    allocationsUntilFailure = 1;
    Require(X509V3_EXT_d2i_nid_postfix(&authorityKeyId.extension) == nullptr);
    Require(allocations.empty());
    RequireQueue({65, 65});
    allocationsUntilFailure = 3;
    Require(X509V3_EXT_d2i_nid_postfix(&authorityKeyId.extension) == nullptr);
    Require(allocations.empty());
    DrainErrors();
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    DrainErrors();
    TestBitStrings();
    TestOctetStrings();
    TestIntegers();
    TestEnumerated();
    TestGeneralizedTime();
    TestIa5Strings();
    TestObjects();
    TestOcspNonce();
    TestAuthorityKeyId();
    TestAuthorityKeyIssuer();
    TestMalformedAuthorityKeyId();
    TestBasicConstraints();
    TestExtendedKeyUsage();
    TestPrivateKeyUsagePeriod();
    TestStructuredRoundTrips();
    TestNidResolution();
    TestGeneralNames();
    TestAuthorityInfoAccess();
    TestCrlDistPoints();
    TestCertificatePolicies();
    TestUnsupported();
    TestUnknownExtensions();
    TestMethodTable();
    TestFaultingArguments();
    TestMalformed();
    TestAllocationFailures();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 X509V3_EXT_d2i tests passed");
}
