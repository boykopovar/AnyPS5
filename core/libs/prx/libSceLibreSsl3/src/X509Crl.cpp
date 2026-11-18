#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int LengthMismatch = 62;
constexpr int MallocFailure = 65;
constexpr int BadObjectHeader = 102;
constexpr int FieldMissing = 121;
constexpr int SequenceLengthMismatch = 148;
constexpr int SequenceNotConstructed = 149;
constexpr int StringTooShort = 152;
constexpr int TooLong = 155;
constexpr int WrongTag = 168;
constexpr int InvalidObjectEncoding = 216;
constexpr int InvalidBitStringBitsLeft = 220;

constexpr int BitStringType = 3;
constexpr int OctetStringType = 4;
constexpr int ObjectType = 6;
constexpr int EnumeratedType = 10;
constexpr int UtcTimeType = 23;
constexpr int GeneralizedTimeType = 24;

constexpr int CrlReasonNid = 141;
constexpr int CrlReasonRemoveFromCrl = 8;

constexpr std::int64_t BitsLeftFlag = 0x08;
constexpr int ObjectDynamic = 0x01;
constexpr int ObjectDynamicData = 0x08;

constexpr unsigned char BooleanTag = 0x01;
constexpr unsigned char IntegerTag = 0x02;
constexpr unsigned char BitStringTag = 0x03;
constexpr unsigned char OctetStringTag = 0x04;
constexpr unsigned char NullTag = 0x05;
constexpr unsigned char ObjectTag = 0x06;
constexpr unsigned char UtcTimeTag = 0x17;
constexpr unsigned char GeneralizedTimeTag = 0x18;
constexpr unsigned char SequenceTag = 0x30;
constexpr unsigned char SetTag = 0x31;
constexpr unsigned char CrlExtensionsTag = 0xa0;

constexpr int StringTags[] = {12, 18, 19, 20, 21, 22, 26, 27, 28, 30};

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

struct Tlv {
    unsigned char tag;
    std::span<const unsigned char> content;
    std::span<const unsigned char> encoded;
};

bool ReadTlv(std::span<const unsigned char>& input, Tlv& value) {
    if (input.size() < 2) {
        PutError(BadObjectHeader);
        return false;
    }
    std::size_t offset = 2;
    std::size_t length = input[1];
    if (length & 0x80) {
        const std::size_t count = length & 0x7f;
        if (count == 0 || count > sizeof(std::size_t) || input.size() < 2 + count || input[2] == 0) {
            PutError(BadObjectHeader);
            return false;
        }
        length = 0;
        for (std::size_t index = 0; index < count; ++index) {
            if (length > (std::numeric_limits<std::size_t>::max() >> 8)) {
                PutError(BadObjectHeader);
                return false;
            }
            length = (length << 8) | input[2 + index];
        }
        if (length < 128) {
            PutError(BadObjectHeader);
            return false;
        }
        offset += count;
    }
    if (length > input.size() - offset) {
        PutError(TooLong);
        return false;
    }
    value.tag = input[0];
    value.content = input.subspan(offset, length);
    value.encoded = input.first(offset + length);
    input = input.subspan(offset + length);
    return true;
}

bool Next(std::span<const unsigned char>& input, unsigned char tag, Tlv& value) {
    if (!ReadTlv(input, value)) return false;
    if (value.tag != tag) {
        if (tag == SequenceTag && (value.tag & 0x1f) == 0x10 && (value.tag & 0x20) == 0) {
            PutError(SequenceNotConstructed);
        } else {
            PutError(WrongTag);
        }
        return false;
    }
    return true;
}

Asn1String* NewString(int type, std::span<const unsigned char> content) {
    if (content.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        PutError(TooLong);
        return nullptr;
    }
    auto* value = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!value) {
        PutError(MallocFailure);
        return nullptr;
    }
    auto* data = static_cast<unsigned char*>(calloc_nid_postfix(content.size() + 1, 1));
    if (!data) {
        free_nid_postfix(value);
        PutError(MallocFailure);
        return nullptr;
    }
    if (!content.empty()) std::memcpy(data, content.data(), content.size());
    *value = {static_cast<std::int32_t>(content.size()), type, data, 0};
    return value;
}

Asn1String* NewInteger(std::span<const unsigned char> encoded) {
    const unsigned char* cursor = encoded.data();
    return static_cast<Asn1String*>(
        d2i_ASN1_INTEGER_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(encoded.size())));
}

Asn1String* NewBitString(std::span<const unsigned char> content) {
    if (content.empty()) {
        PutError(StringTooShort);
        return nullptr;
    }
    const unsigned int unusedBits = content.front();
    if (unusedBits > 7) {
        PutError(InvalidBitStringBitsLeft);
        return nullptr;
    }
    const std::size_t length = content.size() - 1;
    if (length > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        PutError(TooLong);
        return nullptr;
    }
    auto* value = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!value) {
        PutError(MallocFailure);
        return nullptr;
    }
    unsigned char* data = nullptr;
    if (length != 0) {
        data = static_cast<unsigned char*>(malloc_nid_postfix(length));
        if (!data) {
            free_nid_postfix(value);
            PutError(MallocFailure);
            return nullptr;
        }
        std::memcpy(data, content.data() + 1, length);
        data[length - 1] &= static_cast<unsigned char>(0xffu << unusedBits);
    }
    *value = {static_cast<std::int32_t>(length), BitStringType, data, BitsLeftFlag | unusedBits};
    return value;
}

bool IsObjectEncoding(std::span<const unsigned char> content) {
    if (content.empty()) return false;
    std::size_t index = 0;
    while (index < content.size()) {
        bool first = true;
        for (;;) {
            if (index == content.size()) return false;
            const unsigned char byte = content[index++];
            if (first && byte == 0x80) return false;
            first = false;
            if (!(byte & 0x80)) break;
        }
    }
    return true;
}

Asn1Object_nid_no_patch* NewObject(std::span<const unsigned char> content) {
    if (!IsObjectEncoding(content)) {
        PutError(InvalidObjectEncoding);
        return nullptr;
    }
    if (content.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        PutError(TooLong);
        return nullptr;
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(content.size()));
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    std::memcpy(data, content.data(), content.size());
    auto* object = static_cast<Asn1Object_nid_no_patch*>(calloc_nid_postfix(1, sizeof(Asn1Object_nid_no_patch)));
    if (!object) {
        free_nid_postfix(data);
        PutError(MallocFailure);
        return nullptr;
    }
    object->length = static_cast<int>(content.size());
    object->data = data;
    object->flags = ObjectDynamic | ObjectDynamicData;
    return object;
}

void FreeString(Asn1String* value) {
    if (!value) return;
    if (value->data) free_nid_postfix(value->data);
    free_nid_postfix(value);
}

void FreeObject(const LibreSslAsn1Object* object) {
    if (!object) return;
    if (object->flags & ObjectDynamicData) free_nid_postfix(const_cast<unsigned char*>(object->data));
    free_nid_postfix(const_cast<LibreSslAsn1Object*>(object));
}

void FreeParameter(LibreSsl3Asn1Type* parameter) {
    if (!parameter) return;
    if (parameter->value) {
        if (parameter->type == ObjectType) {
            FreeObject(static_cast<const LibreSslAsn1Object*>(parameter->value));
        } else {
            FreeString(static_cast<Asn1String*>(parameter->value));
        }
    }
    free_nid_postfix(parameter);
}

void FreeAlgor(LibreSsl3X509Algor* algor) {
    if (!algor) return;
    FreeObject(static_cast<const LibreSslAsn1Object*>(algor->algorithm));
    FreeParameter(algor->parameter);
    free_nid_postfix(algor);
}

void FreeExtension(LibreSsl3X509Extension* extension) {
    if (!extension) return;
    FreeObject(extension->object);
    FreeString(static_cast<Asn1String*>(extension->value));
    free_nid_postfix(extension);
}

void FreeNameEntry(void* entry) {
    auto* nameEntry = static_cast<LibreSslX509NameEntry*>(entry);
    if (!nameEntry) return;
    FreeObject(static_cast<const LibreSslAsn1Object*>(nameEntry->object));
    FreeString(static_cast<Asn1String*>(nameEntry->value));
    free_nid_postfix(nameEntry);
}

void FreeStack(const LibreSsl3Stack* stack, void (*release)(void*)) {
    if (!stack) return;
    if (stack->num < 0) throw std::invalid_argument("X509_CRL: negative stack count");
    for (int index = 0; index < stack->num; ++index) {
        if (!stack->data) throw std::invalid_argument("X509_CRL: null stack array");
        release(stack->data[index]);
    }
    sk_free_nid_postfix(const_cast<LibreSslStack*>(reinterpret_cast<const LibreSslStack*>(stack)));
}

void FreeExtensionElement(void* extension) {
    FreeExtension(static_cast<LibreSsl3X509Extension*>(extension));
}

void FreeName(LibreSslX509Name* name) {
    if (!name) return;
    FreeStack(reinterpret_cast<const LibreSsl3Stack*>(const_cast<const LibreSslStack*>(name->entries)), FreeNameEntry);
    free_nid_postfix(name);
}

void FreeRevoked(LibreSsl3Revoked* revoked) {
    if (!revoked) return;
    FreeString(revoked->serialNumber);
    FreeString(revoked->revocationDate);
    FreeStack(revoked->extensions, FreeExtensionElement);
    free_nid_postfix(revoked);
}

void FreeRevokedElement(void* revoked) {
    FreeRevoked(static_cast<LibreSsl3Revoked*>(revoked));
}

void FreeCrl(LibreSsl3X509Crl* crl) {
    if (!crl) return;
    LibreSsl3CrlInfo* info = crl->crl;
    if (info) {
        FreeString(info->version);
        FreeAlgor(info->sigAlg);
        FreeName(info->issuer);
        FreeString(info->lastUpdate);
        FreeString(info->nextUpdate);
        FreeStack(info->revoked, FreeRevokedElement);
        FreeStack(info->extensions, FreeExtensionElement);
        free_nid_postfix(info);
    }
    FreeAlgor(crl->sigAlg);
    FreeString(crl->signature);
    free_nid_postfix(crl);
}

LibreSsl3Stack* NewStack() {
    auto* stack = reinterpret_cast<LibreSsl3Stack*>(sk_new_nid_postfix(nullptr));
    if (!stack) PutError(MallocFailure);
    return stack;
}

bool Push(LibreSsl3Stack* stack, void* element) {
    if (sk_push_nid_postfix(reinterpret_cast<LibreSslStack*>(stack), element)) return true;
    PutError(MallocFailure);
    return false;
}

int FindExtension(const LibreSsl3Stack* extensions, const LibreSslAsn1Object* object, int lastPosition) {
    if (!extensions) return -1;
    if (lastPosition == std::numeric_limits<int>::max()) throw std::overflow_error("X509V3_get_d2i: position overflows");
    int position = lastPosition + 1;
    if (position < 0) position = 0;
    if (extensions->num < 0) throw std::invalid_argument("X509V3_get_d2i: negative extension count");
    for (; position < extensions->num; ++position) {
        if (!extensions->data) throw std::invalid_argument("X509V3_get_d2i: null extension array");
        const auto* extension = static_cast<const LibreSsl3X509Extension*>(
            static_cast<const void*>(extensions->data[position]));
        if (!extension) throw std::invalid_argument("X509V3_get_d2i: null extension");
        if (LibreSsl3::CompareObjects(extension->object, object) == 0) return position;
    }
    return -1;
}

void* GetExtension(const LibreSsl3Stack* extensions, int nid, int* crit, int* idx) {
    const int lastPosition = idx ? *idx : -1;
    if (crit) *crit = -1;
    if (idx) *idx = -1;
    if (!extensions) return nullptr;
    const LibreSslAsn1Object* object = LibreSsl3::ObjectByNid(nid);
    if (!object) return nullptr;
    const int position = FindExtension(extensions, object, lastPosition);
    if (position < 0) return nullptr;
    const auto* extension = static_cast<const LibreSsl3X509Extension*>(
        static_cast<const void*>(extensions->data[position]));
    if (!extension) throw std::invalid_argument("X509V3_get_d2i: null extension");
    if (!idx && FindExtension(extensions, object, position) > 0) {
        if (crit) *crit = -2;
        return nullptr;
    }
    if (crit) *crit = extension->critical > 0 ? 1 : 0;
    if (idx) *idx = position;
    return X509V3_EXT_d2i_nid_postfix(const_cast<LibreSsl3X509Extension*>(extension));
}

int EnumeratedValue(const Asn1String* value) {
    if (!value || value->type != EnumeratedType) return 0;
    if (value->length <= 0 || value->length > 4 || !value->data) return 0;
    std::uint32_t result = 0;
    for (int index = 0; index < value->length; ++index) result = (result << 8) | value->data[index];
    if (result > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) return 0;
    return static_cast<int>(result);
}

int EntryReason(const LibreSsl3Stack* extensions) {
    void* value = GetExtension(extensions, CrlReasonNid, nullptr, nullptr);
    if (!value) return 0;
    const int reason = EnumeratedValue(static_cast<const Asn1String*>(value));
    FreeString(static_cast<Asn1String*>(value));
    return reason;
}

bool IsStringTag(unsigned char tag) {
    for (const int candidate : StringTags) {
        if (tag == static_cast<unsigned char>(candidate)) return true;
    }
    return false;
}

Asn1String* DecodeNameValue(const Tlv& value) {
    if (!IsStringTag(value.tag)) {
        PutError(WrongTag);
        return nullptr;
    }
    return NewString(value.tag, value.content);
}

LibreSsl3Asn1Type* DecodeAnyType(const Tlv& value) {
    void* decoded = nullptr;
    if (value.tag == NullTag) {
        if (!value.content.empty()) {
            PutError(LengthMismatch);
            return nullptr;
        }
    } else if (value.tag == IntegerTag) {
        decoded = NewInteger(value.encoded);
    } else if (value.tag == BitStringTag) {
        decoded = NewBitString(value.content);
    } else if (value.tag == OctetStringTag) {
        decoded = NewString(OctetStringType, value.content);
    } else if (value.tag == ObjectTag) {
        decoded = NewObject(value.content);
    } else if (IsStringTag(value.tag)) {
        decoded = NewString(value.tag, value.content);
    } else {
        NotImplemented_nid_no_patch("X509_CRL algorithm parameter");
    }
    if (value.tag != NullTag && !decoded) return nullptr;
    auto* type = static_cast<LibreSsl3Asn1Type*>(calloc_nid_postfix(1, sizeof(LibreSsl3Asn1Type)));
    if (!type) {
        if (value.tag == ObjectTag) {
            FreeObject(static_cast<const LibreSslAsn1Object*>(decoded));
        } else {
            FreeString(static_cast<Asn1String*>(decoded));
        }
        PutError(MallocFailure);
        return nullptr;
    }
    type->type = value.tag;
    type->value = decoded;
    return type;
}

LibreSsl3X509Algor* DecodeAlgor(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    auto* algor = static_cast<LibreSsl3X509Algor*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Algor)));
    if (!algor) {
        PutError(MallocFailure);
        return nullptr;
    }
    std::span<const unsigned char> body = sequence.content;
    Tlv object;
    if (!Next(body, ObjectTag, object)) {
        free_nid_postfix(algor);
        return nullptr;
    }
    algor->algorithm = NewObject(object.content);
    if (!algor->algorithm) {
        free_nid_postfix(algor);
        return nullptr;
    }
    if (!body.empty()) {
        Tlv parameter;
        if (!ReadTlv(body, parameter)) {
            FreeAlgor(algor);
            return nullptr;
        }
        algor->parameter = DecodeAnyType(parameter);
        if (!algor->parameter) {
            FreeAlgor(algor);
            return nullptr;
        }
    }
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        FreeAlgor(algor);
        return nullptr;
    }
    return algor;
}

Asn1String* DecodeTime(std::span<const unsigned char>& input) {
    Tlv value;
    if (!ReadTlv(input, value)) return nullptr;
    if (value.tag == UtcTimeTag) return NewString(UtcTimeType, value.content);
    if (value.tag == GeneralizedTimeTag) return NewString(GeneralizedTimeType, value.content);
    PutError(WrongTag);
    return nullptr;
}

LibreSsl3X509Extension* DecodeExtension(std::span<const unsigned char>& input);

LibreSsl3Stack* DecodeExtensions(std::span<const unsigned char> content) {
    LibreSsl3Stack* stack = NewStack();
    if (!stack) return nullptr;
    while (!content.empty()) {
        LibreSsl3X509Extension* extension = DecodeExtension(content);
        if (!extension) {
            FreeStack(stack, FreeExtensionElement);
            return nullptr;
        }
        if (!Push(stack, extension)) {
            FreeExtension(extension);
            FreeStack(stack, FreeExtensionElement);
            return nullptr;
        }
    }
    return stack;
}

LibreSsl3X509Extension* DecodeExtension(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    auto* extension = static_cast<LibreSsl3X509Extension*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Extension)));
    if (!extension) {
        PutError(MallocFailure);
        return nullptr;
    }
    std::span<const unsigned char> body = sequence.content;
    Tlv object;
    if (!Next(body, ObjectTag, object)) {
        free_nid_postfix(extension);
        return nullptr;
    }
    extension->object = reinterpret_cast<const LibreSslAsn1Object*>(NewObject(object.content));
    if (!extension->object) {
        free_nid_postfix(extension);
        return nullptr;
    }
    if (!body.empty() && body.front() == BooleanTag) {
        Tlv critical;
        if (!Next(body, BooleanTag, critical)) {
            FreeExtension(extension);
            return nullptr;
        }
        if (critical.content.size() != 1) {
            PutError(LengthMismatch);
            FreeExtension(extension);
            return nullptr;
        }
        extension->critical = critical.content.front() ? 0xff : 0;
    }
    Tlv value;
    if (!Next(body, OctetStringTag, value)) {
        FreeExtension(extension);
        return nullptr;
    }
    extension->value = NewString(OctetStringType, value.content);
    if (!extension->value) {
        FreeExtension(extension);
        return nullptr;
    }
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        FreeExtension(extension);
        return nullptr;
    }
    return extension;
}

LibreSslX509NameEntry* DecodeNameEntry(std::span<const unsigned char>& input, int set) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    auto* entry = static_cast<LibreSslX509NameEntry*>(calloc_nid_postfix(1, sizeof(LibreSslX509NameEntry)));
    if (!entry) {
        PutError(MallocFailure);
        return nullptr;
    }
    std::span<const unsigned char> body = sequence.content;
    Tlv object;
    if (!Next(body, ObjectTag, object)) {
        free_nid_postfix(entry);
        return nullptr;
    }
    entry->object = NewObject(object.content);
    if (!entry->object) {
        free_nid_postfix(entry);
        return nullptr;
    }
    Tlv value;
    if (!ReadTlv(body, value)) {
        FreeNameEntry(entry);
        return nullptr;
    }
    entry->value = DecodeNameValue(value);
    if (!entry->value) {
        FreeNameEntry(entry);
        return nullptr;
    }
    entry->set = set;
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        FreeNameEntry(entry);
        return nullptr;
    }
    return entry;
}

LibreSslX509Name* DecodeName(std::span<const unsigned char> content) {
    auto* name = static_cast<LibreSslX509Name*>(calloc_nid_postfix(1, sizeof(LibreSslX509Name)));
    if (!name) {
        PutError(MallocFailure);
        return nullptr;
    }
    name->entries = reinterpret_cast<LibreSslStack*>(NewStack());
    if (!name->entries) {
        free_nid_postfix(name);
        return nullptr;
    }
    std::span<const unsigned char> body = content;
    int set = 0;
    while (!body.empty()) {
        Tlv rdn;
        if (!Next(body, SetTag, rdn)) {
            FreeName(name);
            return nullptr;
        }
        if (rdn.content.empty()) {
            PutError(FieldMissing);
            FreeName(name);
            return nullptr;
        }
        std::span<const unsigned char> attributes = rdn.content;
        while (!attributes.empty()) {
            LibreSslX509NameEntry* entry = DecodeNameEntry(attributes, set);
            if (!entry) {
                FreeName(name);
                return nullptr;
            }
            if (!Push(reinterpret_cast<LibreSsl3Stack*>(name->entries), entry)) {
                FreeNameEntry(entry);
                FreeName(name);
                return nullptr;
            }
        }
        if (set == std::numeric_limits<int>::max()) {
            PutError(TooLong);
            FreeName(name);
            return nullptr;
        }
        ++set;
    }
    return name;
}

LibreSsl3Revoked* DecodeRevoked(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    auto* revoked = static_cast<LibreSsl3Revoked*>(calloc_nid_postfix(1, sizeof(LibreSsl3Revoked)));
    if (!revoked) {
        PutError(MallocFailure);
        return nullptr;
    }
    std::span<const unsigned char> body = sequence.content;
    Tlv serial;
    if (!Next(body, IntegerTag, serial)) {
        free_nid_postfix(revoked);
        return nullptr;
    }
    revoked->serialNumber = NewInteger(serial.encoded);
    if (!revoked->serialNumber) {
        free_nid_postfix(revoked);
        return nullptr;
    }
    revoked->revocationDate = DecodeTime(body);
    if (!revoked->revocationDate) {
        FreeRevoked(revoked);
        return nullptr;
    }
    if (!body.empty() && body.front() == SequenceTag) {
        Tlv extensions;
        if (!Next(body, SequenceTag, extensions)) {
            FreeRevoked(revoked);
            return nullptr;
        }
        revoked->extensions = DecodeExtensions(extensions.content);
        if (!revoked->extensions) {
            FreeRevoked(revoked);
            return nullptr;
        }
    }
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        FreeRevoked(revoked);
        return nullptr;
    }
    revoked->reason = EntryReason(revoked->extensions);
    return revoked;
}

LibreSsl3Stack* DecodeRevokedList(std::span<const unsigned char> content) {
    LibreSsl3Stack* stack = NewStack();
    if (!stack) return nullptr;
    while (!content.empty()) {
        LibreSsl3Revoked* revoked = DecodeRevoked(content);
        if (!revoked) {
            FreeStack(stack, FreeRevokedElement);
            return nullptr;
        }
        if (!Push(stack, revoked)) {
            FreeRevoked(revoked);
            FreeStack(stack, FreeRevokedElement);
            return nullptr;
        }
    }
    return stack;
}

bool DecodeCrlInfo(std::span<const unsigned char> body, LibreSsl3CrlInfo* info) {
    if (!body.empty() && body.front() == IntegerTag) {
        Tlv version;
        if (!Next(body, IntegerTag, version)) return false;
        info->version = NewInteger(version.encoded);
        if (!info->version) return false;
    }
    info->sigAlg = DecodeAlgor(body);
    if (!info->sigAlg) return false;
    Tlv issuer;
    if (!Next(body, SequenceTag, issuer)) return false;
    info->issuer = DecodeName(issuer.content);
    if (!info->issuer) return false;
    info->lastUpdate = DecodeTime(body);
    if (!info->lastUpdate) return false;
    if (!body.empty() && (body.front() == UtcTimeTag || body.front() == GeneralizedTimeTag)) {
        info->nextUpdate = DecodeTime(body);
        if (!info->nextUpdate) return false;
    }
    if (!body.empty() && body.front() == SequenceTag) {
        Tlv revoked;
        if (!Next(body, SequenceTag, revoked)) return false;
        info->revoked = DecodeRevokedList(revoked.content);
        if (!info->revoked) return false;
    }
    if (!body.empty() && body.front() == CrlExtensionsTag) {
        Tlv extensions;
        if (!Next(body, CrlExtensionsTag, extensions)) return false;
        std::span<const unsigned char> content = extensions.content;
        Tlv list;
        if (!Next(content, SequenceTag, list)) return false;
        if (!content.empty()) {
            PutError(SequenceLengthMismatch);
            return false;
        }
        info->extensions = DecodeExtensions(list.content);
        if (!info->extensions) return false;
    }
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return false;
    }
    return true;
}

LibreSsl3X509Crl* DecodeCrl(std::span<const unsigned char>& input) {
    Tlv outer;
    if (!Next(input, SequenceTag, outer)) return nullptr;
    auto* crl = static_cast<LibreSsl3X509Crl*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Crl)));
    if (!crl) {
        PutError(MallocFailure);
        return nullptr;
    }
    crl->references = 1;
    crl->crl = static_cast<LibreSsl3CrlInfo*>(calloc_nid_postfix(1, sizeof(LibreSsl3CrlInfo)));
    if (!crl->crl) {
        FreeCrl(crl);
        return nullptr;
    }
    std::span<const unsigned char> body = outer.content;
    Tlv tbs;
    if (!Next(body, SequenceTag, tbs)) {
        FreeCrl(crl);
        return nullptr;
    }
    if (!DecodeCrlInfo(tbs.content, crl->crl)) {
        FreeCrl(crl);
        return nullptr;
    }
    crl->sigAlg = DecodeAlgor(body);
    if (!crl->sigAlg) {
        FreeCrl(crl);
        return nullptr;
    }
    Tlv signature;
    if (!Next(body, BitStringTag, signature)) {
        FreeCrl(crl);
        return nullptr;
    }
    crl->signature = NewBitString(signature.content);
    if (!crl->signature) {
        FreeCrl(crl);
        return nullptr;
    }
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        FreeCrl(crl);
        return nullptr;
    }
    return crl;
}

bool SameString(const Asn1String* first, const Asn1String* second) {
    if (!first || !second) return first == second;
    if (first->type != second->type || first->length != second->length) return false;
    if (first->length <= 0) return true;
    if (!first->data || !second->data) throw std::invalid_argument("X509_CRL_get0_by_cert: null string data");
    return std::memcmp(first->data, second->data, static_cast<std::size_t>(first->length)) == 0;
}

bool SameName(const LibreSslX509Name* first, const LibreSslX509Name* second) {
    if (first == second) return true;
    if (!first || !second) return false;
    const auto* left = reinterpret_cast<const LibreSsl3Stack*>(const_cast<const LibreSslStack*>(first->entries));
    const auto* right = reinterpret_cast<const LibreSsl3Stack*>(const_cast<const LibreSslStack*>(second->entries));
    const int leftCount = left ? left->num : 0;
    const int rightCount = right ? right->num : 0;
    if (leftCount != rightCount) return false;
    if (leftCount < 0) throw std::invalid_argument("X509_CRL_get0_by_cert: negative name count");
    for (int index = 0; index < leftCount; ++index) {
        if (!left->data || !right->data) throw std::invalid_argument("X509_CRL_get0_by_cert: null name array");
        const auto* leftEntry = static_cast<const LibreSslX509NameEntry*>(
            static_cast<const void*>(left->data[index]));
        const auto* rightEntry = static_cast<const LibreSslX509NameEntry*>(
            static_cast<const void*>(right->data[index]));
        if (!leftEntry || !rightEntry) throw std::invalid_argument("X509_CRL_get0_by_cert: null name entry");
        if (leftEntry->set != rightEntry->set) return false;
        if (LibreSsl3::CompareObjects(static_cast<const LibreSslAsn1Object*>(leftEntry->object),
                static_cast<const LibreSslAsn1Object*>(rightEntry->object)) != 0) return false;
        if (!SameString(static_cast<const Asn1String*>(leftEntry->value),
                static_cast<const Asn1String*>(rightEntry->value))) return false;
    }
    return true;
}

bool IssuerMatches(const LibreSslX509Name* crlIssuer, const LibreSslX509Name* certificateIssuer,
    const LibreSsl3Revoked* revoked) {
    if (revoked->issuer) NotImplemented_nid_no_patch("X509_CRL_get0_by_cert");
    if (!certificateIssuer) return true;
    return SameName(certificateIssuer, crlIssuer);
}

}

extern "C" {

LibreSsl3X509Crl* APS5_VABI d2i_X509_CRL_nid_postfix(
    LibreSsl3X509Crl** output, const unsigned char** input, std::int64_t length) {
    if (!input || !*input || length < 0) {
        PutError(BadObjectHeader);
        return nullptr;
    }
    std::span<const unsigned char> remaining(*input, static_cast<std::size_t>(length));
    LibreSsl3X509Crl* crl = DecodeCrl(remaining);
    if (!crl) return nullptr;
    if (output) {
        X509_CRL_free_nid_postfix(*output);
        *output = crl;
    }
    *input += static_cast<std::size_t>(length) - remaining.size();
    return crl;
}

void* APS5_VABI X509_CRL_get_ext_d2i_nid_postfix(const LibreSsl3X509Crl* crl, int nid, int* crit, int* idx) {
    if (!crl) throw std::invalid_argument(std::string(__func__) + ": null CRL");
    if (!crl->crl) throw std::invalid_argument(std::string(__func__) + ": null CRL info");
    return GetExtension(crl->crl->extensions, nid, crit, idx);
}

void APS5_VABI X509_CRL_free_nid_postfix(LibreSsl3X509Crl* crl) {
    if (!crl) return;
    if (crl->references > 1) {
        --crl->references;
        return;
    }
    FreeCrl(crl);
}

int APS5_VABI X509_CRL_get0_by_cert_nid_postfix(LibreSsl3X509Crl* crl, LibreSsl3Revoked** revoked,
    const LibreSsl3Certificate* certificate) {
    if (!crl || !crl->crl) return -1;
    if (!certificate || !certificate->certInfo) return -1;
    const auto* serial = static_cast<const Asn1String*>(certificate->certInfo->serialNumber);
    if (!serial) return -1;
    const auto* issuer = static_cast<const LibreSslX509Name*>(certificate->certInfo->issuer);
    const LibreSsl3Stack* entries = crl->crl->revoked;
    if (!entries) return 0;
    if (entries->num < 0) throw std::invalid_argument(std::string(__func__) + ": negative revoked count");
    for (int index = 0; index < entries->num; ++index) {
        if (!entries->data) throw std::invalid_argument(std::string(__func__) + ": null revoked array");
        auto* entry = static_cast<LibreSsl3Revoked*>(static_cast<void*>(entries->data[index]));
        if (!entry) throw std::invalid_argument(std::string(__func__) + ": null revoked entry");
        if (!SameString(entry->serialNumber, serial)) continue;
        if (!IssuerMatches(crl->crl->issuer, issuer, entry)) continue;
        if (revoked) *revoked = entry;
        return entry->reason == CrlReasonRemoveFromCrl ? 2 : 1;
    }
    return 0;
}

}
