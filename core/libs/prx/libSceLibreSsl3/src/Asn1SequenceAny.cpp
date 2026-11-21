#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int NestedAsn1Error = 58;
constexpr int LengthMismatch = 62;
constexpr int MallocFailure = 65;
constexpr int BadObjectHeader = 102;
constexpr int BooleanIsWrongLength = 106;
constexpr int InvalidTimeFormat = 132;
constexpr int MissingEoc = 137;
constexpr int NullIsWrongLength = 144;
constexpr int StringTooShort = 152;
constexpr int TooLong = 155;
constexpr int TypeNotConstructed = 156;
constexpr int UnexpectedEoc = 159;
constexpr int WrongTag = 168;
constexpr int NestedAsn1String = 197;
constexpr int BmpStringIsWrongLength = 214;
constexpr int UniversalStringIsWrongLength = 215;
constexpr int InvalidObjectEncoding = 216;
constexpr int InvalidBitStringBitsLeft = 220;
constexpr int TypeNotPrimitive = 231;

constexpr int BooleanType = 1;
constexpr int IntegerType = 2;
constexpr int BitStringType = 3;
constexpr int NullType = 5;
constexpr int ObjectType = 6;
constexpr int RealType = 9;
constexpr int EnumeratedType = 10;
constexpr int RelativeObjectType = 13;
constexpr int TimeType = 14;
constexpr int SequenceType = 16;
constexpr int SetType = 17;
constexpr int UtcTimeType = 23;
constexpr int GeneralizedTimeType = 24;
constexpr int UniversalStringType = 28;
constexpr int BmpStringType = 30;
constexpr int OtherType = -3;
constexpr int UndefinedType = -1;
constexpr int NegativeFlag = 0x100;
constexpr int MaximumStringNesting = 5;
constexpr std::int64_t NoFreeFlag = 0x10;
constexpr std::int64_t BitsLeftFlag = 0x08;
constexpr int DynamicObject = 0x01;
constexpr int DynamicObjectData = 0x08;
constexpr std::size_t MaximumInt = static_cast<std::size_t>(std::numeric_limits<int>::max());

static_assert(sizeof(Asn1String) == 24);
static_assert(offsetof(Asn1String, data) == 8);
static_assert(offsetof(Asn1String, flags) == 16);
static_assert(sizeof(LibreSslStack) == 32);

struct Reader {
    const unsigned char* data;
    std::size_t length;
    std::size_t initial;

    Reader(const unsigned char* bytes, std::size_t size) : data(bytes), length(size), initial(size) {}

    std::size_t Offset() const { return initial - length; }

    bool Skip(std::size_t count) {
        if (count > length) return false;
        if (count != 0) data += count;
        length -= count;
        return true;
    }

    bool Byte(unsigned char& byte) {
        if (length == 0) return false;
        byte = *data;
        return Skip(1);
    }

    bool Take(Reader& output, std::size_t count) {
        if (count > length) return false;
        output = Reader(data, count);
        return Skip(count);
    }
};

struct Header {
    std::size_t length = 0;
    int tag = 0;
    std::uint8_t tagClass = 0;
    bool indefinite = false;
    bool constructed = false;
};

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

bool Fail(int reason) {
    PutError(reason);
    return false;
}

void Cleanse(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
}

void ClearAndFree(void* data, std::size_t size) {
    if (!data) return;
    Cleanse(data, size);
    free_nid_postfix(data);
}

void FreeString(Asn1String* string) {
    if (!string) return;
    if (!(string->flags & NoFreeFlag)) ClearAndFree(string->data, static_cast<std::size_t>(string->length));
    free_nid_postfix(string);
}

bool ReadIdentifier(Reader& input, std::uint8_t& tagClass, bool& constructed, std::uint32_t& tag) {
    unsigned char value;
    if (!input.Byte(value)) return false;
    tagClass = (value >> 6) & 0x3;
    constructed = (value & 0x20) != 0;
    tag = value & 0x1f;
    if (tag != 0x1f) return true;
    tag = 0;
    do {
        if (!input.Byte(value)) return false;
        if (tag > (std::numeric_limits<std::uint32_t>::max() >> 7)) return false;
        tag = (tag << 7) | (value & 0x7f);
    } while (value & 0x80);
    return true;
}

bool ReadLength(Reader& input, bool& indefinite, std::size_t& length) {
    unsigned char value;
    indefinite = false;
    length = 0;
    if (!input.Byte(value)) return false;
    if (value < 0x80) {
        length = value;
        return true;
    }
    if (value == 0x80) {
        indefinite = true;
        return true;
    }
    unsigned int count = value & 0x7f;
    if (count == 0x7f) return false;
    while (count-- > 0) {
        if (!input.Byte(value)) return false;
        if (length > (std::numeric_limits<std::size_t>::max() >> 8)) return false;
        length = (length << 8) | value;
    }
    return true;
}

bool CheckEoc(Reader& input) {
    if (input.length < 2 || input.data[0] != 0 || input.data[1] != 0) return false;
    return input.Skip(2);
}

bool CheckTag(Reader& input, Header& header, int expectedTag, int expectedClass) {
    header = {};
    std::uint8_t tagClass;
    bool constructed;
    std::uint32_t tag;
    if (!ReadIdentifier(input, tagClass, constructed, tag)) return Fail(BadObjectHeader);
    if (expectedTag >= 0 && (static_cast<std::uint32_t>(expectedTag) != tag || expectedClass != tagClass << 6)) return Fail(WrongTag);
    bool indefinite;
    std::size_t length;
    if (!ReadLength(input, indefinite, length)) return Fail(BadObjectHeader);
    if (indefinite && !constructed) return Fail(BadObjectHeader);
    if (!indefinite && input.length < length) return Fail(TooLong);
    if (tag > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) return Fail(TooLong);
    header.length = indefinite ? input.length : length;
    header.tag = static_cast<int>(tag);
    header.tagClass = static_cast<std::uint8_t>(tagClass << 6);
    header.indefinite = indefinite;
    header.constructed = constructed;
    return true;
}

int UniversalType(int type) {
    return type == (IntegerType | NegativeFlag) || type == (EnumeratedType | NegativeFlag) ? type & ~NegativeFlag : type;
}

bool MustBeConstructed(int type) {
    type = UniversalType(type);
    return type == SequenceType || type == SetType;
}

bool MustBePrimitive(int type) {
    switch (UniversalType(type)) {
    case BooleanType:
    case IntegerType:
    case NullType:
    case ObjectType:
    case RealType:
    case EnumeratedType:
    case RelativeObjectType:
    case TimeType:
        return true;
    default:
        return false;
    }
}

void Append(std::vector<unsigned char>& output, const unsigned char* data, std::size_t length) {
    if (output.size() + length > output.capacity()) {
        std::vector<unsigned char> grown;
        grown.reserve(std::max(output.size() + length, output.capacity() * 2));
        grown.assign(output.begin(), output.end());
        Cleanse(output.data(), output.size());
        output.swap(grown);
    }
    output.insert(output.end(), data, data + length);
}

bool Collect(std::vector<unsigned char>& output, Reader& input, bool indefinite, int depth) {
    if (depth > MaximumStringNesting) return Fail(NestedAsn1String);
    const bool needEoc = indefinite;
    while (input.length > 0) {
        if (CheckEoc(input)) return needEoc ? true : Fail(UnexpectedEoc);
        Header header;
        if (!CheckTag(input, header, -1, 0)) return Fail(NestedAsn1Error);
        if (header.constructed) {
            if (!Collect(output, input, header.indefinite, depth + 1)) return false;
            continue;
        }
        Reader content(nullptr, 0);
        if (!input.Take(content, header.length)) return Fail(NestedAsn1Error);
        if (content.length != 0) Append(output, content.data, content.length);
    }
    return needEoc ? Fail(MissingEoc) : true;
}

bool FindEnd(Reader& input, std::size_t length, bool indefinite) {
    if (!indefinite) return input.Skip(length) ? true : Fail(NestedAsn1Error);
    std::size_t eocCount = 1;
    while (input.length > 0) {
        if (CheckEoc(input)) {
            if (--eocCount == 0) break;
            continue;
        }
        Header header;
        if (!CheckTag(input, header, -1, 0)) return Fail(NestedAsn1Error);
        if (header.indefinite) {
            ++eocCount;
            continue;
        }
        if (!input.Skip(header.length)) return false;
    }
    return eocCount > 0 ? Fail(MissingEoc) : true;
}

bool Stow(const Reader& content, unsigned char*& data) {
    data = nullptr;
    if (content.length == 0) return true;
    data = static_cast<unsigned char*>(malloc_nid_postfix(content.length));
    if (!data) return false;
    std::memcpy(data, content.data, content.length);
    return true;
}

Asn1String* NewItemString(int type) {
    auto* string = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!string) {
        PutError(MallocFailure);
        PutError(MallocFailure);
        return nullptr;
    }
    string->type = type;
    return string;
}

bool ConvertObject(void*& value, Reader content) {
    if (content.length == 0) return Fail(InvalidObjectEncoding);
    Reader arcs = content;
    while (arcs.length > 0) {
        std::uint64_t arc = 0;
        unsigned char byte;
        do {
            if (!arcs.Byte(byte)) return Fail(InvalidObjectEncoding);
            if (arc == 0 && byte == 0x80) return Fail(InvalidObjectEncoding);
            arc = (arc << 7) | (byte & 0x7f);
        } while (byte & 0x80);
    }
    unsigned char* data;
    if (!Stow(content, data)) return false;
    auto* object = static_cast<Asn1Object_nid_no_patch*>(calloc_nid_postfix(1, sizeof(Asn1Object_nid_no_patch)));
    if (!object) {
        PutError(MallocFailure);
        free_nid_postfix(data);
        return false;
    }
    object->data = data;
    object->length = static_cast<int>(content.length);
    object->flags = DynamicObject | DynamicObjectData;
    value = object;
    return true;
}

bool ConvertBitString(void*& value, Reader content) {
    unsigned char unusedBits;
    if (!content.Byte(unusedBits)) return Fail(StringTooShort);
    unsigned char* data;
    if (!Stow(content, data)) return false;
    auto* string = NewItemString(BitStringType);
    if (!string) {
        ClearAndFree(data, content.length);
        return false;
    }
    string->data = data;
    string->length = static_cast<std::int32_t>(content.length);
    if (unusedBits > 7) {
        PutError(InvalidBitStringBitsLeft);
        FreeString(string);
        return false;
    }
    string->flags = (string->flags & ~(BitsLeftFlag | 0x07)) | BitsLeftFlag | unusedBits;
    if (string->length > 0) string->data[string->length - 1] &= static_cast<unsigned char>(0xff << unusedBits);
    value = string;
    return true;
}

bool ConvertInteger(void*& value, Reader content, int type) {
    if (content.length == 0) return Fail(LengthMismatch);
    const bool negative = (content.data[0] & 0x80) != 0;
    const unsigned char padding = negative ? 0xff : 0;
    if (content.length > 1 && content.data[0] == padding) {
        bool keepPadding = negative;
        for (std::size_t index = 1; index < content.length; ++index) keepPadding = keepPadding && content.data[index] == 0;
        if (!keepPadding) {
            content.Skip(1);
            if (((content.data[0] & 0x80) != 0) == negative) return Fail(LengthMismatch);
        }
    }
    unsigned char* data;
    if (!Stow(content, data)) return false;
    auto* string = NewItemString(IntegerType);
    if (!string) {
        ClearAndFree(data, content.length);
        return false;
    }
    if (negative) {
        string->type = IntegerType | NegativeFlag;
        unsigned int carry = 1;
        for (std::size_t index = content.length; index != 0; --index) {
            const unsigned int digit = (data[index - 1] ^ 0xffu) + carry;
            data[index - 1] = static_cast<unsigned char>(digit);
            carry = digit >> 8;
        }
    }
    string->data = data;
    string->length = static_cast<std::int32_t>(content.length);
    if (type == EnumeratedType) string->type = EnumeratedType | (string->type & NegativeFlag);
    value = string;
    return true;
}

bool TwoDigits(Reader& input, int& value) {
    unsigned char first;
    unsigned char second;
    if (!input.Byte(first) || first < '0' || first > '9') return false;
    if (!input.Byte(second) || second < '0' || second > '9') return false;
    value = (first - '0') * 10 + (second - '0');
    return true;
}

bool ValidDay(int year, int month, int day) {
    if (day < 1) return false;
    switch (month) {
    case 4:
    case 6:
    case 9:
    case 11:
        return day <= 30;
    case 2:
        return day <= ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0 ? 29 : 28);
    default:
        return day <= 31;
    }
}

bool ValidTime(Reader content, bool generalized) {
    int year;
    int part;
    if (!TwoDigits(content, part)) return false;
    if (generalized) {
        year = part * 100;
        if (!TwoDigits(content, part)) return false;
        year += part;
    } else {
        year = 1900 + part;
        if (year < 1950) year += 100;
    }
    int month;
    int day;
    int hour;
    int minute;
    int second;
    if (!TwoDigits(content, month) || month < 1 || month > 12) return false;
    if (!TwoDigits(content, day) || !ValidDay(year, month, day)) return false;
    if (!TwoDigits(content, hour) || hour > 23) return false;
    if (!TwoDigits(content, minute) || minute > 59) return false;
    if (!TwoDigits(content, second) || second > 59) return false;
    unsigned char zone;
    return content.Byte(zone) && zone == 'Z' && content.length == 0;
}

bool ConvertString(void*& value, Reader content, int type) {
    if (type == BmpStringType && (content.length & 1)) return Fail(BmpStringIsWrongLength);
    if (type == UniversalStringType && (content.length & 3)) return Fail(UniversalStringIsWrongLength);
    if ((type == UtcTimeType || type == GeneralizedTimeType) && !ValidTime(content, type == GeneralizedTimeType)) return Fail(InvalidTimeFormat);
    auto* string = NewItemString(type);
    if (!string) return false;
    if (content.length >= MaximumInt) {
        FreeString(string);
        return false;
    }
    string->data = static_cast<unsigned char*>(calloc_nid_postfix(1, content.length + 1));
    if (!string->data) {
        PutError(MallocFailure);
        FreeString(string);
        return false;
    }
    string->length = static_cast<std::int32_t>(content.length);
    if (content.length != 0) std::memcpy(string->data, content.data, content.length);
    value = string;
    return true;
}

bool ConvertPrimitive(Asn1Type& any, Reader content, int type) {
    switch (type) {
    case ObjectType:
        return ConvertObject(any.value.pointer, content);
    case NullType:
        return content.length == 0 ? true : Fail(NullIsWrongLength);
    case BooleanType:
        if (content.length != 1) return Fail(BooleanIsWrongLength);
        any.value.boolean = content.data[0];
        return true;
    case BitStringType:
        return ConvertBitString(any.value.pointer, content);
    case EnumeratedType:
    case IntegerType:
        return ConvertInteger(any.value.pointer, content, type);
    default:
        return ConvertString(any.value.pointer, content, type);
    }
}

bool ConvertAny(Asn1Type*& value, Reader content, int type) {
    if (content.length > MaximumInt) return false;
    auto* any = static_cast<Asn1Type*>(malloc_nid_postfix(sizeof(Asn1Type)));
    if (!any) return Fail(MallocFailure);
    any->value.pointer = nullptr;
    any->type = UndefinedType;
    if (!ConvertPrimitive(*any, content, type)) {
        ASN1_TYPE_free_nid_postfix(any);
        return false;
    }
    any->type = type;
    value = any;
    return true;
}

bool DecodeContent(Asn1Type*& value, Reader& input, Reader& object, int type, const Header& header) {
    Reader initial = input;
    Reader content(nullptr, 0);
    if (MustBeConstructed(type) && !header.constructed) return Fail(TypeNotConstructed);
    if (MustBePrimitive(type) && header.constructed) return Fail(TypeNotPrimitive);
    std::vector<unsigned char> collected;
    bool decoded = false;
    if (type == SequenceType || type == SetType || type == OtherType) {
        decoded = FindEnd(object, header.length, header.indefinite) && initial.Take(content, object.Offset());
    } else if (header.constructed) {
        decoded = Collect(collected, object, header.indefinite, 0);
        content = Reader(collected.data(), collected.size());
    } else {
        decoded = object.Take(content, header.length);
    }
    decoded = decoded && ConvertAny(value, content, type) && input.Skip(object.Offset());
    Cleanse(collected.data(), collected.size());
    return decoded;
}

bool DecodeAny(Asn1Type*& value, Reader& input) {
    Reader object(input.data, input.length);
    Header header;
    if (!CheckTag(object, header, -1, 0)) return Fail(NestedAsn1Error);
    const int type = header.tagClass != 0 ? OtherType : header.tag;
    return DecodeContent(value, input, object, type, header);
}

void FreeStack(LibreSslStack* stack) {
    if (!stack) return;
    if (stack->num > 0 && !stack->data) throw std::invalid_argument("d2i_ASN1_SEQUENCE_ANY: stack without an element array");
    while (stack->num > 0) ASN1_TYPE_free_nid_postfix(reinterpret_cast<Asn1Type*>(stack->data[--stack->num]));
    sk_free_nid_postfix(stack);
}

bool DecodeSequence(LibreSslStack*& value, Reader& input) {
    Reader object(input.data, input.length);
    FreeStack(value);
    value = nullptr;
    Header header;
    if (!CheckTag(object, header, SequenceType, 0)) return Fail(NestedAsn1Error);
    Reader content(object.data, object.length);
    bool eocNeeded = header.indefinite;
    if (!header.indefinite && !object.Take(content, header.length)) return false;
    auto* stack = sk_new_nid_postfix(nullptr);
    if (!stack) return Fail(MallocFailure);
    Asn1Type* element = nullptr;
    const auto fail = [&](int reason) {
        PutError(reason);
        FreeStack(stack);
        ASN1_TYPE_free_nid_postfix(element);
        return false;
    };
    while (content.length > 0) {
        if (CheckEoc(content)) {
            if (!eocNeeded) return fail(UnexpectedEoc);
            eocNeeded = false;
            break;
        }
        if (!DecodeAny(element, content)) return fail(NestedAsn1Error);
        if (!sk_push_nid_postfix(stack, element)) return fail(MallocFailure);
        element = nullptr;
    }
    if (eocNeeded) return fail(MissingEoc);
    if (header.indefinite) object.Skip(content.Offset());
    input.Skip(object.Offset());
    value = stack;
    return true;
}

} // namespace

extern "C" {

LibreSslStack* APS5_VABI d2i_ASN1_SEQUENCE_ANY_nid_postfix(LibreSslStack** output, const unsigned char** input, std::int64_t length) {
    if (length < 0) return nullptr;
    if (!input || (!*input && length != 0)) throw std::invalid_argument("d2i_ASN1_SEQUENCE_ANY: null input");
    LibreSslStack* local = nullptr;
    auto*& value = output ? *output : local;
    Reader reader(*input, static_cast<std::size_t>(length));
    if (!DecodeSequence(value, reader)) return nullptr;
    *input = reader.data;
    return value;
}

}
