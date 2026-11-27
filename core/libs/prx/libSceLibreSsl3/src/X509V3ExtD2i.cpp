#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int NestedAsn1Error = 58;
constexpr int MallocFailure = 65;
constexpr int BadObjectHeader = 102;
constexpr int InvalidTimeFormat = 132;
constexpr int StringTooShort = 152;
constexpr int SequenceLengthMismatch = 148;
constexpr int SequenceNotConstructed = 149;
constexpr int TooLong = 155;
constexpr int WrongTag = 168;
constexpr int InvalidObjectEncoding = 216;
constexpr int InvalidBitStringBitsLeft = 220;
constexpr int TypeNotPrimitive = 231;
constexpr int TypeNotConstructed = 156;
constexpr int NullIsWrongLength = 144;

constexpr std::uint32_t IntegerTag = 2;
constexpr std::uint32_t BitStringTag = 3;
constexpr std::uint32_t OctetStringTag = 4;
constexpr std::uint32_t NullTag = 5;
constexpr std::uint32_t ObjectTag = 6;
constexpr std::uint32_t EnumeratedTag = 10;
constexpr std::uint32_t SequenceTag = 16;
constexpr std::uint32_t Ia5StringTag = 22;
constexpr std::uint32_t GeneralizedTimeTag = 24;

constexpr std::int32_t BitStringType = 3;
constexpr std::int32_t OctetStringType = 4;
constexpr std::int32_t EnumeratedType = 10;
constexpr std::int32_t Ia5StringType = 22;
constexpr std::int32_t GeneralizedTimeType = 24;
constexpr std::int32_t NegativeType = 0x100;
constexpr std::int64_t BitsLeftFlag = 0x08;
constexpr int ObjectDynamic = 0x01;
constexpr int ObjectDynamicData = 0x08;
constexpr std::uint8_t ContextClass = 2;

enum class Decoder {
    Unsupported,
    BitString,
    OctetString,
    Integer,
    Enumerated,
    Ia5String,
    BasicConstraints,
    ExtendedKeyUsage,
    UsagePeriod,
    GeneralizedTime,
    Object,
    OcspNonce,
    AuthorityKeyId,
    GeneralNames,
    Null,
};

struct Method {
    int nid;
    Decoder decoder;
};

constexpr Method Methods[] = {
    {71, Decoder::BitString},
    {72, Decoder::Ia5String},
    {73, Decoder::Ia5String},
    {74, Decoder::Ia5String},
    {75, Decoder::Ia5String},
    {76, Decoder::Ia5String},
    {77, Decoder::Ia5String},
    {78, Decoder::Ia5String},
    {82, Decoder::OctetString},
    {83, Decoder::BitString},
    {84, Decoder::UsagePeriod},
    {85, Decoder::GeneralNames},
    {86, Decoder::GeneralNames},
    {87, Decoder::BasicConstraints},
    {88, Decoder::Integer},
    {89, Decoder::Unsupported},
    {90, Decoder::AuthorityKeyId},
    {103, Decoder::Unsupported},
    {126, Decoder::ExtendedKeyUsage},
    {140, Decoder::Integer},
    {141, Decoder::Enumerated},
    {142, Decoder::GeneralizedTime},
    {177, Decoder::Unsupported},
    {290, Decoder::Unsupported},
    {291, Decoder::Unsupported},
    {366, Decoder::OcspNonce},
    {367, Decoder::Unsupported},
    {368, Decoder::Unsupported},
    {370, Decoder::GeneralizedTime},
    {371, Decoder::Unsupported},
    {398, Decoder::Unsupported},
    {401, Decoder::Unsupported},
    {430, Decoder::Object},
    {666, Decoder::Unsupported},
    {747, Decoder::Unsupported},
    {748, Decoder::Integer},
    {770, Decoder::Unsupported},
    {771, Decoder::Unsupported},
    {857, Decoder::Unsupported},
    {1018, Decoder::Unsupported},
    {1019, Decoder::Null},
    {1021, Decoder::Unsupported},
};

static_assert(sizeof(Methods) / sizeof(Methods[0]) == 42);

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

void PutHeaderError(int reason) {
    PutError(reason);
    PutError(NestedAsn1Error);
}

struct Header {
    std::uint32_t tag = 0;
    std::uint8_t tagClass = 0;
    bool constructed = false;
    bool indefinite = false;
    std::size_t length = 0;
};

bool ReadIdentifier(std::span<const unsigned char>& input, Header& header) {
    if (input.empty()) return false;
    const unsigned char identifier = input.front();
    input = input.subspan(1);
    header.tagClass = static_cast<std::uint8_t>(identifier >> 6);
    header.constructed = (identifier & 0x20) != 0;
    header.tag = identifier & 0x1f;
    if (header.tag != 0x1f) return true;
    header.tag = 0;
    for (;;) {
        if (input.empty()) return false;
        const unsigned char byte = input.front();
        input = input.subspan(1);
        if (header.tag > (std::numeric_limits<std::uint32_t>::max() >> 7)) return false;
        header.tag = (header.tag << 7) | (byte & 0x7f);
        if (!(byte & 0x80)) return true;
    }
}

bool ReadLength(std::span<const unsigned char>& input, Header& header) {
    if (input.empty()) return false;
    const unsigned char byte = input.front();
    input = input.subspan(1);
    header.indefinite = byte == 0x80;
    if (byte <= 0x80) {
        header.length = byte;
        return true;
    }
    const unsigned int count = byte & 0x7f;
    if (count == 0 || count > sizeof(std::size_t)) return false;
    header.length = 0;
    for (unsigned int index = 0; index < count; ++index) {
        if (input.empty()) return false;
        const unsigned char next = input.front();
        input = input.subspan(1);
        if (header.length > (std::numeric_limits<std::size_t>::max() >> 8)) return false;
        header.length = (header.length << 8) | next;
    }
    return true;
}

std::optional<std::span<const unsigned char>> PrimitiveContent(std::span<const unsigned char> input, std::uint32_t expectedTag) {
    std::span<const unsigned char> remaining = input;
    Header header;
    if (!ReadIdentifier(remaining, header)) {
        PutHeaderError(BadObjectHeader);
        return std::nullopt;
    }
    if (header.tagClass != 0 || header.tag != expectedTag) {
        PutHeaderError(WrongTag);
        return std::nullopt;
    }
    if (!ReadLength(remaining, header)) {
        PutHeaderError(BadObjectHeader);
        return std::nullopt;
    }
    if (header.indefinite && !header.constructed) {
        PutHeaderError(BadObjectHeader);
        return std::nullopt;
    }
    if (!header.indefinite && header.length > remaining.size()) {
        PutHeaderError(TooLong);
        return std::nullopt;
    }
    if (header.tag > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        PutHeaderError(TooLong);
        return std::nullopt;
    }
    if (header.constructed) {
        PutError(TypeNotPrimitive);
        return std::nullopt;
    }
    return remaining.first(header.length);
}

Asn1String* NewString(std::int32_t type, std::span<const unsigned char> content) {
    if (content.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) return nullptr;
    auto* value = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!value) {
        PutError(MallocFailure);
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
    auto* value = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!value) {
        PutError(MallocFailure);
        PutError(MallocFailure);
        return nullptr;
    }
    const std::size_t length = content.size() - 1;
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
    *value = {static_cast<std::int32_t>(length), BitStringType, data,
              static_cast<std::int64_t>(BitsLeftFlag | unusedBits)};
    return value;
}

void* NewInteger(std::span<const unsigned char> value) {
    if (value.empty()) {
        PutHeaderError(BadObjectHeader);
        return nullptr;
    }
    const unsigned char* cursor = value.data();
    return d2i_ASN1_INTEGER_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(value.size()));
}

void AppendLength(std::vector<unsigned char>& encoded, std::size_t length) {
    if (length < 0x80) {
        encoded.push_back(static_cast<unsigned char>(length));
        return;
    }
    unsigned char bytes[sizeof(std::size_t)];
    unsigned int count = 0;
    for (std::size_t remaining = length; remaining != 0; remaining >>= 8) bytes[count++] = remaining & 0xff;
    encoded.push_back(static_cast<unsigned char>(0x80 | count));
    while (count != 0) encoded.push_back(bytes[--count]);
}

Asn1String* NewImplicitInteger(std::span<const unsigned char> content) {
    std::vector<unsigned char> encoded;
    encoded.reserve(content.size() + 5);
    encoded.push_back(static_cast<unsigned char>(IntegerTag));
    AppendLength(encoded, content.size());
    encoded.insert(encoded.end(), content.begin(), content.end());
    const unsigned char* cursor = encoded.data();
    return static_cast<Asn1String*>(
        d2i_ASN1_INTEGER_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(encoded.size())));
}

void* NewEnumerated(std::span<const unsigned char> content) {
    auto* value = NewImplicitInteger(content);
    if (!value) return nullptr;
    value->type = static_cast<std::int32_t>((value->type & NegativeType) | EnumeratedType);
    return value;
}

int DigitPair(std::span<const unsigned char> content, std::size_t offset) {
    const unsigned char high = content[offset];
    const unsigned char low = content[offset + 1];
    if (high < '0' || high > '9' || low < '0' || low > '9') return -1;
    return (high - '0') * 10 + (low - '0');
}

int DaysInMonth(int year, int month) {
    constexpr int Days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month != 2) return Days[month - 1];
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return leap ? 29 : 28;
}

bool IsGeneralizedTime(std::span<const unsigned char> content) {
    if (content.size() != 15 || content[14] != 'Z') return false;
    const int yearHigh = DigitPair(content, 0);
    const int yearLow = DigitPair(content, 2);
    const int month = DigitPair(content, 4);
    const int day = DigitPair(content, 6);
    const int hour = DigitPair(content, 8);
    const int minute = DigitPair(content, 10);
    const int second = DigitPair(content, 12);
    if (yearHigh < 0 || yearLow < 0 || month < 1 || month > 12) return false;
    if (day < 1 || day > DaysInMonth(yearHigh * 100 + yearLow, month)) return false;
    return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59;
}

void* NewGeneralizedTime(std::span<const unsigned char> content) {
    if (!IsGeneralizedTime(content)) {
        PutError(InvalidTimeFormat);
        return nullptr;
    }
    return NewString(GeneralizedTimeType, content);
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

void* NewObject(std::span<const unsigned char> content) {
    if (!IsObjectEncoding(content)) {
        PutError(InvalidObjectEncoding);
        return nullptr;
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(content.size()));
    if (!data) return nullptr;
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

void* NewNull(std::span<const unsigned char> value) {
    Header header;
    if (!ReadIdentifier(value, header)) {
        PutHeaderError(BadObjectHeader);
        return nullptr;
    }
    if (header.tagClass != 0 || header.tag != NullTag) {
        PutHeaderError(WrongTag);
        return nullptr;
    }
    if (!ReadLength(value, header) || (header.indefinite && !header.constructed)) {
        PutHeaderError(BadObjectHeader);
        return nullptr;
    }
    if (!header.indefinite && header.length > value.size()) {
        PutHeaderError(TooLong);
        return nullptr;
    }
    if (header.constructed) {
        PutError(TypeNotPrimitive);
        return nullptr;
    }
    if (header.length != 0) {
        PutError(NullIsWrongLength);
        return nullptr;
    }
    return reinterpret_cast<void*>(std::uintptr_t{1});
}

struct AuthorityKeyId {
    Asn1String* keyId;
    LibreSslStack* issuer;
    Asn1String* serial;
};

static_assert(sizeof(AuthorityKeyId) == 24 && offsetof(AuthorityKeyId, serial) == 16);

void FreeAuthorityKeyId(AuthorityKeyId* id) {
    ASN1_OCTET_STRING_free_nid_postfix(id->keyId);
    GENERAL_NAMES_free_nid_postfix(id->issuer);
    ASN1_INTEGER_free_nid_postfix(id->serial);
    free_nid_postfix(id);
}

bool ReadElement(std::span<const unsigned char>& input, Header& header, std::span<const unsigned char>& content) {
    if (!ReadIdentifier(input, header) || !ReadLength(input, header) || (header.indefinite && !header.constructed)) {
        PutHeaderError(BadObjectHeader);
        return false;
    }
    if (header.indefinite) NotImplemented_nid_no_patch("AUTHORITY_KEYID indefinite length");
    if (header.length > input.size()) {
        PutHeaderError(TooLong);
        return false;
    }
    content = input.first(header.length);
    input = input.subspan(header.length);
    return true;
}

LibreSslStack* NewGeneralNames(std::span<const unsigned char> content);

LibreSslStack* DecodeIssuerNames(const Header& field, std::span<const unsigned char> content) {
    if (!field.constructed) {
        PutHeaderError(TypeNotConstructed);
        return nullptr;
    }
    return NewGeneralNames(content);
}

void* NewAuthorityKeyId(std::span<const unsigned char> value) {
    Header header;
    std::span<const unsigned char> body;
    if (!ReadElement(value, header, body)) return nullptr;
    if (header.tagClass != 0 || header.tag != SequenceTag) {
        PutHeaderError(WrongTag);
        return nullptr;
    }
    if (!header.constructed) {
        PutError(SequenceNotConstructed);
        return nullptr;
    }
    auto* allocated = static_cast<AuthorityKeyId*>(calloc_nid_postfix(1, sizeof(AuthorityKeyId)));
    if (!allocated) {
        PutError(MallocFailure);
        return nullptr;
    }
    std::unique_ptr<AuthorityKeyId, void (*)(AuthorityKeyId*)> id(allocated, FreeAuthorityKeyId);
    std::uint32_t next = 0;
    while (!body.empty()) {
        if (next > 2) {
            PutError(SequenceLengthMismatch);
            return nullptr;
        }
        std::span<const unsigned char> content;
        if (!ReadElement(body, header, content)) return nullptr;
        const bool context = header.tagClass == ContextClass;
        if (context && header.tag == 0 && next == 0) {
            if (header.constructed) NotImplemented_nid_no_patch("AUTHORITY_KEYID constructed keyid");
            id->keyId = NewString(OctetStringType, content);
            if (!id->keyId) return nullptr;
            next = 1;
        } else if (context && header.tag == 1 && next <= 1) {
            id->issuer = DecodeIssuerNames(header, content);
            if (!id->issuer) return nullptr;
            next = 2;
        } else if (context && header.tag == 2) {
            if (header.constructed) {
                PutError(TypeNotPrimitive);
                return nullptr;
            }
            id->serial = NewImplicitInteger(content);
            if (!id->serial) return nullptr;
            next = 3;
        } else {
            PutHeaderError(WrongTag);
            return nullptr;
        }
    }
    return id.release();
}

constexpr int BooleanIsWrongLength = 106;
constexpr int EndOfContentsInDefiniteForm = 159;
constexpr std::uint32_t BooleanTag = 1;

enum class TagCheck {
    Absent,
    Failed,
    Present,
};

TagCheck CheckTag(std::span<const unsigned char>& cursor, std::uint32_t tag, std::uint8_t tagClass, bool optional,
    Header& header) {
    std::span<const unsigned char> input = cursor;
    if (!ReadIdentifier(input, header)) {
        PutError(BadObjectHeader);
        return TagCheck::Failed;
    }
    if (header.tag != tag || header.tagClass != tagClass) {
        if (optional) return TagCheck::Absent;
        PutError(WrongTag);
        return TagCheck::Failed;
    }
    if (!ReadLength(input, header) || (header.indefinite && !header.constructed)) {
        PutError(BadObjectHeader);
        return TagCheck::Failed;
    }
    if (!header.indefinite && header.length > input.size()) {
        PutError(TooLong);
        return TagCheck::Failed;
    }
    cursor = input;
    return TagCheck::Present;
}

bool SequenceBody(std::span<const unsigned char> value, bool sequenceOf, const char* indefiniteForm,
    std::span<const unsigned char>& body) {
    Header header;
    if (CheckTag(value, SequenceTag, 0, false, header) != TagCheck::Present) {
        PutError(NestedAsn1Error);
        return false;
    }
    if (!sequenceOf && !header.constructed) {
        PutError(SequenceNotConstructed);
        return false;
    }
    if (header.indefinite) NotImplemented_nid_no_patch(indefiniteForm);
    body = value.first(header.length);
    return true;
}

bool EndsDefiniteContent(std::span<const unsigned char> body) {
    if (body.size() < 2 || body[0] != 0 || body[1] != 0) return false;
    PutError(EndOfContentsInDefiniteForm);
    return true;
}

constexpr int NoMatchingChoiceType = 143;
constexpr int ExplicitTagNotConstructed = 120;
constexpr std::uint32_t OtherNameTag = 0;
constexpr std::uint32_t EmailTag = 1;
constexpr std::uint32_t DnsNameTag = 2;
constexpr std::uint32_t X400Tag = 3;
constexpr std::uint32_t DirectoryNameTag = 4;
constexpr std::uint32_t EdiPartyNameTag = 5;
constexpr std::uint32_t UriTag = 6;
constexpr std::uint32_t IpAddressTag = 7;
constexpr std::uint32_t RegisteredIdTag = 8;

void PutNestedErrors(int count) {
    for (int index = 0; index < count; ++index) PutError(NestedAsn1Error);
}

bool DecodeDirectoryName(std::span<const unsigned char> content, LibreSslGeneralName& name) {
    std::span<const unsigned char> body;
    if (!SequenceBody(content, true, "GENERAL_NAME directoryName indefinite length", body)) {
        PutNestedErrors(3);
        return false;
    }
    std::span<const unsigned char> remaining = content;
    name.value = LibreSsl3DecodeX509Name_nid_no_patch(remaining);
    if (!name.value) {
        PutNestedErrors(3);
        return false;
    }
    if (!remaining.empty()) {
        PutError(SequenceLengthMismatch);
        PutNestedErrors(1);
        return false;
    }
    return true;
}

LibreSslGeneralName* NewGeneralName(std::span<const unsigned char>& input) {
    std::unique_ptr<LibreSslGeneralName, decltype(&GENERAL_NAME_free_nid_postfix)> name(
        static_cast<LibreSslGeneralName*>(calloc_nid_postfix(1, sizeof(LibreSslGeneralName))),
        GENERAL_NAME_free_nid_postfix);
    if (!name) {
        PutError(MallocFailure);
        PutNestedErrors(1);
        return nullptr;
    }
    const std::span<const unsigned char> element = input;
    Header header;
    if (!ReadIdentifier(input, header)) {
        PutError(BadObjectHeader);
        PutNestedErrors(3);
        return nullptr;
    }
    if (header.tagClass != ContextClass || header.tag > RegisteredIdTag) {
        PutError(NoMatchingChoiceType);
        return nullptr;
    }
    const int headerNesting = header.tag == DirectoryNameTag ? 2 : 3;
    if (!ReadLength(input, header) || (header.indefinite && !header.constructed)) {
        PutError(BadObjectHeader);
        PutNestedErrors(headerNesting);
        return nullptr;
    }
    if (!header.indefinite && header.length > input.size()) {
        PutError(TooLong);
        PutNestedErrors(headerNesting);
        return nullptr;
    }
    const std::size_t headerSize = element.size() - input.size();
    name->type = static_cast<int>(header.tag);
    switch (header.tag) {
    case OtherNameTag:
    case EdiPartyNameTag:
        if (!header.constructed) {
            PutError(SequenceNotConstructed);
            PutNestedErrors(2);
            return nullptr;
        }
        NotImplemented_nid_no_patch("GENERAL_NAME otherName and ediPartyName");
    case X400Tag:
        if (!header.constructed) {
            PutError(TypeNotConstructed);
            PutNestedErrors(2);
            return nullptr;
        }
        break;
    case DirectoryNameTag:
        if (!header.constructed) {
            PutError(ExplicitTagNotConstructed);
            PutNestedErrors(1);
            return nullptr;
        }
        break;
    case RegisteredIdTag:
        if (header.constructed) {
            PutError(TypeNotPrimitive);
            PutNestedErrors(2);
            return nullptr;
        }
        break;
    default:
        if (header.constructed) NotImplemented_nid_no_patch("GENERAL_NAME constructed string");
        break;
    }
    if (header.indefinite) NotImplemented_nid_no_patch("GENERAL_NAME indefinite length");
    const std::span<const unsigned char> content = input.first(header.length);
    input = input.subspan(header.length);
    switch (header.tag) {
    case EmailTag:
    case DnsNameTag:
    case UriTag:
        name->value = NewString(Ia5StringType, content);
        break;
    case IpAddressTag:
        name->value = NewString(OctetStringType, content);
        break;
    case X400Tag:
        name->value = NewString(static_cast<std::int32_t>(SequenceTag), element.first(headerSize + header.length));
        break;
    case DirectoryNameTag:
        if (!DecodeDirectoryName(content, *name)) return nullptr;
        return name.release();
    case RegisteredIdTag:
        name->value = NewObject(content);
        break;
    }
    if (!name->value) {
        PutNestedErrors(2);
        return nullptr;
    }
    return name.release();
}

LibreSslStack* NewGeneralNames(std::span<const unsigned char> content) {
    std::unique_ptr<LibreSslStack, decltype(&GENERAL_NAMES_free_nid_postfix)> names(
        sk_new_nid_postfix(nullptr), GENERAL_NAMES_free_nid_postfix);
    if (!names) {
        PutError(MallocFailure);
        return nullptr;
    }
    while (!content.empty()) {
        if (EndsDefiniteContent(content)) return nullptr;
        LibreSslGeneralName* name = NewGeneralName(content);
        if (!name) {
            PutNestedErrors(1);
            return nullptr;
        }
        if (!sk_push_nid_postfix(names.get(), name)) {
            GENERAL_NAME_free_nid_postfix(name);
            PutError(MallocFailure);
            return nullptr;
        }
    }
    return names.release();
}

void* NewGeneralNamesExtension(std::span<const unsigned char> value) {
    std::span<const unsigned char> body;
    if (!SequenceBody(value, true, "GENERAL_NAMES indefinite length", body)) return nullptr;
    return NewGeneralNames(body);
}

struct BasicConstraintsValue {
    int ca;
    Asn1String* pathLength;
};

static_assert(sizeof(BasicConstraintsValue) == 16 && offsetof(BasicConstraintsValue, pathLength) == 8);

void FreeBasicConstraints(BasicConstraintsValue* constraints) {
    ASN1_INTEGER_free_nid_postfix(constraints->pathLength);
    free_nid_postfix(constraints);
}

void* NewBasicConstraints(std::span<const unsigned char> value) {
    std::span<const unsigned char> body;
    if (!SequenceBody(value, false, "BASIC_CONSTRAINTS indefinite length", body)) return nullptr;
    auto* allocated = static_cast<BasicConstraintsValue*>(calloc_nid_postfix(1, sizeof(BasicConstraintsValue)));
    if (!allocated) {
        PutHeaderError(MallocFailure);
        return nullptr;
    }
    std::unique_ptr<BasicConstraintsValue, void (*)(BasicConstraintsValue*)> constraints(allocated, FreeBasicConstraints);
    if (body.empty()) return constraints.release();
    Header header;
    switch (CheckTag(body, BooleanTag, 0, true, header)) {
    case TagCheck::Failed:
        PutError(NestedAsn1Error);
        PutError(NestedAsn1Error);
        return nullptr;
    case TagCheck::Present:
        if (header.constructed) {
            PutHeaderError(TypeNotPrimitive);
            return nullptr;
        }
        if (header.length != 1) {
            PutHeaderError(BooleanIsWrongLength);
            return nullptr;
        }
        constraints->ca = body.front();
        body = body.subspan(1);
        break;
    case TagCheck::Absent:
        break;
    }
    if (EndsDefiniteContent(body)) return nullptr;
    if (body.empty()) return constraints.release();
    const unsigned char* cursor = body.data();
    constraints->pathLength = static_cast<Asn1String*>(
        d2i_ASN1_INTEGER_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(body.size())));
    if (!constraints->pathLength) {
        PutError(NestedAsn1Error);
        return nullptr;
    }
    if (cursor != body.data() + body.size()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return constraints.release();
}

void FreeObjectStack(LibreSslStack* objects) {
    for (int index = 0; index < objects->num; ++index)
        ASN1_OBJECT_free_nid_postfix(reinterpret_cast<Asn1Object_nid_no_patch*>(objects->data[index]));
    sk_free_nid_postfix(objects);
}

void* NewExtendedKeyUsage(std::span<const unsigned char> value) {
    std::span<const unsigned char> body;
    if (!SequenceBody(value, true, "EXTENDED_KEY_USAGE indefinite length", body)) return nullptr;
    LibreSslStack* allocated = sk_new_nid_postfix(nullptr);
    if (!allocated) {
        PutError(MallocFailure);
        return nullptr;
    }
    std::unique_ptr<LibreSslStack, void (*)(LibreSslStack*)> usages(allocated, FreeObjectStack);
    while (!body.empty()) {
        if (EndsDefiniteContent(body)) return nullptr;
        Header header;
        if (CheckTag(body, ObjectTag, 0, false, header) != TagCheck::Present) {
            PutError(NestedAsn1Error);
            PutError(NestedAsn1Error);
            return nullptr;
        }
        if (header.constructed) {
            PutHeaderError(TypeNotPrimitive);
            return nullptr;
        }
        void* object = NewObject(body.first(header.length));
        if (!object) {
            PutError(NestedAsn1Error);
            return nullptr;
        }
        body = body.subspan(header.length);
        if (sk_push_nid_postfix(usages.get(), object) == 0) {
            ASN1_OBJECT_free_nid_postfix(static_cast<Asn1Object_nid_no_patch*>(object));
            PutError(MallocFailure);
            return nullptr;
        }
    }
    return usages.release();
}

struct UsagePeriodValue {
    Asn1String* notBefore;
    Asn1String* notAfter;
};

static_assert(sizeof(UsagePeriodValue) == 16 && offsetof(UsagePeriodValue, notAfter) == 8);

void FreeUsagePeriod(UsagePeriodValue* period) {
    ASN1_OCTET_STRING_free_nid_postfix(period->notBefore);
    ASN1_OCTET_STRING_free_nid_postfix(period->notAfter);
    free_nid_postfix(period);
}

bool ReadUsageTime(std::span<const unsigned char>& body, std::uint32_t tag, bool optional, Asn1String*& time) {
    Header header;
    switch (CheckTag(body, tag, ContextClass, optional, header)) {
    case TagCheck::Absent:
        return true;
    case TagCheck::Failed:
        PutError(NestedAsn1Error);
        PutError(NestedAsn1Error);
        return false;
    case TagCheck::Present:
        break;
    }
    if (header.constructed) NotImplemented_nid_no_patch("PKEY_USAGE_PERIOD constructed time");
    time = static_cast<Asn1String*>(NewGeneralizedTime(body.first(header.length)));
    if (!time) {
        PutError(NestedAsn1Error);
        return false;
    }
    body = body.subspan(header.length);
    return true;
}

void* NewUsagePeriod(std::span<const unsigned char> value) {
    std::span<const unsigned char> body;
    if (!SequenceBody(value, false, "PKEY_USAGE_PERIOD indefinite length", body)) return nullptr;
    auto* allocated = static_cast<UsagePeriodValue*>(calloc_nid_postfix(1, sizeof(UsagePeriodValue)));
    if (!allocated) {
        PutHeaderError(MallocFailure);
        return nullptr;
    }
    std::unique_ptr<UsagePeriodValue, void (*)(UsagePeriodValue*)> period(allocated, FreeUsagePeriod);
    if (body.empty()) return period.release();
    if (!ReadUsageTime(body, 0, true, period->notBefore)) return nullptr;
    if (EndsDefiniteContent(body)) return nullptr;
    if (body.empty()) return period.release();
    if (!ReadUsageTime(body, 1, false, period->notAfter)) return nullptr;
    if (!body.empty()) {
        PutError(SequenceLengthMismatch);
        return nullptr;
    }
    return period.release();
}

void* Decode(const LibreSsl3X509Extension& extension, Decoder decoder) {
    const auto* encoded = static_cast<const Asn1String*>(extension.value);
    if (!encoded) throw std::invalid_argument(std::string(__func__) + ": null extension value");
    if (encoded->length < 0) return nullptr;
    if (encoded->length > 0 && !encoded->data)
        throw std::invalid_argument(std::string(__func__) + ": null extension value data");
    const std::span<const unsigned char> value(encoded->data, static_cast<std::size_t>(encoded->length));
    switch (decoder) {
    case Decoder::OcspNonce:
        return NewString(OctetStringType, value);
    case Decoder::BitString: {
        const auto content = PrimitiveContent(value, BitStringTag);
        return content ? NewBitString(*content) : nullptr;
    }
    case Decoder::OctetString: {
        const auto content = PrimitiveContent(value, OctetStringTag);
        return content ? NewString(OctetStringType, *content) : nullptr;
    }
    case Decoder::Integer:
        return NewInteger(value);
    case Decoder::Enumerated: {
        const auto content = PrimitiveContent(value, EnumeratedTag);
        return content ? NewEnumerated(*content) : nullptr;
    }
    case Decoder::Ia5String: {
        const auto content = PrimitiveContent(value, Ia5StringTag);
        return content ? NewString(Ia5StringType, *content) : nullptr;
    }
    case Decoder::GeneralizedTime: {
        const auto content = PrimitiveContent(value, GeneralizedTimeTag);
        return content ? NewGeneralizedTime(*content) : nullptr;
    }
    case Decoder::Object: {
        const auto content = PrimitiveContent(value, ObjectTag);
        return content ? NewObject(*content) : nullptr;
    }
    case Decoder::AuthorityKeyId:
        return NewAuthorityKeyId(value);
    case Decoder::GeneralNames:
        return NewGeneralNamesExtension(value);
    case Decoder::Null:
        return NewNull(value);
    case Decoder::Unsupported:
        break;
    case Decoder::BasicConstraints:
        return NewBasicConstraints(value);
    case Decoder::ExtendedKeyUsage:
        return NewExtendedKeyUsage(value);
    case Decoder::UsagePeriod:
        return NewUsagePeriod(value);
    }
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" void* APS5_VABI X509V3_EXT_d2i_nid_postfix(LibreSsl3X509Extension* extension) {
    if (!extension) throw std::invalid_argument(std::string(__func__) + ": null extension");
    if (extension->object && extension->object->length > 0 && !extension->object->data)
        throw std::invalid_argument(std::string(__func__) + ": null extension object data");
    const int nid = OBJ_obj2nid_nid_postfix(extension->object);
    if (nid == 0) return nullptr;
    for (const Method& method : Methods) {
        if (method.nid != nid) continue;
        if (method.decoder == Decoder::Unsupported) NotImplemented_nid_no_patch(__func__);
        return Decode(*extension, method.decoder);
    }
    return nullptr;
}
