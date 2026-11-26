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
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int NestedAsn1Error = 58;
constexpr int BadObjectHeader = 102;
constexpr int BooleanIsWrongLength = 106;
constexpr int MissingEoc = 137;
constexpr int NullIsWrongLength = 144;
constexpr int SequenceLengthMismatch = 148;
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

constexpr int BooleanType = 1;
constexpr int IntegerType = 2;
constexpr int BitStringType = 3;
constexpr int OctetStringType = 4;
constexpr int NullType = 5;
constexpr int ObjectType = 6;
constexpr int EnumeratedType = 10;
constexpr int Utf8StringType = 12;
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
    std::vector<unsigned char> storage;
    bool indefinite = false;
};

bool ReadTlv(std::span<const unsigned char>& input, Tlv& value, int depth = 0);

bool ReadTlvDefinite(std::span<const unsigned char>& input, Tlv& value) {
    if (input.size() < 2) {
        PutError(BadObjectHeader);
        return false;
    }
    if ((input[0] & TagNumberMask) == TagNumberMask)
        NotImplemented_nid_no_patch("d2i_PKCS7: high tag number");
    std::size_t offset = 2;
    std::size_t length = input[1];
    if (length & 0x80) {
        const std::size_t count = length & 0x7f;
        if (count == 0x7f || input.size() < 2 + count) {
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
        offset += count;
    }
    if (length > input.size() - offset) {
        PutError(TooLong);
        return false;
    }
    value.tag = input[0];
    value.content = input.subspan(offset, length);
    value.encoded = input.first(offset + length);
    value.indefinite = false;
    input = input.subspan(offset + length);
    return true;
}

bool ReadTlvIndefinite(std::span<const unsigned char>& input, Tlv& value, int depth) {
    if (depth > MaxNesting) {
        PutError(NestedTooDeep);
        return false;
    }
    const std::size_t offset = 2;
    const unsigned char tag = input[0];
    std::vector<unsigned char> collected;
    std::span<const unsigned char> content = input.subspan(offset);
    while (!content.empty()) {
        if (content.size() >= 2 && content[0] == 0x00 && content[1] == 0x00) break;
        Tlv child;
        if (!ReadTlv(content, child, depth + 1)) return false;
        if (child.encoded.size() > MaxStringLength - collected.size()) {
            PutError(TooLong);
            return false;
        }
        collected.insert(collected.end(), child.encoded.begin(), child.encoded.end());
    }
    if (content.size() < 2 || content[0] != 0x00 || content[1] != 0x00) {
        PutError(MissingEoc);
        return false;
    }
    value.tag = tag;
    value.storage = std::move(collected);
    value.content = std::span<const unsigned char>(value.storage.data(), value.storage.size());
    const std::size_t encodedSize = offset + value.storage.size() + 2;
    value.encoded = input.first(encodedSize);
    value.indefinite = true;
    input = input.subspan(encodedSize);
    return true;
}

bool ReadTlv(std::span<const unsigned char>& input, Tlv& value, int depth) {
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
        return ReadTlvIndefinite(input, value, depth);
    }
    return ReadTlvDefinite(input, value);
}

bool Next(std::span<const unsigned char>& input, unsigned char tag, Tlv& value, int depth = 0) {
    if (!ReadTlv(input, value, depth)) return false;
    if (value.tag == tag) return true;
    PutError(WrongTag);
    return false;
}

bool Peek(std::span<const unsigned char> input, unsigned char tag) {
    return !input.empty() && input[0] == tag;
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

bool CollectConstructedString(std::span<const unsigned char> input, std::vector<unsigned char>& out, int depth) {
    if (depth > MaxStringNest) {
        PutError(NestedAsn1String);
        PutError(NestedAsn1Error);
        return false;
    }
    while (!input.empty()) {
        Tlv child;
        if (!ReadTlv(input, child, depth)) {
            PutError(NestedAsn1Error);
            return false;
        }
        if (child.tag & ConstructedFlag) {
            if (!CollectConstructedString(child.content, out, depth + 1)) return false;
        } else {
            if (child.content.size() > MaxStringLength - out.size()) {
                PutError(TooLong);
                return false;
            }
            out.insert(out.end(), child.content.begin(), child.content.end());
        }
    }
    return true;
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

void FreeRecipInfo(void* element) {
    auto* info = static_cast<LibreSsl3Pkcs7RecipInfo*>(element);
    if (!info) return;
    FreeString(info->version);
    if (info->issuerAndSerial) {
        FreeName_nid_no_patch(info->issuerAndSerial->issuer);
        FreeString(info->issuerAndSerial->serial);
        free_nid_postfix(info->issuerAndSerial);
    }
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
    if (info->issuerAndSerial) {
        FreeName_nid_no_patch(info->issuerAndSerial->issuer);
        FreeString(info->issuerAndSerial->serial);
        free_nid_postfix(info->issuerAndSerial);
    }
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

LibreSsl3Asn1Type* DecodeAny(const Tlv& value) {
    const bool universal = (value.tag & ClassMask) == 0;
    const bool constructed = (value.tag & ConstructedFlag) != 0;
    const int number = value.tag & TagNumberMask;
    const int type = universal ? number : OtherType;
    if ((type == SequenceType || type == SetType) && !constructed) {
        PutError(TypeNotConstructed);
        return nullptr;
    }
    void* decoded = nullptr;
    if (type == BooleanType) {
        if (value.content.size() != 1) {
            PutError(BooleanIsWrongLength);
            return nullptr;
        }
    } else if (type == NullType) {
        if (!value.content.empty()) {
            PutError(NullIsWrongLength);
            return nullptr;
        }
    } else if (type == ObjectType) {
        decoded = NewObject(value.content);
    } else if (type == IntegerType) {
        decoded = NewInteger(value.encoded);
    } else if (type == EnumeratedType) {
        decoded = NewEnumerated(value.encoded);
    } else if (type == BitStringType && !constructed) {
        decoded = NewBitString(value.content);
    } else if (type == SequenceType || type == SetType || type == OtherType) {
        decoded = NewString(type, value.encoded);
    } else if (universal && constructed &&
               (type == OctetStringType || type == BitStringType || type == Utf8StringType ||
                type == NumericStringType || type == PrintableStringType || type == T61StringType ||
                type == VideotexStringType || type == Ia5StringType || type == UtcTimeType ||
                type == GeneralizedTimeType || type == GraphicStringType || type == VisibleStringType ||
                type == GeneralStringType || type == UniversalStringType || type == BmpStringType)) {
        std::vector<unsigned char> collected;
        if (!CollectConstructedString(value.content, collected, 0)) return nullptr;
        if (type == BitStringType) {
            decoded = NewBitString(collected);
        } else {
            if (type == BmpStringType && (collected.size() & 1)) {
                PutError(BmpStringIsWrongLength);
                return nullptr;
            }
            if (type == UniversalStringType && (collected.size() & 3)) {
                PutError(UniversalStringIsWrongLength);
                return nullptr;
            }
            decoded = NewString(type, collected);
        }
    } else if (type == BmpStringType && (value.content.size() & 1)) {
        PutError(BmpStringIsWrongLength);
        return nullptr;
    } else if (type == UniversalStringType && (value.content.size() & 3)) {
        PutError(UniversalStringIsWrongLength);
        return nullptr;
    } else {
        decoded = NewString(type, value.content);
    }
    if (type != BooleanType && type != NullType && !decoded) return nullptr;
    auto* result = static_cast<LibreSsl3Asn1Type*>(calloc_nid_postfix(1, sizeof(LibreSsl3Asn1Type)));
    if (!result) {
        LibreSsl3Asn1Type temporary{type, decoded};
        if (type == ObjectType) {
            FreeObject(static_cast<Asn1Object_nid_no_patch*>(temporary.value));
        } else if (type != BooleanType && type != NullType) {
            FreeString(static_cast<Asn1String*>(temporary.value));
        }
        PutError(MallocFailure);
        return nullptr;
    }
    result->type = type;
    if (type == BooleanType) {
        *reinterpret_cast<int*>(&result->value) = value.content[0];
    } else {
        result->value = decoded;
    }
    return result;
}

template <typename Element, typename Decode>
LibreSsl3Stack* DecodeList(std::span<const unsigned char> content, Decode decode, void (*release)(void*)) {
    LibreSsl3Stack* stack = NewStack();
    if (!stack) return nullptr;
    while (!content.empty()) {
        Element* element = decode(content);
        if (!element || !Push(stack, element, release)) {
            FreeStack(stack, release);
            return nullptr;
        }
    }
    return stack;
}

LibreSsl3X509Attribute* DecodeAttribute(std::span<const unsigned char>& input, int depth) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence, depth)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* attribute = static_cast<LibreSsl3X509Attribute*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Attribute)));
    if (!attribute) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv object;
    Tlv values;
    bool ok = Next(body, ObjectTag, object, depth) && (attribute->object = NewObject(object.content)) != nullptr &&
              Next(body, SetTag, values, depth);
    if (ok) {
        attribute->set = DecodeList<LibreSsl3Asn1Type>(values.content, [](std::span<const unsigned char>& rest) {
            Tlv value;
            return ReadTlv(rest, value, 0) ? DecodeAny(value) : nullptr;
        }, FreeType);
        ok = attribute->set != nullptr;
    }
    if (ok && !body.empty()) {
        PutError(SequenceLengthMismatch);
        ok = false;
    }
    if (ok) return attribute;
    FreeAttribute(attribute);
    return nullptr;
}

LibreSsl3Stack* DecodeAttributes(std::span<const unsigned char> content, int depth) {
    return DecodeList<LibreSsl3X509Attribute>(content, [depth](std::span<const unsigned char>& rest) {
        return DecodeAttribute(rest, depth);
    }, FreeAttribute);
}

LibreSsl3Pkcs7IssuerAndSerial* DecodeIssuerAndSerial(std::span<const unsigned char>& input, int depth) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence, depth)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* value = static_cast<LibreSsl3Pkcs7IssuerAndSerial*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7IssuerAndSerial)));
    if (!value) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv issuer;
    Tlv serial;
    bool ok = Next(body, SequenceTag, issuer) && (value->issuer = DecodeName_nid_no_patch(issuer.content)) != nullptr &&
              Next(body, IntegerTag, serial) && (value->serial = NewInteger(serial.encoded)) != nullptr;
    if (ok && !body.empty()) {
        PutError(SequenceLengthMismatch);
        ok = false;
    }
    if (ok) return value;
    FreeName_nid_no_patch(value->issuer);
    FreeString(value->serial);
    free_nid_postfix(value);
    return nullptr;
}

LibreSsl3Pkcs7RecipInfo* DecodeRecipInfo(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* info = static_cast<LibreSsl3Pkcs7RecipInfo*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7RecipInfo)));
    if (!info) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    Tlv key;
    bool ok = Next(body, IntegerTag, version) && (info->version = NewInteger(version.encoded)) != nullptr &&
              (info->issuerAndSerial = DecodeIssuerAndSerial(body, 0)) != nullptr &&
              (info->keyEncAlgor = DecodeAlgor_nid_no_patch(body)) != nullptr && Next(body, OctetStringTag, key) &&
              (info->encKey = NewString(OctetStringType, key.content)) != nullptr;
    if (ok && !body.empty()) {
        PutError(SequenceLengthMismatch);
        ok = false;
    }
    if (ok) return info;
    FreeRecipInfo(info);
    return nullptr;
}

LibreSsl3Pkcs7EncContent* DecodeEncContent(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* data = static_cast<LibreSsl3Pkcs7EncContent*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7EncContent)));
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    try {
        Tlv object;
        Tlv encrypted;
        bool ok = Next(body, ObjectTag, object) && (data->contentType = NewObject(object.content)) != nullptr &&
                  (data->algorithm = DecodeAlgor_nid_no_patch(body)) != nullptr;
        if (ok && Peek(body, ContextTag0))
            NotImplemented_nid_no_patch("d2i_PKCS7: constructed OCTET STRING");
        if (ok && Peek(body, EncDataTag))
            ok = ReadTlv(body, encrypted) && (data->encData = NewString(OctetStringType, encrypted.content)) != nullptr;
        if (ok && !body.empty()) {
            PutError(SequenceLengthMismatch);
            ok = false;
        }
        if (ok) return data;
        FreeEncContent(data);
        return nullptr;
    } catch (...) {
        FreeEncContent(data);
        throw;
    }
}

LibreSsl3Pkcs7SignerInfo* DecodeSignerInfo(std::span<const unsigned char>& input, int depth) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence, depth)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* info = static_cast<LibreSsl3Pkcs7SignerInfo*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7SignerInfo)));
    if (!info) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    Tlv attributes;
    Tlv digest;
    bool ok = Next(body, IntegerTag, version, depth) && (info->version = NewInteger(version.encoded)) != nullptr &&
              (info->issuerAndSerial = DecodeIssuerAndSerial(body, depth)) != nullptr &&
              (info->digestAlg = DecodeAlgor_nid_no_patch(body)) != nullptr;
    if (ok && Peek(body, ContextTag0))
        ok = ReadTlv(body, attributes, depth) && (info->authAttr = DecodeAttributes(attributes.content, depth)) != nullptr;
    ok = ok && (info->digestEncAlg = DecodeAlgor_nid_no_patch(body)) != nullptr && Next(body, OctetStringTag, digest, depth) &&
         (info->encDigest = NewString(OctetStringType, digest.content)) != nullptr;
    if (ok && Peek(body, ContextTag1))
        ok = ReadTlv(body, attributes, depth) && (info->unauthAttr = DecodeAttributes(attributes.content, depth)) != nullptr;
    if (ok && !body.empty()) {
        PutError(SequenceLengthMismatch);
        ok = false;
    }
    if (ok) return info;
    FreeSignerInfo(info);
    return nullptr;
}

LibreSsl3X509Crl* DecodeCrl(std::span<const unsigned char>& input) {
    const unsigned char* cursor = input.data();
    LibreSsl3X509Crl* crl = d2i_X509_CRL_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(input.size()));
    if (crl) input = input.subspan(static_cast<std::size_t>(cursor - input.data()));
    return crl;
}

LibreSsl3Pkcs7Envelope* DecodeEnvelope(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* data = static_cast<LibreSsl3Pkcs7Envelope*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7Envelope)));
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    try {
        Tlv version;
        Tlv list;
        bool ok = Next(body, IntegerTag, version) && (data->version = NewInteger(version.encoded)) != nullptr &&
                  Next(body, SetTag, list) &&
                  (data->recipientInfo = DecodeList<LibreSsl3Pkcs7RecipInfo>(list.content, DecodeRecipInfo, FreeRecipInfo)) != nullptr &&
                  (data->encData = DecodeEncContent(body)) != nullptr;
        if (ok && !body.empty()) {
            PutError(SequenceLengthMismatch);
            ok = false;
        }
        if (ok) return data;
        FreeEnvelope(data);
        return nullptr;
    } catch (...) {
        FreeEnvelope(data);
        throw;
    }
}

LibreSsl3Pkcs7SignEnvelope* DecodeSignEnvelope(std::span<const unsigned char>& input) {
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* data = static_cast<LibreSsl3Pkcs7SignEnvelope*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7SignEnvelope)));
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    try {
        Tlv version;
        Tlv list;
        bool ok = Next(body, IntegerTag, version) && (data->version = NewInteger(version.encoded)) != nullptr &&
                  Next(body, SetTag, list) &&
                  (data->recipientInfo = DecodeList<LibreSsl3Pkcs7RecipInfo>(list.content, DecodeRecipInfo, FreeRecipInfo)) != nullptr &&
                  Next(body, SetTag, list) &&
                  (data->mdAlgs = DecodeList<LibreSsl3X509Algor>(list.content, DecodeAlgor_nid_no_patch, FreeAlgorElement)) != nullptr &&
                  (data->encData = DecodeEncContent(body)) != nullptr;
        if (ok && Peek(body, ContextTag0))
            ok = ReadTlv(body, list) &&
                 (data->cert = DecodeList<LibreSsl3Certificate>(list.content, DecodeCertificate_nid_no_patch, FreeCertificateElement)) != nullptr;
        if (ok && Peek(body, ContextTag1))
            ok = ReadTlv(body, list) && (data->crl = DecodeList<LibreSsl3X509Crl>(list.content, DecodeCrl, FreeCrlElement)) != nullptr;
        ok = ok && Next(body, SetTag, list) &&
             (data->signerInfo = DecodeList<LibreSsl3Pkcs7SignerInfo>(list.content, [](std::span<const unsigned char>& rest) {
                 return DecodeSignerInfo(rest, 0);
             }, FreeSignerInfo)) != nullptr;
        if (ok && !body.empty()) {
            PutError(SequenceLengthMismatch);
            ok = false;
        }
        if (ok) return data;
        FreeSignEnvelope(data);
        return nullptr;
    } catch (...) {
        FreeSignEnvelope(data);
        throw;
    }
}

LibreSsl3Pkcs7* DecodePkcs7(std::span<const unsigned char>& input, int depth);

LibreSsl3Pkcs7Signed* DecodeSigned(std::span<const unsigned char>& input, int depth) {
    if (depth > MaxNesting) {
        PutError(NestedTooDeep);
        return nullptr;
    }
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence, depth)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* data = static_cast<LibreSsl3Pkcs7Signed*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7Signed)));
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    Tlv list;
    bool ok = Next(body, IntegerTag, version, depth) && (data->version = NewInteger(version.encoded)) != nullptr &&
              Next(body, SetTag, list, depth) &&
              (data->mdAlgs = DecodeList<LibreSsl3X509Algor>(list.content, DecodeAlgor_nid_no_patch, FreeAlgorElement)) != nullptr &&
              (data->contents = DecodePkcs7(body, depth + 1)) != nullptr;
    if (ok && Peek(body, ContextTag0))
        ok = ReadTlv(body, list, depth) &&
             (data->cert = DecodeList<LibreSsl3Certificate>(list.content, DecodeCertificate_nid_no_patch, FreeCertificateElement)) != nullptr;
    if (ok && Peek(body, ContextTag1))
        ok = ReadTlv(body, list, depth) && (data->crl = DecodeList<LibreSsl3X509Crl>(list.content, DecodeCrl, FreeCrlElement)) != nullptr;
    ok = ok && Next(body, SetTag, list, depth) &&
         (data->signerInfo = DecodeList<LibreSsl3Pkcs7SignerInfo>(list.content, [depth](std::span<const unsigned char>& rest) {
             return DecodeSignerInfo(rest, depth);
         }, FreeSignerInfo)) != nullptr;
    if (ok && !body.empty()) {
        PutError(SequenceLengthMismatch);
        ok = false;
    }
    if (ok) return data;
    FreeSigned(data);
    return nullptr;
}

LibreSsl3Pkcs7Encrypt* DecodeEncrypted(std::span<const unsigned char>& input, int depth) {
    if (depth > MaxNesting) {
        PutError(NestedTooDeep);
        return nullptr;
    }
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* data = static_cast<LibreSsl3Pkcs7Encrypt*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7Encrypt)));
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    try {
        Tlv version;
        bool ok = Next(body, IntegerTag, version) && (data->version = NewInteger(version.encoded)) != nullptr &&
                  (data->encData = DecodeEncContent(body)) != nullptr;
        if (ok && !body.empty()) {
            PutError(SequenceLengthMismatch);
            ok = false;
        }
        if (ok) return data;
        FreeEncrypted(data);
        return nullptr;
    } catch (...) {
        FreeEncrypted(data);
        throw;
    }
}

LibreSsl3Pkcs7Digest* DecodeDigest(std::span<const unsigned char>& input, int depth) {
    if (depth > MaxNesting) {
        PutError(NestedTooDeep);
        return nullptr;
    }
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* data = static_cast<LibreSsl3Pkcs7Digest*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7Digest)));
    if (!data) {
        PutError(MallocFailure);
        return nullptr;
    }
    Tlv version;
    Tlv digest;
    bool ok = Next(body, IntegerTag, version) && (data->version = NewInteger(version.encoded)) != nullptr &&
              (data->md = DecodeAlgor_nid_no_patch(body)) != nullptr &&
              (data->contents = DecodePkcs7(body, depth + 1)) != nullptr &&
              Next(body, OctetStringTag, digest) &&
              (data->digest = NewString(OctetStringType, digest.content)) != nullptr;
    if (ok && !body.empty()) {
        PutError(SequenceLengthMismatch);
        ok = false;
    }
    if (ok) return data;
    FreeDigest(data);
    return nullptr;
}

void* DecodeContent(int type, std::span<const unsigned char> content, int depth) {
    if (type == SignedType) {
        LibreSsl3Pkcs7Signed* data = DecodeSigned(content, depth + 1);
        if (data && !content.empty()) {
            PutError(SequenceLengthMismatch);
            FreeSigned(data);
            return nullptr;
        }
        return data;
    }
    if (type == EnvelopedType) {
        LibreSsl3Pkcs7Envelope* data = DecodeEnvelope(content);
        if (data && !content.empty()) {
            PutError(SequenceLengthMismatch);
            FreeEnvelope(data);
            return nullptr;
        }
        return data;
    }
    if (type == SignedAndEnvelopedType) {
        LibreSsl3Pkcs7SignEnvelope* data = DecodeSignEnvelope(content);
        if (data && !content.empty()) {
            PutError(SequenceLengthMismatch);
            FreeSignEnvelope(data);
            return nullptr;
        }
        return data;
    }
    if (type == EncryptedType) {
        LibreSsl3Pkcs7Encrypt* data = DecodeEncrypted(content, depth + 1);
        if (data && !content.empty()) {
            PutError(SequenceLengthMismatch);
            FreeEncrypted(data);
            return nullptr;
        }
        return data;
    }
    if (type == DigestType) {
        LibreSsl3Pkcs7Digest* data = DecodeDigest(content, depth + 1);
        if (data && !content.empty()) {
            PutError(SequenceLengthMismatch);
            FreeDigest(data);
            return nullptr;
        }
        return data;
    }
    Tlv value;
    if (!ReadTlv(content, value, depth)) return nullptr;
    if (!content.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    if (type == 0) return DecodeAny(value);
    if (type != DataType) NotImplemented_nid_no_patch("d2i_PKCS7: unsupported content");
    if (value.tag == OctetStringTag) return NewString(OctetStringType, value.content);
    if (value.tag == (OctetStringTag | ConstructedFlag)) {
        std::vector<unsigned char> collected;
        if (!CollectConstructedString(value.content, collected, 0)) {
            PutError(NestedAsn1Error);
            return nullptr;
        }
        return NewString(OctetStringType, collected);
    }
    PutError(WrongTag);
    return nullptr;
}

LibreSsl3Pkcs7* DecodePkcs7(std::span<const unsigned char>& input, int depth) {
    if (depth > MaxNesting) {
        PutError(NestedTooDeep);
        return nullptr;
    }
    Tlv sequence;
    if (!Next(input, SequenceTag, sequence, depth)) return nullptr;
    std::span<const unsigned char> body = sequence.content;
    auto* pkcs7 = static_cast<LibreSsl3Pkcs7*>(calloc_nid_postfix(1, sizeof(LibreSsl3Pkcs7)));
    if (!pkcs7) {
        PutError(MallocFailure);
        return nullptr;
    }
    try {
        Tlv object;
        bool ok = Next(body, ObjectTag, object, depth) && (pkcs7->type = NewObject(object.content)) != nullptr;
        if (ok && !body.empty()) {
            Tlv content;
            ok = Next(body, ContextTag0, content, depth) && (pkcs7->d = DecodeContent(ContentType(pkcs7->type), content.content, depth)) != nullptr;
        }
        if (ok && !body.empty()) {
            PutError(SequenceLengthMismatch);
            ok = false;
        }
        if (ok) return pkcs7;
        FreePkcs7(pkcs7);
        return nullptr;
    } catch (...) {
        FreePkcs7(pkcs7);
        throw;
    }
}

}

extern "C" {

LibreSsl3Pkcs7* APS5_VABI d2i_PKCS7_nid_postfix(LibreSsl3Pkcs7** output, const unsigned char** input, std::int64_t length) {
    if (!input) throw std::invalid_argument(std::string(__func__) + ": null input");
    if (length < 0) return nullptr;
    if (!*input && length > 0) throw std::invalid_argument(std::string(__func__) + ": null input data");
    std::span<const unsigned char> remaining(*input, static_cast<std::size_t>(length));
    LibreSsl3Pkcs7* pkcs7 = DecodePkcs7(remaining, 1);
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
