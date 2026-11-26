#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int Ok = 0;
constexpr int Unspecified = 1;
constexpr int SubjectIssuerMismatch = 29;
constexpr int AkidSkidMismatch = 30;
constexpr int AkidIssuerSerialMismatch = 31;
constexpr int KeyUsageNoCertSign = 32;
constexpr int KeyUsageNoDigitalSignature = 39;

constexpr int SubjectKeyIdentifierNid = 82;
constexpr int KeyUsageNid = 83;
constexpr int AuthorityKeyIdentifierNid = 90;

constexpr unsigned char KeyCertSignMask = 0x04;
constexpr unsigned char DigitalSignatureMask = 0x80;
constexpr unsigned char BitStringTag = 3;

struct Extension {
    void* object;
    std::int32_t critical;
    std::int32_t padding;
    void* value;
};

struct Slice {
    const unsigned char* data = nullptr;
    std::size_t length = 0;
};

bool ReadTlv(Slice& input, unsigned char& tag, Slice& content) {
    if (input.length < 2) return false;
    tag = input.data[0];
    std::size_t cursor = 1;
    std::size_t length = input.data[cursor++];
    if (length & 0x80) {
        const std::size_t count = length & 0x7f;
        if (count == 0 || count > sizeof(std::size_t) || count > input.length - cursor) return false;
        length = 0;
        for (std::size_t index = 0; index < count; ++index) length = (length << 8) | input.data[cursor++];
    }
    if (length > input.length - cursor) return false;
    content.data = input.data + cursor;
    content.length = length;
    input.data += cursor + length;
    input.length -= cursor + length;
    return true;
}

const LibreSslAsn1Object* ObjectOf(const void* extension) {
    return static_cast<const LibreSslAsn1Object*>(static_cast<const Extension*>(extension)->object);
}

const Asn1String* ValueOf(const void* extension) {
    return static_cast<const Asn1String*>(static_cast<const Extension*>(extension)->value);
}

const void* FindExtension(const LibreSslX509CertInfo& info, int nid) {
    const auto* stack = static_cast<const LibreSslStack*>(info.extensions);
    if (!stack || stack->num <= 0) return nullptr;
    for (int index = 0; index < stack->num; ++index) {
        const void* extension = stack->data[index];
        if (!extension) continue;
        const auto* object = ObjectOf(extension);
        if (object && object->nid == nid) return extension;
    }
    return nullptr;
}

bool ValueBytes(const Asn1String* value, Slice& bytes) {
    if (!value || value->length < 0) return false;
    if (value->length > 0 && !value->data) return false;
    bytes.data = value->data;
    bytes.length = static_cast<std::size_t>(value->length);
    return true;
}

bool SameValue(const Asn1String* first, const Asn1String* second) {
    if (!first || !second) return first == second;
    if (first->length != second->length || first->type != second->type) return false;
    if (first->length <= 0) return true;
    if (!first->data || !second->data) return false;
    return std::memcmp(first->data, second->data, static_cast<std::size_t>(first->length)) == 0;
}

bool SameName(const LibreSslX509Name* first, const LibreSslX509Name* second) {
    if (!first || !second) return first == second;
    const auto* firstStack = static_cast<const LibreSslStack*>(first->entries);
    const auto* secondStack = static_cast<const LibreSslStack*>(second->entries);
    if (!firstStack || !secondStack) return firstStack == secondStack;
    if (firstStack->num != secondStack->num) return false;
    for (int index = 0; index < firstStack->num; ++index) {
        const auto* firstEntry = reinterpret_cast<const LibreSslX509NameEntry*>(firstStack->data[index]);
        const auto* secondEntry = reinterpret_cast<const LibreSslX509NameEntry*>(secondStack->data[index]);
        if (!firstEntry || !secondEntry) return firstEntry == secondEntry;
        const auto* firstObject = static_cast<const LibreSslAsn1Object*>(firstEntry->object);
        const auto* secondObject = static_cast<const LibreSslAsn1Object*>(secondEntry->object);
        if ((!firstObject || !secondObject) ? firstObject != secondObject : firstObject->nid != secondObject->nid) return false;
        if (!SameValue(static_cast<const Asn1String*>(firstEntry->value),
                       static_cast<const Asn1String*>(secondEntry->value))) return false;
    }
    return true;
}

bool KeyIdentifier(const void* extension, Slice& keyId) {
    Slice content;
    if (!ValueBytes(ValueOf(extension), content)) return false;
    unsigned char tag = 0;
    Slice body;
    if (!ReadTlv(content, tag, body)) return false;
    if (tag == 0x30) {
        Slice sequence = body;
        while (sequence.length != 0) {
            unsigned char childTag = 0;
            Slice child;
            if (!ReadTlv(sequence, childTag, child)) return false;
            if (childTag == 0x80) {
                keyId = child;
                return true;
            }
        }
        return false;
    }
    if (tag != 0x04) return false;
    keyId = body;
    return true;
}

bool AkidSerial(const void* extension, Slice& serial, bool& hasIssuer) {
    Slice content;
    if (!ValueBytes(ValueOf(extension), content)) return false;
    unsigned char tag = 0;
    Slice sequence;
    if (!ReadTlv(content, tag, sequence) || tag != 0x30) return false;
    while (sequence.length != 0) {
        unsigned char childTag = 0;
        Slice child;
        if (!ReadTlv(sequence, childTag, child)) return false;
        if (childTag == 0x82) serial = child;
        if (childTag == 0xa1) hasIssuer = true;
    }
    return true;
}

bool IssuerKeyUsage(const void* extension, unsigned char& firstBits) {
    Slice content;
    if (!ValueBytes(ValueOf(extension), content)) return false;
    unsigned char tag = 0;
    Slice body;
    if (!ReadTlv(content, tag, body) || tag != BitStringTag || body.length == 0) return false;
    if (body.length < 2) return false;
    firstBits = body.data[1];
    return true;
}

bool HasSymbolExtension(const LibreSslX509CertInfo& info, const char* name) {
    const int nid = OBJ_sn2nid_nid_postfix(name);
    if (nid == 0) return false;
    return FindExtension(info, nid) != nullptr;
}

}

extern "C" {

int APS5_VABI X509_check_issued_nid_postfix(LibreSslX509* issuer, LibreSslX509* subject) {
    if (!issuer || !subject) throw std::invalid_argument("X509_check_issued: null certificate");
    const auto* issuerInfo = static_cast<const LibreSslX509CertInfo*>(issuer->certificateInfo);
    const auto* subjectInfo = static_cast<const LibreSslX509CertInfo*>(subject->certificateInfo);
    if (!issuerInfo || !subjectInfo) return Unspecified;
    if (!SameName(static_cast<const LibreSslX509Name*>(issuerInfo->subject),
                  static_cast<const LibreSslX509Name*>(subjectInfo->issuer)))
        return SubjectIssuerMismatch;
    const void* akid = FindExtension(*subjectInfo, AuthorityKeyIdentifierNid);
    if (akid) {
        Slice subjectKeyId;
        const void* skid = FindExtension(*issuerInfo, SubjectKeyIdentifierNid);
        if (skid && KeyIdentifier(skid, subjectKeyId)) {
            Slice akidKeyId;
            if (KeyIdentifier(akid, akidKeyId) &&
                (akidKeyId.length != subjectKeyId.length ||
                 std::memcmp(akidKeyId.data, subjectKeyId.data, akidKeyId.length) != 0))
                return AkidSkidMismatch;
        }
        Slice akidSerial;
        bool hasIssuer = false;
        if (AkidSerial(akid, akidSerial, hasIssuer)) {
            if (hasIssuer) NotImplemented_nid_no_patch("X509_check_issued: an authority key identifier with an issuer name");
            if (akidSerial.length != 0) {
                const auto* serial = static_cast<const Asn1String*>(issuerInfo->serialNumber);
                std::size_t length = 0;
                const unsigned char* data = nullptr;
                if (serial) { length = static_cast<std::size_t>(serial->length); data = serial->data; }
                if (length != akidSerial.length || std::memcmp(data, akidSerial.data, length) != 0)
                    return AkidIssuerSerialMismatch;
            }
        }
    }
    const void* usage = FindExtension(*issuerInfo, KeyUsageNid);
    if (usage) {
        unsigned char bits = 0;
        if (IssuerKeyUsage(usage, bits)) {
            const bool proxy = HasSymbolExtension(*subjectInfo, "proxyCertInfo");
            if (proxy) {
                if ((bits & DigitalSignatureMask) == 0) return KeyUsageNoDigitalSignature;
            } else if ((bits & KeyCertSignMask) == 0) {
                return KeyUsageNoCertSign;
            }
        }
    }
    return Ok;
}

}
