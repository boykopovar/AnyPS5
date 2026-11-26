#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Pkcs7.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FileStream.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int X509Library = 11;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int BadObjectHeader = 102;
constexpr int FieldMissing = 121;
constexpr int SequenceLengthMismatch = 148;
constexpr int SequenceNotConstructed = 149;
constexpr int StringTooShort = 152;
constexpr int TooLong = 155;
constexpr int WrongTag = 168;
constexpr int InvalidDirectory = 113;
constexpr int InvalidObjectEncoding = 216;
constexpr int InvalidBitStringBitsLeft = 220;
constexpr int IllegalCharacters = 124;
constexpr int InvalidBmpStringLength = 129;
constexpr int InvalidUniversalStringLength = 133;
constexpr int InvalidUtf8String = 134;
constexpr int BmpStringWrongLength = 214;
constexpr int UniversalStringWrongLength = 215;
constexpr std::uint32_t UnicodeMax = 0x10ffff;

constexpr int FileTypePem = 1;
constexpr int FileTypeDefault = 0;
constexpr int AddDirectoryCommand = 2;

constexpr int IntegerType = 2;
constexpr int BitStringType = 3;
constexpr int OctetStringType = 4;
constexpr int Utf8StringType = 12;
constexpr int NumericStringType = 18;
constexpr int PrintableStringType = 19;
constexpr int T61StringType = 20;
constexpr int Ia5StringType = 22;
constexpr int UtcTimeType = 23;
constexpr int GeneralizedTimeType = 24;
constexpr int VisibleStringType = 26;
constexpr int UniversalStringType = 28;
constexpr int BmpStringType = 30;

constexpr std::int64_t BitsLeftFlag = 0x08;
constexpr unsigned char Constructed = 0x20;
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
constexpr unsigned char VersionTag = 0xa0;
constexpr unsigned char IssuerUniqueIdTag = 0x81;
constexpr unsigned char SubjectUniqueIdTag = 0x82;
constexpr unsigned char ExtensionsTag = 0xa3;

constexpr int StringTags[] = {12, 18, 19, 20, 21, 22, 26, 27, 28, 30};

struct Encoding {
    unsigned char* bytes;
    std::int64_t length;
    std::int32_t modified;
    std::int32_t padding;
};

struct CertificateObject {
    LibreSsl3CertificateInfo* certInfo;
    LibreSsl3X509Algor* sigAlg;
    LibreSsl3Asn1String* signature;
    std::int32_t valid;
    std::int32_t references;
    unsigned char rest[184];
};

struct DirectoryList {
    LibreSslStack* directories;
};

struct Tlv {
    unsigned char tag;
    std::span<const unsigned char> content;
    std::span<const unsigned char> encoded;
};

static_assert(sizeof(Encoding) == sizeof(LibreSsl3CertificateInfo::encoding));
static_assert(offsetof(LibreSsl3CertificateInfo, encoding) == 80);
static_assert(sizeof(CertificateObject) == sizeof(LibreSsl3Certificate));
static_assert(offsetof(CertificateObject, signature) == 16);
static_assert(offsetof(CertificateObject, references) == 28);
static_assert(sizeof(DirectoryList) == 8);
static_assert(sizeof(LibreSslX509NameEntry) == 24);
static_assert(sizeof(LibreSslX509Name) == 40);

void PutAsn1Error(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

void PutX509Error(int reason) {
    LibreSslPutError_nid_no_patch(X509Library, AnyFunction, reason);
}

template <typename T>
T ReadGuest(const T* source, const char* function) {
    T value{};
    if (!source) throw std::invalid_argument(std::string(function) + ": null guest pointer");
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(value));
    if (!copied) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void ReadGuestBytes(void* destination, const void* source, std::size_t size, const char* function) {
    if (size == 0) return;
    if (!source) throw std::invalid_argument(std::string(function) + ": null guest buffer");
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, size);
    if (!copied) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

bool ReadTlv(std::span<const unsigned char>& input, Tlv& value) {
    if (input.size() < 2) {
        PutAsn1Error(BadObjectHeader);
        return false;
    }
    std::size_t offset = 2;
    std::size_t length = input[1];
    if (length & 0x80) {
        const std::size_t count = length & 0x7f;
        if (count == 0 || count > sizeof(std::size_t) || input.size() < 2 + count || input[2] == 0) {
            PutAsn1Error(BadObjectHeader);
            return false;
        }
        length = 0;
        for (std::size_t index = 0; index < count; ++index) {
            if (length > (std::numeric_limits<std::size_t>::max() >> 8)) {
                PutAsn1Error(BadObjectHeader);
                return false;
            }
            length = (length << 8) | input[2 + index];
        }
        if (length < 128) {
            PutAsn1Error(BadObjectHeader);
            return false;
        }
        offset += count;
    }
    if (length > input.size() - offset) {
        PutAsn1Error(TooLong);
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
            PutAsn1Error(SequenceNotConstructed);
        } else {
            PutAsn1Error(WrongTag);
        }
        return false;
    }
    return true;
}

Asn1String* NewString(int type, std::span<const unsigned char> content) {
    if (type == BmpStringType && content.size() % 2 != 0) {
        PutAsn1Error(BmpStringWrongLength);
        return nullptr;
    }
    if (type == UniversalStringType && content.size() % 4 != 0) {
        PutAsn1Error(UniversalStringWrongLength);
        return nullptr;
    }
    if (content.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        PutAsn1Error(TooLong);
        return nullptr;
    }
    auto* value = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!value) {
        PutAsn1Error(MallocFailure);
        return nullptr;
    }
    auto* data = static_cast<unsigned char*>(calloc_nid_postfix(content.size() + 1, 1));
    if (!data) {
        free_nid_postfix(value);
        PutAsn1Error(MallocFailure);
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
        PutAsn1Error(StringTooShort);
        return nullptr;
    }
    const unsigned int unusedBits = content.front();
    if (unusedBits > 7) {
        PutAsn1Error(InvalidBitStringBitsLeft);
        return nullptr;
    }
    const std::size_t length = content.size() - 1;
    if (length > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        PutAsn1Error(TooLong);
        return nullptr;
    }
    auto* value = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!value) {
        PutAsn1Error(MallocFailure);
        return nullptr;
    }
    unsigned char* data = nullptr;
    if (length != 0) {
        data = static_cast<unsigned char*>(malloc_nid_postfix(length));
        if (!data) {
            free_nid_postfix(value);
            PutAsn1Error(MallocFailure);
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
        PutAsn1Error(InvalidObjectEncoding);
        return nullptr;
    }
    if (content.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        PutAsn1Error(TooLong);
        return nullptr;
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(content.size()));
    if (!data) {
        PutAsn1Error(MallocFailure);
        return nullptr;
    }
    std::memcpy(data, content.data(), content.size());
    auto* object = static_cast<Asn1Object_nid_no_patch*>(calloc_nid_postfix(1, sizeof(Asn1Object_nid_no_patch)));
    if (!object) {
        free_nid_postfix(data);
        PutAsn1Error(MallocFailure);
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
        if (parameter->type == ObjectTag) {
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

void FreeNameEntry(void* entry) {
    auto* nameEntry = static_cast<LibreSslX509NameEntry*>(entry);
    if (!nameEntry) return;
    FreeObject(static_cast<const LibreSslAsn1Object*>(nameEntry->object));
    FreeString(static_cast<Asn1String*>(nameEntry->value));
    free_nid_postfix(nameEntry);
}

void FreeStack(const LibreSsl3Stack* stack, void (*release)(void*)) {
    if (!stack) return;
    if (stack->num < 0) throw std::invalid_argument("X509_STORE_load_locations: negative stack count");
    for (int index = 0; index < stack->num; ++index) {
        if (!stack->data) throw std::invalid_argument("X509_STORE_load_locations: null stack array");
        release(stack->data[index]);
    }
    sk_free_nid_postfix(const_cast<LibreSslStack*>(reinterpret_cast<const LibreSslStack*>(stack)));
}

void FreeExtension(void* element);

void FreeName(LibreSslX509Name* name) {
    if (!name) return;
    FreeStack(reinterpret_cast<const LibreSsl3Stack*>(reinterpret_cast<const LibreSslStack*>(name->entries)), FreeNameEntry);
    free_nid_postfix(name->canonEnc);
    free_nid_postfix(name);
}

void FreeValidity(LibreSsl3X509Validity* validity) {
    if (!validity) return;
    FreeString(static_cast<Asn1String*>(validity->notBefore));
    FreeString(static_cast<Asn1String*>(validity->notAfter));
    free_nid_postfix(validity);
}

void FreePubkey(LibreSsl3X509Pubkey* pubkey) {
    if (!pubkey) return;
    FreeAlgor(pubkey->algor);
    FreeString(reinterpret_cast<Asn1String*>(pubkey->publicKey));
    EVP_PKEY_free_nid_disambig1_nid_postfix(static_cast<LibreSsl3EvpPkey*>(pubkey->pkey));
    free_nid_postfix(pubkey);
}

void FreeExtensionElement(void* element) {
    FreeExtension(element);
}

void FreeCertificateInfo(LibreSsl3CertificateInfo* info) {
    if (!info) return;
    FreeString(static_cast<Asn1String*>(info->version));
    FreeString(static_cast<Asn1String*>(info->serialNumber));
    FreeAlgor(static_cast<LibreSsl3X509Algor*>(info->signature));
    FreeName(static_cast<LibreSslX509Name*>(info->issuer));
    FreeValidity(static_cast<LibreSsl3X509Validity*>(info->validity));
    FreeName(static_cast<LibreSslX509Name*>(info->subject));
    FreePubkey(info->key);
    FreeString(static_cast<Asn1String*>(info->issuerUid));
    FreeString(static_cast<Asn1String*>(info->subjectUid));
    FreeStack(info->extensions, FreeExtensionElement);
    const auto& encoding = *reinterpret_cast<const Encoding*>(info->encoding);
    free_nid_postfix(encoding.bytes);
    free_nid_postfix(info);
}

void FreeCertificate(CertificateObject* certificate) {
    if (!certificate) return;
    FreeCertificateInfo(certificate->certInfo);
    FreeAlgor(certificate->sigAlg);
    FreeString(reinterpret_cast<Asn1String*>(certificate->signature));
    free_nid_postfix(certificate);
}

LibreSsl3Stack* NewStack() {
    auto* stack = reinterpret_cast<LibreSsl3Stack*>(sk_new_nid_postfix(nullptr));
    if (!stack) PutAsn1Error(MallocFailure);
    return stack;
}

bool Push(LibreSsl3Stack* stack, void* element) {
    if (sk_push_nid_postfix(reinterpret_cast<LibreSslStack*>(stack), element)) return true;
    PutAsn1Error(MallocFailure);
    return false;
}

bool IsStringTag(unsigned char tag) {
    for (const int candidate : StringTags) {
        if (tag == static_cast<unsigned char>(candidate)) return true;
    }
    return false;
}

Asn1String* DecodeNameValue(const Tlv& value) {
    if (!IsStringTag(value.tag)) {
        PutAsn1Error(WrongTag);
        return nullptr;
    }
    return NewString(value.tag, value.content);
}

LibreSsl3Asn1Type* DecodeAnyType(const Tlv& value) {
    void* decoded = nullptr;
    if (value.tag == NullTag) {
        if (!value.content.empty()) {
            PutAsn1Error(SequenceLengthMismatch);
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
        NotImplemented_nid_no_patch("X509_STORE_load_locations: certificate algorithm parameter");
    }
    if (value.tag != NullTag && !decoded) return nullptr;
    auto* type = static_cast<LibreSsl3Asn1Type*>(calloc_nid_postfix(1, sizeof(LibreSsl3Asn1Type)));
    if (!type) {
        if (value.tag == ObjectTag) {
            FreeObject(static_cast<const LibreSslAsn1Object*>(decoded));
        } else {
            FreeString(static_cast<Asn1String*>(decoded));
        }
        PutAsn1Error(MallocFailure);
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
        PutAsn1Error(MallocFailure);
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
        PutAsn1Error(SequenceLengthMismatch);
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
    PutAsn1Error(WrongTag);
    return nullptr;
}

LibreSsl3X509Validity* DecodeValidity(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    auto* validity = static_cast<LibreSsl3X509Validity*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Validity)));
    if (!validity) {
        PutAsn1Error(MallocFailure);
        return nullptr;
    }
    std::span<const unsigned char> body = sequence.content;
    validity->notBefore = DecodeTime(body);
    if (!validity->notBefore) {
        free_nid_postfix(validity);
        return nullptr;
    }
    validity->notAfter = DecodeTime(body);
    if (!validity->notAfter) {
        FreeValidity(validity);
        return nullptr;
    }
    if (!body.empty()) {
        PutAsn1Error(SequenceLengthMismatch);
        FreeValidity(validity);
        return nullptr;
    }
    return validity;
}

LibreSsl3X509Pubkey* DecodePubkey(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    auto* pubkey = static_cast<LibreSsl3X509Pubkey*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Pubkey)));
    if (!pubkey) {
        PutAsn1Error(MallocFailure);
        return nullptr;
    }
    std::span<const unsigned char> body = sequence.content;
    pubkey->algor = DecodeAlgor(body);
    if (!pubkey->algor) {
        free_nid_postfix(pubkey);
        return nullptr;
    }
    Tlv bits;
    if (!Next(body, BitStringTag, bits)) {
        FreePubkey(pubkey);
        return nullptr;
    }
    pubkey->publicKey = reinterpret_cast<LibreSsl3Asn1String*>(NewBitString(bits.content));
    if (!pubkey->publicKey) {
        FreePubkey(pubkey);
        return nullptr;
    }
    if (!body.empty()) {
        PutAsn1Error(SequenceLengthMismatch);
        FreePubkey(pubkey);
        return nullptr;
    }
    return pubkey;
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
            FreeExtensionElement(extension);
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
        PutAsn1Error(MallocFailure);
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
            FreeExtensionElement(extension);
            return nullptr;
        }
        if (critical.content.size() != 1) {
            PutAsn1Error(SequenceLengthMismatch);
            FreeExtensionElement(extension);
            return nullptr;
        }
        extension->critical = critical.content.front() ? 0xff : 0;
    }
    Tlv value;
    if (!Next(body, OctetStringTag, value)) {
        FreeExtensionElement(extension);
        return nullptr;
    }
    extension->value = NewString(OctetStringType, value.content);
    if (!extension->value) {
        FreeExtensionElement(extension);
        return nullptr;
    }
    if (!body.empty()) {
        PutAsn1Error(SequenceLengthMismatch);
        FreeExtensionElement(extension);
        return nullptr;
    }
    return extension;
}

void FreeExtension(void* element) {
    auto* extension = static_cast<LibreSsl3X509Extension*>(element);
    if (!extension) return;
    FreeObject(extension->object);
    FreeString(static_cast<Asn1String*>(extension->value));
    free_nid_postfix(extension);
}

LibreSslX509NameEntry* DecodeNameEntry(std::span<const unsigned char>& input, int set) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    auto* entry = static_cast<LibreSslX509NameEntry*>(calloc_nid_postfix(1, sizeof(LibreSslX509NameEntry)));
    if (!entry) {
        PutAsn1Error(MallocFailure);
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
        PutAsn1Error(SequenceLengthMismatch);
        FreeNameEntry(entry);
        return nullptr;
    }
    return entry;
}

LibreSslX509Name* DecodeName(std::span<const unsigned char> content) {
    auto* name = static_cast<LibreSslX509Name*>(calloc_nid_postfix(1, sizeof(LibreSslX509Name)));
    if (!name) {
        PutAsn1Error(MallocFailure);
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
            PutAsn1Error(FieldMissing);
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
        ++set;
    }
    return name;
}

bool IsAsciiSpace(unsigned char character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\v' ||
           character == '\f' || character == '\r';
}

bool IsCanonicalizedType(int type) {
    return type == Utf8StringType || type == PrintableStringType || type == T61StringType || type == Ia5StringType ||
           type == VisibleStringType || type == UniversalStringType || type == BmpStringType;
}

bool IsSurrogate(std::uint32_t value) {
    return value >= 0xd800 && value <= 0xdfff;
}

std::size_t Utf8SequenceLength(std::span<const unsigned char> bytes) {
    const unsigned char lead = bytes.front();
    if (lead < 0x80) return 1;
    std::size_t length = 0;
    std::uint32_t minimum = 0;
    std::uint32_t value = 0;
    if (lead >= 0xc2 && lead <= 0xdf) {
        length = 2;
        minimum = 0x80;
        value = lead & 0x1f;
    } else if ((lead & 0xf0) == 0xe0) {
        length = 3;
        minimum = 0x800;
        value = lead & 0x0f;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
        length = 4;
        minimum = 0x10000;
        value = lead & 0x07;
    } else {
        return 0;
    }
    if (bytes.size() < length) return 0;
    for (std::size_t index = 1; index < length; ++index) {
        if ((bytes[index] & 0xc0) != 0x80) return 0;
        value = (value << 6) | (bytes[index] & 0x3f);
    }
    if (value < minimum || value > UnicodeMax || IsSurrogate(value)) return 0;
    return length;
}

void AppendUtf8(std::vector<unsigned char>& output, std::uint32_t code) {
    if (code <= 0x7f) {
        output.push_back(static_cast<unsigned char>(code));
    } else if (code <= 0x7ff) {
        output.push_back(static_cast<unsigned char>(0xc0 | (code >> 6)));
        output.push_back(static_cast<unsigned char>(0x80 | (code & 0x3f)));
    } else if (code <= 0xffff) {
        output.push_back(static_cast<unsigned char>(0xe0 | (code >> 12)));
        output.push_back(static_cast<unsigned char>(0x80 | ((code >> 6) & 0x3f)));
        output.push_back(static_cast<unsigned char>(0x80 | (code & 0x3f)));
    } else {
        output.push_back(static_cast<unsigned char>(0xf0 | (code >> 18)));
        output.push_back(static_cast<unsigned char>(0x80 | ((code >> 12) & 0x3f)));
        output.push_back(static_cast<unsigned char>(0x80 | ((code >> 6) & 0x3f)));
        output.push_back(static_cast<unsigned char>(0x80 | (code & 0x3f)));
    }
}

std::optional<std::vector<unsigned char>> ToUtf8(const std::vector<unsigned char>& bytes, int type) {
    if (type == Utf8StringType) {
        for (std::size_t index = 0; index < bytes.size();) {
            const std::size_t length = Utf8SequenceLength(std::span<const unsigned char>(bytes).subspan(index));
            if (length == 0) {
                PutAsn1Error(InvalidUtf8String);
                return std::nullopt;
            }
            index += length;
        }
        return bytes;
    }
    std::vector<unsigned char> converted;
    converted.reserve(bytes.size());
    if (type == BmpStringType || type == UniversalStringType) {
        const std::size_t width = type == BmpStringType ? 2 : 4;
        if (bytes.size() % width != 0) {
            PutAsn1Error(width == 2 ? InvalidBmpStringLength : InvalidUniversalStringLength);
            return std::nullopt;
        }
        for (std::size_t index = 0; index < bytes.size(); index += width) {
            std::uint32_t code = 0;
            for (std::size_t byte = 0; byte < width; ++byte) code = (code << 8) | bytes[index + byte];
            if (code > UnicodeMax || IsSurrogate(code)) {
                PutAsn1Error(IllegalCharacters);
                return std::nullopt;
            }
            AppendUtf8(converted, code);
        }
        return converted;
    }
    for (const unsigned char character : bytes) AppendUtf8(converted, character);
    return converted;
}

std::optional<std::vector<unsigned char>> CanonicalStringContent(const Asn1String& value, const char* function) {
    if (value.length < 0) throw std::invalid_argument(std::string(function) + ": negative name value length");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(value.length));
    ReadGuestBytes(bytes.data(), value.data, bytes.size(), function);
    if (!IsCanonicalizedType(value.type)) return bytes;
    auto converted = ToUtf8(bytes, value.type);
    if (!converted) return std::nullopt;
    bytes = std::move(*converted);
    std::size_t begin = 0;
    std::size_t end = bytes.size();
    while (begin < end && !(bytes[begin] & 0x80) && IsAsciiSpace(bytes[begin])) ++begin;
    while (end > begin && !(bytes[end - 1] & 0x80) && IsAsciiSpace(bytes[end - 1])) --end;
    std::vector<unsigned char> canonical;
    for (std::size_t index = begin; index < end;) {
        const unsigned char character = bytes[index];
        if (character & 0x80) {
            canonical.push_back(character);
            ++index;
        } else if (IsAsciiSpace(character)) {
            canonical.push_back(' ');
            do {
                ++index;
            } while (index < end && !(bytes[index] & 0x80) && IsAsciiSpace(bytes[index]));
        } else {
            canonical.push_back(static_cast<unsigned char>(character >= 'A' && character <= 'Z' ? character | 0x20 : character));
            ++index;
        }
    }
    return canonical;
}

void AppendLength(std::vector<unsigned char>& output, std::size_t length) {
    if (length < 0x80) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    unsigned char bytes[sizeof(length)];
    int count = 0;
    for (std::size_t rest = length; rest != 0; rest >>= 8) bytes[count++] = static_cast<unsigned char>(rest);
    output.push_back(static_cast<unsigned char>(0x80 | count));
    while (count > 0) output.push_back(bytes[--count]);
}

void AppendElement(std::vector<unsigned char>& output, unsigned char tag, const std::vector<unsigned char>& content) {
    output.push_back(tag);
    AppendLength(output, content.size());
    output.insert(output.end(), content.begin(), content.end());
}

std::optional<std::vector<unsigned char>> EncodeNameEntry(const LibreSslX509NameEntry& entry, const char* function) {
    const auto object = ReadGuest(static_cast<const LibreSslAsn1Object*>(entry.object), function);
    if (object.length < 0) throw std::invalid_argument(std::string(function) + ": negative name object length");
    std::vector<unsigned char> objectContent(static_cast<std::size_t>(object.length));
    ReadGuestBytes(objectContent.data(), object.data, objectContent.size(), function);
    const auto value = ReadGuest(static_cast<const Asn1String*>(entry.value), function);
    const int type = IsCanonicalizedType(value.type) ? Utf8StringType : value.type;
    if (type < 0 || type > 0xff) throw std::invalid_argument(std::string(function) + ": name value type out of range");
    const auto canonical = CanonicalStringContent(value, function);
    if (!canonical) return std::nullopt;
    std::vector<unsigned char> content;
    AppendElement(content, ObjectTag, objectContent);
    AppendElement(content, static_cast<unsigned char>(type), *canonical);
    std::vector<unsigned char> encoded;
    AppendElement(encoded, SequenceTag, content);
    return encoded;
}

std::optional<std::vector<unsigned char>> CanonicalName(const LibreSslX509Name* name, const char* function) {
    if (!name) throw std::invalid_argument(std::string(function) + ": null name");
    const LibreSslStack stack = ReadGuest(name->entries, function);
    if (stack.num < 0) throw std::invalid_argument(std::string(function) + ": negative name entry count");
    std::vector<std::int32_t> sets;
    std::vector<std::vector<std::vector<unsigned char>>> groups;
    for (int index = 0; index < stack.num; ++index) {
        if (!stack.data) throw std::invalid_argument(std::string(function) + ": null name entry array");
        const auto entry = ReadGuest(reinterpret_cast<const LibreSslX509NameEntry*>(stack.data[index]), function);
        if (!entry.object || !entry.value)
            throw std::invalid_argument(std::string(function) + ": incomplete name entry");
        if (index == 0 || entry.set != sets.back()) {
            sets.push_back(entry.set);
            groups.emplace_back();
        }
        auto encoded = EncodeNameEntry(entry, function);
        if (!encoded) return std::nullopt;
        groups.back().push_back(std::move(*encoded));
    }
    std::vector<unsigned char> content;
    for (auto& group : groups) {
        std::sort(group.begin(), group.end());
        std::vector<unsigned char> set;
        for (const auto& entry : group) set.insert(set.end(), entry.begin(), entry.end());
        AppendElement(content, SetTag, set);
    }
    return content;
}

std::vector<unsigned char> SubjectEncoding(const LibreSslX509Name* name, const char* function) {
    if (!name) throw std::invalid_argument(std::string(function) + ": null subject name");
    if (name->canonEnclen < 0) throw std::invalid_argument(std::string(function) + ": negative canonical length");
    if (!name->canonEnc) {
        const LibreSslStack stack = ReadGuest(name->entries, function);
        if (stack.num <= 0) return {};
        auto canonical = CanonicalName(name, function);
        if (!canonical) NotImplemented_nid_no_patch("X509_LOOKUP_hash_dir: a lookup name with no canonical encoding");
        return std::move(*canonical);
    }
    std::vector<unsigned char> encoding(static_cast<std::size_t>(name->canonEnclen));
    ReadGuestBytes(encoding.data(), name->canonEnc, encoding.size(), function);
    return encoding;
}

bool StoreCanonicalName(LibreSslX509Name* name, const char* function) {
    const auto canonical = CanonicalName(name, function);
    if (!canonical) return false;
    const std::vector<unsigned char>& encoding = *canonical;
    if (encoding.empty()) return true;
    if (encoding.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        PutAsn1Error(TooLong);
        return false;
    }
    auto* copy = static_cast<unsigned char*>(malloc_nid_postfix(encoding.size()));
    if (!copy) {
        PutAsn1Error(MallocFailure);
        return false;
    }
    std::memcpy(copy, encoding.data(), encoding.size());
    name->canonEnc = copy;
    name->canonEnclen = static_cast<std::int32_t>(encoding.size());
    return true;
}

LibreSsl3CertificateInfo* DecodeCertificateInfo(const Tlv& tbs) {
    auto* info = static_cast<LibreSsl3CertificateInfo*>(calloc_nid_postfix(1, sizeof(LibreSsl3CertificateInfo)));
    if (!info) {
        PutAsn1Error(MallocFailure);
        return nullptr;
    }
    std::span<const unsigned char> body = tbs.content;
    if (!body.empty() && body.front() == VersionTag) {
        Tlv version;
        if (!Next(body, VersionTag, version)) {
            free_nid_postfix(info);
            return nullptr;
        }
        std::span<const unsigned char> versionBody = version.content;
        Tlv integer;
        if (!Next(versionBody, IntegerTag, integer)) {
            free_nid_postfix(info);
            return nullptr;
        }
        info->version = NewInteger(integer.encoded);
        if (!info->version || !versionBody.empty()) {
            if (info->version) PutAsn1Error(SequenceLengthMismatch);
            FreeCertificateInfo(info);
            return nullptr;
        }
    }
    Tlv serial;
    if (!Next(body, IntegerTag, serial)) {
        free_nid_postfix(info);
        return nullptr;
    }
    info->serialNumber = NewInteger(serial.encoded);
    if (!info->serialNumber) {
        FreeCertificateInfo(info);
        return nullptr;
    }
    info->signature = DecodeAlgor(body);
    if (!info->signature) {
        FreeCertificateInfo(info);
        return nullptr;
    }
    Tlv issuer;
    if (!Next(body, SequenceTag, issuer)) {
        FreeCertificateInfo(info);
        return nullptr;
    }
    info->issuer = DecodeName(issuer.content);
    if (!info->issuer) {
        FreeCertificateInfo(info);
        return nullptr;
    }
    if (!StoreCanonicalName(static_cast<LibreSslX509Name*>(info->issuer), __func__)) {
        FreeCertificateInfo(info);
        return nullptr;
    }
    info->validity = DecodeValidity(body);
    if (!info->validity) {
        FreeCertificateInfo(info);
        return nullptr;
    }
    Tlv subject;
    if (!Next(body, SequenceTag, subject)) {
        FreeCertificateInfo(info);
        return nullptr;
    }
    info->subject = DecodeName(subject.content);
    if (!info->subject) {
        FreeCertificateInfo(info);
        return nullptr;
    }
    if (!StoreCanonicalName(static_cast<LibreSslX509Name*>(info->subject), __func__)) {
        FreeCertificateInfo(info);
        return nullptr;
    }
    info->key = DecodePubkey(body);
    if (!info->key) {
        FreeCertificateInfo(info);
        return nullptr;
    }
    if (!body.empty() && body.front() == IssuerUniqueIdTag) {
        Tlv identifier;
        if (!Next(body, IssuerUniqueIdTag, identifier)) {
            FreeCertificateInfo(info);
            return nullptr;
        }
        info->issuerUid = NewBitString(identifier.content);
        if (!info->issuerUid) {
            FreeCertificateInfo(info);
            return nullptr;
        }
    }
    if (!body.empty() && body.front() == SubjectUniqueIdTag) {
        Tlv identifier;
        if (!Next(body, SubjectUniqueIdTag, identifier)) {
            FreeCertificateInfo(info);
            return nullptr;
        }
        info->subjectUid = NewBitString(identifier.content);
        if (!info->subjectUid) {
            FreeCertificateInfo(info);
            return nullptr;
        }
    }
    if (!body.empty() && body.front() == ExtensionsTag) {
        Tlv wrapper;
        if (!Next(body, ExtensionsTag, wrapper)) {
            FreeCertificateInfo(info);
            return nullptr;
        }
        Tlv extensions;
        std::span<const unsigned char> wrapperBody = wrapper.content;
        if (!Next(wrapperBody, SequenceTag, extensions) || !wrapperBody.empty()) {
            if (wrapperBody.empty()) PutAsn1Error(SequenceLengthMismatch);
            FreeCertificateInfo(info);
            return nullptr;
        }
        info->extensions = DecodeExtensions(extensions.content);
        if (!info->extensions) {
            FreeCertificateInfo(info);
            return nullptr;
        }
    }
    if (!body.empty()) {
        PutAsn1Error(SequenceLengthMismatch);
        FreeCertificateInfo(info);
        return nullptr;
    }
    if (tbs.encoded.size() > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
        PutAsn1Error(TooLong);
        FreeCertificateInfo(info);
        return nullptr;
    }
    auto* encoding = static_cast<unsigned char*>(malloc_nid_postfix(tbs.encoded.size()));
    if (!encoding) {
        PutAsn1Error(MallocFailure);
        FreeCertificateInfo(info);
        return nullptr;
    }
    std::memcpy(encoding, tbs.encoded.data(), tbs.encoded.size());
    auto& stored = *reinterpret_cast<Encoding*>(info->encoding);
    stored = {encoding, static_cast<std::int64_t>(tbs.encoded.size()), 0, 0};
    return info;
}

CertificateObject* DecodeCertificate(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    Tlv tbs;
    if (!Next(body, SequenceTag, tbs)) return nullptr;
    auto* certificate = static_cast<CertificateObject*>(calloc_nid_postfix(1, sizeof(CertificateObject)));
    if (!certificate) {
        PutAsn1Error(MallocFailure);
        return nullptr;
    }
    certificate->certInfo = DecodeCertificateInfo(tbs);
    if (!certificate->certInfo) {
        free_nid_postfix(certificate);
        return nullptr;
    }
    certificate->sigAlg = DecodeAlgor(body);
    if (!certificate->sigAlg) {
        FreeCertificateInfo(certificate->certInfo);
        free_nid_postfix(certificate);
        return nullptr;
    }
    Tlv signature;
    if (!Next(body, BitStringTag, signature)) {
        FreeCertificateInfo(certificate->certInfo);
        FreeAlgor(certificate->sigAlg);
        free_nid_postfix(certificate);
        return nullptr;
    }
    certificate->signature = reinterpret_cast<LibreSsl3Asn1String*>(NewBitString(signature.content));
    if (!certificate->signature) {
        FreeCertificateInfo(certificate->certInfo);
        FreeAlgor(certificate->sigAlg);
        free_nid_postfix(certificate);
        return nullptr;
    }
    if (!body.empty()) {
        PutAsn1Error(SequenceLengthMismatch);
        FreeCertificate(certificate);
        return nullptr;
    }
    certificate->references = 1;
    return certificate;
}

constexpr int SystemLibrary = 2;
constexpr int PemLibrary = 9;
constexpr int BioLibrary = 32;
constexpr int SystemFailure = 2;
constexpr int PemFailure = 9;
constexpr int Asn1Failure = 13;
constexpr int BadBase64Decode = 100;
constexpr int BadEndLine = 102;
constexpr int NoStartLine = 108;
constexpr int NoSuchFile = 128;
constexpr int NoCertificateOrCrlFound = 136;
constexpr int GuestNoEntry = 2;

enum class LoadFailure { None, Unreadable, Open, EndLine, Base64, Empty, Decode };

struct LoadReport {
    LoadFailure failure = LoadFailure::None;
    int openError = 0;
    bool endsOnEmptyRecord = false;
    bool revocationList = false;
};

bool NextLine(const std::string& text, std::size_t& position, std::string& line) {
    if (position >= text.size()) return false;
    std::size_t end = text.find('\n', position);
    if (end == std::string::npos) end = text.size();
    line.assign(text, position, end - position);
    position = end < text.size() ? end + 1 : text.size();
    while (!line.empty() && static_cast<signed char>(line.back()) <= ' ') line.pop_back();
    return true;
}

bool DecodeBase64(const std::string& text, std::vector<unsigned char>& output) {
    unsigned int accumulator = 0;
    int bits = 0;
    bool padded = false;
    for (const unsigned char character : text) {
        if (character == ' ' || character == '\t' || character == '\r' || character == '\n') continue;
        if (character == '=') {
            padded = true;
            continue;
        }
        if (padded) return false;
        unsigned int value;
        if (character >= 'A' && character <= 'Z') value = character - 'A';
        else if (character >= 'a' && character <= 'z') value = character - 'a' + 26;
        else if (character >= '0' && character <= '9') value = character - '0' + 52;
        else if (character == '+') value = 62;
        else if (character == '/') value = 63;
        else return false;
        accumulator = (accumulator << 6) | value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<unsigned char>((accumulator >> bits) & 0xff));
        }
    }
    if (bits >= 6) return false;
    if (bits > 0 && (accumulator & ((1u << bits) - 1)) != 0) return false;
    return true;
}

bool IsCertificateLabel(const std::string& label) {
    return label == "CERTIFICATE" || label == "X509 CERTIFICATE" || label == "TRUSTED CERTIFICATE";
}

template<typename TConsumer>
void ReadPemCertificates(const std::string& text, LoadReport& report, TConsumer&& consume) {
    std::size_t position = 0;
    std::string line;
    while (NextLine(text, position, line)) {
        if (line.rfind("-----BEGIN ", 0) != 0) continue;
        if (line.size() < 16 || line.compare(line.size() - 5, 5, "-----") != 0) continue;
        const std::string label = line.substr(11, line.size() - 16);
        std::string body;
        bool closed = false;
        while (NextLine(text, position, line)) {
            if (line == "-----END " + label + "-----") {
                closed = true;
                break;
            }
            body.append(line);
            body.push_back('\n');
        }
        if (!closed) {
            report.failure = LoadFailure::EndLine;
            return;
        }
        if (label == "X509 CRL") report.revocationList = true;
        if (!IsCertificateLabel(label)) continue;
        std::vector<unsigned char> encoded;
        if (!DecodeBase64(body, encoded)) {
            report.failure = LoadFailure::Base64;
            return;
        }
        if (encoded.empty()) {
            if (!report.endsOnEmptyRecord) report.failure = LoadFailure::Empty;
            return;
        }
        if (!consume(encoded)) return;
    }
}

bool ReadFile(const char* path, std::string& contents, LoadReport& report) {
    std::string guestPath;
    if (CopyGuestPath_nid_no_patch(path, guestPath) != 0) {
        report.failure = LoadFailure::Unreadable;
        return false;
    }
    auto* stream = fopen_nid_postfix(path, "rb");
    if (!stream) {
        report.openError = errno;
        report.failure = LoadFailure::Open;
        return false;
    }
    std::vector<char> buffer(4096);
    bool readable = true;
    for (;;) {
        const std::size_t count = fread_nid_postfix(buffer.data(), 1, buffer.size(), stream);
        if (count == 0) break;
        contents.append(buffer.data(), count);
    }
    if (ferror_nid_postfix(stream)) readable = false;
    fclose_nid_postfix(stream);
    if (!readable) report.failure = LoadFailure::Unreadable;
    return readable;
}

void AddCertificate(LibreSslX509Store* store, CertificateObject* certificate) {
    int added = 0;
    try {
        added = X509_STORE_add_cert_nid_postfix(
            store, reinterpret_cast<LibreSslX509*>(reinterpret_cast<LibreSsl3X509*>(certificate)));
    } catch (...) {
        FreeCertificate(certificate);
        throw;
    }
    if (!added) {
        FreeCertificate(certificate);
    } else {
        std::atomic_ref<std::int32_t>(certificate->references).fetch_sub(1, std::memory_order_relaxed);
    }
}

int LoadCertificates(LibreSslX509Store* store, const char* path, LoadReport& report, bool wholeFileFirst) {
    std::string contents;
    if (!ReadFile(path, contents, report)) return 0;
    std::vector<CertificateObject*> pending;
    int count = 0;
    try {
        ReadPemCertificates(contents, report, [&](const std::vector<unsigned char>& bytes) {
            std::span<const unsigned char> input(bytes);
            CertificateObject* certificate = DecodeCertificate(input);
            if (!certificate) {
                report.failure = LoadFailure::Decode;
                return false;
            }
            if (wholeFileFirst) {
                pending.push_back(certificate);
            } else {
                AddCertificate(store, certificate);
                ++count;
            }
            return true;
        });
        if (report.failure == LoadFailure::None) {
            while (!pending.empty()) {
                CertificateObject* certificate = pending.front();
                pending.erase(pending.begin());
                AddCertificate(store, certificate);
                ++count;
            }
        }
    } catch (...) {
        for (CertificateObject* certificate : pending) FreeCertificate(certificate);
        throw;
    }
    for (CertificateObject* certificate : pending) FreeCertificate(certificate);
    return report.failure != LoadFailure::None ? 0 : count;
}

void PutPemError(int reason) {
    LibreSslPutError_nid_no_patch(PemLibrary, AnyFunction, reason);
}

int LoadCertificateFile(LibreSslX509Store* store, const char* path) {
    LoadReport report;
    report.endsOnEmptyRecord = static_cast<int>(ERR_peek_last_error_nid_postfix() & 0xfff) == NoStartLine;
    const int count = LoadCertificates(store, path, report, true);
    switch (report.failure) {
    case LoadFailure::None:
        break;
    case LoadFailure::Unreadable:
        return 0;
    case LoadFailure::Open:
        LibreSslPutError_nid_no_patch(SystemLibrary, AnyFunction, report.openError);
        LibreSslPutError_nid_no_patch(BioLibrary, AnyFunction,
            report.openError == GuestNoEntry ? NoSuchFile : SystemFailure);
        PutX509Error(SystemFailure);
        return 0;
    case LoadFailure::EndLine:
        PutPemError(BadEndLine);
        PutX509Error(PemFailure);
        return 0;
    case LoadFailure::Base64:
        PutPemError(BadBase64Decode);
        PutX509Error(PemFailure);
        return 0;
    case LoadFailure::Empty:
        PutX509Error(PemFailure);
        return 0;
    case LoadFailure::Decode:
        PutPemError(Asn1Failure);
        PutX509Error(PemFailure);
        return 0;
    }
    ERR_clear_error_nid_postfix();
    if (count == 0 && !report.revocationList) PutX509Error(NoCertificateOrCrlFound);
    return count;
}

unsigned long NameHash(const LibreSslX509Name* name) {
    const std::vector<unsigned char> encoding = SubjectEncoding(name, __func__);
    const unsigned char* data = encoding.empty() ? reinterpret_cast<const unsigned char*>("") : encoding.data();
    unsigned char digest[20];
    if (!SHA1_nid_postfix(data, encoding.size(), digest))
        throw std::runtime_error("X509_STORE_load_locations: name hashing failed");
    return static_cast<unsigned long>(digest[0]) | (static_cast<unsigned long>(digest[1]) << 8) |
           (static_cast<unsigned long>(digest[2]) << 16) | (static_cast<unsigned long>(digest[3]) << 24);
}

bool FindStoredCertificate(LibreSslX509Store* store, const std::vector<unsigned char>& subject,
    LibreSslX509Object& result) {
    const LibreSslStack stack = ReadGuest(static_cast<const LibreSslStack*>(store->objects), __func__);
    if (stack.num < 0) throw std::invalid_argument(std::string(__func__) + ": negative object count");
    for (int index = 0; index < stack.num; ++index) {
        if (!stack.data) throw std::invalid_argument(std::string(__func__) + ": null object array");
        const auto object = ReadGuest(reinterpret_cast<const LibreSslX509Object*>(stack.data[index]), __func__);
        if (object.type != LibreSslX509LookupX509 || !object.data) continue;
        const auto* certificate = static_cast<const LibreSsl3X509*>(object.data);
        const auto info = ReadGuest(reinterpret_cast<const LibreSsl3CertificateInfo*>(certificate->certInfo), __func__);
        if (!info.subject) continue;
        const auto name = ReadGuest(static_cast<const LibreSslX509Name*>(info.subject), __func__);
        if (SubjectEncoding(&name, __func__) != subject) continue;
        result = object;
        return true;
    }
    return false;
}

int APS5_VABI NewDirectoryLookup(LibreSslX509Lookup* lookup) {
    auto* list = static_cast<DirectoryList*>(calloc_nid_postfix(1, sizeof(DirectoryList)));
    if (!list) return 0;
    list->directories = sk_new_nid_postfix(nullptr);
    if (!list->directories) {
        free_nid_postfix(list);
        return 0;
    }
    lookup->methodData = reinterpret_cast<char*>(list);
    return 1;
}

void APS5_VABI FreeDirectoryLookup(LibreSslX509Lookup* lookup) {
    if (!lookup) return;
    auto* list = reinterpret_cast<DirectoryList*>(lookup->methodData);
    if (!list) return;
    const LibreSslStack stack = ReadGuest(list->directories, __func__);
    for (int index = 0; index < stack.num; ++index) free_nid_postfix(stack.data[index]);
    sk_free_nid_postfix(list->directories);
    free_nid_postfix(list);
    lookup->methodData = nullptr;
}

bool AddDirectory(DirectoryList* list, const std::string& directory) {
    const LibreSslStack stack = ReadGuest(list->directories, __func__);
    for (int index = 0; index < stack.num; ++index) {
        if (!stack.data) throw std::invalid_argument(std::string(__func__) + ": null directory array");
        std::string existing;
        if (CopyGuestString_nid_no_patch(stack.data[index], 1024, existing) != 0) continue;
        if (existing == directory) return true;
    }
    char* copy = strndup_nid_postfix(directory.data(), directory.size());
    if (!copy) return false;
    if (sk_push_nid_postfix(list->directories, copy) <= 0) {
        free_nid_postfix(copy);
        return false;
    }
    return true;
}

int APS5_VABI DirectoryControl(LibreSslX509Lookup* lookup, int command, const char* argument,
    std::int64_t type, char**) {
    if (command != AddDirectoryCommand) return 0;
    if (type == FileTypeDefault)
        NotImplemented_nid_no_patch("X509_LOOKUP_hash_dir: the default certificate directory");
    if (type != FileTypePem)
        NotImplemented_nid_no_patch("X509_LOOKUP_hash_dir: an ASN.1 certificate directory");
    if (!lookup->methodData) throw std::invalid_argument("X509_LOOKUP_hash_dir: lookup without directory data");
    std::string directories;
    if (CopyGuestString_nid_no_patch(argument, 1024, directories) != 0) {
        PutX509Error(InvalidDirectory);
        return 0;
    }
    if (directories.empty()) {
        PutX509Error(InvalidDirectory);
        return 0;
    }
    auto* list = reinterpret_cast<DirectoryList*>(lookup->methodData);
    std::size_t position = 0;
    while (position < directories.size()) {
        const std::size_t separator = directories.find(':', position);
        const std::size_t end = separator == std::string::npos ? directories.size() : separator;
        if (end > position && !AddDirectory(list, directories.substr(position, end - position))) return 0;
        if (separator == std::string::npos) break;
        position = separator + 1;
    }
    return 1;
}

int APS5_VABI DirectoryBySubject(LibreSslX509Lookup* lookup, int type, LibreSslX509Name* name,
    LibreSslX509Object* result) {
    if (!name) return 0;
    if (type != LibreSslX509LookupX509)
        NotImplemented_nid_no_patch("X509_LOOKUP_hash_dir: a CRL lookup");
    if (!lookup->methodData) throw std::invalid_argument("X509_LOOKUP_hash_dir: lookup without directory data");
    auto* list = reinterpret_cast<DirectoryList*>(lookup->methodData);
    const LibreSslStack stack = ReadGuest(list->directories, __func__);
    const unsigned long hash = NameHash(name);
    const std::vector<unsigned char> subject = SubjectEncoding(name, __func__);
    for (int index = 0; index < stack.num; ++index) {
        if (!stack.data) throw std::invalid_argument(std::string(__func__) + ": null directory array");
        std::string directory;
        if (CopyGuestString_nid_no_patch(stack.data[index], 1024, directory) != 0) continue;
        for (int suffix = 0;; ++suffix) {
            char path[1024];
            const int length = std::snprintf(path, sizeof(path), "%s/%08lx.%d", directory.c_str(), hash, suffix);
            if (length < 0 || static_cast<std::size_t>(length) >= sizeof(path))
                throw std::overflow_error("X509_LOOKUP_hash_dir: hashed file path is too long");
            LoadReport report;
            if (LoadCertificates(lookup->storeContext, path, report, false) == 0) break;
        }
        LibreSslX509Object found{};
        if (FindStoredCertificate(lookup->storeContext, subject, found)) {
            const bool copied = GuestCopyOut_nid_no_patch(result, &found, sizeof(found));
            if (!copied) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
            return 1;
        }
    }
    return 0;
}

LibreSslX509LookupMethod directoryLookupMethod{"Load certs from files in a directory",
    reinterpret_cast<void*>(NewDirectoryLookup), reinterpret_cast<void*>(FreeDirectoryLookup), nullptr, nullptr,
    reinterpret_cast<void*>(DirectoryControl), reinterpret_cast<void*>(DirectoryBySubject), nullptr, nullptr, nullptr};

}

LibreSsl3Certificate* DecodeCertificate_nid_no_patch(std::span<const unsigned char>& input) {
    return reinterpret_cast<LibreSsl3Certificate*>(DecodeCertificate(input));
}

void FreeCertificate_nid_no_patch(LibreSsl3Certificate* certificate) {
    FreeCertificate(reinterpret_cast<CertificateObject*>(certificate));
}

LibreSsl3X509Algor* DecodeAlgor_nid_no_patch(std::span<const unsigned char>& input) {
    return DecodeAlgor(input);
}

void FreeAlgor_nid_no_patch(LibreSsl3X509Algor* algor) {
    FreeAlgor(algor);
}

LibreSslX509Name* DecodeName_nid_no_patch(std::span<const unsigned char> content) {
    LibreSslX509Name* name = DecodeName(content);
    if (name && !StoreCanonicalName(name, __func__)) {
        FreeName(name);
        return nullptr;
    }
    return name;
}

void FreeName_nid_no_patch(LibreSslX509Name* name) {
    FreeName(name);
}

extern "C" {

int APS5_VABI X509_STORE_load_locations_nid_postfix(LibreSslX509Store* store, const char* file, const char* directory) {
    if (!file && !directory) return 0;
    if (!store) throw std::invalid_argument(std::string(__func__) + ": null store");
    if (file) {
        if (!X509_STORE_add_lookup_nid_postfix(store, X509_LOOKUP_file_nid_postfix())) return 0;
        if (LoadCertificateFile(store, file) == 0) return 0;
    }
    if (directory) {
        LibreSslX509Lookup* lookup = X509_STORE_add_lookup_nid_postfix(store, &directoryLookupMethod);
        if (!lookup) return 0;
        if (!lookup->method || !lookup->method->ctrl)
            throw std::invalid_argument(std::string(__func__) + ": directory lookup without a control function");
        const auto control = reinterpret_cast<int (APS5_VABI*)(LibreSslX509Lookup*, int, const char*, std::int64_t, char**)>(lookup->method->ctrl);
        if (control(lookup, AddDirectoryCommand, directory, FileTypePem, nullptr) != 1) return 0;
    }
    return 1;
}

}
