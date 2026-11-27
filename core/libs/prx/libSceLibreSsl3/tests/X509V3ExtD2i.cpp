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
    const auto issuerName = [id](int index, int type) {
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

    RequireMalformed(85, {
        {{}, {102, 58}},
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
        FromHex("3004a1028000"),
        FromHex("3004a7028000"),
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
        FromHex("300a300806022a03a1028000"),
        FromHex("300a300806022a03a3800000"),
        FromHex("301a300e06082b0601050507300186026162300806022a03a1028000"),
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

void TestUnsupported() {
    Fixture certificatePolicies(89, {0x30, 0x00});
    RequireNotImplemented([&] { X509V3_EXT_d2i_nid_postfix(&certificatePolicies.extension); });
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
    TestUnsupported();
    TestUnknownExtensions();
    TestMethodTable();
    TestFaultingArguments();
    TestMalformed();
    TestAllocationFailures();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 X509V3_EXT_d2i tests passed");
}
