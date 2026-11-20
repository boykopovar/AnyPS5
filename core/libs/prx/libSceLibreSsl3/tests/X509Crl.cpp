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

using Bytes = std::vector<unsigned char>;

std::unordered_map<void*, std::size_t> allocations;
int allocationsUntilFailure = -1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_CRL check failed at line %u\n", location.line());
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

Bytes Ascii(const char* text) {
    return Bytes(text, text + std::strlen(text));
}

Bytes Join(std::initializer_list<Bytes> parts) {
    Bytes result;
    for (const Bytes& part : parts) result.insert(result.end(), part.begin(), part.end());
    return result;
}

Bytes Tlv(unsigned char tag, const Bytes& content) {
    Bytes result;
    result.push_back(tag);
    std::size_t remaining = content.size();
    if (remaining < 0x80) {
        result.push_back(static_cast<unsigned char>(remaining));
    } else {
        Bytes length;
        while (remaining != 0) {
            length.insert(length.begin(), static_cast<unsigned char>(remaining & 0xff));
            remaining >>= 8;
        }
        result.push_back(static_cast<unsigned char>(0x80 | length.size()));
        result.insert(result.end(), length.begin(), length.end());
    }
    result.insert(result.end(), content.begin(), content.end());
    return result;
}

Bytes Sequence(std::initializer_list<Bytes> parts) {
    return Tlv(0x30, Join(parts));
}

Bytes Oid(std::initializer_list<unsigned char> content) {
    return Tlv(0x06, Bytes(content));
}

Bytes UtcTime(const char* text) {
    return Tlv(0x17, Ascii(text));
}

Bytes IssuerName(const char* commonName) {
    return Sequence({
        Tlv(0x31, Sequence({
            Oid({0x55, 0x04, 0x03}),
            Tlv(0x0c, Ascii(commonName)),
        })),
    });
}

Bytes SignatureAlgorithm() {
    return Sequence({
        Oid({0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b}),
        Tlv(0x05, Bytes{}),
    });
}

Bytes SignatureValue() {
    Bytes bits(32, 0x5a);
    bits.insert(bits.begin(), 0x00);
    return Tlv(0x03, bits);
}

Bytes Extension(std::initializer_list<unsigned char> oid, bool critical, const Bytes& value) {
    Bytes body = Oid(oid);
    if (critical) body = Join({body, Bytes{0x01, 0x01, 0xff}});
    body = Join({body, Tlv(0x04, value)});
    return Sequence({body});
}

Bytes CrlNumberExtension(int number) {
    return Extension({0x55, 0x1d, 0x14}, false, Tlv(0x02, Bytes{static_cast<unsigned char>(number)}));
}

Bytes CrlReasonExtension(unsigned char reason) {
    return Extension({0x55, 0x1d, 0x15}, false, Tlv(0x0a, Bytes{reason}));
}

Bytes RevokedEntry(const Bytes& serial) {
    return Sequence({Tlv(0x02, serial), UtcTime("240102000000Z")});
}

Bytes RevokedEntryWithExtensions(const Bytes& serial, const Bytes& extensions) {
    return Sequence({Tlv(0x02, serial), UtcTime("240102000000Z"), Tlv(0x30, extensions)});
}

Bytes TbsHead() {
    return Join({
        Tlv(0x02, Bytes{0x01}),
        SignatureAlgorithm(),
        IssuerName("Test CA"),
        UtcTime("240101000000Z"),
    });
}

Bytes BuildCrl(const Bytes& tbsContent) {
    return Sequence({
        Tlv(0x30, tbsContent),
        SignatureAlgorithm(),
        SignatureValue(),
    });
}

Bytes BuildCrl(const Bytes& tbsContent, const Bytes& outerAlgorithm) {
    return Sequence({
        Tlv(0x30, tbsContent),
        outerAlgorithm,
        SignatureValue(),
    });
}

LibreSsl3X509Crl* Parse(const Bytes& der, const unsigned char** consumed = nullptr) {
    const unsigned char* input = der.data();
    LibreSsl3X509Crl* crl = d2i_X509_CRL_nid_postfix(nullptr, &input, static_cast<std::int64_t>(der.size()));
    if (consumed) *consumed = input;
    return crl;
}

const LibreSslX509NameEntry* NameEntry(const LibreSslX509Name* name, int index) {
    if (!name || !name->entries || !name->entries->data) return nullptr;
    if (index < 0 || index >= name->entries->num) return nullptr;
    return reinterpret_cast<const LibreSslX509NameEntry*>(name->entries->data[index]);
}

const LibreSsl3Revoked* RevokedEntryAt(const LibreSsl3X509Crl* crl, int index) {
    if (!crl || !crl->crl || !crl->crl->revoked || !crl->crl->revoked->data) return nullptr;
    if (index < 0 || index >= crl->crl->revoked->num) return nullptr;
    return reinterpret_cast<const LibreSsl3Revoked*>(crl->crl->revoked->data[index]);
}

void FreeStringValue(Asn1String* value) {
    if (!value) return;
    if (value->data) free_nid_postfix(value->data);
    free_nid_postfix(value);
}

void FreeObjectValue(const LibreSslAsn1Object* object) {
    if (!object) return;
    if (object->flags & 0x08) free_nid_postfix(const_cast<unsigned char*>(object->data));
    free_nid_postfix(const_cast<LibreSslAsn1Object*>(object));
}

void FreeParameterValue(LibreSsl3Asn1Type* parameter) {
    if (!parameter) return;
    if (parameter->value) {
        if (parameter->type == 6) FreeObjectValue(static_cast<const LibreSslAsn1Object*>(parameter->value));
        else FreeStringValue(static_cast<Asn1String*>(parameter->value));
    }
    free_nid_postfix(parameter);
}

void FreeAlgorValue(LibreSsl3X509Algor* algor) {
    if (!algor) return;
    FreeObjectValue(static_cast<const LibreSslAsn1Object*>(algor->algorithm));
    FreeParameterValue(algor->parameter);
    free_nid_postfix(algor);
}

void FreeExtensionValue(void* value) {
    auto* extension = static_cast<LibreSsl3X509Extension*>(value);
    if (!extension) return;
    FreeObjectValue(extension->object);
    FreeStringValue(static_cast<Asn1String*>(extension->value));
    free_nid_postfix(extension);
}

void FreeNameEntryValue(void* value) {
    auto* entry = static_cast<LibreSslX509NameEntry*>(value);
    if (!entry) return;
    FreeObjectValue(static_cast<const LibreSslAsn1Object*>(entry->object));
    FreeStringValue(static_cast<Asn1String*>(entry->value));
    free_nid_postfix(entry);
}

void FreeStackValue(const LibreSsl3Stack* stack, void (*release)(void*)) {
    if (!stack) return;
    for (int index = 0; index < stack->num; ++index) release(stack->data[index]);
    sk_free_nid_postfix(const_cast<LibreSslStack*>(reinterpret_cast<const LibreSslStack*>(stack)));
}

void FreeRevokedValue(void* value) {
    auto* revoked = static_cast<LibreSsl3Revoked*>(value);
    if (!revoked) return;
    FreeStringValue(revoked->serialNumber);
    FreeStringValue(revoked->revocationDate);
    FreeStackValue(revoked->extensions, FreeExtensionValue);
    free_nid_postfix(revoked);
}

void FreeCrlValue(LibreSsl3X509Crl* crl) {
    X509_CRL_free_nid_postfix(crl);
}

void RequireString(const Asn1String* value, int type, const char* text) {
    Require(value != nullptr);
    Require(value->type == type);
    Require(value->length == static_cast<int>(std::strlen(text)));
    Require(value->data != nullptr);
    Require(std::memcmp(value->data, text, std::strlen(text)) == 0);
}

void RequireInteger(const Asn1String* value, std::initializer_list<unsigned char> bytes) {
    Require(value != nullptr);
    Require(value->type == 2 || value->type == 0x102);
    Require(value->length == static_cast<int>(bytes.size()));
    Require(value->data != nullptr);
    Require(std::memcmp(value->data, bytes.begin(), bytes.size()) == 0);
}

void SetCertificate(LibreSsl3Certificate& certificate, LibreSsl3CertificateInfo& info,
    const Asn1String* serial, const LibreSslX509Name* issuer) {
    info.serialNumber = const_cast<Asn1String*>(serial);
    info.issuer = const_cast<LibreSslX509Name*>(issuer);
    certificate = LibreSsl3Certificate{};
    certificate.certInfo = &info;
}

struct Serial {
    unsigned char data[2]{0x10, 0x00};
    Asn1String value{};

    explicit Serial(unsigned char low) {
        data[1] = low;
        value.length = 2;
        value.type = 2;
        value.data = data;
        value.flags = 0;
    }
};

void TestParsedStructure() {
    const Bytes der = BuildCrl(Join({
        TbsHead(),
        UtcTime("240201000000Z"),
        Tlv(0x30, Join({RevokedEntry(Bytes{0x10, 0x01}), RevokedEntry(Bytes{0x10, 0x02})})),
        Tlv(0xa0, Sequence({CrlNumberExtension(7)})),
    }));
    const unsigned char* consumed = nullptr;
    LibreSsl3X509Crl* crl = Parse(der, &consumed);
    Require(crl != nullptr);
    Require(consumed == der.data() + der.size());
    Require(crl->references == 1);
    Require(crl->flags == 0);
    Require(crl->crl != nullptr);

    RequireInteger(crl->crl->version, {0x01});

    Require(crl->crl->sigAlg != nullptr);
    const auto* innerAlgorithm = static_cast<const LibreSslAsn1Object*>(crl->crl->sigAlg->algorithm);
    Require(innerAlgorithm != nullptr && innerAlgorithm->length == 9 && innerAlgorithm->nid == 0);
    Require(std::memcmp(innerAlgorithm->data, "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b", 9) == 0);
    Require(crl->crl->sigAlg->parameter != nullptr);
    Require(crl->crl->sigAlg->parameter->type == 5);
    Require(crl->crl->sigAlg->parameter->value == nullptr);
    Require(crl->sigAlg != nullptr);
    Require(crl->sigAlg->parameter != nullptr && crl->sigAlg->parameter->type == 5);

    Require(crl->crl->issuer != nullptr);
    Require(crl->crl->issuer->entries != nullptr);
    Require(crl->crl->issuer->entries->num == 1);
    Require(crl->crl->issuer->modified == 0);
    Require(crl->crl->issuer->bytes == nullptr);
    Require(crl->crl->issuer->canonEnc == nullptr);
    Require(crl->crl->issuer->canonEnclen == 0);
    const LibreSslX509NameEntry* nameEntry = NameEntry(crl->crl->issuer, 0);
    Require(nameEntry != nullptr);
    Require(nameEntry->set == 0);
    Require(nameEntry->size == 0);
    const auto* nameObject = static_cast<const LibreSslAsn1Object*>(nameEntry->object);
    Require(nameObject != nullptr && nameObject->length == 3 && nameObject->nid == 0);
    Require(std::memcmp(nameObject->data, "\x55\x04\x03", 3) == 0);
    RequireString(static_cast<const Asn1String*>(nameEntry->value), 12, "Test CA");

    RequireString(crl->crl->lastUpdate, 23, "240101000000Z");
    RequireString(crl->crl->nextUpdate, 23, "240201000000Z");

    Require(crl->crl->revoked != nullptr);
    Require(crl->crl->revoked->num == 2);
    const LibreSsl3Revoked* first = RevokedEntryAt(crl, 0);
    Require(first != nullptr);
    RequireInteger(first->serialNumber, {0x10, 0x01});
    RequireString(first->revocationDate, 23, "240102000000Z");
    Require(first->extensions == nullptr);
    Require(first->issuer == nullptr);
    Require(first->reason == 0);
    const LibreSsl3Revoked* second = RevokedEntryAt(crl, 1);
    Require(second != nullptr);
    RequireInteger(second->serialNumber, {0x10, 0x02});

    Require(crl->crl->extensions != nullptr);
    Require(crl->crl->extensions->num == 1);

    Require(crl->signature != nullptr);
    Require(crl->signature->type == 3);
    Require(crl->signature->length == 32);
    Require((crl->signature->flags & 0x08) == 0x08);
    Require((crl->signature->flags & 0x07) == 0);
    Require(crl->signature->data[0] == 0x5a);

    FreeCrlValue(crl);
    Require(allocations.empty());
}

void TestOptionalFields() {
    const Bytes minimal = BuildCrl(Join({
        SignatureAlgorithm(),
        IssuerName("Test CA"),
        UtcTime("240101000000Z"),
    }));
    LibreSsl3X509Crl* crl = Parse(minimal);
    Require(crl != nullptr);
    Require(crl->crl->version == nullptr);
    Require(crl->crl->nextUpdate == nullptr);
    Require(crl->crl->revoked == nullptr);
    Require(crl->crl->extensions == nullptr);
    FreeCrlValue(crl);

    const Bytes emptyLists = BuildCrl(Join({
        TbsHead(),
        Tlv(0x30, Bytes{}),
        Tlv(0xa0, Sequence({})),
    }));
    crl = Parse(emptyLists);
    Require(crl != nullptr);
    Require(crl->crl->revoked != nullptr && crl->crl->revoked->num == 0);
    Require(crl->crl->extensions != nullptr && crl->crl->extensions->num == 0);
    FreeCrlValue(crl);

    const Bytes generalized = BuildCrl(Join({
        TbsHead(),
        Tlv(0x18, Ascii("20240201000000Z")),
    }));
    crl = Parse(generalized);
    Require(crl != nullptr);
    RequireString(crl->crl->nextUpdate, 24, "20240201000000Z");
    FreeCrlValue(crl);
    Require(allocations.empty());
}

void TestExtensions() {
    const Bytes der = BuildCrl(Join({
        TbsHead(),
        Tlv(0xa0, Sequence({Extension({0x55, 0x1d, 0x14}, true, Tlv(0x02, Bytes{0x2a}))})),
    }));
    LibreSsl3X509Crl* crl = Parse(der);
    Require(crl != nullptr);

    int crit = 99;
    int idx = -1;
    void* value = X509_CRL_get_ext_d2i_nid_postfix(crl, 88, &crit, &idx);
    Require(value != nullptr);
    Require(crit == 1);
    Require(idx == 0);
    RequireInteger(static_cast<const Asn1String*>(value), {0x2a});
    ASN1_INTEGER_free_nid_postfix(static_cast<Asn1String*>(value));

    crit = 99;
    idx = -1;
    Require(X509_CRL_get_ext_d2i_nid_postfix(crl, 141, &crit, &idx) == nullptr);
    Require(crit == -1);
    Require(idx == -1);

    value = X509_CRL_get_ext_d2i_nid_postfix(crl, 88, nullptr, nullptr);
    Require(value != nullptr);
    ASN1_INTEGER_free_nid_postfix(static_cast<Asn1String*>(value));

    crit = 99;
    value = X509_CRL_get_ext_d2i_nid_postfix(crl, 88, &crit, nullptr);
    Require(value != nullptr && crit == 1);
    ASN1_INTEGER_free_nid_postfix(static_cast<Asn1String*>(value));

    idx = -1;
    value = X509_CRL_get_ext_d2i_nid_postfix(crl, 88, nullptr, &idx);
    Require(value != nullptr && idx == 0);
    ASN1_INTEGER_free_nid_postfix(static_cast<Asn1String*>(value));

    FreeCrlValue(crl);

    const Bytes noExtensions = BuildCrl(Join({TbsHead(), UtcTime("240201000000Z")}));
    crl = Parse(noExtensions);
    Require(crl != nullptr);
    crit = 99;
    idx = -1;
    Require(X509_CRL_get_ext_d2i_nid_postfix(crl, 88, &crit, &idx) == nullptr);
    Require(crit == -1 && idx == -1);
    FreeCrlValue(crl);

    const Bytes duplicates = BuildCrl(Join({
        TbsHead(),
        Tlv(0xa0, Sequence({CrlNumberExtension(1), CrlNumberExtension(2)})),
    }));
    crl = Parse(duplicates);
    Require(crl != nullptr);
    crit = 99;
    Require(X509_CRL_get_ext_d2i_nid_postfix(crl, 88, &crit, nullptr) == nullptr);
    Require(crit == -2);
    idx = -1;
    value = X509_CRL_get_ext_d2i_nid_postfix(crl, 88, nullptr, &idx);
    Require(value != nullptr && idx == 0);
    ASN1_INTEGER_free_nid_postfix(static_cast<Asn1String*>(value));
    value = X509_CRL_get_ext_d2i_nid_postfix(crl, 88, nullptr, &idx);
    Require(value != nullptr && idx == 1);
    ASN1_INTEGER_free_nid_postfix(static_cast<Asn1String*>(value));
    idx = 1;
    Require(X509_CRL_get_ext_d2i_nid_postfix(crl, 88, nullptr, &idx) == nullptr);
    Require(idx == -1);
    FreeCrlValue(crl);
    DrainErrors();
    Require(allocations.empty());
}

void TestExtensionArgumentErrors() {
    const Bytes der = BuildCrl(Join({
        TbsHead(),
        Tlv(0xa0, Sequence({CrlNumberExtension(7)})),
    }));
    LibreSsl3X509Crl* crl = Parse(der);
    Require(crl != nullptr);
    RequireInvalidArgument([&] { X509_CRL_get_ext_d2i_nid_postfix(nullptr, 88, nullptr, nullptr); });
    LibreSsl3X509Crl withoutInfo{};
    RequireInvalidArgument([&] { X509_CRL_get_ext_d2i_nid_postfix(&withoutInfo, 88, nullptr, nullptr); });
    FreeCrlValue(crl);
    Require(allocations.empty());
}

void TestGet0ByCert() {
    const Bytes der = BuildCrl(Join({
        TbsHead(),
        UtcTime("240201000000Z"),
        Tlv(0x30, Join({RevokedEntry(Bytes{0x10, 0x01}), RevokedEntry(Bytes{0x10, 0x02})})),
    }));
    LibreSsl3X509Crl* crl = Parse(der);
    Require(crl != nullptr);

    Serial matching(0x01);
    LibreSsl3CertificateInfo matchingInfo{};
    LibreSsl3Certificate matchingCertificate{};
    SetCertificate(matchingCertificate, matchingInfo, &matching.value, nullptr);
    LibreSsl3Revoked* found = nullptr;
    Require(X509_CRL_get0_by_cert_nid_postfix(crl, &found, &matchingCertificate) == 1);
    Require(reinterpret_cast<const void*>(found) == reinterpret_cast<const void*>(RevokedEntryAt(crl, 0)));

    found = nullptr;
    Require(X509_CRL_get0_by_cert_nid_postfix(crl, nullptr, &matchingCertificate) == 1);

    Serial unmatched(0x03);
    LibreSsl3CertificateInfo unmatchedInfo{};
    LibreSsl3Certificate unmatchedCertificate{};
    SetCertificate(unmatchedCertificate, unmatchedInfo, &unmatched.value, nullptr);
    found = nullptr;
    Require(X509_CRL_get0_by_cert_nid_postfix(crl, &found, &unmatchedCertificate) == 0);
    Require(found == nullptr);

    LibreSsl3CertificateInfo withoutSerialInfo{};
    LibreSsl3Certificate withoutSerial{};
    SetCertificate(withoutSerial, withoutSerialInfo, nullptr, nullptr);
    Require(X509_CRL_get0_by_cert_nid_postfix(crl, nullptr, &withoutSerial) == -1);
    Require(X509_CRL_get0_by_cert_nid_postfix(crl, nullptr, nullptr) == -1);
    Require(X509_CRL_get0_by_cert_nid_postfix(nullptr, nullptr, &matchingCertificate) == -1);
    LibreSsl3X509Crl withoutInfo{};
    Require(X509_CRL_get0_by_cert_nid_postfix(&withoutInfo, nullptr, &matchingCertificate) == -1);

    LibreSsl3CertificateInfo matchingIssuerInfo{};
    LibreSsl3Certificate matchingIssuer{};
    SetCertificate(matchingIssuer, matchingIssuerInfo, &matching.value, crl->crl->issuer);
    Require(X509_CRL_get0_by_cert_nid_postfix(crl, nullptr, &matchingIssuer) == 1);

    const Bytes otherDer = BuildCrl(Join({
        Tlv(0x02, Bytes{0x01}),
        SignatureAlgorithm(),
        IssuerName("Other CA"),
        UtcTime("240101000000Z"),
        Tlv(0x30, Join({RevokedEntry(Bytes{0x10, 0x01})})),
    }));
    LibreSsl3X509Crl* other = Parse(otherDer);
    Require(other != nullptr);
    LibreSsl3CertificateInfo mismatchedInfo{};
    LibreSsl3Certificate mismatched{};
    SetCertificate(mismatched, mismatchedInfo, &matching.value, other->crl->issuer);
    Require(X509_CRL_get0_by_cert_nid_postfix(crl, nullptr, &mismatched) == 0);
    Require(X509_CRL_get0_by_cert_nid_postfix(other, nullptr, &mismatched) == 1);
    LibreSsl3CertificateInfo sameSerialInfo{};
    LibreSsl3Certificate sameSerial{};
    SetCertificate(sameSerial, sameSerialInfo, &matching.value, nullptr);
    Require(X509_CRL_get0_by_cert_nid_postfix(other, nullptr, &sameSerial) == 1);
    FreeCrlValue(crl);
    FreeCrlValue(other);
    Require(allocations.empty());
}

void TestRemoveFromCrl() {
    const Bytes der = BuildCrl(Join({
        TbsHead(),
        Tlv(0x30, Join({
            RevokedEntryWithExtensions(Bytes{0x10, 0x01}, CrlReasonExtension(8)),
            RevokedEntryWithExtensions(Bytes{0x10, 0x02}, CrlReasonExtension(1)),
        })),
    }));
    LibreSsl3X509Crl* crl = Parse(der);
    Require(crl != nullptr);
    Require(RevokedEntryAt(crl, 0)->extensions != nullptr && RevokedEntryAt(crl, 0)->extensions->num == 1);
    Require(RevokedEntryAt(crl, 0)->reason == 8);
    Require(RevokedEntryAt(crl, 1)->reason == 1);
    Serial removed(0x01);
    LibreSsl3CertificateInfo removedInfo{};
    LibreSsl3Certificate removedCertificate{};
    SetCertificate(removedCertificate, removedInfo, &removed.value, nullptr);
    Require(X509_CRL_get0_by_cert_nid_postfix(crl, nullptr, &removedCertificate) == 2);
    Serial retained(0x02);
    LibreSsl3CertificateInfo retainedInfo{};
    LibreSsl3Certificate retainedCertificate{};
    SetCertificate(retainedCertificate, retainedInfo, &retained.value, nullptr);
    Require(X509_CRL_get0_by_cert_nid_postfix(crl, nullptr, &retainedCertificate) == 1);
    FreeCrlValue(crl);
    Require(allocations.empty());
}

void TestTrailingData() {
    Bytes der = BuildCrl(Join({TbsHead(), UtcTime("240201000000Z")}));
    const std::size_t length = der.size();
    der.push_back(0x30);
    der.push_back(0x00);
    const unsigned char* consumed = nullptr;
    LibreSsl3X509Crl* crl = Parse(der, &consumed);
    Require(crl != nullptr);
    Require(consumed == der.data() + length);
    FreeCrlValue(crl);
    Require(allocations.empty());
}

void TestMalformed() {
    const Bytes der = BuildCrl(Join({
        TbsHead(),
        UtcTime("240201000000Z"),
        Tlv(0x30, Join({RevokedEntry(Bytes{0x10, 0x01})})),
        Tlv(0xa0, Sequence({CrlNumberExtension(7)})),
    }));
    for (std::size_t length = 0; length < der.size(); ++length) {
        const unsigned char* input = der.data();
        LibreSsl3X509Crl* crl = d2i_X509_CRL_nid_postfix(nullptr, &input, static_cast<std::int64_t>(length));
        Require(crl == nullptr);
        Require(input == der.data());
        Require(allocations.empty());
    }
    DrainErrors();

    const Bytes wrongOuter = Join({Tlv(0x31, Bytes{0x00})});
    Require(Parse(wrongOuter) == nullptr);
    RequireQueue({168});

    const Bytes truncatedSequence = Join({Bytes{0x30, 0x05, 0x30}});
    Require(Parse(truncatedSequence) == nullptr);
    RequireQueue({155});

    const Bytes indefinite = Join({Bytes{0x30, 0x80, 0x00, 0x00}});
    Require(Parse(indefinite) == nullptr);
    RequireQueue({102});

    const Bytes badLength = Join({Bytes{0x30, 0x84, 0x00, 0x00, 0x00, 0x00}});
    Require(Parse(badLength) == nullptr);
    RequireQueue({102});

    const Bytes extraField = BuildCrl(Join({TbsHead(), Bytes{0x05, 0x00}}));
    Require(Parse(extraField) == nullptr);
    RequireQueue({148});

    const Bytes shortTbs = BuildCrl(Join({Tlv(0x02, Bytes{0x01})}));
    Require(Parse(shortTbs) == nullptr);
    Require(allocations.empty());
    DrainErrors();

    const Bytes missingSignature = Sequence({Tlv(0x30, TbsHead()), SignatureAlgorithm()});
    Require(Parse(missingSignature) == nullptr);
    Require(allocations.empty());
    DrainErrors();

    const Bytes extraOuterField = Sequence({
        Tlv(0x30, TbsHead()),
        SignatureAlgorithm(),
        SignatureValue(),
        Bytes{0x05, 0x00},
    });
    Require(Parse(extraOuterField) == nullptr);
    RequireQueue({148});

    const Bytes emptyBitString = Sequence({
        Tlv(0x30, TbsHead()),
        SignatureAlgorithm(),
        Tlv(0x03, Bytes{}),
    });
    Require(Parse(emptyBitString) == nullptr);
    RequireQueue({152});

    const Bytes badUnusedBits = Sequence({
        Tlv(0x30, TbsHead()),
        SignatureAlgorithm(),
        Tlv(0x03, Bytes{0x08, 0x5a}),
    });
    Require(Parse(badUnusedBits) == nullptr);
    RequireQueue({220});

    const Bytes emptyOuterAlgorithm = BuildCrl(TbsHead(), Sequence({}));
    Require(Parse(emptyOuterAlgorithm) == nullptr);
    Require(allocations.empty());
    DrainErrors();

    const Bytes badOid = BuildCrl(Join({
        TbsHead(),
        Tlv(0xa0, Sequence({Extension({0x80, 0x01}, false, Tlv(0x02, Bytes{0x07}))})),
    }));
    Require(Parse(badOid) == nullptr);
    RequireQueue({216});

    const Bytes badBoolean = BuildCrl(Join({
        TbsHead(),
        Tlv(0xa0, Sequence({Sequence({
            Oid({0x55, 0x1d, 0x14}),
            Bytes{0x01, 0x02, 0xff, 0xff},
            Tlv(0x04, Tlv(0x02, Bytes{0x07})),
        })})),
    }));
    Require(Parse(badBoolean) == nullptr);
    RequireQueue({62});

    const Bytes emptySet = BuildCrl(Join({TbsHead(), UtcTime("240201000000Z"), Tlv(0x31, Bytes{})}));
    Require(Parse(emptySet) == nullptr);
    RequireQueue({148});

    Require(allocations.empty());
    DrainErrors();
}

void TestArgumentErrors() {
    const Bytes der = BuildCrl(Join({TbsHead(), UtcTime("240201000000Z")}));
    const unsigned char* input = der.data();
    Require(d2i_X509_CRL_nid_postfix(nullptr, nullptr, static_cast<std::int64_t>(der.size())) == nullptr);
    RequireQueue({102});
    Require(d2i_X509_CRL_nid_postfix(nullptr, &input, -1) == nullptr);
    RequireQueue({102});
    const unsigned char* nullInput = nullptr;
    Require(d2i_X509_CRL_nid_postfix(nullptr, &nullInput, static_cast<std::int64_t>(der.size())) == nullptr);
    RequireQueue({102});
    Require(input == der.data());

    LibreSsl3X509Crl* output = nullptr;
    Require(d2i_X509_CRL_nid_postfix(&output, &input, static_cast<std::int64_t>(der.size())) == output);
    Require(output != nullptr);
    Require(input == der.data() + der.size());
    const std::size_t retained = allocations.size();

    LibreSsl3X509Crl* const first = output;
    const unsigned char* second = der.data();
    Require(d2i_X509_CRL_nid_postfix(&output, &second, static_cast<std::int64_t>(der.size())) == output);
    Require(output != nullptr && output != first);
    Require(second == der.data() + der.size());
    Require(allocations.size() == retained);
    X509_CRL_free_nid_postfix(output);
    Require(allocations.empty());
    DrainErrors();
}

void TestReferenceCountedFree() {
    const Bytes der = BuildCrl(Join({TbsHead(), UtcTime("240201000000Z")}));
    const unsigned char* input = der.data();
    LibreSsl3X509Crl* crl = d2i_X509_CRL_nid_postfix(nullptr, &input, static_cast<std::int64_t>(der.size()));
    Require(crl != nullptr);
    const std::size_t live = allocations.size();
    crl->references = 2;
    X509_CRL_free_nid_postfix(crl);
    Require(crl->references == 1);
    Require(allocations.size() == live);
    X509_CRL_free_nid_postfix(crl);
    Require(allocations.empty());
    X509_CRL_free_nid_postfix(nullptr);
    DrainErrors();
}

void TestAllocationFailures() {
    const Bytes der = BuildCrl(Join({
        TbsHead(),
        UtcTime("240201000000Z"),
        Tlv(0x30, Join({RevokedEntry(Bytes{0x10, 0x01})})),
        Tlv(0xa0, Sequence({CrlNumberExtension(7)})),
    }));
    bool completed = false;
    for (int budget = 0; budget < 60; ++budget) {
        const unsigned char* input = der.data();
        allocationsUntilFailure = budget;
        LibreSsl3X509Crl* crl = d2i_X509_CRL_nid_postfix(nullptr, &input, static_cast<std::int64_t>(der.size()));
        allocationsUntilFailure = -1;
        if (crl) {
            FreeCrlValue(crl);
            completed = true;
            break;
        }
        Require(allocations.empty());
        DrainErrors();
    }
    Require(completed);
    Require(allocations.empty());
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch),
        reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    DrainErrors();
    TestParsedStructure();
    TestOptionalFields();
    TestExtensions();
    TestExtensionArgumentErrors();
    TestGet0ByCert();
    TestRemoveFromCrl();
    TestTrailingData();
    TestMalformed();
    TestArgumentErrors();
    TestReferenceCountedFree();
    TestAllocationFailures();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 X509_CRL tests passed");
}
