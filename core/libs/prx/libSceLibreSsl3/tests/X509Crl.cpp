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

constexpr int MallocFailure = 65;
constexpr int AuxError = 100;

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
    if (!pointer) return Allocate_nid_no_patch(size);
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
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
    Require(first->reason == -1);
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
    Require(crl->issuers != nullptr && crl->issuers->num == 1);
    Require(crl->issuers->data[0] == reinterpret_cast<const char*>(RevokedEntryAt(crl, 1)->issuer));
    Require(RevokedEntryAt(crl, 0)->issuer == nullptr);
    Require(RevokedEntryAt(crl, 1)->issuer != nullptr && RevokedEntryAt(crl, 1)->issuer->num == 1);
    for (const int index : {2, 3}) Require(RevokedEntryAt(crl, index)->issuer == RevokedEntryAt(crl, 1)->issuer);
    Require(RevokedEntryAt(crl, 3)->reason == 8);

    LibreSsl3Revoked* found = nullptr;
    Require(LookupSerial(crl, 0x01, nullptr, &found) == 1);
    Require(reinterpret_cast<const void*>(found) == reinterpret_cast<const void*>(RevokedEntryAt(crl, 0)));
    Require(LookupSerial(crl, 0x01, crlIssuer, nullptr) == 1);
    Require(LookupSerial(crl, 0x01, otherIssuer, nullptr) == 0);
    Require(LookupSerial(crl, 0x05, nullptr, nullptr) == 0);
    Require(LookupSerial(crl, 0x05, otherIssuer, nullptr) == 0);

    found = nullptr;
    Require(LookupSerial(crl, 0x02, otherIssuer, &found) == 1);
    Require(reinterpret_cast<const void*>(found) == reinterpret_cast<const void*>(RevokedEntryAt(crl, 1)));
    Require(LookupSerial(crl, 0x02, crlIssuer, nullptr) == 0);
    Require(LookupSerial(crl, 0x02, nullptr, nullptr) == 0);
    Require(LookupSerial(crl, 0x03, otherIssuer, nullptr) == 1);
    Require(LookupSerial(crl, 0x03, crlIssuer, nullptr) == 0);
    Require(LookupSerial(crl, 0x03, nullptr, nullptr) == 0);
    Require(LookupSerial(crl, 0x04, otherIssuer, nullptr) == 2);
    Require(LookupSerial(crl, 0x04, crlIssuer, nullptr) == 0);
    Require(LookupSerial(crl, 0x04, nullptr, nullptr) == 0);

    LibreSsl3X509Crl* second = Parse(BuildCrl(Join({
        TbsHead(),
        Tlv(0x30, RevokedEntryWithExtensions(Bytes{0x10, 0x01},
            Join({CrlReasonExtension(1), CertificateIssuerExtension("Other CA")}))),
    })));
    Require(second != nullptr);
    Require(RevokedEntryAt(second, 0)->extensions->num == 2 && RevokedEntryAt(second, 0)->reason == 1);
    Require(LookupSerial(second, 0x01, otherIssuer, nullptr) == 1);
    Require(LookupSerial(second, 0x01, crlIssuer, nullptr) == 0);
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

constexpr const char FiveNamesCrlHex[] =
    "3082012c3081df020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
    "3030305a3081af308198020101170d3234303130323030303030305a3081833081800603551d1d0101ff04763074a415"
    "30133111300f06035504030c084f74686572204341a41530133111300f06035504030c085468697264204341a4163014"
    "3112301006035504030c09466f75727468204341a41530133111300f06035504030c084f74686572204341a415301331"
    "11300f06035504030c0854686972642043413012020102170d3234303130323030303030305a300506032b6570034100"
    "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
    "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a";

constexpr const char IndirectCrlHex[] =
    "3082014b3081fe020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
    "3030305a3081ce3012020101170d3234303130323030303030305a3039020102170d3234303130323030303030305a30"
    "2530230603551d1d0101ff04193017a41530133111300f06035504030c084f746865722043413020020103170d323430"
    "3130323030303030305a300c300a0603551d1504030a01083039020104170d3234303130323030303030305a30253023"
    "0603551d1d0101ff04193017a41530133111300f06035504030c0854686972642043413020020105170d323430313032"
    "3030303030305a300c300a0603551d1504030a0101300506032b6570034100565a96eb6b3da972631048c7755bda412a"
    "60a195473320813afd51e28f96a3936a6565ef3de5fa91901a48ef619ba931b45ee9124ab13bc868b5a0c3cc2e4f06";

constexpr const char DirectCrlHex[] =
    "3081bf3073020101300506032b657030123110300e06035504030c0754657374204341170d3234303130313030303030"
    "305a30443020020101170d3234303130323030303030305a300c300a0603551d1504030a01013020020102170d323430"
    "3130323030303030305a300c300a0603551d1504030a0108300506032b65700341006864c5547b18fbc565f805823fff"
    "98eb55a40fc36d42b5155ac5e6f946752034aa91c20abd1c72158cb63908d3a99de0a76f1ecc024ab0e27119ba49ea9c"
    "df07";

constexpr const char EmptyNamesCrlHex[] =
    "3081b33067020101300506032b657030123110300e06035504030c0754657374204341170d3234303130313030303030"
    "305a30383022020101170d3234303130323030303030305a300e300c0603551d1d0101ff040230003012020102170d32"
    "34303130323030303030305a300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
    "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a";

constexpr const char TrailingNamesCrlHex[] =
    "3081cd308180020101300506032b657030123110300e06035504030c0754657374204341170d32343031303130303030"
    "30305a3051303b020101170d3234303130323030303030305a302730250603551d1d0101ff041b3017a4153013311130"
    "0f06035504030c084f7468657220434105003012020102170d3234303130323030303030305a300506032b6570034100"
    "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
    "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a";

constexpr const char TwoNamesCrlHex[] =
    "3082011e3081d1020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
    "3030305a3081a13050020101170d3234303130323030303030305a303c303a0603551d1d0101ff0430302ea415301331"
    "11300f06035504030c084f74686572204341a41530133111300f06035504030c0854686972642043413012020102170d"
    "3234303130323030303030305a3039020103170d3234303130323030303030305a302530230603551d1d0101ff041930"
    "17a41530133111300f06035504030c085468697264204341300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a"
    "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
    "5a5a";

constexpr const char SameSerialCrlHex[] =
    "3082013a3081ed020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
    "3030305a3081bd3045020101170d3234303130323030303030305a303130230603551d1d0101ff04193017a415301331"
    "11300f06035504030c084f74686572204341300a0603551d1504030a01083039020102170d3234303130323030303030"
    "305a302530230603551d1d0101ff04193017a41530133111300f06035504030c0854686972642043413039020101170d"
    "3234303130323030303030305a302530230603551d1d0101ff04193017a41530133111300f06035504030c0854686972"
    "64204341300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
    "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a";

constexpr const char FourIssuersCrlHex[] =
    "3082016b3082011d020101300506032b657030123110300e06035504030c0754657374204341170d3234303130313030"
    "303030305a3081ed3039020101170d3234303130323030303030305a302530230603551d1d0101ff04193017a4153013"
    "3111300f06035504030c084f746865722043413039020102170d3234303130323030303030305a302530230603551d1d"
    "0101ff04193017a41530133111300f06035504030c085468697264204341303a020103170d3234303130323030303030"
    "305a302630240603551d1d0101ff041a3018a41630143112301006035504030c09466f75727468204341303902010417"
    "0d3234303130323030303030305a302530230603551d1d0101ff04193017a41530133111300f06035504030c08466966"
    "7468204341300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
    "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a";

LibreSsl3X509Crl* NamedCrl(const char* commonName) {
    LibreSsl3X509Crl* crl = Parse(CrlWithIssuer(IssuerName(commonName)));
    Require(crl != nullptr);
    return crl;
}

int LookupSerialBytes(LibreSsl3X509Crl* crl, const Bytes& serial, const LibreSslX509Name* issuer, LibreSsl3Revoked** found) {
    Bytes content = serial;
    Asn1String value{};
    value.length = static_cast<int>(content.size());
    value.type = 2;
    value.data = content.data();
    LibreSsl3CertificateInfo info{};
    LibreSsl3Certificate certificate{};
    SetCertificate(certificate, info, &value, issuer);
    return X509_CRL_get0_by_cert_nid_postfix(crl, found, &certificate);
}

struct Expected {
    unsigned char serial;
    const LibreSslX509Name* issuer;
    int result;
    int index;
};

void RequireExpected(LibreSsl3X509Crl* crl, const Expected& expected) {
    LibreSsl3Revoked* found = nullptr;
    Require(LookupSerialBytes(crl, Bytes{expected.serial}, expected.issuer, &found) == expected.result);
    if (expected.index < 0) {
        Require(found == nullptr);
    } else {
        Require(found == RevokedEntryAt(crl, expected.index));
    }
}

void TestOracleIndirectCrl() {
    LibreSsl3X509Crl* indirectCrl = Parse(Hex(IndirectCrlHex));
    Require(indirectCrl != nullptr);
    Require(indirectCrl->flags == 0);
    Require(indirectCrl->issuers != nullptr && indirectCrl->issuers->num == 2);
    LibreSsl3X509Crl* other = NamedCrl("Other CA");
    LibreSsl3X509Crl* third = NamedCrl("Third CA");
    LibreSsl3X509Crl* fourth = NamedCrl("Fourth CA");
    const LibreSslX509Name* test = indirectCrl->crl->issuer;
    const LibreSslX509Name* otherName = other->crl->issuer;
    const LibreSslX509Name* thirdName = third->crl->issuer;
    const LibreSslX509Name* fourthName = fourth->crl->issuer;
    const Expected indirectExpected[] = {
        {0x01, nullptr, 1, 0}, {0x01, test, 1, 0}, {0x01, otherName, 0, -1}, {0x01, thirdName, 0, -1}, {0x01, fourthName, 0, -1},
        {0x02, nullptr, 0, -1}, {0x02, test, 0, -1}, {0x02, otherName, 1, 1}, {0x02, thirdName, 0, -1}, {0x02, fourthName, 0, -1},
        {0x03, nullptr, 0, -1}, {0x03, test, 0, -1}, {0x03, otherName, 2, 2}, {0x03, thirdName, 0, -1}, {0x03, fourthName, 0, -1},
        {0x04, nullptr, 0, -1}, {0x04, test, 0, -1}, {0x04, otherName, 0, -1}, {0x04, thirdName, 1, 3}, {0x04, fourthName, 0, -1},
        {0x05, nullptr, 0, -1}, {0x05, test, 0, -1}, {0x05, otherName, 0, -1}, {0x05, thirdName, 1, 4}, {0x05, fourthName, 0, -1},
        {0x06, nullptr, 0, -1}, {0x06, test, 0, -1}, {0x06, otherName, 0, -1}, {0x06, thirdName, 0, -1}, {0x06, fourthName, 0, -1},
    };
    for (const Expected& row : indirectExpected) RequireExpected(indirectCrl, row);
    Require(RevokedEntryAt(indirectCrl, 0)->issuer == nullptr && RevokedEntryAt(indirectCrl, 0)->reason == -1);
    Require(RevokedEntryAt(indirectCrl, 1)->issuer != nullptr && RevokedEntryAt(indirectCrl, 1)->issuer->num == 1 &&
            RevokedEntryAt(indirectCrl, 1)->reason == -1);
    Require(RevokedEntryAt(indirectCrl, 2)->issuer == RevokedEntryAt(indirectCrl, 1)->issuer &&
            RevokedEntryAt(indirectCrl, 2)->reason == 8);
    Require(RevokedEntryAt(indirectCrl, 3)->issuer != RevokedEntryAt(indirectCrl, 1)->issuer &&
            RevokedEntryAt(indirectCrl, 3)->issuer != nullptr && RevokedEntryAt(indirectCrl, 3)->issuer->num == 1 &&
            RevokedEntryAt(indirectCrl, 3)->reason == -1);
    Require(RevokedEntryAt(indirectCrl, 4)->issuer == RevokedEntryAt(indirectCrl, 3)->issuer &&
            RevokedEntryAt(indirectCrl, 4)->reason == 1);
    Require(indirectCrl->issuers->data[0] == reinterpret_cast<const char*>(RevokedEntryAt(indirectCrl, 1)->issuer));
    Require(indirectCrl->issuers->data[1] == reinterpret_cast<const char*>(RevokedEntryAt(indirectCrl, 3)->issuer));
    FreeCrlValue(fourth);
    FreeCrlValue(third);
    FreeCrlValue(other);
    FreeCrlValue(indirectCrl);
    RequireQueue({});
    Require(allocations.empty());
}

void TestOracleDirectCrl() {
    LibreSsl3X509Crl* directCrl = Parse(Hex(DirectCrlHex));
    Require(directCrl != nullptr);
    Require(directCrl->flags == 0);
    Require(directCrl->issuers == nullptr);
    Require(RevokedEntryAt(directCrl, 0)->issuer == nullptr && RevokedEntryAt(directCrl, 0)->reason == 1);
    Require(RevokedEntryAt(directCrl, 1)->issuer == nullptr && RevokedEntryAt(directCrl, 1)->reason == 8);
    LibreSsl3X509Crl* other = NamedCrl("Other CA");
    const LibreSslX509Name* test = directCrl->crl->issuer;
    const LibreSslX509Name* otherName = other->crl->issuer;
    const Expected directExpected[] = {
        {0x01, nullptr, 1, 0}, {0x01, test, 1, 0}, {0x01, otherName, 0, -1},
        {0x02, nullptr, 2, 1}, {0x02, test, 2, 1}, {0x02, otherName, 0, -1},
        {0x03, nullptr, 0, -1}, {0x03, test, 0, -1}, {0x03, otherName, 0, -1},
    };
    for (const Expected& row : directExpected) RequireExpected(directCrl, row);

    LibreSsl3X509Crl* fallbackCrl = Parse(Hex(
        "3081b53069020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
        "3030305a303a3038020101170d3234303130323030303030305a302430220603551d1d0101ff04183016a41430"
        "123110300e06035504030c0754657374204341300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a"));
    Require(fallbackCrl != nullptr);
    Require(fallbackCrl->flags == 0);
    Require(fallbackCrl->issuers != nullptr && fallbackCrl->issuers->num == 1);
    Require(RevokedEntryAt(fallbackCrl, 0)->issuer != nullptr && RevokedEntryAt(fallbackCrl, 0)->issuer->num == 1);
    RequireExpected(fallbackCrl, {0x01, nullptr, 1, 0});
    RequireExpected(fallbackCrl, {0x01, fallbackCrl->crl->issuer, 1, 0});
    RequireExpected(fallbackCrl, {0x01, otherName, 0, -1});
    FreeCrlValue(fallbackCrl);

    FreeCrlValue(other);
    FreeCrlValue(directCrl);
    RequireQueue({});
    Require(allocations.empty());
}

void TestOracleCertificateIssuerForms() {
    LibreSsl3X509Crl* emptyNames = Parse(Hex(EmptyNamesCrlHex));
    LibreSsl3X509Crl* trailingNames = Parse(Hex(TrailingNamesCrlHex));
    LibreSsl3X509Crl* twoNames = Parse(Hex(TwoNamesCrlHex));
    LibreSsl3X509Crl* sameSerial = Parse(Hex(SameSerialCrlHex));
    Require(emptyNames != nullptr && trailingNames != nullptr && twoNames != nullptr && sameSerial != nullptr);
    Require(emptyNames->flags == 0);
    Require(emptyNames->issuers != nullptr && emptyNames->issuers->num == 1);
    Require(RevokedEntryAt(emptyNames, 0)->issuer != nullptr && RevokedEntryAt(emptyNames, 0)->issuer->num == 0 &&
            RevokedEntryAt(emptyNames, 0)->reason == -1);
    Require(RevokedEntryAt(emptyNames, 1)->issuer != nullptr && RevokedEntryAt(emptyNames, 1)->issuer->num == 0 &&
            RevokedEntryAt(emptyNames, 1)->reason == -1);
    Require(trailingNames->flags == 0);
    Require(RevokedEntryAt(trailingNames, 0)->issuer != nullptr && RevokedEntryAt(trailingNames, 0)->issuer->num == 1 &&
            RevokedEntryAt(trailingNames, 0)->reason == -1);
    Require(RevokedEntryAt(trailingNames, 1)->issuer != nullptr && RevokedEntryAt(trailingNames, 1)->issuer->num == 1 &&
            RevokedEntryAt(trailingNames, 1)->reason == -1);
    Require(twoNames->flags == 0);
    Require(twoNames->issuers != nullptr && twoNames->issuers->num == 2);
    Require(RevokedEntryAt(twoNames, 0)->issuer != nullptr && RevokedEntryAt(twoNames, 0)->issuer->num == 2 &&
            RevokedEntryAt(twoNames, 0)->reason == -1);
    Require(RevokedEntryAt(twoNames, 1)->issuer != nullptr && RevokedEntryAt(twoNames, 1)->issuer->num == 2 &&
            RevokedEntryAt(twoNames, 1)->reason == -1);
    Require(RevokedEntryAt(twoNames, 2)->issuer != nullptr && RevokedEntryAt(twoNames, 2)->issuer->num == 1 &&
            RevokedEntryAt(twoNames, 2)->reason == -1);
    Require(sameSerial->flags == 0);
    Require(sameSerial->issuers != nullptr && sameSerial->issuers->num == 3);
    Require(RevokedEntryAt(sameSerial, 0)->issuer != nullptr && RevokedEntryAt(sameSerial, 0)->issuer->num == 1 &&
            RevokedEntryAt(sameSerial, 0)->reason == 8);
    Require(RevokedEntryAt(sameSerial, 1)->issuer != nullptr && RevokedEntryAt(sameSerial, 1)->issuer->num == 1 &&
            RevokedEntryAt(sameSerial, 1)->reason == -1);
    Require(RevokedEntryAt(sameSerial, 2)->issuer != nullptr && RevokedEntryAt(sameSerial, 2)->issuer->num == 1 &&
            RevokedEntryAt(sameSerial, 2)->reason == -1);
    LibreSsl3X509Crl* other = NamedCrl("Other CA");
    LibreSsl3X509Crl* third = NamedCrl("Third CA");
    const LibreSslX509Name* otherName = other->crl->issuer;
    const LibreSslX509Name* thirdName = third->crl->issuer;

    const Expected emptyRows[] = {
        {0x01, nullptr, 0, -1}, {0x01, emptyNames->crl->issuer, 0, -1}, {0x01, otherName, 0, -1},
        {0x02, nullptr, 0, -1}, {0x02, otherName, 0, -1},
    };
    for (const Expected& row : emptyRows) RequireExpected(emptyNames, row);

    const Expected trailingRows[] = {
        {0x01, otherName, 1, 0}, {0x01, thirdName, 0, -1}, {0x01, trailingNames->crl->issuer, 0, -1},
        {0x01, nullptr, 0, -1}, {0x02, otherName, 1, 1}, {0x02, trailingNames->crl->issuer, 0, -1},
    };
    for (const Expected& row : trailingRows) RequireExpected(trailingNames, row);

    const Expected twoNamesRows[] = {
        {0x01, otherName, 1, 0}, {0x01, thirdName, 1, 0}, {0x01, twoNames->crl->issuer, 0, -1},
        {0x02, otherName, 1, 1}, {0x02, thirdName, 1, 1},
        {0x03, thirdName, 1, 2}, {0x03, otherName, 0, -1},
    };
    for (const Expected& row : twoNamesRows) RequireExpected(twoNames, row);

    const Expected sameSerialRows[] = {
        {0x01, otherName, 2, 0}, {0x01, thirdName, 1, 2}, {0x01, sameSerial->crl->issuer, 0, -1},
        {0x01, nullptr, 0, -1}, {0x02, thirdName, 1, 1}, {0x02, otherName, 0, -1},
    };
    for (const Expected& row : sameSerialRows) RequireExpected(sameSerial, row);

    LibreSsl3X509Crl* noncritical = Parse(BuildCrl(Join({
        TbsHead(),
        Tlv(0x30, Join({
            RevokedEntryWithExtensions(Bytes{0x01}, Extension({0x55, 0x1d, 0x1d}, false, Sequence({Tlv(0xa4, IssuerName("Other CA"))}))),
            RevokedEntry(Bytes{0x02}),
        })),
    })));
    Require(noncritical != nullptr);
    Require(noncritical->flags == 0);
    Require(RevokedEntryAt(noncritical, 0)->issuer != nullptr && RevokedEntryAt(noncritical, 0)->issuer->num == 1);
    Require(RevokedEntryAt(noncritical, 1)->issuer == RevokedEntryAt(noncritical, 0)->issuer);
    RequireExpected(noncritical, {0x01, otherName, 1, 0});
    RequireExpected(noncritical, {0x01, noncritical->crl->issuer, 0, -1});
    RequireExpected(noncritical, {0x02, otherName, 1, 1});
    FreeCrlValue(noncritical);

    for (const char* validHex : {
             "3081b8306c020101300506032b657030123110300e06035504030c0754657374204341170d3234303130313030"
             "303030305a303d303b020101170d3234303130323030303030305a302730250603551d1d0101ff041b3080a4"
             "1530133111300f06035504030c084f746865722043410000300506032b65700341005a5a5a5a5a5a5a5a5a5a"
             "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
             "5a5a5a5a5a5a5a5a5a5a",
             "3081b8306c020101300506032b657030123110300e06035504030c0754657374204341170d3234303130313030"
             "303030305a303d303b020101170d3234303130323030303030305a302730250603551d1d0101ff041b3019a4"
             "8030133111300f06035504030c084f746865722043410000300506032b65700341005a5a5a5a5a5a5a5a5a5a"
             "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
             "5a5a5a5a5a5a5a5a5a5a",
             "3081b6306a020101300506032b657030123110300e06035504030c0754657374204341170d3234303130313030"
             "303030305a303b3039020101170d3234303130323030303030305a302530230603551d1d0101ff04191017a4"
             "1530133111300f06035504030c084f74686572204341300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a"
             "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
             "5a5a5a5a5a5a5a5a"}) {
        LibreSsl3X509Crl* berCrl = Parse(Hex(validHex));
        Require(berCrl != nullptr);
        Require(berCrl->flags == 0);
        Require(berCrl->issuers != nullptr && berCrl->issuers->num == 1);
        Require(RevokedEntryAt(berCrl, 0)->issuer != nullptr && RevokedEntryAt(berCrl, 0)->issuer->num == 1 &&
                RevokedEntryAt(berCrl, 0)->reason == -1);
        RequireExpected(berCrl, {0x01, otherName, 1, 0});
        RequireExpected(berCrl, {0x01, berCrl->crl->issuer, 0, -1});
        FreeCrlValue(berCrl);
    }

    LibreSsl3X509Crl* primitiveEmpty = Parse(Hex(
        "30819f3053020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
        "3030305a30243022020101170d3234303130323030303030305a300e300c0603551d1d0101ff04021000300506"
        "032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"));
    Require(primitiveEmpty != nullptr);
    Require(primitiveEmpty->flags == 0);
    Require(primitiveEmpty->issuers != nullptr && primitiveEmpty->issuers->num == 1);
    Require(RevokedEntryAt(primitiveEmpty, 0)->issuer != nullptr && RevokedEntryAt(primitiveEmpty, 0)->issuer->num == 0 &&
            RevokedEntryAt(primitiveEmpty, 0)->reason == -1);
    RequireExpected(primitiveEmpty, {0x01, otherName, 0, -1});
    RequireExpected(primitiveEmpty, {0x01, primitiveEmpty->crl->issuer, 0, -1});
    FreeCrlValue(primitiveEmpty);

    FreeCrlValue(third);
    FreeCrlValue(other);
    FreeCrlValue(sameSerial);
    FreeCrlValue(twoNames);
    FreeCrlValue(trailingNames);
    FreeCrlValue(emptyNames);
    RequireQueue({});
    Require(allocations.empty());
}

Bytes SingleEntryCrl(const Bytes& entry) {
    return BuildCrl(Join({TbsHead(), Tlv(0x30, entry)}));
}

void TestCertificateIssuerUnsupportedForms() {
    const Bytes dnsnameDer = SingleEntryCrl(RevokedEntryWithExtensions(Bytes{0x10, 0x01},
        Extension({0x55, 0x1d, 0x1d}, true, Sequence({Tlv(0x82, Ascii("example.test"))}))));
    RequireNotImplemented([&] { Parse(dnsnameDer); });
    Require(allocations.empty());
    DrainErrors();

    const Bytes secondEntryDnsname = BuildCrl(Join({
        TbsHead(),
        Tlv(0x30, Join({
            RevokedEntryWithExtensions(Bytes{0x01}, CertificateIssuerExtension("Other CA")),
            RevokedEntryWithExtensions(Bytes{0x02}, Extension({0x55, 0x1d, 0x1d}, true, Sequence({Tlv(0x82, Ascii("example.test"))}))),
        })),
    }));
    RequireNotImplemented([&] { Parse(secondEntryDnsname); });
    Require(allocations.empty());
    DrainErrors();

    LibreSsl3X509Crl* other = NamedCrl("Other CA");
    LibreSsl3X509Crl* third = NamedCrl("Third CA");
    const LibreSslX509Name* otherName = other->crl->issuer;
    const LibreSslX509Name* thirdName = third->crl->issuer;

    LibreSsl3X509Crl* primitiveDirname = Parse(Hex(
        "3081b6306a020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
        "3030305a303b3039020101170d3234303130323030303030305a302530230603551d1d0101ff04193017841530"
        "133111300f06035504030c084f74686572204341300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a5a"));
    Require(primitiveDirname != nullptr);
    Require(primitiveDirname->flags == 0x80);
    Require(primitiveDirname->issuers == nullptr);
    Require(RevokedEntryAt(primitiveDirname, 0)->issuer == nullptr);
    Require(RevokedEntryAt(primitiveDirname, 0)->reason == 0);
    RequireQueue({120, 58, 58});
    const Expected primitiveDirnameRows[] = {
        {0x01, otherName, 0, -1}, {0x01, thirdName, 0, -1},
        {0x01, primitiveDirname->crl->issuer, 1, 0}, {0x01, nullptr, 1, 0},
    };
    for (const Expected& row : primitiveDirnameRows) RequireExpected(primitiveDirname, row);
    FreeCrlValue(primitiveDirname);

    LibreSsl3X509Crl* dirnameTrailing = Parse(Hex(
        "3081cd308180020101300506032b657030123110300e06035504030c0754657374204341170d32343031303130"
        "30303030305a3051303b020101170d3234303130323030303030305a302730250603551d1d0101ff041b3019a4"
        "1730133111300f06035504030c084f7468657220434105003012020102170d3234303130323030303030305a30"
        "0506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"));
    Require(dirnameTrailing != nullptr);
    Require(dirnameTrailing->flags == 0x80);
    Require(RevokedEntryAt(dirnameTrailing, 0)->issuer == nullptr &&
            RevokedEntryAt(dirnameTrailing, 0)->reason == 0);
    Require(RevokedEntryAt(dirnameTrailing, 1)->issuer == nullptr &&
            RevokedEntryAt(dirnameTrailing, 1)->reason == 0);
    RequireQueue({148, 58, 58});
    const Expected dirnameTrailingRows[] = {
        {0x01, otherName, 0, -1}, {0x01, dirnameTrailing->crl->issuer, 1, 0}, {0x01, nullptr, 1, 0},
        {0x02, otherName, 0, -1}, {0x02, dirnameTrailing->crl->issuer, 1, 1}, {0x02, nullptr, 1, 1},
    };
    for (const Expected& row : dirnameTrailingRows) RequireExpected(dirnameTrailing, row);
    FreeCrlValue(dirnameTrailing);

    LibreSsl3X509Crl* twoIssuerExts = Parse(Hex(
        "3081f03081a3020101300506032b657030123110300e06035504030c0754657374204341170d32343031303130"
        "30303030305a3074305e020101170d3234303130323030303030305a304a30230603551d1d0101ff04193017a4"
        "1530133111300f06035504030c084f7468657220434130230603551d1d0101ff04193017a41530133111300f06"
        "035504030c0854686972642043413012020102170d3234303130323030303030305a300506032b65700341005a"
        "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"));
    Require(twoIssuerExts != nullptr);
    Require(twoIssuerExts->flags == 0x80);
    Require(RevokedEntryAt(twoIssuerExts, 0)->issuer == nullptr &&
            RevokedEntryAt(twoIssuerExts, 0)->reason == 0);
    Require(RevokedEntryAt(twoIssuerExts, 1)->issuer == nullptr &&
            RevokedEntryAt(twoIssuerExts, 1)->reason == 0);
    RequireQueue({});
    const Expected twoIssuerExtsRows[] = {
        {0x01, otherName, 0, -1}, {0x01, thirdName, 0, -1},
        {0x01, twoIssuerExts->crl->issuer, 1, 0}, {0x01, nullptr, 1, 0},
        {0x02, otherName, 0, -1}, {0x02, twoIssuerExts->crl->issuer, 1, 1}, {0x02, nullptr, 1, 1},
    };
    for (const Expected& row : twoIssuerExtsRows) RequireExpected(twoIssuerExts, row);
    FreeCrlValue(twoIssuerExts);

    LibreSsl3X509Crl* badIssuerTag = Parse(SingleEntryCrl(RevokedEntryWithExtensions(Bytes{0x01},
        Extension({0x55, 0x1d, 0x1d}, true, Bytes{0x05, 0x00}))));
    Require(badIssuerTag != nullptr);
    Require(badIssuerTag->flags == 0x80);
    Require(RevokedEntryAt(badIssuerTag, 0)->issuer == nullptr);
    Require(RevokedEntryAt(badIssuerTag, 0)->reason == 0);
    RequireQueue({168, 58});
    RequireExpected(badIssuerTag, {0x01, badIssuerTag->crl->issuer, 1, 0});
    FreeCrlValue(badIssuerTag);

    LibreSsl3X509Crl* badChoice = Parse(SingleEntryCrl(RevokedEntryWithExtensions(Bytes{0x01},
        Extension({0x55, 0x1d, 0x1d}, true, Sequence({Tlv(0x89, Ascii("x"))})))));
    Require(badChoice != nullptr);
    Require(badChoice->flags == 0x80);
    Require(RevokedEntryAt(badChoice, 0)->issuer == nullptr && RevokedEntryAt(badChoice, 0)->reason == 0);
    RequireQueue({143, 58});
    RequireExpected(badChoice, {0x01, badChoice->crl->issuer, 1, 0});
    FreeCrlValue(badChoice);

    struct MalformedCase {
        const char* hex;
        std::initializer_list<int> errors;
    };
    for (const MalformedCase& malformed : {
             MalformedCase{"3081b6306a020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a303b3039020101170d3234303130323030303030305a302530230603551d1d0101ff04193080a41530"
                           "133111300f06035504030c084f74686572204341300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a",
                 {137}},
             MalformedCase{"3081b6306a020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a303b3039020101170d3234303130323030303030305a302530230603551d1d0101ff04193017a48030"
                           "133111300f06035504030c084f74686572204341300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a",
                 {137, 58, 58}},
             MalformedCase{"3081a13055020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30263024020101170d3234303130323030303030305a3010300e0603551d1d0101ff04043002000030"
                           "0506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {159}},
             MalformedCase{"30819d3051020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30223020020101170d3234303130323030303030305a300c300a0603551d1d0101ff0400300506032b"
                           "65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {102, 58}},
             MalformedCase{"30819e3052020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30233021020101170d3234303130323030303030305a300d300b0603551d1d0101ff04013030050603"
                           "2b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {102, 58}},
             MalformedCase{"3081a13055020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30263024020101170d3234303130323030303030305a3010300e0603551d1d0101ff04043002890530"
                           "0506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {143, 58}},
             MalformedCase{"3081a13055020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30263024020101170d3234303130323030303030305a3010300e0603551d1d0101ff04043002a40530"
                           "0506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {155, 58, 58, 58}},
             MalformedCase{"3081a13055020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30263024020101170d3234303130323030303030305a3010300e0603551d1d0101ff04043002820530"
                           "0506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {155, 58, 58, 58, 58}},
             MalformedCase{"3081a13055020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30263024020101170d3234303130323030303030305a3010300e0603551d1d0101ff04043002240030"
                           "0506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {143, 58}},
             MalformedCase{"3081a33057020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30283026020101170d3234303130323030303030305a301230100603551d1d0101ff04063004a40205"
                           "00300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {168, 58, 58, 58, 58, 58}},
             MalformedCase{"3081a13055020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30263024020101170d3234303130323030303030305a3010300e0603551d1d0101ff04043002a40030"
                           "0506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {102, 58, 58, 58, 58, 58}},
             MalformedCase{"3081af3063020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30343032020101170d3234303130323030303030305a301e301c0603551d1d0101ff04123010a40e30"
                           "0c310a300806035504030c01ff300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {134, 58, 58, 58, 58, 58}},
             MalformedCase{"3081a03054020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30253023020101170d3234303130323030303030305a300f300d0603551d1d0101ff040330019f3005"
                           "06032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {102, 58, 58, 58, 58}},
             MalformedCase{"3081a33057020101300506032b657030123110300e06035504030c0754657374204341170d323430313031303030"
                           "3030305a30283026020101170d3234303130323030303030305a301230100603551d1d0101ff04063004a40200"
                           "00300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
                           "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a",
                 {168, 58, 58, 58, 58, 58}}}) {
        LibreSsl3X509Crl* crl = Parse(Hex(malformed.hex));
        Require(crl != nullptr);
        Require(crl->flags == 0x80);
        Require(crl->issuers == nullptr);
        Require(RevokedEntryAt(crl, 0)->issuer == nullptr && RevokedEntryAt(crl, 0)->reason == 0);
        RequireQueue(malformed.errors);
        RequireExpected(crl, {0x01, otherName, 0, -1});
        RequireExpected(crl, {0x01, crl->crl->issuer, 1, 0});
        FreeCrlValue(crl);
    }

    FreeCrlValue(third);
    FreeCrlValue(other);
    Require(allocations.empty());
}

void TestCertificateIssuerInvalidMiddle() {
    LibreSsl3X509Crl* other = NamedCrl("Other CA");
    LibreSsl3X509Crl* third = NamedCrl("Third CA");
    LibreSsl3X509Crl* fourth = NamedCrl("Fourth CA");
    const LibreSslX509Name* otherName = other->crl->issuer;
    const LibreSslX509Name* thirdName = third->crl->issuer;
    const LibreSslX509Name* fourthName = fourth->crl->issuer;

    LibreSsl3X509Crl* criticalUnknown = Parse(Hex(
        "3081de308191020101300506032b657030123110300e06035504030c0754657374204341170d32343031303130"
        "30303030305a3062304c020101170d3234303130323030303030305a303830230603551d1d0101ff04193017a4"
        "1530133111300f06035504030c084f74686572204341301106062b06010401010101ff04040402616230120201"
        "02170d3234303130323030303030305a300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"));
    Require(criticalUnknown != nullptr);
    Require(criticalUnknown->flags == 0x200);
    Require(RevokedEntryAt(criticalUnknown, 0)->issuer != nullptr &&
            RevokedEntryAt(criticalUnknown, 0)->issuer->num == 1 &&
            RevokedEntryAt(criticalUnknown, 0)->reason == -1);
    Require(RevokedEntryAt(criticalUnknown, 1)->issuer != nullptr &&
            RevokedEntryAt(criticalUnknown, 1)->issuer->num == 1 &&
            RevokedEntryAt(criticalUnknown, 1)->reason == -1);
    RequireQueue({});
    RequireExpected(criticalUnknown, {0x01, otherName, 1, 0});
    RequireExpected(criticalUnknown, {0x02, otherName, 1, 1});
    FreeCrlValue(criticalUnknown);

    LibreSsl3X509Crl* noncriticalUnknown = Parse(Hex(
        "3081db30818e020101300506032b657030123110300e06035504030c0754657374204341170d32343031303130"
        "30303030305a305f3049020101170d3234303130323030303030305a303530230603551d1d0101ff04193017a4"
        "1530133111300f06035504030c084f74686572204341300e06062b06010401010404040261623012020102170d"
        "3234303130323030303030305a300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"));
    Require(noncriticalUnknown != nullptr);
    Require(noncriticalUnknown->flags == 0);
    Require(RevokedEntryAt(noncriticalUnknown, 0)->issuer != nullptr &&
            RevokedEntryAt(noncriticalUnknown, 0)->issuer->num == 1 &&
            RevokedEntryAt(noncriticalUnknown, 0)->reason == -1);
    Require(RevokedEntryAt(noncriticalUnknown, 1)->issuer != nullptr &&
            RevokedEntryAt(noncriticalUnknown, 1)->issuer->num == 1 &&
            RevokedEntryAt(noncriticalUnknown, 1)->reason == -1);
    RequireQueue({});
    RequireExpected(noncriticalUnknown, {0x01, otherName, 1, 0});
    RequireExpected(noncriticalUnknown, {0x02, otherName, 1, 1});
    FreeCrlValue(noncriticalUnknown);

    LibreSsl3X509Crl* invalidMiddleIssuer = Parse(Hex(
        "3082012f3081e2020101300506032b657030123110300e06035504030c0754657374204341170d323430313031"
        "3030303030305a3081b23045020101170d3234303130323030303030305a303130230603551d1d0101ff041930"
        "17a41530133111300f06035504030c084f74686572204341300a0603551d1504030a01013022020102170d3234"
        "303130323030303030305a300e300c0603551d1d0101ff040205003045020103170d3234303130323030303030"
        "305a303130230603551d1d0101ff04193017a41530133111300f06035504030c085468697264204341300a0603"
        "551d1504030a0108300506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"));
    Require(invalidMiddleIssuer != nullptr);
    Require(invalidMiddleIssuer->flags == 0x80);
    Require(invalidMiddleIssuer->issuers != nullptr && invalidMiddleIssuer->issuers->num == 1);
    Require(RevokedEntryAt(invalidMiddleIssuer, 0)->issuer != nullptr &&
            RevokedEntryAt(invalidMiddleIssuer, 0)->issuer->num == 1 &&
            RevokedEntryAt(invalidMiddleIssuer, 0)->reason == 1);
    Require(RevokedEntryAt(invalidMiddleIssuer, 1)->issuer == nullptr &&
            RevokedEntryAt(invalidMiddleIssuer, 1)->reason == 0);
    Require(RevokedEntryAt(invalidMiddleIssuer, 2)->issuer == nullptr &&
            RevokedEntryAt(invalidMiddleIssuer, 2)->reason == 0);
    RequireQueue({168, 58});
    for (const Expected& row : std::initializer_list<Expected>{
             {0x01, otherName, 1, 0}, {0x01, invalidMiddleIssuer->crl->issuer, 0, -1},
             {0x02, otherName, 0, -1}, {0x02, invalidMiddleIssuer->crl->issuer, 1, 1}, {0x02, nullptr, 1, 1},
             {0x03, thirdName, 0, -1}, {0x03, invalidMiddleIssuer->crl->issuer, 1, 2}, {0x03, nullptr, 1, 2}}) {
        RequireExpected(invalidMiddleIssuer, row);
    }
    FreeCrlValue(invalidMiddleIssuer);

    LibreSsl3X509Crl* invalidMiddleReason = Parse(Hex(
        "3082015330820105020101300506032b657030123110300e06035504030c0754657374204341170d3234303130"
        "313030303030305a3081d53045020101170d3234303130323030303030305a303130230603551d1d0101ff0419"
        "3017a41530133111300f06035504030c084f74686572204341300a0603551d1504030a01013044020102170d32"
        "34303130323030303030305a303030230603551d1d0101ff04193017a41530133111300f06035504030c085468"
        "69726420434130090603551d15040205003046020103170d3234303130323030303030305a303230240603551d"
        "1d0101ff041a3018a41630143112301006035504030c09466f75727468204341300a0603551d1504030a010830"
        "0506032b65700341005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
        "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"));
    Require(invalidMiddleReason != nullptr);
    Require(invalidMiddleReason->flags == 0x80);
    Require(invalidMiddleReason->issuers != nullptr && invalidMiddleReason->issuers->num == 2);
    Require(RevokedEntryAt(invalidMiddleReason, 0)->issuer != nullptr &&
            RevokedEntryAt(invalidMiddleReason, 0)->issuer->num == 1 &&
            RevokedEntryAt(invalidMiddleReason, 0)->reason == 1);
    Require(RevokedEntryAt(invalidMiddleReason, 1)->issuer != nullptr &&
            RevokedEntryAt(invalidMiddleReason, 1)->issuer->num == 1 &&
            RevokedEntryAt(invalidMiddleReason, 1)->reason == 0);
    Require(RevokedEntryAt(invalidMiddleReason, 2)->issuer == nullptr &&
            RevokedEntryAt(invalidMiddleReason, 2)->reason == 0);
    RequireQueue({168, 58});
    for (const Expected& row : std::initializer_list<Expected>{
             {0x01, otherName, 1, 0}, {0x02, otherName, 0, -1}, {0x02, thirdName, 1, 1},
             {0x03, fourthName, 0, -1}, {0x03, invalidMiddleReason->crl->issuer, 1, 2}, {0x03, nullptr, 1, 2}}) {
        RequireExpected(invalidMiddleReason, row);
    }
    FreeCrlValue(invalidMiddleReason);

    FreeCrlValue(fourth);
    FreeCrlValue(third);
    FreeCrlValue(other);
    Require(allocations.empty());
}

void RequireAuxiliaryAllocationFailures(const Bytes& der, bool allowSilentReasonMalloc, bool expectAuxError) {
    bool invalid = false;
    bool sawMallocFailure = false;
    int auxErrors = 0;
    bool completed = false;
    for (int budget = 0; budget < 500; ++budget) {
        const unsigned char* input = der.data();
        allocationsUntilFailure = budget;
        LibreSsl3X509Crl* crl = d2i_X509_CRL_nid_postfix(nullptr, &input, static_cast<std::int64_t>(der.size()));
        const bool failed = allocationsUntilFailure < 0;
        allocationsUntilFailure = -1;
        if (crl) {
            if (failed) {
                Require(crl->flags == 0x80);
                const std::uint64_t first = ERR_get_error_nid_postfix();
                if (first == Error(MallocFailure)) {
                    sawMallocFailure = true;
                } else {
                    Require(allowSilentReasonMalloc && first == 0);
                }
                DrainErrors();
                invalid = true;
            } else {
                Require(crl->flags == 0);
                RequireQueue({});
                completed = true;
                FreeCrlValue(crl);
                Require(allocations.empty());
                break;
            }
            FreeCrlValue(crl);
            Require(allocations.empty());
            continue;
        }
        Require(failed);
        Require(allocations.empty());
        const std::uint64_t first = ERR_get_error_nid_postfix();
        if (first == Error(AuxError)) {
            RequireQueue({});
            ++auxErrors;
        } else {
            DrainErrors();
        }
    }
    Require(completed);
    Require(invalid);
    Require(sawMallocFailure);
    Require(expectAuxError ? auxErrors >= 1 : auxErrors == 0);
    Require(allocations.empty());
}

void TestCertificateIssuerAllocationFailures() {
    RequireAuxiliaryAllocationFailures(Hex(IndirectCrlHex), true, true);
    RequireAuxiliaryAllocationFailures(Hex(FiveNamesCrlHex), false, true);
    RequireAuxiliaryAllocationFailures(Hex(DirectCrlHex), true, false);
    RequireAuxiliaryAllocationFailures(Hex(FourIssuersCrlHex), false, true);
}

void TestFiveNamesOneEntry() {
    LibreSsl3X509Crl* fiveNamesCrl = Parse(Hex(FiveNamesCrlHex));
    Require(fiveNamesCrl != nullptr);
    Require(fiveNamesCrl->flags == 0);
    Require(fiveNamesCrl->issuers != nullptr && fiveNamesCrl->issuers->num == 1);
    Require(RevokedEntryAt(fiveNamesCrl, 0)->issuer != nullptr &&
            RevokedEntryAt(fiveNamesCrl, 0)->issuer->num == 5 &&
            RevokedEntryAt(fiveNamesCrl, 0)->reason == -1);
    Require(RevokedEntryAt(fiveNamesCrl, 1)->issuer != nullptr &&
            RevokedEntryAt(fiveNamesCrl, 1)->issuer->num == 5 &&
            RevokedEntryAt(fiveNamesCrl, 1)->reason == -1);
    LibreSsl3X509Crl* other = NamedCrl("Other CA");
    LibreSsl3X509Crl* fourth = NamedCrl("Fourth CA");
    const Expected fiveNamesExpected[] = {
        {0x01, other->crl->issuer, 1, 0}, {0x01, fourth->crl->issuer, 1, 0}, {0x01, fiveNamesCrl->crl->issuer, 0, -1},
        {0x01, nullptr, 0, -1}, {0x02, other->crl->issuer, 1, 1}, {0x02, fourth->crl->issuer, 1, 1},
        {0x02, fiveNamesCrl->crl->issuer, 0, -1},
    };
    for (const Expected& row : fiveNamesExpected) RequireExpected(fiveNamesCrl, row);
    FreeCrlValue(fourth);
    FreeCrlValue(other);
    FreeCrlValue(fiveNamesCrl);
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

Bytes Sha512(const Bytes& data) {
    Sha512Context context;
    Require(SHA512_Init_nid_postfix(&context) == 1);
    Require(SHA512_Update_nid_postfix(&context, data.data(), data.size()) == 1);
    Bytes digest(64);
    Require(SHA512_Final_nid_postfix(digest.data(), &context) == 1);
    return digest;
}

void RequireHash(const LibreSsl3X509Crl* crl, const Bytes& expected) {
    Require(expected.size() == sizeof(crl->hash));
    Require(std::memcmp(crl->hash, expected.data(), sizeof(crl->hash)) == 0);
}

Bytes AuthorityKeyIdExtension(bool critical) {
    return Extension({0x55, 0x1d, 0x23}, critical, Sequence({Tlv(0x80, Bytes{0x01, 0x02, 0x03})}));
}

Bytes DeltaCrlExtension(bool critical) {
    return Extension({0x55, 0x1d, 0x1b}, critical, Tlv(0x02, Bytes{0x07}));
}

Bytes IssuingDistributionPointExtension() {
    return Extension({0x55, 0x1d, 0x1c}, true, Sequence({}));
}

Bytes FreshestCrlExtension() {
    return Extension({0x55, 0x1d, 0x2e}, false, Sequence({}));
}

Bytes DistributionPointsExtension(bool critical) {
    return Extension({0x55, 0x1d, 0x1f}, critical, Sequence({}));
}

Bytes CrlWithExtensions(const Bytes& extensions) {
    return BuildCrl(Join({TbsHead(), Tlv(0xa0, Tlv(0x30, extensions))}));
}

Bytes CrlWithEntries(const Bytes& entries) {
    return BuildCrl(Join({TbsHead(), Tlv(0x30, entries)}));
}

int ParsedFlags(const Bytes& der) {
    LibreSsl3X509Crl* crl = Parse(der);
    Require(crl != nullptr);
    const int flags = crl->flags;
    FreeCrlValue(crl);
    return flags;
}

void TestPostDecodeDefaults() {
    const Bytes der = BuildCrl(TbsHead());
    LibreSsl3X509Crl* crl = Parse(der);
    Require(crl != nullptr);
    RequireHash(crl, Hex("806340a019a7679f4c7c7be838792eff1a93a1743df6505d9d2c9a8a8c75b022"
                         "fb4e8c195b0cf7e1b25ced77f9594ac87e4fec74149ade8a513ef0ea9e28590f"));
    RequireHash(crl, Sha512(der));
    Require(crl->flags == 0);
    Require(crl->akid == nullptr && crl->idp == nullptr && crl->issuers == nullptr);
    Require(crl->crlNumber == nullptr && crl->baseCrlNumber == nullptr);
    Require(crl->idpFlags == 0);
    Require(crl->idpReasons == 0x807f);
    FreeCrlValue(crl);
    RequireQueue({});
    Require(allocations.empty());
}

void TestHashOfReencoding() {
    const Bytes tbs = NonMinimal(0x30, Join({NonMinimal(0x02, Bytes{0x01}), SignatureAlgorithm(),
        IssuerName("Test CA"), UtcTime("240101000000Z")}));
    const Bytes algorithm = Join({Oid({0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b}), Tlv(0x05, Bytes{})});
    const Bytes received = NonMinimal(0x30, Join({tbs, NonMinimal(0x30, algorithm), Bytes{0x03, 0x81, 0x03, 0x04, 0x5a, 0x5f}}));
    LibreSsl3X509Crl* crl = Parse(received);
    Require(crl != nullptr);
    Require(crl->signature->length == 2 && crl->signature->data[1] == 0x50);
    RequireHash(crl, Sha512(Sequence({tbs, Tlv(0x30, algorithm), Bytes{0x03, 0x03, 0x04, 0x5a, 0x50}})));
    FreeCrlValue(crl);

    const Bytes octetOid = Oid({0x2b, 0x06, 0x01, 0x04, 0x01});
    crl = Parse(BuildCrl(TbsHead(), Sequence({octetOid, NonMinimal(0x04, Bytes{0x01, 0x02})})));
    Require(crl != nullptr);
    RequireHash(crl, Sha512(BuildCrl(TbsHead(), Sequence({octetOid, Tlv(0x04, Bytes{0x01, 0x02})}))));
    FreeCrlValue(crl);

    const Bytes absent = BuildCrl(TbsHead(), Sequence({Oid({0x2b, 0x65, 0x70})}));
    crl = Parse(absent);
    Require(crl != nullptr);
    RequireHash(crl, Sha512(absent));
    FreeCrlValue(crl);

    const Bytes integerParameter = BuildCrl(TbsHead(), Sequence({Oid({0x2b, 0x65, 0x70}), Tlv(0x02, Bytes{0x00, 0x80})}));
    crl = Parse(integerParameter);
    Require(crl != nullptr);
    RequireHash(crl, Sha512(integerParameter));
    FreeCrlValue(crl);

    crl = Parse(BuildCrl(TbsHead(), Sequence({Oid({0x2b, 0x65, 0x70}), Tlv(0x03, Bytes{0x04, 0x5f})})));
    Require(crl != nullptr);
    RequireHash(crl, Sha512(BuildCrl(TbsHead(), Sequence({Oid({0x2b, 0x65, 0x70}), Tlv(0x03, Bytes{0x04, 0x50})}))));
    FreeCrlValue(crl);

    crl = Parse(BuildCrl(TbsHead(), Sequence({Oid({0x2b, 0x65, 0x70}), Tlv(0x03, Bytes{0x03})})));
    Require(crl != nullptr);
    RequireHash(crl, Sha512(BuildCrl(TbsHead(), Sequence({Oid({0x2b, 0x65, 0x70}), Tlv(0x03, Bytes{0x00})}))));
    FreeCrlValue(crl);
    Require(allocations.empty());
}

void TestCachedExtensions() {
    LibreSsl3X509Crl* crl = Parse(CrlWithExtensions(Join({CrlNumberExtension(5), DeltaCrlExtension(true),
        AuthorityKeyIdExtension(false)})));
    Require(crl != nullptr);
    RequireInteger(crl->crlNumber, {0x05});
    RequireInteger(crl->baseCrlNumber, {0x07});
    Require(crl->akid != nullptr);
    RequireString(*static_cast<Asn1String* const*>(crl->akid), 4, "\x01\x02\x03");
    Require(crl->flags == 0);
    FreeCrlValue(crl);

    Require(ParsedFlags(CrlWithExtensions(DeltaCrlExtension(false))) == 0x80);
    Require(ParsedFlags(CrlWithExtensions(FreshestCrlExtension())) == 0x1000);
    Require(ParsedFlags(CrlWithExtensions(DistributionPointsExtension(true))) == 0x200);
    Require(ParsedFlags(CrlWithExtensions(Join({DistributionPointsExtension(false), FreshestCrlExtension()}))) == 0x1000);
    Require(ParsedFlags(CrlWithExtensions(Join({AuthorityKeyIdExtension(true), DistributionPointsExtension(true)}))) == 0x200);
    Require(ParsedFlags(CrlWithExtensions(Join({CrlNumberExtension(5), DeltaCrlExtension(true), DistributionPointsExtension(true)}))) == 0x200);
    Require(ParsedFlags(CrlWithExtensions(Join({AuthorityKeyIdExtension(true), DistributionPointsExtension(false)}))) == 0);
    Require(ParsedFlags(CrlWithExtensions(Join({DistributionPointsExtension(true), FreshestCrlExtension()}))) == 0x200);

    RequireNotImplemented([] { Parse(CrlWithExtensions(Join({CrlNumberExtension(5), IssuingDistributionPointExtension()}))); });
    Require(allocations.empty());
    crl = Parse(CrlWithExtensions(Join({IssuingDistributionPointExtension(), IssuingDistributionPointExtension()})));
    Require(crl != nullptr);
    Require(crl->idp == nullptr);
    Require(crl->flags == 0);
    FreeCrlValue(crl);
    RequireQueue({});
    Require(allocations.empty());
}

void TestEntryReasons() {
    LibreSsl3X509Crl* crl = Parse(CrlWithEntries(Join({
        RevokedEntry(Bytes{0x10, 0x01}),
        RevokedEntryWithExtensions(Bytes{0x10, 0x02}, CrlReasonExtension(1)),
        RevokedEntryWithExtensions(Bytes{0x10, 0x03}, Extension({0x55, 0x1d, 0x15}, false, Tlv(0x0a, Bytes{0xff}))),
        RevokedEntryWithExtensions(Bytes{0x10, 0x04}, CertificateIssuerExtension("Other CA")),
    })));
    Require(crl != nullptr);
    Require(RevokedEntryAt(crl, 0)->reason == -1);
    Require(RevokedEntryAt(crl, 1)->reason == 1);
    Require(RevokedEntryAt(crl, 2)->reason == -1);
    Require(RevokedEntryAt(crl, 3)->reason == -1);
    Require(crl->flags == 0);
    FreeCrlValue(crl);
    RequireQueue({});

    crl = Parse(CrlWithEntries(Join({
        RevokedEntryWithExtensions(Bytes{0x10, 0x01}, CrlReasonExtension(1)),
        RevokedEntryWithExtensions(Bytes{0x10, 0x02}, Join({CrlReasonExtension(1), CrlReasonExtension(2)})),
        RevokedEntryWithExtensions(Bytes{0x10, 0x03}, CrlReasonExtension(3)),
    })));
    Require(crl != nullptr);
    Require(RevokedEntryAt(crl, 0)->reason == 1);
    Require(RevokedEntryAt(crl, 1)->reason == 0);
    Require(RevokedEntryAt(crl, 2)->reason == 0);
    Require(crl->flags == 0x80);
    FreeCrlValue(crl);

    Require(ParsedFlags(CrlWithEntries(RevokedEntryWithExtensions(Bytes{0x10, 0x01},
        Extension({0x55, 0x1d, 0x15}, false, Tlv(0x02, Bytes{0x01}))))) == 0x80);
    DrainErrors();
    Require(ParsedFlags(CrlWithEntries(RevokedEntryWithExtensions(Bytes{0x10, 0x01},
        Extension({0x55, 0x1d, 0x18}, true, GeneralizedTime("20240102000000Z"))))) == 0x200);
    Require(ParsedFlags(CrlWithEntries(RevokedEntryWithExtensions(Bytes{0x10, 0x01},
        Join({CertificateIssuerExtension("Other CA"), CertificateIssuerExtension("Other CA")})))) == 0x80);

    crl = Parse(CrlWithEntries(RevokedEntryWithExtensions(Bytes{0x10, 0x01},
        Extension({0x55, 0x1d, 0x15}, false, Tlv(0x0a, Bytes(9, 0x01))))));
    Require(crl != nullptr);
    Require(RevokedEntryAt(crl, 0)->reason == -1);
    FreeCrlValue(crl);
    RequireQueue({223});
    Require(allocations.empty());
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
    TestOracleIndirectCrl();
    TestOracleDirectCrl();
    TestOracleCertificateIssuerForms();
    TestCertificateIssuerUnsupportedForms();
    TestCertificateIssuerInvalidMiddle();
    TestFiveNamesOneEntry();
    TestCertificateIssuerAllocationFailures();
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
    TestPostDecodeDefaults();
    TestHashOfReencoding();
    TestCachedExtensions();
    TestEntryReasons();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 X509_CRL tests passed");
}
