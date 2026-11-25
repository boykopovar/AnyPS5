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

Bytes Hex(const char* text) {
    Bytes result;
    for (std::size_t index = 0; text[index] != '\0' && text[index + 1] != '\0'; index += 2) {
        unsigned int value = 0;
        Require(std::sscanf(text + index, "%2x", &value) == 1);
        result.push_back(static_cast<unsigned char>(value));
    }
    return result;
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

Bytes GeneralizedTime(const char* text) {
    return Tlv(0x18, Ascii(text));
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

Bytes CertificateIssuerExtension(const char* commonName) {
    return Extension({0x55, 0x1d, 0x1d}, true, Sequence({Tlv(0xa4, IssuerName(commonName))}));
}

Bytes InvalidityDateExtension() {
    return Extension({0x55, 0x1d, 0x18}, false, GeneralizedTime("20240102000000Z"));
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

Bytes CrlWithIssuer(const Bytes& name) {
    return BuildCrl(Join({Tlv(0x02, Bytes{0x01}), SignatureAlgorithm(), name, UtcTime("240101000000Z")}));
}

Bytes CommonName(const Bytes& value) {
    return Sequence({Tlv(0x31, Sequence({Oid({0x55, 0x04, 0x03}), value}))});
}

struct NameBuffer {
    std::size_t length;
    char* data;
    std::size_t max;
};

void RequireNameEncodings(const LibreSslX509Name* name, const Bytes& der, const Bytes& canonical) {
    Require(name != nullptr);
    Require(name->modified == 0);
    const auto* buffer = static_cast<const NameBuffer*>(name->bytes);
    Require(buffer != nullptr && buffer->data != nullptr);
    Require(buffer->length == der.size());
    Require(buffer->max == (der.size() + 3) / 3 * 4);
    Require(std::memcmp(buffer->data, der.data(), der.size()) == 0);
    Require(name->canonEnclen == static_cast<int>(canonical.size()));
    if (canonical.empty()) {
        Require(name->canonEnc == nullptr);
    } else {
        Require(name->canonEnc != nullptr);
        Require(std::memcmp(name->canonEnc, canonical.data(), canonical.size()) == 0);
    }
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
    RequireNameEncodings(crl->crl->issuer, IssuerName("Test CA"), Hex("3110300e06035504030c0774657374206361"));
    const LibreSslX509NameEntry* nameEntry = NameEntry(crl->crl->issuer, 0);
    Require(nameEntry != nullptr);
    Require(nameEntry->set == 0);
    Require(nameEntry->size == 0);
    const auto* nameObject = static_cast<const LibreSslAsn1Object*>(nameEntry->object);
    Require(nameObject != nullptr && nameObject->length == 3 && nameObject->nid == 0);
    Require(std::memcmp(nameObject->data, "\x55\x04\x03", 3) == 0);
    RequireString(static_cast<const Asn1String*>(nameEntry->value), 12, "Test CA");
    Require(static_cast<const Asn1String*>(nameEntry->value)->flags == 0x40);

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

int LookupSerial(LibreSsl3X509Crl* crl, int low, const LibreSslX509Name* issuer, LibreSsl3Revoked** found) {
    Serial serial(static_cast<unsigned char>(low));
    LibreSsl3CertificateInfo info{};
    LibreSsl3Certificate certificate{};
    SetCertificate(certificate, info, &serial.value, issuer);
    return X509_CRL_get0_by_cert_nid_postfix(crl, found, &certificate);
}

void TestCertificateIssuerEntries() {
    LibreSsl3X509Crl* crl = Parse(BuildCrl(Join({
        TbsHead(),
        Tlv(0x30, Join({
            RevokedEntry(Bytes{0x10, 0x01}),
            RevokedEntryWithExtensions(Bytes{0x10, 0x02}, CertificateIssuerExtension("Other CA")),
            RevokedEntry(Bytes{0x10, 0x03}),
            RevokedEntryWithExtensions(Bytes{0x10, 0x04}, CrlReasonExtension(8)),
        })),
    })));
    Require(crl != nullptr);
    LibreSsl3X509Crl* other = Parse(CrlWithIssuer(IssuerName("Other CA")));
    Require(other != nullptr);
    const LibreSslX509Name* crlIssuer = crl->crl->issuer;
    const LibreSslX509Name* otherIssuer = other->crl->issuer;
    Require(RevokedEntryAt(crl, 1)->extensions != nullptr && RevokedEntryAt(crl, 1)->extensions->num == 1);
    for (int index = 0; index < 4; ++index) Require(RevokedEntryAt(crl, index)->issuer == nullptr);
    Require(RevokedEntryAt(crl, 3)->reason == 8);

    LibreSsl3Revoked* found = nullptr;
    Require(LookupSerial(crl, 0x01, nullptr, &found) == 1);
    Require(reinterpret_cast<const void*>(found) == reinterpret_cast<const void*>(RevokedEntryAt(crl, 0)));
    Require(LookupSerial(crl, 0x01, crlIssuer, nullptr) == 1);
    Require(LookupSerial(crl, 0x01, otherIssuer, nullptr) == 0);
    Require(LookupSerial(crl, 0x05, nullptr, nullptr) == 0);
    Require(LookupSerial(crl, 0x05, otherIssuer, nullptr) == 0);

    for (const int low : {0x02, 0x03, 0x04}) {
        for (const LibreSslX509Name* issuer : {otherIssuer, crlIssuer, static_cast<const LibreSslX509Name*>(nullptr)}) {
            found = nullptr;
            RequireNotImplemented([&] { LookupSerial(crl, low, issuer, &found); });
            Require(found == nullptr);
        }
    }

    LibreSsl3X509Crl* second = Parse(BuildCrl(Join({
        TbsHead(),
        Tlv(0x30, RevokedEntryWithExtensions(Bytes{0x10, 0x01},
            Join({CrlReasonExtension(1), CertificateIssuerExtension("Other CA")}))),
    })));
    Require(second != nullptr);
    Require(RevokedEntryAt(second, 0)->extensions->num == 2 && RevokedEntryAt(second, 0)->reason == 1);
    RequireNotImplemented([&] { LookupSerial(second, 0x01, otherIssuer, nullptr); });
    Require(LookupSerial(second, 0x02, otherIssuer, nullptr) == 0);

    LibreSsl3X509Crl* direct = Parse(BuildCrl(Join({
        TbsHead(),
        Tlv(0x30, Join({
            RevokedEntryWithExtensions(Bytes{0x10, 0x01}, CrlReasonExtension(1)),
            RevokedEntryWithExtensions(Bytes{0x10, 0x02}, Join({InvalidityDateExtension(), CrlReasonExtension(8)})),
        })),
    })));
    Require(direct != nullptr);
    Require(LookupSerial(direct, 0x01, crlIssuer, nullptr) == 1);
    Require(LookupSerial(direct, 0x02, nullptr, &found) == 2);
    Require(reinterpret_cast<const void*>(found) == reinterpret_cast<const void*>(RevokedEntryAt(direct, 1)));
    Require(LookupSerial(direct, 0x02, otherIssuer, nullptr) == 0);

    FreeCrlValue(direct);
    FreeCrlValue(second);
    FreeCrlValue(other);
    FreeCrlValue(crl);
    RequireQueue({});
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

    const Bytes badLength = Join({Bytes{0x30, 0x84, 0x00, 0x00, 0x00}});
    Require(Parse(badLength) == nullptr);
    RequireQueue({123});

    const Bytes longLength = Bytes{0x30, 0x89, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Require(Parse(longLength) == nullptr);
    RequireQueue({123});

    Require(Parse(Bytes{0x30}) == nullptr);
    RequireQueue({123});

    const unsigned char* emptyInput = der.data();
    Require(d2i_X509_CRL_nid_postfix(nullptr, &emptyInput, 0) == nullptr);
    Require(emptyInput == der.data());
    RequireQueue({123});

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

Bytes CrlWithTime(int position, const Bytes& time) {
    const Bytes good = UtcTime("240101000000Z");
    return BuildCrl(Join({
        Tlv(0x02, Bytes{0x01}),
        SignatureAlgorithm(),
        IssuerName("Test CA"),
        position == 0 ? time : good,
        position == 1 ? time : good,
        Tlv(0x30, Join({Sequence({Tlv(0x02, Bytes{0x10, 0x01}), position == 2 ? time : good})})),
    }));
}

void TestTimes() {
    for (const Bytes& time : {UtcTime("240229235959Z"), UtcTime("491231000000Z"), UtcTime("500101000000Z"),
             GeneralizedTime("20500101000000Z"), GeneralizedTime("20000229000000Z"), GeneralizedTime("00000101000000Z"),
             GeneralizedTime("99991231235959Z")}) {
        for (int position = 0; position < 3; ++position) {
            LibreSsl3X509Crl* crl = Parse(CrlWithTime(position, time));
            Require(crl != nullptr);
            FreeCrlValue(crl);
            Require(allocations.empty());
        }
    }
    RequireQueue({});

    for (const Bytes& time : {UtcTime("2401010000Z"), UtcTime("240101000000"), UtcTime("240101000000z"), UtcTime("240101000000+0000"),
             UtcTime("240101000000ZZ"), UtcTime("241301000000Z"), UtcTime("240001000000Z"), UtcTime("240100000000Z"),
             UtcTime("230229000000Z"), UtcTime("240431000000Z"), UtcTime("240101240000Z"), UtcTime("240101006000Z"),
             UtcTime("240101000060Z"), UtcTime("24010100000 Z"), UtcTime("2401010000-1Z"), UtcTime(""),
             UtcTime("20240101000000Z"), GeneralizedTime("240101000000Z"), GeneralizedTime("20240101000000.0Z"),
             GeneralizedTime("202401010000Z"), GeneralizedTime("21000229000000Z"), GeneralizedTime("20240101000000")}) {
        for (int position = 0; position < 3; ++position) {
            Require(Parse(CrlWithTime(position, time)) == nullptr);
            RequireQueue({132});
            Require(allocations.empty());
        }
    }
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
    Require(output != nullptr);
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

void TestReusedOutput() {
    const Bytes der = BuildCrl(Join({TbsHead(), UtcTime("240201000000Z")}));
    const Bytes truncated(der.begin(), der.end() - 1);
    Bytes wrongTag = der;
    wrongTag[0] = 0x31;
    const Bytes inner = BuildCrl(Join({TbsHead(), Tlv(0x04, Bytes{0x00})}));
    const std::int64_t size = static_cast<std::int64_t>(der.size());

    LibreSsl3X509Crl* first = Parse(der);
    Require(first != nullptr);
    const std::size_t live = allocations.size();
    first->references = 2;
    LibreSsl3X509Crl* output = first;
    const unsigned char* input = truncated.data();
    Require(d2i_X509_CRL_nid_postfix(&output, &input, static_cast<std::int64_t>(truncated.size())) == nullptr);
    Require(output == nullptr);
    Require(input == truncated.data());
    Require(allocations.size() == live);
    Require(first->references == 1);
    Require(ERR_get_error_nid_postfix() != 0);
    DrainErrors();

    output = first;
    input = wrongTag.data();
    Require(d2i_X509_CRL_nid_postfix(&output, &input, size) == nullptr);
    Require(output == nullptr);
    Require(input == wrongTag.data());
    Require(allocations.empty());
    DrainErrors();

    LibreSsl3X509Crl* kept = Parse(der);
    Require(kept != nullptr);
    Require(allocations.size() == live);
    kept->references = 2;
    output = kept;
    input = inner.data();
    Require(d2i_X509_CRL_nid_postfix(&output, &input, static_cast<std::int64_t>(inner.size())) == nullptr);
    Require(output == nullptr);
    Require(input == inner.data());
    Require(allocations.size() == live);
    Require(kept->references == 1);
    DrainErrors();

    kept->references = 2;
    output = kept;
    input = der.data();
    Require(d2i_X509_CRL_nid_postfix(&output, &input, 0) == nullptr);
    Require(output == nullptr);
    Require(input == der.data());
    Require(allocations.size() == live);
    Require(kept->references == 1);
    DrainErrors();

    output = kept;
    Require(d2i_X509_CRL_nid_postfix(&output, &input, -1) == nullptr);
    Require(output == kept);
    Require(input == der.data());
    Require(allocations.size() == live);
    Require(kept->references == 1);
    DrainErrors();

    output = nullptr;
    input = truncated.data();
    Require(d2i_X509_CRL_nid_postfix(&output, &input, static_cast<std::int64_t>(truncated.size())) == nullptr);
    Require(output == nullptr);
    Require(allocations.size() == live);
    DrainErrors();

    kept->references = 2;
    output = kept;
    input = der.data();
    LibreSsl3X509Crl* replaced = d2i_X509_CRL_nid_postfix(&output, &input, size);
    Require(replaced != nullptr && replaced == output && replaced != kept);
    Require(input == der.data() + der.size());
    Require(allocations.size() == 2 * live);
    Require(kept->references == 1);
    X509_CRL_free_nid_postfix(replaced);
    X509_CRL_free_nid_postfix(kept);
    Require(allocations.empty());
    RequireQueue({});
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

Bytes NonMinimal(unsigned char tag, const Bytes& content) {
    Bytes result{tag, 0x82, static_cast<unsigned char>(content.size() >> 8), static_cast<unsigned char>(content.size())};
    result.insert(result.end(), content.begin(), content.end());
    return result;
}

void TestNonMinimalLengths() {
    const Bytes der = NonMinimal(0x30, Join({
        NonMinimal(0x30, Join({
            NonMinimal(0x02, Bytes{0x01}),
            SignatureAlgorithm(),
            IssuerName("Test CA"),
            NonMinimal(0x17, Ascii("240101000000Z")),
            Tlv(0x30, Sequence({NonMinimal(0x02, Bytes{0x10, 0x01}), UtcTime("240102000000Z")})),
        })),
        SignatureAlgorithm(),
        Bytes{0x03, 0x81, 0x02, 0x00, 0x5a},
    }));
    const unsigned char* consumed = nullptr;
    LibreSsl3X509Crl* crl = Parse(der, &consumed);
    Require(crl != nullptr);
    Require(consumed == der.data() + der.size());
    RequireInteger(crl->crl->version, {0x01});
    RequireString(crl->crl->lastUpdate, 23, "240101000000Z");
    RequireInteger(RevokedEntryAt(crl, 0)->serialNumber, {0x10, 0x01});
    Require(crl->signature->length == 1 && crl->signature->data[0] == 0x5a);
    FreeCrlValue(crl);
    RequireQueue({});
    Require(allocations.empty());
}

void TestRejectedNameValues() {
    struct Case {
        const char* value;
        int reason;
    };
    for (const Case& rejected : {
             Case{"1a024162", 140}, Case{"15024162", 140}, Case{"1b024162", 140}, Case{"04024162", 140},
             Case{"1f15024162", 140}, Case{"8c024162", 139}, Case{"9f0c024162", 139}, Case{"4c024162", 139},
             Case{"0000", 159}, Case{"1000", 156}, Case{"1e0100", 214}, Case{"1c020000", 215},
             Case{"0c02c080", 134}, Case{"0c03eda080", 134}, Case{"0c04f4908080", 134}, Case{"1e02d800", 124},
             Case{"1e04d83dde00", 124}, Case{"1c0400110000", 124}, Case{"1c040000dfff", 124}, Case{"2c80040141", 137},
             Case{"2c0400000401", 159}, Case{"2c102c0e2c0c2c0a2c082c062c040c024162", 197}, Case{"2c0404850141", 123},
             Case{"0300", 152}, Case{"03020841", 220}, Case{"2304030208ff", 220}, Case{"30803000", 137},
             Case{"0c8a01000000000000000000", 123}, Case{"0c80", 123}, Case{"0c", 123}, Case{"0c03", 155},
             Case{"1fffffffff7f024162", 123}, Case{"0c8880000000000000004162", 123}, Case{"0c887fffffffffffffff4162", 155}}) {
        Require(Parse(CrlWithIssuer(CommonName(Hex(rejected.value)))) == nullptr);
        RequireQueue({rejected.reason});
        Require(allocations.empty());
    }
}

void TestNameValueTypes() {
    struct Case {
        const char* value;
        int type;
        const char* data;
        std::int64_t flags;
        const char* canonical;
    };
    for (const Case& accepted : {
             Case{"07024162", 7, "4162", 0x40, "310b3009060355040307024162"},
             Case{"1d024162", 29, "4162", 0x40, "310b300906035504031d024162"},
             Case{"12023132", 18, "3132", 0x40, "310b3009060355040312023132"},
             Case{"1f0c024162", 12, "4162", 0x40, "310b300906035504030c026162"},
             Case{"0c81024162", 12, "4162", 0x40, "310b300906035504030c026162"},
             Case{"0c8200024162", 12, "4162", 0x40, "310b300906035504030c026162"},
             Case{"2c06040141040162", 12, "4162", 0x40, "310b300906035504030c026162"},
             Case{"2c06800141c40162", 12, "4162", 0x40, "310b300906035504030c026162"},
             Case{"2c800401410401620000", 12, "4162", 0x40, "310b300906035504030c026162"},
             Case{"2c0c2c0a2c082c062c040c024162", 12, "4162", 0x40, "310b300906035504030c026162"},
             Case{"030207ff", 3, "80", 0x4f, "310b3009060355040303020780"},
             Case{"030103", 3, "", 0x4b, "310a30080603550403030100"},
             Case{"2304030207ff", 3, "80", 0x4f, "310b3009060355040303020780"},
             Case{"0a01ff", 266, "01", 0x40, "310c300a06035504031f820a0101"},
             Case{"0a02ff00", 266, "0100", 0x40, "310d300b06035504031f820a020100"},
             Case{"0a02ff7f", 266, "81", 0x40, "310c300a06035504031f820a0181"},
             Case{"0a020001", 10, "01", 0x40, "310a300806035504030a0101"},
             Case{"0a0200ff", 10, "ff", 0x40, "310b300906035504030a0200ff"},
             Case{"0a00", 10, "", 0x40, "310a300806035504030a0100"},
             Case{"2a030401ff", 266, "01", 0x40, "310c300a06035504031f820a0101"},
             Case{"30023000", 16, "30023000", 0x40, "310b3009060355040330023000"},
             Case{"308030000000", 16, "308030000000", 0x40, "310d300b0603550403308030000000"},
             Case{"3080308000000000", 16, "3080308000000000", 0x40, "310f300d06035504033080308000000000"},
             Case{"130d20205465737420090a20434120", 19, "20205465737420090a20434120", 0x40, "3110300e06035504030c0774657374206361"},
             Case{"1304436166e9", 19, "436166e9", 0x40, "310e300c06035504030c05636166c3a9"},
             Case{"160341e980", 22, "41e980", 0x40, "310e300c06035504030c0561c3a9c280"},
             Case{"1403e94162", 20, "e94162", 0x40, "310d300b06035504030c04c3a96162"},
             Case{"1e060041006200e9", 30, "0041006200e9", 0x40, "310d300b06035504030c046162c3a9"},
             Case{"1c080001f6000000004b", 28, "0001f6000000004b", 0x40, "310e300c06035504030c05f09f98806b"},
             Case{"0c06f09f9880c3a9", 12, "f09f9880c3a9", 0x40, "310f300d06035504030c06f09f9880c3a9"},
             Case{"0c00", 12, "", 0x40, "3109300706035504030c00"},
             Case{"2c0a24800401410000040162", 12, "4162", 0x40, "310b300906035504030c026162"},
             Case{"360730800000040180", 22, "80", 0x40, "310b300906035504030c02c280"},
             Case{"1203313233", 18, "313233", 0x40, "310c300a06035504031203313233"},
             Case{"030107", 3, "", 0x4f, "310a30080603550403030100"}}) {
        const Bytes name = CommonName(Hex(accepted.value));
        LibreSsl3X509Crl* crl = Parse(CrlWithIssuer(name));
        Require(crl != nullptr);
        Require(crl->crl->issuer->entries->num == 1);
        const LibreSslX509NameEntry* entry = NameEntry(crl->crl->issuer, 0);
        Require(entry != nullptr && entry->set == 0);
        const auto* value = static_cast<const Asn1String*>(entry->value);
        const Bytes data = Hex(accepted.data);
        Require(value->type == accepted.type);
        Require(value->flags == accepted.flags);
        Require(value->length == static_cast<int>(data.size()));
        Require(data.empty() || std::memcmp(value->data, data.data(), data.size()) == 0);
        RequireNameEncodings(crl->crl->issuer, name, Hex(accepted.canonical));
        FreeCrlValue(crl);
        RequireQueue({});
        Require(allocations.empty());
    }

    const Bytes longName = CommonName(Tlv(0x0c, Bytes(128, 'A')));
    LibreSsl3X509Crl* crl = Parse(CrlWithIssuer(longName));
    Require(crl != nullptr);
    RequireNameEncodings(crl->crl->issuer, longName, Tlv(0x31, Sequence({Oid({0x55, 0x04, 0x03}), Tlv(0x0c, Bytes(128, 'a'))})));
    FreeCrlValue(crl);
    RequireQueue({});
    Require(allocations.empty());
}

void TestRejectedNameStructures() {
    struct Case {
        const char* name;
        int reason;
    };
    for (const Case& rejected : {
             Case{"300d310b100906035504030c024162", 149}, Case{"3009310730050603550403", 121},
             Case{"300f310d300b06035504030c0241620500", 148}, Case{"300d310b308006035504030c024162", 137},
             Case{"300431020000", 159}, Case{"300d300b300906035504030c024162", 168},
             Case{"a00d310b300906035504030c024162", 168}, Case{"3080310b300906035504030c024162", 168},
             Case{"30803100", 168}, Case{"300c310a3008060255840c024162", 216},
             Case{"300f310d300b26030401800c024162", 216}, Case{"3089010000000000000000", 123},
             Case{"30820100", 155}, Case{"0000", 159},
             Case{"3100", 168}, Case{"3002310100", 155}, Case{"300db10b300906035504030c024162", 168}}) {
        Require(Parse(CrlWithIssuer(Hex(rejected.name))) == nullptr);
        RequireQueue({rejected.reason});
        Require(allocations.empty());
    }

    Require(Parse(BuildCrl(Join({Tlv(0x02, Bytes{0x01}), SignatureAlgorithm()}))) == nullptr);
    RequireQueue({121});
    Require(allocations.empty());
}

void TestNameStructures() {
    struct Case {
        const char* name;
        int count;
        int lastSet;
        const char* canonical;
    };
    for (const Case& accepted : {
             Case{"3082000f31810c30810906035504030c024162", 1, 0, "310b300906035504030c026162"},
             Case{"30803180308006035504030c024162000000000000", 1, 0, "310b300906035504030c026162"},
             Case{"300d110b300906035504030c024162", 1, 0, "310b300906035504030c026162"},
             Case{"100d310b300906035504030c024162", 1, 0, "310b300906035504030c026162"},
             Case{"3f100d310b300906035504030c024162", 1, 0, "310b300906035504030c026162"},
             Case{"300f3100310b300906035504030c024162", 1, 1, "310b300906035504030c026162"},
             Case{"300f310d300b260504035504030c024162", 1, 0, "310b300906035504030c026162"},
             Case{"300e310c300a1f06035504030c024162", 1, 0, "310b300906035504030c026162"},
             Case{"300431003100", 0, 0, ""},
             Case{"3000", 0, 0, ""},
             Case{"302431163009060355040a0c025a7a3009060355040313024162310a30080603550403160158", 3, 1,
                 "3116300906035504030c0261623009060355040a0c027a7a310a300806035504030c0178"}}) {
        const Bytes name = Hex(accepted.name);
        LibreSsl3X509Crl* crl = Parse(CrlWithIssuer(name));
        Require(crl != nullptr);
        Require(crl->crl->issuer->entries->num == accepted.count);
        if (accepted.count > 0) Require(NameEntry(crl->crl->issuer, accepted.count - 1)->set == accepted.lastSet);
        RequireNameEncodings(crl->crl->issuer, name, Hex(accepted.canonical));
        FreeCrlValue(crl);
        RequireQueue({});
        Require(allocations.empty());
    }
}

Bytes AlgorithmWithParameter(const Bytes& parameter) {
    return Sequence({Oid({0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b}), parameter});
}

Bytes LongTag(const char* identifier, const Bytes& encoded) {
    return Join({Hex(identifier), Bytes(encoded.begin() + 1, encoded.end())});
}

void TestLongFormTags() {
    const Bytes longNull = Hex("1f0500");
    for (const Bytes& der : {
             BuildCrl(Join({Tlv(0x02, Bytes{0x01}), AlgorithmWithParameter(longNull), IssuerName("Test CA"), UtcTime("240101000000Z")})),
             BuildCrl(TbsHead(), AlgorithmWithParameter(longNull))}) {
        LibreSsl3X509Crl* crl = Parse(der);
        Require(crl != nullptr);
        Require(crl->crl->sigAlg->parameter != nullptr && crl->crl->sigAlg->parameter->type == 5);
        Require(crl->sigAlg->parameter != nullptr && crl->sigAlg->parameter->type == 5);
        FreeCrlValue(crl);
        RequireQueue({});
        Require(allocations.empty());
    }

    const Bytes extension = Sequence({Oid({0x55, 0x1d, 0x14}), Hex("1f0101ff"), Tlv(0x04, Hex("020107"))});
    const Bytes revoked = Sequence({Tlv(0x02, Bytes{0x10, 0x01}), UtcTime("240102000000Z"), LongTag("3f10", Tlv(0x30, extension))});
    const Bytes der = BuildCrl(Join({
        Hex("1f020101"),
        SignatureAlgorithm(),
        IssuerName("Test CA"),
        UtcTime("240101000000Z"),
        LongTag("1f17", Tlv(0x17, Ascii("240201000000Z"))),
        LongTag("3f10", Tlv(0x30, revoked)),
        LongTag("bf00", Tlv(0xa0, Sequence({extension}))),
    }));
    LibreSsl3X509Crl* crl = Parse(der);
    Require(crl != nullptr);
    RequireInteger(crl->crl->version, {0x01});
    RequireString(crl->crl->nextUpdate, 23, "240201000000Z");
    Require(crl->crl->revoked != nullptr && crl->crl->revoked->num == 1);
    const LibreSsl3Revoked* entry = RevokedEntryAt(crl, 0);
    Require(entry != nullptr && entry->extensions != nullptr && entry->extensions->num == 1);
    Require(crl->crl->extensions != nullptr && crl->crl->extensions->num == 1);
    const auto* crlNumber = reinterpret_cast<const LibreSsl3X509Extension*>(crl->crl->extensions->data[0]);
    Require(crlNumber->critical == 0xff);
    FreeCrlValue(crl);
    RequireQueue({});
    Require(allocations.empty());

    struct Case {
        const char* parameter;
        int reason;
    };
    for (const Case& rejected : {Case{"0000", 159}, Case{"00000500", 159}, Case{"0100", 106}, Case{"0102ffff", 106}, Case{"1f0100", 106}, Case{"00", 123},
             Case{"050100", 144}, Case{"1f050100", 144}, Case{"1e0141", 214}, Case{"1f1e0141", 214}, Case{"1c020000", 215},
             Case{"1f1c03000000", 215}}) {
        for (const Bytes& candidate : {
                 BuildCrl(Join({Tlv(0x02, Bytes{0x01}), AlgorithmWithParameter(Hex(rejected.parameter)), IssuerName("Test CA"),
                     UtcTime("240101000000Z")})),
                 BuildCrl(TbsHead(), AlgorithmWithParameter(Hex(rejected.parameter)))}) {
            Require(Parse(candidate) == nullptr);
            RequireQueue({rejected.reason});
            Require(allocations.empty());
        }
    }

    crl = Parse(BuildCrl(Join({TbsHead(), LongTag("1f18", Tlv(0x18, Ascii("20240201000000Z")))})));
    Require(crl != nullptr);
    RequireString(crl->crl->nextUpdate, 24, "20240201000000Z");
    FreeCrlValue(crl);
    RequireQueue({});
    Require(allocations.empty());

    for (const Case& truncated : {Case{"170d3234", 155}, Case{"17", 123}}) {
        Require(Parse(BuildCrl(Join({TbsHead(), Hex(truncated.parameter)}))) == nullptr);
        RequireQueue({truncated.reason});
        Require(allocations.empty());
    }

    RequireNotImplemented([] { Parse(BuildCrl(TbsHead(), AlgorithmWithParameter(Hex("8102ffff")))); });
    DrainErrors();
    for (const auto& [pointer, size] : allocations) std::free(pointer);
    allocations.clear();
}

int LookupWithIssuer(LibreSsl3X509Crl* crl, LibreSslX509Name* issuer) {
    Serial serial(0x01);
    LibreSsl3CertificateInfo info{};
    LibreSsl3Certificate certificate{};
    SetCertificate(certificate, info, &serial.value, issuer);
    return X509_CRL_get0_by_cert_nid_postfix(crl, nullptr, &certificate);
}

void TestGet0ByCertCanonicalIssuer() {
    const Bytes revokedList = Tlv(0x30, RevokedEntry(Bytes{0x10, 0x01}));
    LibreSsl3X509Crl* crl = Parse(BuildCrl(Join({TbsHead(), revokedList})));
    Require(crl != nullptr);
    struct Case {
        const char* value;
        int result;
    };
    for (const Case& lookup : {
             Case{"130a20207465737420204341", 1}, Case{"1e0e0054006500730074002000430041", 1},
             Case{"1408544553542063610a", 1}, Case{"0c0754657374204341", 1},
             Case{"1c200000005400000065000000730000007400000020000000430000004100000020", 1}, Case{"160909544553540b0c4341", 1},
             Case{"0c08546573742043412e", 0}, Case{"0c06546573744341", 0},
             Case{"120754657374204341", 0}, Case{"0c08546573742043c381", 0},
             Case{"1407546573742043c1", 0}}) {
        LibreSsl3X509Crl* other = Parse(CrlWithIssuer(CommonName(Hex(lookup.value))));
        Require(other != nullptr);
        Require(LookupWithIssuer(crl, other->crl->issuer) == lookup.result);
        FreeCrlValue(other);
    }
    LibreSsl3X509Crl* organization = Parse(CrlWithIssuer(
        Sequence({Tlv(0x31, Sequence({Oid({0x55, 0x04, 0x0a}), Tlv(0x0c, Ascii("Test CA"))}))})));
    Require(organization != nullptr);
    Require(LookupWithIssuer(crl, organization->crl->issuer) == 0);
    FreeCrlValue(organization);

    LibreSsl3X509Crl* empty = Parse(CrlWithIssuer(Sequence({})));
    Require(empty != nullptr);
    Require(LookupWithIssuer(crl, empty->crl->issuer) == 0);
    LibreSsl3X509Crl* emptyCrl = Parse(BuildCrl(Join({
        Tlv(0x02, Bytes{0x01}), SignatureAlgorithm(), Sequence({}), UtcTime("240101000000Z"), revokedList})));
    Require(emptyCrl != nullptr);
    Require(LookupWithIssuer(emptyCrl, empty->crl->issuer) == 1);

    LibreSslX509Name bare = *crl->crl->issuer;
    bare.canonEnc = nullptr;
    bare.canonEnclen = 0;
    Require(LookupWithIssuer(crl, &bare) == 0);
    Require(LookupWithIssuer(emptyCrl, &bare) == 1);
    bare.canonEnclen = 3;
    RequireInvalidArgument([&] { LookupWithIssuer(crl, &bare); });
    LibreSslX509Name negative = *crl->crl->issuer;
    negative.canonEnclen = -1;
    RequireInvalidArgument([&] { LookupWithIssuer(crl, &negative); });
    LibreSslX509Name modified = *crl->crl->issuer;
    modified.modified = 1;
    RequireNotImplemented([&] { LookupWithIssuer(crl, &modified); });
    crl->crl->issuer->modified = 1;
    RequireNotImplemented([&] { LookupWithIssuer(crl, empty->crl->issuer); });
    crl->crl->issuer->modified = 0;
    FreeCrlValue(empty);
    FreeCrlValue(emptyCrl);
    FreeCrlValue(crl);
    RequireQueue({});
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
    TestCertificateIssuerEntries();
    TestTrailingData();
    TestMalformed();
    TestTimes();
    TestArgumentErrors();
    TestReferenceCountedFree();
    TestReusedOutput();
    TestAllocationFailures();
    TestNonMinimalLengths();
    TestRejectedNameValues();
    TestNameValueTypes();
    TestRejectedNameStructures();
    TestNameStructures();
    TestLongFormTags();
    TestGet0ByCertCanonicalIssuer();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 X509_CRL tests passed");
}
