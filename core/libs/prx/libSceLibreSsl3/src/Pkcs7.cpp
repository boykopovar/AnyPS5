#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Pkcs7.hpp"
#include "prx/libSceLibreSsl3/include/X509.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int NestedAsn1Error = 58;
constexpr int ExplicitTagNotConstructed = 120;
constexpr int BadObjectHeader = 102;
constexpr int BooleanIsWrongLength = 106;
constexpr int MissingEoc = 137;
constexpr int NullIsWrongLength = 144;
constexpr int SequenceLengthMismatch = 148;
constexpr int SequenceNotConstructed = 149;
constexpr int StringTooShort = 152;
constexpr int TooLong = 155;
constexpr int TypeNotConstructed = 156;
constexpr int UnexpectedEoc = 159;
constexpr int WrongTag = 168;
constexpr int NestedAsn1String = 197;
constexpr int BmpStringIsWrongLength = 214;
constexpr int UniversalStringIsWrongLength = 215;
constexpr int InvalidObjectEncoding = 216;
constexpr int NestedTooDeep = 219;
constexpr int InvalidBitStringBitsLeft = 220;
constexpr int TypeNotPrimitive = 231;

constexpr int BooleanType = 1;
constexpr int IntegerType = 2;
constexpr int BitStringType = 3;
constexpr int OctetStringType = 4;
constexpr int NullType = 5;
constexpr int ObjectType = 6;
constexpr int RealType = 9;
constexpr int EnumeratedType = 10;
constexpr int Utf8StringType = 12;
constexpr int RelativeObjectType = 13;
constexpr int TimeType = 14;
constexpr int SequenceType = 16;
constexpr int SetType = 17;
constexpr int NumericStringType = 18;
constexpr int PrintableStringType = 19;
constexpr int T61StringType = 20;
constexpr int VideotexStringType = 21;
constexpr int Ia5StringType = 22;
constexpr int UtcTimeType = 23;
constexpr int GeneralizedTimeType = 24;
constexpr int GraphicStringType = 25;
constexpr int VisibleStringType = 26;
constexpr int GeneralStringType = 27;
constexpr int UniversalStringType = 28;
constexpr int BmpStringType = 30;
constexpr int OtherType = -3;
constexpr int NegativeFlag = 0x100;
constexpr int MaxNesting = 30;
constexpr int MaxStringNest = 5;

constexpr std::size_t MaxStringLength = static_cast<std::size_t>(std::numeric_limits<int>::max());

constexpr std::int64_t BitsLeftFlag = 0x08;
constexpr int ObjectDynamic = 0x01;
constexpr int ObjectDynamicData = 0x08;

constexpr unsigned char IntegerTag = 0x02;
constexpr unsigned char OctetStringTag = 0x04;
constexpr unsigned char ObjectTag = 0x06;
constexpr unsigned char SequenceTag = 0x30;
constexpr unsigned char SetTag = 0x31;
constexpr unsigned char ConstructedFlag = 0x20;
constexpr unsigned char ClassMask = 0xc0;
constexpr unsigned char TagNumberMask = 0x1f;
constexpr unsigned char ContextTag0 = 0xa0;
constexpr unsigned char ContextTag1 = 0xa1;
constexpr unsigned char EncDataTag = 0x80;

constexpr unsigned char Pkcs7Prefix[] = {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07};
constexpr unsigned char DataType = 1;
constexpr unsigned char SignedType = 2;
constexpr unsigned char EnvelopedType = 3;
constexpr unsigned char SignedAndEnvelopedType = 4;
constexpr unsigned char DigestType = 5;
constexpr unsigned char EncryptedType = 6;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

struct Tlv {
    unsigned char tag;
    std::span<const unsigned char> content;
    std::span<const unsigned char> encoded;
    bool indefinite = false;
};

struct Header {
    unsigned char tag;
    std::size_t offset;
    std::size_t length;
    bool indefinite;
};

thread_local const char* unsupportedEncoding = nullptr;

bool ReadTlv(std::span<const unsigned char>& input, Tlv& value);

bool ReadHeader(std::span<const unsigned char> input, Header& header) {
    if (input.size() < 2) {
        PutError(BadObjectHeader);
        return false;
    }
    if ((input[0] & TagNumberMask) == TagNumberMask)
        NotImplemented_nid_no_patch("d2i_PKCS7: high tag number");
    header = {input[0], 2, input[1], false};
    if (input[1] == 0x80) {
        if ((input[0] & ConstructedFlag) == 0) {
            PutError(BadObjectHeader);
            return false;
        }
        header.length = 0;
        header.indefinite = true;
        return true;
    }
    if (input[1] & 0x80) {
        const std::size_t count = input[1] & 0x7f;
        if (count == 0x7f || input.size() < 2 + count) {
            PutError(BadObjectHeader);
            return false;
        }
        header.length = 0;
        for (std::size_t index = 0; index < count; ++index) {
            if (header.length > (std::numeric_limits<std::size_t>::max() >> 8)) {
                PutError(BadObjectHeader);
                return false;
            }
            header.length = (header.length << 8) | input[2 + index];
        }
        header.offset += count;
    }
    if (header.length > input.size() - header.offset) {
        PutError(TooLong);
        return false;
    }
    return true;
}

bool ReadTlvDefinite(std::span<const unsigned char>& input, Tlv& value) {
    Header header;
    if (!ReadHeader(input, header)) return false;
    value.tag = header.tag;
    value.content = input.subspan(header.offset, header.length);
    value.encoded = input.first(header.offset + header.length);
    value.indefinite = false;
    input = input.subspan(header.offset + header.length);
    return true;
}

bool ReadTlvIndefinite(std::span<const unsigned char>& input, Tlv& value) {
    const std::size_t offset = 2;
    const unsigned char tag = input[0];
    std::span<const unsigned char> contentStart = input.subspan(offset);
    std::span<const unsigned char> cursor = contentStart;
    std::size_t eocCount = 1;
    while (!cursor.empty()) {
        if (cursor.size() >= 2 && cursor[0] == 0x00 && cursor[1] == 0x00) {
            cursor = cursor.subspan(2);
            if (--eocCount == 0) break;
            continue;
        }
        Header header;
        if (!ReadHeader(cursor, header)) return false;
        cursor = cursor.subspan(header.offset);
        if (header.indefinite) {
            ++eocCount;
            continue;
        }
        cursor = cursor.subspan(header.length);
    }
    if (eocCount > 0) {
        PutError(MissingEoc);
        return false;
    }
    const std::size_t consumed = contentStart.size() - cursor.size();
    value.tag = tag;
    value.content = contentStart.first(consumed - 2);
    value.encoded = input.first(offset + consumed);
    value.indefinite = true;
    input = cursor;
    return true;
}

bool ReadTlv(std::span<const unsigned char>& input, Tlv& value) {
    if (input.size() < 2) {
        PutError(BadObjectHeader);
        return false;
    }
    if (input[0] == 0x00 && input[1] == 0x00) {
        PutError(UnexpectedEoc);
        return false;
    }
    if ((input[0] & TagNumberMask) == TagNumberMask)
        NotImplemented_nid_no_patch("d2i_PKCS7: high tag number");
    if (input[1] == 0x80) {
        if ((input[0] & ConstructedFlag) == 0) {
            PutError(BadObjectHeader);
            PutError(NestedAsn1Error);
            PutError(NestedAsn1Error);
            PutError(NestedAsn1Error);
            return false;
        }
        return ReadTlvIndefinite(input, value);
    }
    return ReadTlvDefinite(input, value);
}

bool Next(std::span<const unsigned char>& input, unsigned char tag, Tlv& value) {
    if ((tag == IntegerTag || tag == ObjectTag) && !input.empty() && input[0] == (tag | ConstructedFlag)) {
        Header header;
        if (ReadHeader(input, header)) PutError(TypeNotPrimitive);
        return false;
    }
    if (!ReadTlv(input, value)) return false;
    if (value.tag == tag) return true;
    if (tag == SequenceTag && value.tag == (tag & ~ConstructedFlag)) {
        PutError(SequenceNotConstructed);
    } else if (tag == ContextTag0 && value.tag == (tag & ~ConstructedFlag)) {
        PutError(ExplicitTagNotConstructed);
    } else {
        PutError(WrongTag);
    }
    return false;
}

bool Peek(std::span<const unsigned char> input, unsigned char tag) {
    return !input.empty() && input[0] == tag;
}

bool PeekSet(std::span<const unsigned char> input, unsigned char tag) {
    return Peek(input, tag) || Peek(input, tag & ~ConstructedFlag);
}

bool NextSet(std::span<const unsigned char>& input, unsigned char tag, Tlv& value) {
    if (!ReadTlv(input, value)) return false;
    if (value.tag == tag || value.tag == (tag & ~ConstructedFlag)) return true;
    PutError(WrongTag);
    return false;
}

enum class Encoding { Der, Ber, NotPrimitive };

bool IsPrimitiveOnly(int type) {
    return type == BooleanType || type == IntegerType || type == NullType || type == ObjectType || type == RealType ||
           type == EnumeratedType || type == RelativeObjectType || type == TimeType;
}

Encoding ScanEncoding(std::span<const unsigned char>& input, int depth) {
    if (input.size() < 2 || (input[0] & TagNumberMask) == TagNumberMask) {
        input = {};
        return Encoding::Der;
    }
    const unsigned char tag = input[0];
    const bool constructed = (tag & ConstructedFlag) != 0;
    const int number = tag & TagNumberMask;
    const bool primitiveOnly = constructed && (tag & ClassMask) == 0 && IsPrimitiveOnly(number);
    std::size_t offset = 2;
    std::size_t length = input[1];
    bool nonMinimal = false;
    if (length == 0x80) {
        if (!constructed) {
            input = {};
            return Encoding::Der;
        }
        return primitiveOnly ? Encoding::NotPrimitive : Encoding::Ber;
    }
    if (length & 0x80) {
        const std::size_t count = length & 0x7f;
        if (count == 0x7f || input.size() < 2 + count) {
            input = {};
            return Encoding::Der;
        }
        nonMinimal = input[2] == 0;
        length = 0;
        for (std::size_t index = 0; index < count; ++index) {
            if (length > (std::numeric_limits<std::size_t>::max() >> 8)) {
                input = {};
                return Encoding::Der;
            }
            length = (length << 8) | input[2 + index];
        }
        if (length < 0x80) nonMinimal = true;
        offset += count;
    }
    if (length > input.size() - offset) {
        input = {};
        return Encoding::Der;
    }
    if (primitiveOnly) return Encoding::NotPrimitive;
    if (nonMinimal) return Encoding::Ber;
    std::span<const unsigned char> content = input.subspan(offset, length);
    input = input.subspan(offset + length);
    if (!constructed) return Encoding::Der;
    if ((tag & ClassMask) == 0 && number != SequenceType && number != SetType) return Encoding::Ber;
    if (depth >= MaxNesting) return Encoding::Der;
    while (!content.empty()) {
        const Encoding encoding = ScanEncoding(content, depth + 1);
        if (encoding != Encoding::Der) return encoding;
    }
    return Encoding::Der;
}

bool Accepts(Encoding encoding, const char* name) {
    if (encoding == Encoding::Ber) {
        unsupportedEncoding = name;
        return false;
    }
    if (encoding == Encoding::NotPrimitive) {
        PutError(TypeNotPrimitive);
        return false;
    }
    return true;
}

bool IsDer(std::span<const unsigned char> input, const char* name) {
    return Accepts(ScanEncoding(input, 0), name);
}

bool IsDerName(std::span<const unsigned char> input) {
    if (input.size() < 2 || input[0] != SequenceTag) return true;
    std::size_t offset = 2;
    std::size_t length = input[1];
    const bool indefinite = length == 0x80;
    if (!indefinite && (length & 0x80)) {
        const std::size_t count = length & 0x7f;
        if (count == 0x7f || input.size() < 2 + count) return true;
        length = 0;
        for (std::size_t index = 0; index < count; ++index) {
            if (length > (std::numeric_limits<std::size_t>::max() >> 8)) return true;
            length = (length << 8) | input[2 + index];
        }
        offset += count;
    }
    if (!indefinite && length > input.size() - offset) return true;
    std::span<const unsigned char> content = indefinite ? input.subspan(offset) : input.subspan(offset, length);
    while (content.size() >= 2 && (content[0] != 0x00 || content[1] != 0x00)) {
        if (!Accepts(ScanEncoding(content, 1), "d2i_PKCS7: BER encoding in a Name")) return false;
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

Asn1String* NewEnumerated(std::span<const unsigned char> encoded) {
    std::vector<unsigned char> integer(encoded.begin(), encoded.end());
    integer[0] = IntegerTag;
    Asn1String* value = NewInteger(integer);
    if (value) value->type = EnumeratedType | (value->type & NegativeFlag);
    return value;
}

Asn1String* NewBitString(std::span<const unsigned char> content) {
    if (content.empty()) {
        PutError(StringTooShort);
        return nullptr;
    }
    if (content.front() > 7) {
        PutError(InvalidBitStringBitsLeft);
        return nullptr;
    }
    const unsigned int unusedBits = content.front();
    Asn1String* value = NewString(BitStringType, content.subspan(1));
    if (!value) return nullptr;
    if (value->length != 0) value->data[value->length - 1] &= static_cast<unsigned char>(0xffu << unusedBits);
    value->flags = BitsLeftFlag | unusedBits;
    return value;
}

bool IsObjectEncoding(std::span<const unsigned char> content) {
    if (content.empty() || (content.back() & 0x80)) return false;
    bool first = true;
    for (const unsigned char byte : content) {
        if (first && byte == 0x80) return false;
        first = !(byte & 0x80);
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
    free_nid_postfix(value->data);
    free_nid_postfix(value);
}

void FreeObject(Asn1Object_nid_no_patch* object) {
    ASN1_OBJECT_free_nid_postfix(object);
}

bool CollectSegments(std::span<const unsigned char>& input, bool needEoc, std::vector<unsigned char>& out, int depth) {
    if (depth > MaxStringNest) {
        PutError(NestedAsn1String);
        return false;
    }
    while (!input.empty()) {
        if (input.size() >= 2 && input[0] == 0x00 && input[1] == 0x00) {
            input = input.subspan(2);
            if (needEoc) return true;
            PutError(UnexpectedEoc);
            return false;
        }
        Header header;
        if (!ReadHeader(input, header)) {
            PutError(NestedAsn1Error);
            return false;
        }
        input = input.subspan(header.offset);
        if (header.tag & ConstructedFlag) {
            if (!CollectSegments(input, header.indefinite, out, depth + 1)) return false;
            continue;
        }
        if (header.length > MaxStringLength - out.size()) {
            PutError(TooLong);
            return false;
        }
        out.insert(out.end(), input.begin(), input.begin() + static_cast<std::ptrdiff_t>(header.length));
        input = input.subspan(header.length);
    }
    if (needEoc) {
        PutError(MissingEoc);
        return false;
    }
    return true;
}

bool ReadString(std::span<const unsigned char>& input, bool enclosedIndefinite, std::vector<unsigned char>& out) {
    Header header;
    if (!ReadHeader(input, header)) return false;
    std::span<const unsigned char> rest = input.subspan(header.offset);
    if (!(header.tag & ConstructedFlag)) {
        out.assign(rest.begin(), rest.begin() + static_cast<std::ptrdiff_t>(header.length));
        input = rest.subspan(header.length);
        return true;
    }
    if (!CollectSegments(rest, header.indefinite, out, 0)) return false;
    if (!header.indefinite && enclosedIndefinite) {
        PutError(UnexpectedEoc);
        return false;
    }
    input = rest;
    return true;
}

Asn1String* NextOctetString(std::span<const unsigned char>& input, bool enclosedIndefinite) {
    if (Peek(input, OctetStringTag | ConstructedFlag)) {
        std::vector<unsigned char> octets;
        if (!ReadString(input, enclosedIndefinite, octets)) return nullptr;
        return NewString(OctetStringType, octets);
    }
    Tlv value;
    if (!Next(input, OctetStringTag, value)) return nullptr;
    return NewString(OctetStringType, value.content);
}

void FreeType(void* element) {
    auto* type = static_cast<LibreSsl3Asn1Type*>(element);
    if (!type) return;
    if (type->type == ObjectType) {
        FreeObject(static_cast<Asn1Object_nid_no_patch*>(type->value));
    } else if (type->type != BooleanType && type->type != NullType) {
        FreeString(static_cast<Asn1String*>(type->value));
    }
    free_nid_postfix(type);
}

void FreeStack(LibreSsl3Stack* stack, void (*release)(void*)) {
    if (!stack) return;
    if (stack->num < 0) throw std::invalid_argument("PKCS7_free: negative stack count");
    for (int index = 0; index < stack->num; ++index) {
        if (!stack->data) throw std::invalid_argument("PKCS7_free: null stack array");
        release(stack->data[index]);
    }
    sk_free_nid_postfix(reinterpret_cast<LibreSslStack*>(stack));
}

LibreSsl3Stack* NewStack() {
    auto* stack = reinterpret_cast<LibreSsl3Stack*>(sk_new_nid_postfix(nullptr));
    if (!stack) PutError(MallocFailure);
    return stack;
}

bool Push(LibreSsl3Stack* stack, void* element, void (*release)(void*)) {
    if (sk_push_nid_postfix(reinterpret_cast<LibreSslStack*>(stack), element)) return true;
    release(element);
    PutError(MallocFailure);
    return false;
}

void FreeAlgorElement(void* element) {
    FreeAlgor_nid_no_patch(static_cast<LibreSsl3X509Algor*>(element));
}

void FreeCertificateElement(void* element) {
    auto* certificate = static_cast<LibreSsl3X509*>(element);
    if (!certificate) return;
    if (certificate->references > 1) {
        --certificate->references;
        return;
    }
    FreeCertificate_nid_no_patch(reinterpret_cast<LibreSsl3Certificate*>(certificate));
}

void FreeCrlElement(void* element) {
    X509_CRL_free_nid_postfix(static_cast<LibreSsl3X509Crl*>(element));
}

void FreeEncContent(LibreSsl3Pkcs7EncContent* data) {
    if (!data) return;
    FreeObject(data->contentType);
    FreeAlgor_nid_no_patch(data->algorithm);
    FreeString(data->encData);
    free_nid_postfix(data);
}

void FreeIssuerAndSerial(LibreSsl3Pkcs7IssuerAndSerial* value) {
    if (!value) return;
    FreeName_nid_no_patch(value->issuer);
    FreeString(value->serial);
    free_nid_postfix(value);
}

void FreeRecipInfo(void* element) {
    auto* info = static_cast<LibreSsl3Pkcs7RecipInfo*>(element);
    if (!info) return;
    FreeString(info->version);
    FreeIssuerAndSerial(info->issuerAndSerial);
    FreeAlgor_nid_no_patch(info->keyEncAlgor);
    FreeString(info->encKey);
    FreeCertificateElement(info->cert);
    free_nid_postfix(info);
}

void FreeAttribute(void* element) {
    auto* attribute = static_cast<LibreSsl3X509Attribute*>(element);
    if (!attribute) return;
    FreeObject(attribute->object);
    FreeStack(attribute->set, FreeType);
    free_nid_postfix(attribute);
}

void FreeSignerInfo(void* element) {
    auto* info = static_cast<LibreSsl3Pkcs7SignerInfo*>(element);
    if (!info) return;
    if (info->pkey) NotImplemented_nid_no_patch("PKCS7_free: signer private key");
    FreeString(info->version);
    FreeIssuerAndSerial(info->issuerAndSerial);
    FreeAlgor_nid_no_patch(info->digestAlg);
    FreeStack(info->authAttr, FreeAttribute);
    FreeAlgor_nid_no_patch(info->digestEncAlg);
    FreeString(info->encDigest);
    FreeStack(info->unauthAttr, FreeAttribute);
    free_nid_postfix(info);
}

void FreePkcs7(LibreSsl3Pkcs7* pkcs7);

void FreeEnvelope(LibreSsl3Pkcs7Envelope* data) {
    if (!data) return;
    FreeString(data->version);
    FreeStack(data->recipientInfo, FreeRecipInfo);
    FreeEncContent(data->encData);
    free_nid_postfix(data);
}

void FreeSignEnvelope(LibreSsl3Pkcs7SignEnvelope* data) {
    if (!data) return;
    FreeString(data->version);
    FreeStack(data->mdAlgs, FreeAlgorElement);
    FreeStack(data->cert, FreeCertificateElement);
    FreeStack(data->crl, FreeCrlElement);
    FreeStack(data->signerInfo, FreeSignerInfo);
    FreeEncContent(data->encData);
    FreeStack(data->recipientInfo, FreeRecipInfo);
    free_nid_postfix(data);
}

void FreeEncrypted(LibreSsl3Pkcs7Encrypt* data) {
    if (!data) return;
    FreeString(data->version);
    FreeEncContent(data->encData);
    free_nid_postfix(data);
}

void FreeSigned(LibreSsl3Pkcs7Signed* data) {
    if (!data) return;
    FreeString(data->version);
    FreeStack(data->mdAlgs, FreeAlgorElement);
    FreeStack(data->cert, FreeCertificateElement);
    FreeStack(data->crl, FreeCrlElement);
    FreeStack(data->signerInfo, FreeSignerInfo);
    FreePkcs7(data->contents);
    free_nid_postfix(data);
}

void FreeDigest(LibreSsl3Pkcs7Digest* data) {
    if (!data) return;
    FreeString(data->version);
    FreeAlgor_nid_no_patch(data->md);
    FreePkcs7(data->contents);
    FreeString(data->digest);
    free_nid_postfix(data);
}

int ContentType(const Asn1Object_nid_no_patch* object) {
    if (!object || !object->data || object->length != sizeof(Pkcs7Prefix) + 1) return 0;
    if (std::memcmp(object->data, Pkcs7Prefix, sizeof(Pkcs7Prefix)) != 0) return 0;
    const unsigned char type = object->data[sizeof(Pkcs7Prefix)];
    return type >= DataType && type <= EncryptedType ? type : 0;
}

void FreePkcs7(LibreSsl3Pkcs7* pkcs7) {
    if (!pkcs7) return;
    if (pkcs7->d) {
        const int type = ContentType(pkcs7->type);
        if (type == DataType) {
            FreeString(static_cast<Asn1String*>(pkcs7->d));
        } else if (type == SignedType) {
            FreeSigned(static_cast<LibreSsl3Pkcs7Signed*>(pkcs7->d));
        } else if (type == EnvelopedType) {
            FreeEnvelope(static_cast<LibreSsl3Pkcs7Envelope*>(pkcs7->d));
        } else if (type == SignedAndEnvelopedType) {
            FreeSignEnvelope(static_cast<LibreSsl3Pkcs7SignEnvelope*>(pkcs7->d));
        } else if (type == EncryptedType) {
            FreeEncrypted(static_cast<LibreSsl3Pkcs7Encrypt*>(pkcs7->d));
        } else if (type == DigestType) {
            FreeDigest(static_cast<LibreSsl3Pkcs7Digest*>(pkcs7->d));
        } else if (type == 0) {
            FreeType(pkcs7->d);
        } else {
            NotImplemented_nid_no_patch("PKCS7_free: unsupported content");
        }
    }
    FreeObject(pkcs7->type);
    free_nid_postfix(pkcs7->asn1);
    free_nid_postfix(pkcs7);
}

void* NewStringValue(int type, std::span<const unsigned char> content) {
    if (type == BitStringType) return NewBitString(content);
    if (type == BmpStringType && (content.size() & 1)) {
        PutError(BmpStringIsWrongLength);
        return nullptr;
    }
    if (type == UniversalStringType && (content.size() & 3)) {
        PutError(UniversalStringIsWrongLength);
        return nullptr;
    }
    return NewString(type, content);
}

LibreSsl3Asn1Type* NewType(int type, void* decoded, unsigned char boolean) {
    if (type != BooleanType && type != NullType && !decoded) return nullptr;
    auto* result = static_cast<LibreSsl3Asn1Type*>(calloc_nid_postfix(1, sizeof(LibreSsl3Asn1Type)));
    if (!result) {
        if (type == ObjectType) {
            FreeObject(static_cast<Asn1Object_nid_no_patch*>(decoded));
        } else if (type != BooleanType && type != NullType) {
            FreeString(static_cast<Asn1String*>(decoded));
        }
        PutError(MallocFailure);
        return nullptr;
    }
    result->type = type;
    if (type == BooleanType) {
        *reinterpret_cast<int*>(&result->value) = boolean;
    } else {
        result->value = decoded;
    }
    return result;
}

LibreSsl3Asn1Type* DecodeAny(std::span<const unsigned char>& input, bool enclosedIndefinite) {
    if (input.size() >= 2 && (input[0] & (ClassMask | ConstructedFlag)) == ConstructedFlag &&
        (input[0] & TagNumberMask) != SequenceType && (input[0] & TagNumberMask) != SetType) {
        const int type = input[0] & TagNumberMask;
        if (IsPrimitiveOnly(type)) {
            Header header;
            if (ReadHeader(input, header)) PutError(TypeNotPrimitive);
            return nullptr;
        }
        std::vector<unsigned char> collected;
        if (!ReadString(input, enclosedIndefinite, collected)) return nullptr;
        return NewType(type, NewStringValue(type, collected), 0);
    }
    Tlv value;
    if (!ReadTlv(input, value)) return nullptr;
    const bool universal = (value.tag & ClassMask) == 0;
    const bool constructed = (value.tag & ConstructedFlag) != 0;
    const int type = universal ? value.tag & TagNumberMask : OtherType;
    if ((type == SequenceType || type == SetType) && !constructed) {
        PutError(TypeNotConstructed);
        return nullptr;
    }
    if (type == BooleanType) {
        if (value.content.size() != 1) {
            PutError(BooleanIsWrongLength);
            return nullptr;
        }
        return NewType(type, nullptr, value.content[0]);
    }
    if (type == NullType) {
        if (!value.content.empty()) {
            PutError(NullIsWrongLength);
            return nullptr;
        }
        return NewType(type, nullptr, 0);
    }
    if (type == ObjectType) return NewType(type, NewObject(value.content), 0);
    if (type == IntegerType) return NewType(type, NewInteger(value.encoded), 0);
    if (type == EnumeratedType) return NewType(type, NewEnumerated(value.encoded), 0);
    if (type == SequenceType || type == SetType || type == OtherType) return NewType(type, NewString(type, value.encoded), 0);
    return NewType(type, NewStringValue(type, value.content), 0);
}

LibreSsl3X509Algor* DecodeAlgor(std::span<const unsigned char>& input) {
    if (!IsDer(input, "d2i_PKCS7: BER encoding in an AlgorithmIdentifier")) return nullptr;
    return DecodeAlgor_nid_no_patch(input);
}

LibreSsl3Certificate* DecodeCertificate(std::span<const unsigned char>& input) {
    if (!IsDer(input, "d2i_PKCS7: BER encoding in a certificate")) return nullptr;
    return DecodeCertificate_nid_no_patch(input);
}

template <typename Element, typename Decode>
LibreSsl3Stack* DecodeList(std::span<const unsigned char> content, Decode decode, void (*release)(void*)) {
    auto freeStack = [release](LibreSsl3Stack* value) { FreeStack(value, release); };
    std::unique_ptr<LibreSsl3Stack, decltype(freeStack)> stack(NewStack(), freeStack);
    if (!stack) return nullptr;
    while (!content.empty()) {
        Element* element = decode(content);
        if (!element || !Push(stack.get(), element, release)) return nullptr;
    }
    return stack.release();
}

LibreSsl3X509Attribute* DecodeAttribute(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    std::unique_ptr<LibreSsl3X509Attribute, void (*)(void*)> attribute(
        static_cast<LibreSsl3X509Attribute*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Attribute))), FreeAttribute);
    if (!attribute) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv object;
    Tlv values;
    if (!Next(body, ObjectTag, object) || (attribute->object = NewObject(object.content)) == nullptr ||
        !NextSet(body, SetTag, values))
        return nullptr;
    attribute->set = DecodeList<LibreSsl3Asn1Type>(values.content, [&values](std::span<const unsigned char>& rest) {
        return DecodeAny(rest, values.indefinite);
    }, FreeType);
    if (!attribute->set) return nullptr;
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return attribute.release();
}

LibreSsl3Stack* DecodeAttributes(std::span<const unsigned char> content) {
    return DecodeList<LibreSsl3X509Attribute>(content, DecodeAttribute, FreeAttribute);
}

LibreSsl3Pkcs7IssuerAndSerial* DecodeIssuerAndSerial(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    if (!IsDerName(body)) return nullptr;
    std::unique_ptr<LibreSsl3Pkcs7IssuerAndSerial, decltype(&FreeIssuerAndSerial)> value(
        static_cast<LibreSsl3Pkcs7IssuerAndSerial*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7IssuerAndSerial))),
        FreeIssuerAndSerial);
    if (!value) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv issuer;
    Tlv serial;
    if (!Next(body, SequenceTag, issuer) || (value->issuer = DecodeName_nid_no_patch(issuer.content)) == nullptr ||
        !Next(body, IntegerTag, serial) || (value->serial = NewInteger(serial.encoded)) == nullptr)
        return nullptr;
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return value.release();
}

LibreSsl3Pkcs7RecipInfo* DecodeRecipInfo(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    std::unique_ptr<LibreSsl3Pkcs7RecipInfo, void (*)(void*)> info(
        static_cast<LibreSsl3Pkcs7RecipInfo*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7RecipInfo))), FreeRecipInfo);
    if (!info) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    if (!Next(body, IntegerTag, version) || (info->version = NewInteger(version.encoded)) == nullptr ||
        (info->issuerAndSerial = DecodeIssuerAndSerial(body)) == nullptr ||
        (info->keyEncAlgor = DecodeAlgor(body)) == nullptr ||
        (info->encKey = NextOctetString(body, sequence.indefinite)) == nullptr)
        return nullptr;
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return info.release();
}

LibreSsl3Pkcs7EncContent* DecodeEncContent(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    std::unique_ptr<LibreSsl3Pkcs7EncContent, decltype(&FreeEncContent)> data(
        static_cast<LibreSsl3Pkcs7EncContent*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7EncContent))),
        FreeEncContent);
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv object;
    if (!Next(body, ObjectTag, object) || (data->contentType = NewObject(object.content)) == nullptr ||
        (data->algorithm = DecodeAlgor(body)) == nullptr)
        return nullptr;
    if (Peek(body, EncDataTag) || Peek(body, ContextTag0)) {
        std::vector<unsigned char> encrypted;
        if (!ReadString(body, sequence.indefinite, encrypted) ||
            (data->encData = NewString(OctetStringType, encrypted)) == nullptr)
            return nullptr;
    }
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return data.release();
}

LibreSsl3Pkcs7SignerInfo* DecodeSignerInfo(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    std::unique_ptr<LibreSsl3Pkcs7SignerInfo, void (*)(void*)> info(
        static_cast<LibreSsl3Pkcs7SignerInfo*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7SignerInfo))), FreeSignerInfo);
    if (!info) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    Tlv attributes;
    if (!Next(body, IntegerTag, version) || (info->version = NewInteger(version.encoded)) == nullptr ||
        (info->issuerAndSerial = DecodeIssuerAndSerial(body)) == nullptr ||
        (info->digestAlg = DecodeAlgor(body)) == nullptr)
        return nullptr;
    if (PeekSet(body, ContextTag0)) {
        if (!ReadTlv(body, attributes) || (info->authAttr = DecodeAttributes(attributes.content)) == nullptr)
            return nullptr;
    }
    if ((info->digestEncAlg = DecodeAlgor(body)) == nullptr ||
        (info->encDigest = NextOctetString(body, sequence.indefinite)) == nullptr)
        return nullptr;
    if (PeekSet(body, ContextTag1)) {
        if (!ReadTlv(body, attributes) || (info->unauthAttr = DecodeAttributes(attributes.content)) == nullptr)
            return nullptr;
    }
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return info.release();
}

LibreSsl3X509Crl* DecodeCrl(std::span<const unsigned char>& input) {
    if (!IsDer(input, "d2i_PKCS7: BER encoding in a CRL")) return nullptr;
    const unsigned char* cursor = input.data();
    LibreSsl3X509Crl* crl = d2i_X509_CRL_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(input.size()));
    if (crl) input = input.subspan(static_cast<std::size_t>(cursor - input.data()));
    return crl;
}

LibreSsl3Pkcs7Envelope* DecodeEnvelope(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    std::unique_ptr<LibreSsl3Pkcs7Envelope, decltype(&FreeEnvelope)> data(
        static_cast<LibreSsl3Pkcs7Envelope*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7Envelope))), FreeEnvelope);
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    Tlv list;
    if (!Next(body, IntegerTag, version) || (data->version = NewInteger(version.encoded)) == nullptr ||
        !NextSet(body, SetTag, list) ||
        (data->recipientInfo = DecodeList<LibreSsl3Pkcs7RecipInfo>(list.content, DecodeRecipInfo, FreeRecipInfo)) == nullptr ||
        (data->encData = DecodeEncContent(body)) == nullptr)
        return nullptr;
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return data.release();
}

LibreSsl3Pkcs7SignEnvelope* DecodeSignEnvelope(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    std::unique_ptr<LibreSsl3Pkcs7SignEnvelope, decltype(&FreeSignEnvelope)> data(
        static_cast<LibreSsl3Pkcs7SignEnvelope*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7SignEnvelope))),
        FreeSignEnvelope);
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    Tlv list;
    if (!Next(body, IntegerTag, version) || (data->version = NewInteger(version.encoded)) == nullptr ||
        !NextSet(body, SetTag, list) ||
        (data->recipientInfo = DecodeList<LibreSsl3Pkcs7RecipInfo>(list.content, DecodeRecipInfo, FreeRecipInfo)) == nullptr ||
        !NextSet(body, SetTag, list) ||
        (data->mdAlgs = DecodeList<LibreSsl3X509Algor>(list.content, DecodeAlgor, FreeAlgorElement)) == nullptr ||
        (data->encData = DecodeEncContent(body)) == nullptr)
        return nullptr;
    if (PeekSet(body, ContextTag0)) {
        if (!ReadTlv(body, list) ||
            (data->cert = DecodeList<LibreSsl3Certificate>(list.content, DecodeCertificate, FreeCertificateElement)) == nullptr)
            return nullptr;
    }
    if (PeekSet(body, ContextTag1)) {
        if (!ReadTlv(body, list) ||
            (data->crl = DecodeList<LibreSsl3X509Crl>(list.content, DecodeCrl, FreeCrlElement)) == nullptr)
            return nullptr;
    }
    if (!NextSet(body, SetTag, list) ||
        (data->signerInfo = DecodeList<LibreSsl3Pkcs7SignerInfo>(list.content, DecodeSignerInfo, FreeSignerInfo)) == nullptr)
        return nullptr;
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return data.release();
}

LibreSsl3Pkcs7* DecodePkcs7(std::span<const unsigned char>& input, int depth);

LibreSsl3Pkcs7Signed* DecodeSigned(std::span<const unsigned char>& input, int depth) {
    if (depth > MaxNesting) {
        PutError(NestedTooDeep);
        return nullptr;
    }
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    std::unique_ptr<LibreSsl3Pkcs7Signed, decltype(&FreeSigned)> data(
        static_cast<LibreSsl3Pkcs7Signed*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7Signed))), FreeSigned);
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    Tlv list;
    if (!Next(body, IntegerTag, version) || (data->version = NewInteger(version.encoded)) == nullptr ||
        !NextSet(body, SetTag, list) ||
        (data->mdAlgs = DecodeList<LibreSsl3X509Algor>(list.content, DecodeAlgor, FreeAlgorElement)) == nullptr ||
        (data->contents = DecodePkcs7(body, depth + 1)) == nullptr)
        return nullptr;
    if (PeekSet(body, ContextTag0)) {
        if (!ReadTlv(body, list) ||
            (data->cert = DecodeList<LibreSsl3Certificate>(list.content, DecodeCertificate, FreeCertificateElement)) == nullptr)
            return nullptr;
    }
    if (PeekSet(body, ContextTag1)) {
        if (!ReadTlv(body, list) ||
            (data->crl = DecodeList<LibreSsl3X509Crl>(list.content, DecodeCrl, FreeCrlElement)) == nullptr)
            return nullptr;
    }
    if (!NextSet(body, SetTag, list) ||
        (data->signerInfo = DecodeList<LibreSsl3Pkcs7SignerInfo>(list.content, DecodeSignerInfo, FreeSignerInfo)) == nullptr)
        return nullptr;
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return data.release();
}

LibreSsl3Pkcs7Encrypt* DecodeEncrypted(std::span<const unsigned char>& input, int depth) {
    if (depth > MaxNesting) {
        PutError(NestedTooDeep);
        return nullptr;
    }
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    std::unique_ptr<LibreSsl3Pkcs7Encrypt, decltype(&FreeEncrypted)> data(
        static_cast<LibreSsl3Pkcs7Encrypt*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7Encrypt))), FreeEncrypted);
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    if (!Next(body, IntegerTag, version) || (data->version = NewInteger(version.encoded)) == nullptr ||
        (data->encData = DecodeEncContent(body)) == nullptr)
        return nullptr;
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return data.release();
}

LibreSsl3Pkcs7Digest* DecodeDigest(std::span<const unsigned char>& input, int depth) {
    if (depth > MaxNesting) {
        PutError(NestedTooDeep);
        return nullptr;
    }
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    std::unique_ptr<LibreSsl3Pkcs7Digest, decltype(&FreeDigest)> data(
        static_cast<LibreSsl3Pkcs7Digest*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7Digest))), FreeDigest);
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    if (!Next(body, IntegerTag, version) || (data->version = NewInteger(version.encoded)) == nullptr ||
        (data->md = DecodeAlgor(body)) == nullptr || (data->contents = DecodePkcs7(body, depth + 1)) == nullptr ||
        (data->digest = NextOctetString(body, sequence.indefinite)) == nullptr)
        return nullptr;
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return data.release();
}

void* DecodeContent(int type, std::span<const unsigned char> content, bool indefinite, int depth) {
    if (type == SignedType) {
        std::unique_ptr<LibreSsl3Pkcs7Signed, decltype(&FreeSigned)> data(DecodeSigned(content, depth + 1), FreeSigned);
        if (data && !content.empty()) {
            PutError(SequenceLengthMismatch);
            return nullptr;
        }
        return data.release();
    }
    if (type == EnvelopedType) {
        std::unique_ptr<LibreSsl3Pkcs7Envelope, decltype(&FreeEnvelope)> data(DecodeEnvelope(content), FreeEnvelope);
        if (data && !content.empty()) {
            PutError(SequenceLengthMismatch);
            return nullptr;
        }
        return data.release();
    }
    if (type == SignedAndEnvelopedType) {
        std::unique_ptr<LibreSsl3Pkcs7SignEnvelope, decltype(&FreeSignEnvelope)> data(
            DecodeSignEnvelope(content), FreeSignEnvelope);
        if (data && !content.empty()) {
            PutError(SequenceLengthMismatch);
            return nullptr;
        }
        return data.release();
    }
    if (type == EncryptedType) {
        std::unique_ptr<LibreSsl3Pkcs7Encrypt, decltype(&FreeEncrypted)> data(
            DecodeEncrypted(content, depth + 1), FreeEncrypted);
        if (data && !content.empty()) {
            PutError(SequenceLengthMismatch);
            return nullptr;
        }
        return data.release();
    }
    if (type == DigestType) {
        std::unique_ptr<LibreSsl3Pkcs7Digest, decltype(&FreeDigest)> data(DecodeDigest(content, depth + 1), FreeDigest);
        if (data && !content.empty()) {
            PutError(SequenceLengthMismatch);
            return nullptr;
        }
        return data.release();
    }
    if (type == 0) {
        std::unique_ptr<LibreSsl3Asn1Type, void (*)(void*)> any(DecodeAny(content, indefinite), FreeType);
        if (any && !content.empty()) {
            PutError(SequenceLengthMismatch);
            return nullptr;
        }
        return any.release();
    }
    if (type == DataType && (Peek(content, OctetStringTag) || Peek(content, OctetStringTag | ConstructedFlag))) {
        std::vector<unsigned char> octets;
        if (!ReadString(content, indefinite, octets)) {
            PutError(NestedAsn1Error);
            return nullptr;
        }
        if (!content.empty()) {
            PutError(SequenceLengthMismatch);
            return nullptr;
        }
        return NewString(OctetStringType, octets);
    }
    Tlv value;
    if (!ReadTlv(content, value)) return nullptr;
    if (!content.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    if (type != DataType) NotImplemented_nid_no_patch("d2i_PKCS7: unsupported content");
    PutError(WrongTag);
    return nullptr;
}

LibreSsl3Pkcs7* DecodePkcs7(std::span<const unsigned char>& input, int depth) {
    if (depth > MaxNesting) {
        PutError(NestedTooDeep);
        return nullptr;
    }
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    std::unique_ptr<LibreSsl3Pkcs7, decltype(&FreePkcs7)> pkcs7(
        static_cast<LibreSsl3Pkcs7*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7))), FreePkcs7);
    if (!pkcs7) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv object;
    if (!Next(body, ObjectTag, object) || (pkcs7->type = NewObject(object.content)) == nullptr) return nullptr;
    if (!body.empty()) {
        Tlv content;
        if (!Next(body, ContextTag0, content) ||
            (pkcs7->d = DecodeContent(ContentType(pkcs7->type), content.content, content.indefinite, depth)) == nullptr)
            return nullptr;
    }
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return pkcs7.release();
}

}

extern "C" {

LibreSsl3Pkcs7* APS5_VABI d2i_PKCS7_nid_postfix(LibreSsl3Pkcs7** output, const unsigned char** input, std::int64_t length) {
    if (!input) throw std::invalid_argument(std::string(__func__) + ": null input");
    if (length < 0) return nullptr;
    if (!*input && length > 0) throw std::invalid_argument(std::string(__func__) + ": null input data");
    std::span<const unsigned char> remaining(*input, static_cast<std::size_t>(length));
    unsupportedEncoding = nullptr;
    LibreSsl3Pkcs7* pkcs7 = DecodePkcs7(remaining, 1);
    if (unsupportedEncoding) {
        FreePkcs7(pkcs7);
        const char* name = unsupportedEncoding;
        unsupportedEncoding = nullptr;
        NotImplemented_nid_no_patch(name);
    }
    if (!pkcs7) {
        if (output) {
            FreePkcs7(*output);
            *output = nullptr;
        }
        return nullptr;
    }
    if (output) {
        FreePkcs7(*output);
        *output = pkcs7;
    }
    *input += static_cast<std::size_t>(length) - remaining.size();
    return pkcs7;
}

void APS5_VABI PKCS7_free_nid_postfix(LibreSsl3Pkcs7* pkcs7) {
    if (!pkcs7) return;
    LibreSsl3Pkcs7 header{};
    const bool readable = GuestCopyIn_nid_no_patch(&header, pkcs7, sizeof(header));
    if (!readable) throw std::invalid_argument(std::string(__func__) + ": unreadable PKCS7");
    FreePkcs7(pkcs7);
}

}
