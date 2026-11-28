#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

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

void Require(bool condition, std::source_location location = std::source_location::current());

std::unordered_map<void*, std::size_t> allocations;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* result = std::malloc(size);
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
    void* result = std::calloc(count, size);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned allocation");
}

void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned reallocation");
}

int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected posix_memalign");
}

void Require(bool condition, std::source_location location) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 X509_add1_ext_i2d check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
void RequireInvalidArgument(Function function, std::source_location location = std::source_location::current()) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

template <typename Function>
void RequireNotImplemented(Function function, std::source_location location = std::source_location::current()) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, location);
}

constexpr int NetscapeCertTypeNid = 71;
constexpr int NetscapeBaseUrlNid = 72;
constexpr int SubjectKeyIdentifierNid = 82;
constexpr int KeyUsageNid = 83;
constexpr int CrlNumberNid = 88;
constexpr int DeltaCrlNid = 140;
constexpr int CrlReasonNid = 141;
constexpr int InvalidityDateNid = 142;
constexpr int OcspNonceNid = 366;
constexpr int OcspArchiveCutoffNid = 370;
constexpr int HoldInstructionCodeNid = 430;
constexpr int InhibitAnyPolicyNid = 748;
constexpr int SubjectAltNameNid = 85;
constexpr int UndefinedNid = 0;
constexpr int UnregisteredExtensionNid = 30000;

constexpr std::uint64_t DefaultFlag = 0;
constexpr std::uint64_t AppendFlag = 1;
constexpr std::uint64_t ReplaceFlag = 2;
constexpr std::uint64_t ReplaceExistingFlag = 3;
constexpr std::uint64_t KeepExistingFlag = 4;
constexpr std::uint64_t DeleteFlag = 5;
constexpr int UnregisteredNid = 100000;
constexpr std::uint64_t SilentFlag = 0x10;

constexpr std::uint64_t ErrorCode(int reason) {
    return (34ULL << 24) | (0xfffULL << 12) | static_cast<std::uint64_t>(reason);
}

constexpr int UnknownNid = 101;

std::vector<int> Reasons() {
    std::vector<int> reasons;
    for (std::uint64_t error = ERR_get_error_nid_postfix(); error != 0; error = ERR_get_error_nid_postfix())
        reasons.push_back(static_cast<int>(error & 0xfff));
    return reasons;
}

Asn1String StringValue(int type, const unsigned char* data, int length, std::int64_t flags = 0) {
    return Asn1String{length, type, const_cast<unsigned char*>(data), flags};
}

std::vector<unsigned char> Expected(std::initializer_list<unsigned char> bytes) {
    return std::vector<unsigned char>(bytes);
}

LibreSslStack* Stack(const LibreSsl3ExtensionsCertInfo& info) {
    return reinterpret_cast<LibreSslStack*>(const_cast<LibreSsl3Stack*>(info.extensions));
}

struct Certificate {
    LibreSsl3ExtensionsCertInfo info{};
    LibreSsl3ExtensionsX509 x509{};

    Certificate() {
        x509.certInfo = &info;
    }

    ~Certificate() {
        auto* stack = Stack(info);
        if (!stack) return;
        for (int index = 0; index < stack->num; ++index) X509_EXTENSION_free_nid_postfix(stack->data[index]);
        sk_free_nid_postfix(stack);
        info.extensions = nullptr;
    }

    int Count() const {
        auto* stack = Stack(info);
        return stack ? stack->num : 0;
    }
};

const LibreSsl3X509Extension* ExtensionAt(const Certificate& certificate, int index) {
    auto* stack = Stack(certificate.info);
    Require(stack != nullptr);
    Require(index >= 0 && index < stack->num);
    return reinterpret_cast<const LibreSsl3X509Extension*>(stack->data[index]);
}

std::vector<unsigned char> ValueBytes(const LibreSsl3X509Extension* extension) {
    const auto* value = static_cast<const Asn1String*>(extension->value);
    Require(value != nullptr);
    if (value->length == 0) return {};
    Require(value->data != nullptr);
    return std::vector<unsigned char>(value->data, value->data + value->length);
}

void ClearErrors() {
    while (ERR_get_error_nid_postfix() != 0) {
    }
}

void CheckBitStrings() {
    Certificate certificate;
    const unsigned char highBit[] = {0x80};
    const unsigned char trailingZero[] = {0x80, 0x00};
    Asn1String explicitBits = StringValue(3, highBit, 1, 0x08 | 7);
    Asn1String trimmed = StringValue(3, trailingZero, 2);
    Asn1String empty = StringValue(3, nullptr, 0);

    ClearErrors();
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &explicitBits, 1, DefaultFlag) == 1);
    Require(certificate.Count() == 1);
    Require(ExtensionAt(certificate, 0)->object->nid == KeyUsageNid);
    Require(X509_EXTENSION_get_critical_nid_postfix(ExtensionAt(certificate, 0)) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x03, 0x02, 0x07, 0x80}));
    Require(ERR_get_error_nid_postfix() == 0);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, NetscapeCertTypeNid, &trimmed, 0, DefaultFlag) == 1);
    Require(certificate.Count() == 2);
    Require(ValueBytes(ExtensionAt(certificate, 1)) == Expected({0x03, 0x02, 0x07, 0x80}));
    Require(X509_EXTENSION_get_critical_nid_postfix(ExtensionAt(certificate, 1)) == 0);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &empty, 0, AppendFlag) == 1);
    Require(certificate.Count() == 3);
    Require(ValueBytes(ExtensionAt(certificate, 2)) == Expected({0x03, 0x01, 0x00}));
}

void CheckStrings() {
    Certificate certificate;
    const unsigned char octets[] = {0xAA, 0xBB, 0xCC};
    const unsigned char ia5[] = {'a', 'b', 'c'};
    const unsigned char time[] = "20240102030405Z";
    Asn1String octetValue = StringValue(4, octets, 3);
    Asn1String ia5Value = StringValue(22, ia5, 3);
    Asn1String timeValue = StringValue(24, time, 15);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectKeyIdentifierNid, &octetValue, 1, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x04, 0x03, 0xAA, 0xBB, 0xCC}));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, NetscapeBaseUrlNid, &ia5Value, 0, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 1)) == Expected({0x16, 0x03, 'a', 'b', 'c'}));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, InvalidityDateNid, &timeValue, 0, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 2)) ==
            Expected({0x18, 0x0F, '2', '0', '2', '4', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', 'Z'}));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, OcspArchiveCutoffNid, &timeValue, 0, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 3)) ==
            Expected({0x18, 0x0F, '2', '0', '2', '4', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', 'Z'}));
}

void CheckIntegers() {
    Certificate certificate;
    const unsigned char positive[] = {0x80};
    const unsigned char negative[] = {0x81};
    const unsigned char one[] = {0x01};
    Asn1String crlNumber = StringValue(2, positive, 1);
    Asn1String deltaCrl = StringValue(0x102, negative, 1);
    Asn1String crlReason = StringValue(2, one, 1);
    Asn1String zero = StringValue(2, nullptr, 0);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, CrlNumberNid, &crlNumber, 0, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x02, 0x02, 0x00, 0x80}));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, DeltaCrlNid, &deltaCrl, 0, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 1)) == Expected({0x02, 0x02, 0xFF, 0x7F}));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, CrlReasonNid, &crlReason, 0, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 2)) == Expected({0x0A, 0x01, 0x01}));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, InhibitAnyPolicyNid, &zero, 0, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 3)) == Expected({0x02, 0x01, 0x00}));

    const unsigned char trailing[] = {0x01, 0x00};
    const unsigned char padded[] = {0x80, 0x00};
    Certificate negatives;
    Asn1String trailingZero = StringValue(0x102, trailing, 2);
    Require(X509_add1_ext_i2d_nid_postfix(&negatives.x509, DeltaCrlNid, &trailingZero, 0, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(negatives, 0)) == Expected({0x02, 0x02, 0xFF, 0x00}));
    Asn1String paddedZero = StringValue(0x102, padded, 2);
    Require(X509_add1_ext_i2d_nid_postfix(&negatives.x509, DeltaCrlNid, &paddedZero, 0, AppendFlag) == 1);
    Require(ValueBytes(ExtensionAt(negatives, 1)) == Expected({0x02, 0x02, 0x80, 0x00}));
}

void CheckObject() {
    Certificate certificate;
    const unsigned char oid[] = {0x55, 0x1D, 0x13};
    LibreSslAsn1Object object{nullptr, nullptr, 0, 3, oid, 0};

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, HoldInstructionCodeNid, &object, 1, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x06, 0x03, 0x55, 0x1D, 0x13}));
}

void CheckNonce() {
    Certificate certificate;
    const unsigned char nonce[] = {0x01, 0x02, 0x03};
    Asn1String nonceValue = StringValue(4, nonce, 3);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, OcspNonceNid, &nonceValue, 0, DefaultFlag) == 1);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x01, 0x02, 0x03}));

    Asn1String empty = StringValue(4, nullptr, 0);
    ClearErrors();
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, OcspNonceNid, &empty, 0, AppendFlag) == 0);
    Require(certificate.Count() == 1);
    Require(ERR_get_error_nid_postfix() == ErrorCode(65));
    Require(ERR_get_error_nid_postfix() == ErrorCode(144));
    Require(ERR_get_error_nid_postfix() == 0);
}

void CheckFlags() {
    Certificate certificate;
    const unsigned char highBit[] = {0x80};
    const unsigned char secondBit[] = {0x40};
    const unsigned char octets[] = {0xAA, 0xBB};
    Asn1String first = StringValue(3, highBit, 1, 0x08);
    Asn1String second = StringValue(3, secondBit, 1, 0x08 | 1);
    Asn1String octetValue = StringValue(4, octets, 2);

    ClearErrors();
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &first, 1, DefaultFlag) == 1);
    Require(ERR_get_error_nid_postfix() == 0);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &second, 0, DefaultFlag) == 0);
    Require(certificate.Count() == 1);
    Require(ERR_get_error_nid_postfix() == ErrorCode(145));
    Require(ERR_get_error_nid_postfix() == 0);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x03, 0x02, 0x00, 0x80}));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &second, 0, DefaultFlag | SilentFlag) == 0);
    Require(ERR_get_error_nid_postfix() == 0);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &second, 0, KeepExistingFlag) == 1);
    Require(certificate.Count() == 1);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x03, 0x02, 0x00, 0x80}));
    Require(ERR_get_error_nid_postfix() == 0);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &second, 0, ReplaceFlag) == 1);
    Require(certificate.Count() == 1);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x03, 0x02, 0x01, 0x40}));
    Require(X509_EXTENSION_get_critical_nid_postfix(ExtensionAt(certificate, 0)) == 0);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &first, 0, AppendFlag) == 1);
    Require(certificate.Count() == 2);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x03, 0x02, 0x01, 0x40}));
    Require(ValueBytes(ExtensionAt(certificate, 1)) == Expected({0x03, 0x02, 0x00, 0x80}));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &second, 0, DeleteFlag) == 1);
    Require(certificate.Count() == 1);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x03, 0x02, 0x00, 0x80}));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &second, 0, DeleteFlag) == 1);
    Require(certificate.Count() == 0);

    ClearErrors();
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &second, 0, DeleteFlag) == 0);
    Require(ERR_get_error_nid_postfix() == ErrorCode(102));
    Require(ERR_get_error_nid_postfix() == 0);

    for (const std::uint64_t operation : {ReplaceExistingFlag, DeleteFlag}) {
        ClearErrors();
        Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, UnregisteredNid, &second, 0, operation) == 0);
        Require(Reasons() == std::vector<int>{UnknownNid, 129});
        ClearErrors();
        Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, UnregisteredNid, &second, 0,
                                              operation | SilentFlag) == 0);
        Require(Reasons() == std::vector<int>{UnknownNid});
    }
    Require(certificate.Count() == 0);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, KeyUsageNid, &second, 0, DeleteFlag | SilentFlag) == 0);
    Require(ERR_get_error_nid_postfix() == 0);

    ClearErrors();
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectKeyIdentifierNid, &octetValue, 1,
                                          ReplaceExistingFlag) == 0);
    Require(certificate.Count() == 0);
    Require(ERR_get_error_nid_postfix() == ErrorCode(102));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectKeyIdentifierNid, &octetValue, 1,
                                          ReplaceFlag) == 1);
    Require(certificate.Count() == 1);
    Require(ValueBytes(ExtensionAt(certificate, 0)) == Expected({0x04, 0x02, 0xAA, 0xBB}));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectKeyIdentifierNid, &octetValue, 0,
                                          ReplaceExistingFlag) == 1);
    Require(certificate.Count() == 1);
    Require(X509_EXTENSION_get_critical_nid_postfix(ExtensionAt(certificate, 0)) == 0);

    ClearErrors();
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectKeyIdentifierNid, &octetValue, 1, 6) == -1);
    Require(certificate.Count() == 1);
    Require(ERR_get_error_nid_postfix() == ErrorCode(117));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectKeyIdentifierNid, &octetValue, 1,
                                          6 | SilentFlag) == -1);
    Require(ERR_get_error_nid_postfix() == 0);
    Require(certificate.Count() == 1);
}

void CheckUnsupportedOrdering() {
    Certificate certificate;
    const unsigned char sanOid[] = {0x55, 0x1D, 0x11};
    LibreSslAsn1Object sanObject{nullptr, nullptr, SubjectAltNameNid, 3, sanOid, 0};
    Asn1String sanValue = StringValue(4, nullptr, 0);
    LibreSsl3X509Extension sanExtension{&sanObject, 1, &sanValue};
    Require(X509_add_ext_nid_postfix(&certificate.x509, &sanExtension, -1) == 1);
    Require(certificate.Count() == 1);

    ClearErrors();
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectAltNameNid, &sanValue, 0, DefaultFlag) == 0);
    Require(certificate.Count() == 1);
    Require(ERR_get_error_nid_postfix() == ErrorCode(145));

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectAltNameNid, &sanValue, 0, KeepExistingFlag) == 1);
    Require(certificate.Count() == 1);
    Require(ERR_get_error_nid_postfix() == 0);

    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectAltNameNid, &sanValue, 0, DeleteFlag) == 1);
    Require(certificate.Count() == 0);

    RequireNotImplemented([&] {
        X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectAltNameNid, &sanValue, 0, DefaultFlag);
    });
    Require(certificate.Count() == 0);
}

void CheckFailures() {
    Certificate certificate;
    const unsigned char highBit[] = {0x80};
    Asn1String value = StringValue(3, highBit, 1, 0x08);

    RequireInvalidArgument([&] {
        X509_add1_ext_i2d_nid_postfix(nullptr, KeyUsageNid, &value, 0, DefaultFlag);
    });

    Certificate missingInfo;
    missingInfo.x509.certInfo = nullptr;
    RequireInvalidArgument([&] {
        X509_add1_ext_i2d_nid_postfix(&missingInfo.x509, KeyUsageNid, &value, 0, DefaultFlag);
    });

    ClearErrors();
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, CrlNumberNid, nullptr, 0, DefaultFlag) == 0);
    Require(certificate.Count() == 0);
    Require(ERR_get_error_nid_postfix() == ErrorCode(144));

    ClearErrors();
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, UndefinedNid, &value, 0, DefaultFlag) == 0);
    Require(certificate.Count() == 0);
    Require(ERR_get_error_nid_postfix() == ErrorCode(129));
    Require(ERR_get_error_nid_postfix() == ErrorCode(144));

    const unsigned char octets[] = {0xAA};
    Asn1String negativeLength = StringValue(4, nullptr, -1);
    RequireInvalidArgument([&] {
        X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectKeyIdentifierNid, &negativeLength, 0, DefaultFlag);
    });
    Asn1String nullData = StringValue(4, nullptr, 2);
    RequireInvalidArgument([&] {
        X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectKeyIdentifierNid, &nullData, 0, DefaultFlag);
    });
    LibreSslAsn1Object negativeObject{nullptr, nullptr, 0, -1, nullptr, 0};
    RequireInvalidArgument([&] {
        X509_add1_ext_i2d_nid_postfix(&certificate.x509, HoldInstructionCodeNid, &negativeObject, 0, DefaultFlag);
    });
    Require(certificate.Count() == 0);

    Asn1String octetValue = StringValue(4, octets, 1);
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, SubjectKeyIdentifierNid, &octetValue, 0,
                                          ReplaceExistingFlag) == 0);
    Require(certificate.Count() == 0);
}

}

void CheckAppendSkipsObjectLookup() {
    Certificate certificate;
    const unsigned char octets[] = {0xAA};
    Asn1String value = StringValue(4, octets, 1);
    ClearErrors();
    Require(X509_add1_ext_i2d_nid_postfix(&certificate.x509, UnregisteredExtensionNid, &value, 0, AppendFlag) == 0);
    Require(certificate.Count() == 0);
    Require(ERR_get_error_nid_postfix() == ErrorCode(129));
    Require(ERR_get_error_nid_postfix() == ErrorCode(144));
    Require(ERR_get_error_nid_postfix() == 0);
}

int main() {
    std::array<void*, 7> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckBitStrings();
    CheckStrings();
    CheckIntegers();
    CheckObject();
    CheckNonce();
    CheckFlags();
    CheckAppendSkipsObjectLookup();
    CheckUnsupportedOrdering();
    CheckFailures();

    Require(allocations.empty());
    std::puts("LibreSSL3 X509_add1_ext_i2d tests passed");
}
