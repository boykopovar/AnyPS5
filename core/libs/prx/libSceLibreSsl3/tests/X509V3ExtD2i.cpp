#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
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
    void* issuer;
    Asn1String* serial;
};

void FreeAuthorityKeyId(AuthorityKeyId* id) {
    FreeOctetString(id->keyId);
    FreeInteger(id->serial);
    free_nid_postfix(id);
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
             {0x30, 0x0c, 0x80, 0x01, 0x01, 0xa1, 0x04, 0x82, 0x02, 'a', 'b', 0x82, 0x01, 0x01},
             {0x30, 0x80, 0x80, 0x01, 0x01, 0x00, 0x00},
             {0x30, 0x05, 0xa0, 0x03, 0x04, 0x01, 0x01}}) {
        Fixture unsupported(90, bytes);
        RequireNotImplemented([&] { X509V3_EXT_d2i_nid_postfix(&unsupported.extension); });
        Require(allocations.empty());
    }
    DrainErrors();
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

void TestUnsupported() {
    Fixture basicConstraints(87, {0x30, 0x00});
    RequireNotImplemented([&] { X509V3_EXT_d2i_nid_postfix(&basicConstraints.extension); });
    Fixture subjectAltName(85, {0x30, 0x00});
    RequireNotImplemented([&] { X509V3_EXT_d2i_nid_postfix(&subjectAltName.extension); });
    Fixture ocspNoCheck(369, {0x05, 0x00});
    RequireNotImplemented([&] { X509V3_EXT_d2i_nid_postfix(&ocspNoCheck.extension); });
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
    std::array<void*, 7> api{
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
    TestMalformedAuthorityKeyId();
    TestNidResolution();
    TestUnsupported();
    TestUnknownExtensions();
    TestFaultingArguments();
    TestMalformed();
    TestAllocationFailures();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 X509V3_EXT_d2i tests passed");
}
