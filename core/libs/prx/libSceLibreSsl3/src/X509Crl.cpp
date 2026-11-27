#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int LengthMismatch = 62;
constexpr int MallocFailure = 65;
constexpr int BadObjectHeader = 102;
constexpr int BooleanWrongLength = 106;
constexpr int FieldMissing = 121;
constexpr int HeaderTooLong = 123;
constexpr int IllegalCharacters = 124;
constexpr int InvalidTimeFormat = 132;
constexpr int InvalidUtf8String = 134;
constexpr int MissingEndOfContents = 137;
constexpr int MstringNotUniversal = 139;
constexpr int MstringWrongTag = 140;
constexpr int NullIsWrongLength = 144;
constexpr int SequenceLengthMismatch = 148;
constexpr int SequenceNotConstructed = 149;
constexpr int StringTooShort = 152;
constexpr int TooLong = 155;
constexpr int TypeNotConstructed = 156;
constexpr int UnexpectedEndOfContents = 159;
constexpr int WrongTag = 168;
constexpr int NestedAsn1String = 197;
constexpr int BmpStringWrongLength = 214;
constexpr int UniversalStringWrongLength = 215;
constexpr int InvalidObjectEncoding = 216;
constexpr int InvalidBitStringBitsLeft = 220;

constexpr int BitStringType = 3;
constexpr int OctetStringType = 4;
constexpr int ObjectType = 6;
constexpr int EnumeratedType = 10;
constexpr int Utf8StringType = 12;
constexpr int SequenceType = 16;
constexpr int SetType = 17;
constexpr int PrintableStringType = 19;
constexpr int T61StringType = 20;
constexpr int Ia5StringType = 22;
constexpr int UtcTimeType = 23;
constexpr int GeneralizedTimeType = 24;
constexpr int VisibleStringType = 26;
constexpr int UniversalStringType = 28;
constexpr int BmpStringType = 30;
constexpr int NegativeType = 0x100;

constexpr int TooLarge = 223;
constexpr int TooSmall = 224;
constexpr int WrongIntegerType = 225;

constexpr int CrlNumberNid = 88;
constexpr int AuthorityKeyIdNid = 90;
constexpr int DeltaCrlNid = 140;
constexpr int CrlReasonNid = 141;
constexpr int IssuingDistributionPointNid = 770;
constexpr int CertificateIssuerNid = 771;
constexpr int FreshestCrlNid = 857;
constexpr int CrlReasonRemoveFromCrl = 8;
constexpr int NoCrlReason = -1;
constexpr int AllCrlReasons = 0x807f;
constexpr int InvalidFlag = 0x80;
constexpr int CriticalFlag = 0x200;
constexpr int FreshestFlag = 0x1000;

constexpr std::int64_t BitsLeftFlag = 0x08;
constexpr std::int64_t BitsLeftMask = 0x07;
constexpr std::int64_t MultiStringFlag = 0x40;
constexpr int ObjectDynamic = 0x01;
constexpr int ObjectDynamicData = 0x08;

constexpr unsigned char Constructed = 0x20;
constexpr unsigned char ClassMask = 0xc0;
constexpr unsigned char TagNumberMask = 0x1f;
constexpr std::uint64_t MaxTagNumberBeforeShift = std::numeric_limits<std::int32_t>::max() >> 7;
constexpr std::uint64_t MaxLength = std::numeric_limits<std::int64_t>::max();
constexpr int MaxStringNesting = 5;
constexpr std::uint32_t UnicodeMax = 0x10ffff;
constexpr std::uint64_t NameValueTags[] = {3, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 22, 28, 29, 30};

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

struct Header {
    unsigned char identifier;
    std::uint64_t number;
    bool indefinite;
    std::size_t headerLength;
    std::size_t length;
};

struct NameBuffer {
    std::size_t length;
    char* data;
    std::size_t max;
};

int ReadHeader(std::span<const unsigned char> input, Header& header) {
    if (input.empty()) return HeaderTooLong;
    header = {input[0], static_cast<std::uint64_t>(input[0] & TagNumberMask), false, 1, 0};
    if (header.number == TagNumberMask) {
        header.number = 0;
        for (;;) {
            if (header.headerLength == input.size()) return HeaderTooLong;
            const unsigned char octet = input[header.headerLength++];
            header.number = (header.number << 7) | (octet & 0x7f);
            if (!(octet & 0x80)) break;
            if (header.number > MaxTagNumberBeforeShift) return HeaderTooLong;
        }
    }
    if (header.headerLength == input.size()) return HeaderTooLong;
    const unsigned char first = input[header.headerLength++];
    if (first == 0x80) {
        if (!(header.identifier & Constructed)) return HeaderTooLong;
        header.indefinite = true;
        header.length = input.size() - header.headerLength;
        return 0;
    }
    std::uint64_t length = first & 0x7f;
    if (first & 0x80) {
        std::size_t count = first & 0x7f;
        if (input.size() - header.headerLength < count) return HeaderTooLong;
        while (count != 0 && input[header.headerLength] == 0) {
            ++header.headerLength;
            --count;
        }
        if (count > sizeof(std::uint64_t)) return HeaderTooLong;
        length = 0;
        for (; count != 0; --count) length = (length << 8) | input[header.headerLength++];
        if (length > MaxLength) return HeaderTooLong;
    }
    if (length > input.size() - header.headerLength) return TooLong;
    header.length = static_cast<std::size_t>(length);
    return 0;
}

unsigned char TagByte(const Header& header) {
    if (header.number >= TagNumberMask) return header.identifier;
    return static_cast<unsigned char>((header.identifier & ~TagNumberMask) | header.number);
}

unsigned char PeekTag(std::span<const unsigned char> input) {
    Header header;
    if (ReadHeader(input, header)) return input.front();
    return TagByte(header);
}

bool ReadTlv(std::span<const unsigned char>& input, Tlv& value) {
    Header header;
    if (const int reason = ReadHeader(input, header)) {
        PutError(reason);
        return false;
    }
    if (header.indefinite) {
        PutError(BadObjectHeader);
        return false;
    }
    value.tag = TagByte(header);
    value.content = input.subspan(header.headerLength, header.length);
    value.encoded = input.first(header.headerLength + header.length);
    input = input.subspan(header.headerLength + header.length);
    return true;
}

bool IsEndOfContents(std::span<const unsigned char> input) {
    return input.size() >= 2 && input[0] == 0 && input[1] == 0;
}

std::size_t Consumed(std::span<const unsigned char> input, std::span<const unsigned char> rest) {
    return static_cast<std::size_t>(rest.data() - input.data());
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

Asn1String* NewString(int type, std::span<const unsigned char> content, std::int64_t flags = 0) {
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
    *value = {static_cast<std::int32_t>(content.size()), type, data, flags};
    return value;
}

Asn1String* NewInteger(std::span<const unsigned char> encoded) {
    const unsigned char* cursor = encoded.data();
    return static_cast<Asn1String*>(
        d2i_ASN1_INTEGER_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(encoded.size())));
}

Asn1String* NewBitString(std::span<const unsigned char> content, std::int64_t flags = 0) {
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
    *value = {static_cast<std::int32_t>(length), BitStringType, data, flags | BitsLeftFlag | unusedBits};
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
    if (auto* buffer = static_cast<NameBuffer*>(name->bytes)) {
        if (buffer->data) free_nid_postfix(buffer->data);
        free_nid_postfix(buffer);
    }
    if (name->canonEnc) free_nid_postfix(name->canonEnc);
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
    AUTHORITY_KEYID_free_nid_postfix(crl->akid);
    ASN1_INTEGER_free_nid_postfix(crl->crlNumber);
    ASN1_INTEGER_free_nid_postfix(crl->baseCrlNumber);
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

std::int64_t EnumeratedValue(const Asn1String& value) {
    if (value.length < 0) return -1;
    if (value.type != EnumeratedType && value.type != (EnumeratedType | NegativeType)) {
        PutError(WrongIntegerType);
        return -1;
    }
    if (value.length != 0 && !value.data) throw std::invalid_argument("d2i_X509_CRL: CRL reason without data");
    std::uint64_t magnitude = 0;
    for (int index = 0; index < value.length; ++index) {
        if (magnitude > (std::numeric_limits<std::uint64_t>::max() >> 8)) {
            PutError(TooLarge);
            return -1;
        }
        magnitude = (magnitude << 8) | value.data[index];
    }
    if (value.type & NegativeType) {
        if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1) {
            PutError(TooSmall);
            return -1;
        }
        return static_cast<std::int64_t>(0 - magnitude);
    }
    if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        PutError(TooLarge);
        return -1;
    }
    return static_cast<std::int64_t>(magnitude);
}

bool HasSingleExtension(const LibreSsl3Stack* extensions, int nid) {
    const LibreSslAsn1Object* object = LibreSsl3::ObjectByNid(nid);
    if (!object) throw std::logic_error("d2i_X509_CRL: no object for an extension NID");
    const int first = FindExtension(extensions, object, -1);
    return first >= 0 && FindExtension(extensions, object, first) < 0;
}

const LibreSsl3X509Extension& ExtensionAt(const LibreSsl3Stack& extensions, int index) {
    if (!extensions.data) throw std::invalid_argument("d2i_X509_CRL: null extension array");
    const auto* extension = static_cast<const LibreSsl3X509Extension*>(static_cast<const void*>(extensions.data[index]));
    if (!extension) throw std::invalid_argument("d2i_X509_CRL: null extension");
    return *extension;
}

bool SetEntryIssuers(LibreSsl3X509Crl* crl) {
    const LibreSsl3Stack* entries = crl->crl->revoked;
    if (!entries) return true;
    for (int index = 0; index < entries->num; ++index) {
        if (!entries->data) throw std::invalid_argument("d2i_X509_CRL: null revoked array");
        auto* entry = static_cast<LibreSsl3Revoked*>(static_cast<void*>(entries->data[index]));
        if (!entry) throw std::invalid_argument("d2i_X509_CRL: null revoked entry");
        int position = -1;
        if (!HasSingleExtension(entry->extensions, CertificateIssuerNid)) {
            GetExtension(entry->extensions, CertificateIssuerNid, &position, nullptr);
            if (position != -1) return false;
        }
        auto* reason = static_cast<Asn1String*>(GetExtension(entry->extensions, CrlReasonNid, &position, nullptr));
        if (!reason && position != -1) return false;
        entry->reason = reason ? static_cast<int>(EnumeratedValue(*reason)) : NoCrlReason;
        FreeString(reason);
        const LibreSsl3Stack* extensions = entry->extensions;
        for (int extension = 0; extensions && extension < extensions->num; ++extension) {
            const LibreSsl3X509Extension& value = ExtensionAt(*extensions, extension);
            if (value.critical <= 0 || OBJ_obj2nid_nid_postfix(value.object) == CertificateIssuerNid) continue;
            crl->flags |= CriticalFlag;
            break;
        }
    }
    return true;
}

void CacheExtensions(LibreSsl3X509Crl* crl) {
    const LibreSsl3Stack* extensions = crl->crl->extensions;
    if (HasSingleExtension(extensions, IssuingDistributionPointNid))
        NotImplemented_nid_no_patch("d2i_X509_CRL: an issuing distribution point extension");
    crl->akid = GetExtension(extensions, AuthorityKeyIdNid, nullptr, nullptr);
    crl->crlNumber = static_cast<Asn1String*>(GetExtension(extensions, CrlNumberNid, nullptr, nullptr));
    crl->baseCrlNumber = static_cast<Asn1String*>(GetExtension(extensions, DeltaCrlNid, nullptr, nullptr));
    if (crl->baseCrlNumber && !crl->crlNumber) crl->flags |= InvalidFlag;
    for (int index = 0; extensions && index < extensions->num; ++index) {
        const LibreSsl3X509Extension& extension = ExtensionAt(*extensions, index);
        const int nid = OBJ_obj2nid_nid_postfix(extension.object);
        if (nid == FreshestCrlNid) crl->flags |= FreshestFlag;
        if (extension.critical <= 0) continue;
        if (nid == IssuingDistributionPointNid || nid == AuthorityKeyIdNid || nid == DeltaCrlNid) continue;
        crl->flags |= CriticalFlag;
        break;
    }
    if (!SetEntryIssuers(crl)) crl->flags |= InvalidFlag;
}

bool IsStringTag(unsigned char tag) {
    for (const int candidate : StringTags) {
        if (tag == static_cast<unsigned char>(candidate)) return true;
    }
    return false;
}

bool IsNameValueTag(std::uint64_t number) {
    return std::find(std::begin(NameValueTags), std::end(NameValueTags), number) != std::end(NameValueTags);
}

int CollectString(std::span<const unsigned char>& input, bool indefinite, int depth, std::vector<unsigned char>& output) {
    while (!input.empty()) {
        if (IsEndOfContents(input)) {
            if (!indefinite) return UnexpectedEndOfContents;
            input = input.subspan(2);
            return 0;
        }
        Header header;
        if (const int reason = ReadHeader(input, header)) return reason;
        const std::span<const unsigned char> inner = input.subspan(header.headerLength, header.length);
        if (header.identifier & Constructed) {
            if (depth >= MaxStringNesting) return NestedAsn1String;
            std::span<const unsigned char> rest = inner;
            if (const int reason = CollectString(rest, header.indefinite, depth + 1, output)) return reason;
            input = input.subspan(Consumed(input, rest));
        } else {
            output.insert(output.end(), inner.begin(), inner.end());
            input = input.subspan(header.headerLength + inner.size());
        }
    }
    return indefinite ? MissingEndOfContents : 0;
}

int FindEnd(std::span<const unsigned char>& input) {
    std::uint64_t expected = 1;
    while (!input.empty()) {
        if (IsEndOfContents(input)) {
            input = input.subspan(2);
            if (--expected == 0) return 0;
            continue;
        }
        Header header;
        if (const int reason = ReadHeader(input, header)) return reason;
        if (header.indefinite) {
            ++expected;
            input = input.subspan(header.headerLength);
        } else {
            input = input.subspan(header.headerLength + header.length);
        }
    }
    return MissingEndOfContents;
}

int ReadPrimitiveContent(std::span<const unsigned char>& input, const Header& header, std::vector<unsigned char>& collected,
    std::span<const unsigned char>& content) {
    std::span<const unsigned char> rest = input.subspan(header.headerLength, header.length);
    if (header.identifier & Constructed) {
        if (const int reason = CollectString(rest, header.indefinite, 0, collected)) return reason;
        content = collected;
    } else {
        content = rest;
        rest = rest.subspan(rest.size());
    }
    input = input.subspan(Consumed(input, rest));
    return 0;
}

Asn1String* NewEnumerated(std::span<const unsigned char> content) {
    int type = EnumeratedType;
    std::vector<unsigned char> magnitude;
    if (!content.empty() && !(content.front() & 0x80)) {
        if (content.front() == 0 && content.size() != 1) content = content.subspan(1);
        magnitude.assign(content.begin(), content.end());
    } else if (!content.empty()) {
        type |= NegativeType;
        if (content.front() == 0xff && content.size() != 1) content = content.subspan(1);
        magnitude.assign(content.size(), 0);
        std::size_t index = content.size();
        while (index != 0 && content[index - 1] == 0) --index;
        if (index == 0) {
            magnitude.front() = 1;
            magnitude.push_back(0);
        } else {
            magnitude[index - 1] = static_cast<unsigned char>((content[index - 1] ^ 0xff) + 1);
            for (std::size_t position = index - 1; position-- > 0;)
                magnitude[position] = static_cast<unsigned char>(content[position] ^ 0xff);
        }
    }
    return NewString(type, magnitude, MultiStringFlag);
}

Asn1String* DecodeNameValue(std::span<const unsigned char>& input) {
    Header header;
    if (const int reason = ReadHeader(input, header)) {
        PutError(reason);
        return nullptr;
    }
    if (header.identifier & ClassMask) {
        PutError(MstringNotUniversal);
        return nullptr;
    }
    if (!IsNameValueTag(header.number)) {
        PutError(MstringWrongTag);
        return nullptr;
    }
    const int type = static_cast<int>(header.number);
    if (type == SequenceType) {
        if (!(header.identifier & Constructed)) {
            PutError(TypeNotConstructed);
            return nullptr;
        }
        std::span<const unsigned char> rest = input.subspan(header.headerLength, header.length);
        if (header.indefinite) {
            if (const int reason = FindEnd(rest)) {
                PutError(reason);
                return nullptr;
            }
        } else {
            rest = rest.subspan(rest.size());
        }
        const std::size_t size = Consumed(input, rest);
        Asn1String* value = NewString(type, input.first(size), MultiStringFlag);
        if (value) input = input.subspan(size);
        return value;
    }
    std::vector<unsigned char> collected;
    std::span<const unsigned char> content;
    std::span<const unsigned char> rest = input;
    if (const int reason = ReadPrimitiveContent(rest, header, collected, content)) {
        PutError(reason);
        return nullptr;
    }
    Asn1String* value = nullptr;
    if (type == BitStringType) {
        value = NewBitString(content, MultiStringFlag);
    } else if (type == EnumeratedType) {
        value = NewEnumerated(content);
    } else if (type == BmpStringType && content.size() % 2 != 0) {
        PutError(BmpStringWrongLength);
    } else if (type == UniversalStringType && content.size() % 4 != 0) {
        PutError(UniversalStringWrongLength);
    } else {
        value = NewString(type, content, MultiStringFlag);
    }
    if (value) input = rest;
    return value;
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

std::optional<std::vector<unsigned char>> ToUtf8(std::span<const unsigned char> bytes, int type) {
    if (type == Utf8StringType) {
        for (std::size_t index = 0; index < bytes.size();) {
            const std::size_t length = Utf8SequenceLength(bytes.subspan(index));
            if (length == 0) {
                PutError(InvalidUtf8String);
                return std::nullopt;
            }
            index += length;
        }
        return std::vector<unsigned char>(bytes.begin(), bytes.end());
    }
    std::vector<unsigned char> converted;
    if (type == BmpStringType || type == UniversalStringType) {
        const std::size_t width = type == BmpStringType ? 2 : 4;
        for (std::size_t index = 0; index + width <= bytes.size(); index += width) {
            std::uint32_t code = 0;
            for (std::size_t byte = 0; byte < width; ++byte) code = (code << 8) | bytes[index + byte];
            if (code > UnicodeMax || IsSurrogate(code)) {
                PutError(IllegalCharacters);
                return std::nullopt;
            }
            AppendUtf8(converted, code);
        }
        return converted;
    }
    for (const unsigned char character : bytes) AppendUtf8(converted, character);
    return converted;
}

std::optional<std::vector<unsigned char>> CanonicalText(std::span<const unsigned char> data, int type) {
    auto converted = ToUtf8(data, type);
    if (!converted) return std::nullopt;
    const std::vector<unsigned char>& bytes = *converted;
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

void AppendHeader(std::vector<unsigned char>& output, unsigned char identifier, std::uint64_t number, std::size_t length) {
    if (number < TagNumberMask) {
        output.push_back(static_cast<unsigned char>(identifier | number));
    } else {
        output.push_back(static_cast<unsigned char>(identifier | TagNumberMask));
        unsigned char groups[10];
        int count = 0;
        for (std::uint64_t rest = number; rest != 0 || count == 0; rest >>= 7) groups[count++] = static_cast<unsigned char>(rest & 0x7f);
        while (count > 1) output.push_back(static_cast<unsigned char>(groups[--count] | 0x80));
        output.push_back(groups[0]);
    }
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

void AppendElement(std::vector<unsigned char>& output, unsigned char identifier, std::uint64_t number,
    std::span<const unsigned char> content) {
    AppendHeader(output, identifier, number, content.size());
    output.insert(output.end(), content.begin(), content.end());
}

std::optional<std::vector<unsigned char>> CanonicalValue(const Asn1String& value) {
    if (value.length < 0) throw std::invalid_argument("X509_CRL: negative name value length");
    if (value.length > 0 && !value.data) throw std::invalid_argument("X509_CRL: null name value data");
    const std::span<const unsigned char> data(value.data, static_cast<std::size_t>(value.length));
    std::vector<unsigned char> output;
    if (IsCanonicalizedType(value.type)) {
        const auto text = CanonicalText(data, value.type);
        if (!text) return std::nullopt;
        AppendElement(output, 0, Utf8StringType, *text);
    } else if (value.type == SequenceType) {
        output.assign(data.begin(), data.end());
    } else if (value.type == BitStringType) {
        std::vector<unsigned char> content{static_cast<unsigned char>(data.empty() ? 0 : value.flags & BitsLeftMask)};
        content.insert(content.end(), data.begin(), data.end());
        AppendElement(output, 0, BitStringType, content);
    } else if (value.type == EnumeratedType) {
        std::vector<unsigned char> content;
        if (data.empty() || data.front() & 0x80) content.push_back(0);
        content.insert(content.end(), data.begin(), data.end());
        AppendElement(output, 0, EnumeratedType, content);
    } else {
        if (value.type < 0) throw std::invalid_argument("X509_CRL: negative name value type");
        AppendElement(output, 0, static_cast<std::uint64_t>(value.type), data);
    }
    return output;
}

std::optional<std::vector<unsigned char>> CanonicalName(const LibreSslX509Name* name) {
    const auto* stack = reinterpret_cast<const LibreSsl3Stack*>(const_cast<const LibreSslStack*>(name->entries));
    std::vector<std::vector<std::vector<unsigned char>>> groups;
    int set = 0;
    for (int index = 0; index < stack->num; ++index) {
        const auto* entry = static_cast<const LibreSslX509NameEntry*>(static_cast<const void*>(stack->data[index]));
        if (index == 0 || entry->set != set) {
            set = entry->set;
            groups.emplace_back();
        }
        const auto* object = static_cast<const LibreSslAsn1Object*>(entry->object);
        const auto value = CanonicalValue(*static_cast<const Asn1String*>(entry->value));
        if (!value) return std::nullopt;
        std::vector<unsigned char> content;
        AppendElement(content, 0, ObjectType, std::span<const unsigned char>(object->data, static_cast<std::size_t>(object->length)));
        content.insert(content.end(), value->begin(), value->end());
        std::vector<unsigned char> encoded;
        AppendElement(encoded, Constructed, SequenceType, content);
        groups.back().push_back(std::move(encoded));
    }
    std::vector<unsigned char> canonical;
    for (auto& group : groups) {
        std::sort(group.begin(), group.end());
        std::vector<unsigned char> members;
        for (const auto& entry : group) members.insert(members.end(), entry.begin(), entry.end());
        AppendElement(canonical, Constructed, SetType, members);
    }
    return canonical;
}

bool StoreNameEncodings(LibreSslX509Name* name, std::span<const unsigned char> encoded) {
    auto* buffer = static_cast<NameBuffer*>(calloc_nid_postfix(1, sizeof(NameBuffer)));
    if (!buffer) {
        PutError(MallocFailure);
        return false;
    }
    name->bytes = buffer;
    const std::size_t capacity = (encoded.size() + 3) / 3 * 4;
    buffer->data = static_cast<char*>(calloc_nid_postfix(capacity, 1));
    if (!buffer->data) {
        PutError(MallocFailure);
        return false;
    }
    std::memcpy(buffer->data, encoded.data(), encoded.size());
    buffer->length = encoded.size();
    buffer->max = capacity;
    const auto canonical = CanonicalName(name);
    if (!canonical) return false;
    if (canonical->empty()) return true;
    if (canonical->size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        PutError(TooLong);
        return false;
    }
    name->canonEnc = static_cast<unsigned char*>(malloc_nid_postfix(canonical->size()));
    if (!name->canonEnc) {
        PutError(MallocFailure);
        return false;
    }
    std::memcpy(name->canonEnc, canonical->data(), canonical->size());
    name->canonEnclen = static_cast<int>(canonical->size());
    return true;
}

LibreSsl3Asn1Type* DecodeAnyType(const Tlv& value) {
    void* decoded = nullptr;
    if (value.tag == NullTag) {
        if (!value.content.empty()) {
            PutError(NullIsWrongLength);
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
    } else if (value.tag == BmpStringType && value.content.size() % 2 != 0) {
        PutError(BmpStringWrongLength);
    } else if (value.tag == UniversalStringType && value.content.size() % 4 != 0) {
        PutError(UniversalStringWrongLength);
    } else if (IsStringTag(value.tag)) {
        decoded = NewString(value.tag, value.content);
    } else if (value.tag == BooleanTag && value.content.size() != 1) {
        PutError(BooleanWrongLength);
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
        if (IsEndOfContents(body)) {
            PutError(UnexpectedEndOfContents);
            FreeAlgor(algor);
            return nullptr;
        }
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

bool TwoDigits(std::span<const unsigned char>& text, int& value) {
    if (text.size() < 2 || text[0] < '0' || text[0] > '9' || text[1] < '0' || text[1] > '9') return false;
    value = (text[0] - '0') * 10 + (text[1] - '0');
    text = text.subspan(2);
    return true;
}

bool ValidDay(int year, int month, int day) {
    if (day < 1) return false;
    if (month == 2) return day <= ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0 ? 29 : 28);
    return day <= (month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31);
}

bool ValidTime(std::span<const unsigned char> text, bool generalized) {
    int year = 1900;
    int value = 0;
    if (generalized) {
        if (!TwoDigits(text, value)) return false;
        year = value * 100;
    }
    if (!TwoDigits(text, value)) return false;
    year += value;
    if (!generalized && year < 1950) year += 100;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!TwoDigits(text, month) || month < 1 || month > 12) return false;
    if (!TwoDigits(text, day) || !ValidDay(year, month, day)) return false;
    if (!TwoDigits(text, hour) || hour > 23) return false;
    if (!TwoDigits(text, minute) || minute > 59) return false;
    if (!TwoDigits(text, second) || second > 59) return false;
    return text.size() == 1 && text[0] == 'Z';
}

Asn1String* DecodeTime(std::span<const unsigned char>& input) {
    Tlv value;
    if (!ReadTlv(input, value)) return nullptr;
    if (value.tag != UtcTimeTag && value.tag != GeneralizedTimeTag) {
        PutError(WrongTag);
        return nullptr;
    }
    const bool generalized = value.tag == GeneralizedTimeTag;
    if (!ValidTime(value.content, generalized)) {
        PutError(InvalidTimeFormat);
        return nullptr;
    }
    return NewString(generalized ? GeneralizedTimeType : UtcTimeType, value.content);
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
    if (!body.empty() && PeekTag(body) == BooleanTag) {
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

bool IsUniversal(const Header& header, std::uint64_t number) {
    return !(header.identifier & ClassMask) && header.number == number;
}

Asn1Object_nid_no_patch* DecodeNameObject(std::span<const unsigned char>& input) {
    Header header;
    if (const int reason = ReadHeader(input, header)) {
        PutError(reason);
        return nullptr;
    }
    if (!IsUniversal(header, ObjectType)) {
        PutError(WrongTag);
        return nullptr;
    }
    std::vector<unsigned char> collected;
    std::span<const unsigned char> content;
    std::span<const unsigned char> rest = input;
    if (const int reason = ReadPrimitiveContent(rest, header, collected, content)) {
        PutError(reason);
        return nullptr;
    }
    Asn1Object_nid_no_patch* object = NewObject(content);
    if (object) input = rest;
    return object;
}

LibreSslX509NameEntry* DecodeNameEntry(std::span<const unsigned char>& input, int set) {
    Header header;
    if (const int reason = ReadHeader(input, header)) {
        PutError(reason);
        return nullptr;
    }
    if (!IsUniversal(header, SequenceType)) {
        PutError(WrongTag);
        return nullptr;
    }
    if (!(header.identifier & Constructed)) {
        PutError(SequenceNotConstructed);
        return nullptr;
    }
    auto* entry = static_cast<LibreSslX509NameEntry*>(calloc_nid_postfix(1, sizeof(LibreSslX509NameEntry)));
    if (!entry) {
        PutError(MallocFailure);
        return nullptr;
    }
    entry->set = set;
    std::span<const unsigned char> body = input.subspan(header.headerLength, header.length);
    bool expectEnd = header.indefinite;
    int fields = 0;
    for (; fields < 2; ++fields) {
        if (body.empty()) break;
        if (IsEndOfContents(body)) {
            if (!expectEnd) {
                PutError(UnexpectedEndOfContents);
                FreeNameEntry(entry);
                return nullptr;
            }
            body = body.subspan(2);
            expectEnd = false;
            break;
        }
        if (fields == 0) {
            entry->object = DecodeNameObject(body);
            if (!entry->object) {
                FreeNameEntry(entry);
                return nullptr;
            }
        } else {
            entry->value = DecodeNameValue(body);
            if (!entry->value) {
                FreeNameEntry(entry);
                return nullptr;
            }
        }
    }
    if (expectEnd) {
        if (!IsEndOfContents(body)) {
            PutError(MissingEndOfContents);
            FreeNameEntry(entry);
            return nullptr;
        }
        body = body.subspan(2);
    }
    if (!header.indefinite && !body.empty()) {
        PutError(SequenceLengthMismatch);
        FreeNameEntry(entry);
        return nullptr;
    }
    if (fields < 2) {
        PutError(FieldMissing);
        FreeNameEntry(entry);
        return nullptr;
    }
    input = input.subspan(Consumed(input, body));
    return entry;
}

bool DecodeRelativeName(std::span<const unsigned char>& input, int set, LibreSslX509Name* name) {
    Header header;
    if (const int reason = ReadHeader(input, header)) {
        PutError(reason);
        return false;
    }
    if (!IsUniversal(header, SetType)) {
        PutError(WrongTag);
        return false;
    }
    std::span<const unsigned char> body = input.subspan(header.headerLength, header.length);
    bool expectEnd = header.indefinite;
    while (!body.empty()) {
        if (IsEndOfContents(body)) {
            if (!expectEnd) {
                PutError(UnexpectedEndOfContents);
                return false;
            }
            body = body.subspan(2);
            expectEnd = false;
            break;
        }
        LibreSslX509NameEntry* entry = DecodeNameEntry(body, set);
        if (!entry) return false;
        if (!Push(reinterpret_cast<LibreSsl3Stack*>(name->entries), entry)) {
            FreeNameEntry(entry);
            return false;
        }
    }
    if (expectEnd) {
        PutError(MissingEndOfContents);
        return false;
    }
    input = input.subspan(Consumed(input, body));
    return true;
}

LibreSslX509Name* DecodeName(std::span<const unsigned char>& input) {
    if (input.empty()) {
        PutError(FieldMissing);
        return nullptr;
    }
    if (IsEndOfContents(input)) {
        PutError(UnexpectedEndOfContents);
        return nullptr;
    }
    Header header;
    if (const int reason = ReadHeader(input, header)) {
        PutError(reason);
        return nullptr;
    }
    if (!IsUniversal(header, SequenceType)) {
        PutError(WrongTag);
        return nullptr;
    }
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
    std::span<const unsigned char> body = input.subspan(header.headerLength, header.length);
    bool expectEnd = header.indefinite;
    int set = 0;
    while (!body.empty()) {
        if (IsEndOfContents(body)) {
            if (!expectEnd) {
                PutError(UnexpectedEndOfContents);
                FreeName(name);
                return nullptr;
            }
            body = body.subspan(2);
            expectEnd = false;
            break;
        }
        if (!DecodeRelativeName(body, set, name)) {
            FreeName(name);
            return nullptr;
        }
        if (set == std::numeric_limits<int>::max()) {
            PutError(TooLong);
            FreeName(name);
            return nullptr;
        }
        ++set;
    }
    if (expectEnd) {
        PutError(MissingEndOfContents);
        FreeName(name);
        return nullptr;
    }
    const std::size_t consumed = Consumed(input, body);
    if (!StoreNameEncodings(name, input.first(consumed))) {
        FreeName(name);
        return nullptr;
    }
    input = input.subspan(consumed);
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
    if (!body.empty() && PeekTag(body) == SequenceTag) {
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
    if (!body.empty() && PeekTag(body) == IntegerTag) {
        Tlv version;
        if (!Next(body, IntegerTag, version)) return false;
        info->version = NewInteger(version.encoded);
        if (!info->version) return false;
    }
    info->sigAlg = DecodeAlgor(body);
    if (!info->sigAlg) return false;
    info->issuer = DecodeName(body);
    if (!info->issuer) return false;
    info->lastUpdate = DecodeTime(body);
    if (!info->lastUpdate) return false;
    if (!body.empty() && (PeekTag(body) == UtcTimeTag || PeekTag(body) == GeneralizedTimeTag)) {
        info->nextUpdate = DecodeTime(body);
        if (!info->nextUpdate) return false;
    }
    if (!body.empty() && PeekTag(body) == SequenceTag) {
        Tlv revoked;
        if (!Next(body, SequenceTag, revoked)) return false;
        info->revoked = DecodeRevokedList(revoked.content);
        if (!info->revoked) return false;
    }
    if (!body.empty() && PeekTag(body) == CrlExtensionsTag) {
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

void AppendHeader(std::vector<unsigned char>& output, unsigned char tag, std::size_t length) {
    output.push_back(tag);
    if (length < 0x80) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    unsigned char octets[sizeof(length)];
    std::size_t count = 0;
    for (std::size_t remaining = length; remaining != 0; remaining >>= 8)
        octets[count++] = static_cast<unsigned char>(remaining);
    output.push_back(static_cast<unsigned char>(0x80 | count));
    while (count != 0) output.push_back(octets[--count]);
}

void ReparseTlv(std::span<const unsigned char>& input, Tlv& value) {
    if (!ReadTlv(input, value)) throw std::logic_error("d2i_X509_CRL: a decoded signature algorithm does not parse again");
}

std::vector<unsigned char> AlgorithmEncoding(std::span<const unsigned char> encoded) {
    Tlv sequence;
    ReparseTlv(encoded, sequence);
    std::span<const unsigned char> body = sequence.content;
    Tlv object;
    ReparseTlv(body, object);
    std::vector<unsigned char> content;
    AppendHeader(content, ObjectTag, object.content.size());
    content.insert(content.end(), object.content.begin(), object.content.end());
    if (!body.empty()) {
        Tlv parameter;
        ReparseTlv(body, parameter);
        if (parameter.tag == BitStringTag) {
            const unsigned char unusedBits = parameter.content.size() > 1 ? parameter.content.front() : 0;
            AppendHeader(content, BitStringTag, parameter.content.size());
            content.push_back(unusedBits);
            content.insert(content.end(), parameter.content.begin() + 1, parameter.content.end());
            if (parameter.content.size() > 1) content.back() &= static_cast<unsigned char>(0xff << unusedBits);
        } else {
            if (parameter.tag != NullTag && parameter.tag != IntegerTag && parameter.tag != OctetStringTag &&
                parameter.tag != ObjectTag && !IsStringTag(parameter.tag))
                NotImplemented_nid_no_patch("d2i_X509_CRL: hashing a signature algorithm with a BOOLEAN parameter");
            AppendHeader(content, parameter.tag, parameter.content.size());
            content.insert(content.end(), parameter.content.begin(), parameter.content.end());
        }
    }
    std::vector<unsigned char> output;
    AppendHeader(output, SequenceTag, content.size());
    output.insert(output.end(), content.begin(), content.end());
    return output;
}

std::vector<unsigned char> SignatureEncoding(const Asn1String& signature) {
    const auto length = static_cast<std::size_t>(signature.length);
    std::vector<unsigned char> output;
    AppendHeader(output, BitStringTag, length + 1);
    output.push_back(length == 0 ? 0 : static_cast<unsigned char>(signature.flags & BitsLeftMask));
    if (length != 0) output.insert(output.end(), signature.data, signature.data + length);
    return output;
}

void HashCrl(LibreSsl3X509Crl* crl, std::span<const unsigned char> infoEncoding, std::span<const unsigned char> algorithmEncoding) {
    const std::vector<unsigned char> algorithm = AlgorithmEncoding(algorithmEncoding);
    const std::vector<unsigned char> signature = SignatureEncoding(*crl->signature);
    std::vector<unsigned char> header;
    AppendHeader(header, SequenceTag, infoEncoding.size() + algorithm.size() + signature.size());
    Sha512Context context;
    if (!SHA512_Init_nid_postfix(&context)) throw std::runtime_error("d2i_X509_CRL: SHA-512 initialization failed");
    for (const std::span<const unsigned char> part : {std::span<const unsigned char>(header), infoEncoding,
             std::span<const unsigned char>(algorithm), std::span<const unsigned char>(signature)}) {
        if (!SHA512_Update_nid_postfix(&context, part.data(), part.size()))
            throw std::runtime_error("d2i_X509_CRL: SHA-512 update failed");
    }
    if (!SHA512_Final_nid_postfix(crl->hash, &context)) throw std::runtime_error("d2i_X509_CRL: SHA-512 finalization failed");
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
    crl->idpReasons = AllCrlReasons;
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
    const std::span<const unsigned char> algorithmStart = body;
    crl->sigAlg = DecodeAlgor(body);
    if (!crl->sigAlg) {
        FreeCrl(crl);
        return nullptr;
    }
    const std::span<const unsigned char> algorithmEncoding = algorithmStart.first(algorithmStart.size() - body.size());
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
    try {
        HashCrl(crl, tbs.encoded, algorithmEncoding);
        CacheExtensions(crl);
    } catch (...) {
        FreeCrl(crl);
        throw;
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

std::span<const unsigned char> CanonicalEncoding(const LibreSslX509Name* name) {
    if (!name) throw std::invalid_argument("X509_CRL_get0_by_cert: null name");
    if (name->modified) NotImplemented_nid_no_patch("X509_CRL_get0_by_cert: re-encoding a modified X509_NAME");
    if (name->canonEnclen < 0) throw std::invalid_argument("X509_CRL_get0_by_cert: negative canonical length");
    if (!name->canonEnc && name->canonEnclen != 0) throw std::invalid_argument("X509_CRL_get0_by_cert: null canonical encoding");
    return {name->canonEnc, static_cast<std::size_t>(name->canonEnclen)};
}

bool SameName(const LibreSslX509Name* first, const LibreSslX509Name* second) {
    const std::span<const unsigned char> left = CanonicalEncoding(first);
    const std::span<const unsigned char> right = CanonicalEncoding(second);
    return std::equal(left.begin(), left.end(), right.begin(), right.end());
}

bool IssuerMatches(const LibreSslX509Name* crlIssuer, const LibreSslX509Name* certificateIssuer,
    const LibreSsl3Revoked* revoked) {
    if (revoked->issuer) NotImplemented_nid_no_patch("X509_CRL_get0_by_cert");
    if (!certificateIssuer) return true;
    return SameName(certificateIssuer, crlIssuer);
}

bool NamesCertificateIssuer(const LibreSsl3Revoked* revoked) {
    const LibreSslAsn1Object* object = LibreSsl3::ObjectByNid(CertificateIssuerNid);
    if (!object) throw std::logic_error("X509_CRL_get0_by_cert: no certificateIssuer object");
    return FindExtension(revoked->extensions, object, -1) >= 0;
}

}

LibreSslX509Name* LibreSsl3DecodeX509Name_nid_no_patch(std::span<const unsigned char>& input) {
    return DecodeName(input);
}

extern "C" {

LibreSsl3X509Crl* APS5_VABI d2i_X509_CRL_nid_postfix(
    LibreSsl3X509Crl** output, const unsigned char** input, std::int64_t length) {
    if (!input || !*input || length < 0) {
        PutError(BadObjectHeader);
        return nullptr;
    }
    if (output) {
        X509_CRL_free_nid_postfix(*output);
        *output = nullptr;
    }
    std::span<const unsigned char> remaining(*input, static_cast<std::size_t>(length));
    LibreSsl3X509Crl* crl = DecodeCrl(remaining);
    if (!crl) return nullptr;
    if (output) *output = crl;
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
    bool entryIssuers = false;
    for (int index = 0; index < entries->num; ++index) {
        if (!entries->data) throw std::invalid_argument(std::string(__func__) + ": null revoked array");
        auto* entry = static_cast<LibreSsl3Revoked*>(static_cast<void*>(entries->data[index]));
        if (!entry) throw std::invalid_argument(std::string(__func__) + ": null revoked entry");
        if (NamesCertificateIssuer(entry)) entryIssuers = true;
        if (!SameString(entry->serialNumber, serial)) continue;
        if (entryIssuers) NotImplemented_nid_no_patch("X509_CRL_get0_by_cert: certificateIssuer entry extension");
        if (!IssuerMatches(crl->crl->issuer, issuer, entry)) continue;
        if (revoked) *revoked = entry;
        return entry->reason == CrlReasonRemoveFromCrl ? 2 : 1;
    }
    return 0;
}

}
