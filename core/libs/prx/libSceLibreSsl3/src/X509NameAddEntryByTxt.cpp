#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

static_assert(sizeof(Asn1String) == 24);
static_assert(offsetof(Asn1String, type) == 4);
static_assert(offsetof(Asn1String, data) == 8);
static_assert(offsetof(Asn1String, flags) == 16);
static_assert(sizeof(Asn1Object_nid_no_patch) == 40);
static_assert(offsetof(Asn1Object_nid_no_patch, nid) == 16);
static_assert(offsetof(Asn1Object_nid_no_patch, length) == 20);
static_assert(offsetof(Asn1Object_nid_no_patch, data) == 24);
static_assert(offsetof(Asn1Object_nid_no_patch, flags) == 32);
static_assert(sizeof(LibreSslX509NameEntry) == 24);
static_assert(offsetof(LibreSslX509NameEntry, object) == 0);
static_assert(offsetof(LibreSslX509NameEntry, value) == 8);
static_assert(offsetof(LibreSslX509NameEntry, set) == 16);
static_assert(sizeof(LibreSslX509Name) == 40);
static_assert(offsetof(LibreSslX509Name, entries) == 0);
static_assert(offsetof(LibreSslX509Name, modified) == 8);
static_assert(offsetof(LibreSslX509Name, bytes) == 16);
static_assert(offsetof(LibreSslX509Name, canonEnc) == 24);
static_assert(offsetof(LibreSslX509Name, canonEnclen) == 32);
static_assert(sizeof(LibreSslStack) == 32);
static_assert(offsetof(LibreSslStack, num) == 0);
static_assert(offsetof(LibreSslStack, data) == 8);
static_assert(offsetof(LibreSslStack, sorted) == 16);
static_assert(offsetof(LibreSslStack, numAlloc) == 20);

namespace {

constexpr int X509Library = 11;
constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int AllocationFailure = 65;
constexpr int InvalidFieldName = 119;
constexpr int FirstNumberTooLarge = 122;
constexpr int IllegalCharacters = 124;
constexpr int InvalidBmpStringLength = 129;
constexpr int InvalidDigit = 130;
constexpr int InvalidSeparator = 131;
constexpr int InvalidUniversalStringLength = 133;
constexpr int InvalidUtf8String = 134;
constexpr int MissingSecondNumber = 138;
constexpr int SecondNumberTooLarge = 147;
constexpr int StringTooLong = 151;
constexpr int StringTooShort = 152;
constexpr int UnknownFormat = 160;

constexpr int Asn1Undefined = -1;
constexpr int Asn1AppChoose = -2;
constexpr int Asn1Boolean = 1;
constexpr int Asn1Null = 5;
constexpr int Asn1ObjectType = 6;
constexpr int Asn1Utf8String = 12;
constexpr int Asn1PrintableString = 19;
constexpr int Asn1T61String = 20;
constexpr int Asn1Ia5String = 22;
constexpr int Asn1UniversalString = 28;
constexpr int Asn1BmpString = 30;

constexpr std::int32_t ObjectDynamic = 0x01;
constexpr std::int32_t DynamicData = 0x08;
constexpr std::int64_t StringFlagMstring = 0x040;

constexpr int MbstringFlag = 0x1000;
constexpr int MbstringUtf8 = MbstringFlag;
constexpr int MbstringAsc = MbstringFlag | 1;
constexpr int MbstringBmp = MbstringFlag | 2;
constexpr int MbstringUniversal = MbstringFlag | 4;

constexpr unsigned long BitPrintableString = 0x0002;
constexpr unsigned long BitT61String = 0x0004;
constexpr unsigned long BitIa5String = 0x0010;
constexpr unsigned long BitUniversalString = 0x0100;
constexpr unsigned long BitBmpString = 0x0800;
constexpr unsigned long BitUtf8String = 0x2000;
constexpr unsigned long DirectoryType = BitPrintableString | BitT61String | BitBmpString | BitUtf8String;
constexpr unsigned long Pkcs9Type = DirectoryType | BitIa5String;
constexpr unsigned long GlobalMask = BitUtf8String;
constexpr unsigned long UnicodeMax = 0x10ffff;
constexpr std::uint64_t BigNumberThreshold = (std::numeric_limits<std::uint64_t>::max() - 80) / 10;

enum class Format { Ascii, Bmp, Universal, Utf8 };

enum class DataResult { Ok, Failed, Unsupported };

struct StringType {
    int nid;
    long minimum;
    long maximum;
    unsigned long mask;
    bool keepMask;
};

constexpr StringType StringTypes[] {
    {13, 1, 64, DirectoryType, false},
    {14, 2, 2, BitPrintableString, true},
    {15, 1, 128, DirectoryType, false},
    {16, 1, 128, DirectoryType, false},
    {17, 1, 64, DirectoryType, false},
    {18, 1, 64, DirectoryType, false},
    {48, 1, 128, BitIa5String, true},
    {49, 1, -1, Pkcs9Type, false},
    {54, 1, -1, Pkcs9Type, false},
    {55, 1, -1, DirectoryType, false},
    {99, 1, 32768, DirectoryType, false},
    {100, 1, 32768, DirectoryType, false},
    {101, 1, 32768, DirectoryType, false},
    {105, 1, 64, BitPrintableString, true},
    {156, -1, -1, BitBmpString, true},
    {173, 1, 32768, DirectoryType, false},
    {174, -1, -1, BitPrintableString, true},
    {391, 1, -1, BitIa5String, true},
    {417, -1, -1, BitBmpString, true},
};

void PutAsn1Error(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

void PutX509Error(int reason) {
    LibreSslPutError_nid_no_patch(X509Library, AnyFunction, reason);
}

bool IsSurrogate(unsigned long value) {
    return value >= 0xd800 && value < 0xe000;
}

int Utf8Getc(const unsigned char* text, int length, unsigned long& value) {
    if (length <= 0) return 0;
    const unsigned char* cursor = text;
    unsigned long result;
    int consumed;
    if ((*cursor & 0x80) == 0) {
        result = *cursor++ & 0x7f;
        consumed = 1;
    } else if ((*cursor & 0xe0) == 0xc0) {
        if (*cursor < 0xc2) return -2;
        if (length < 2) return -1;
        if ((cursor[1] & 0xc0) != 0x80) return -3;
        result = static_cast<unsigned long>(*cursor++ & 0x1f) << 6;
        result |= *cursor++ & 0x3f;
        if (result < 0x80) return -4;
        consumed = 2;
    } else if ((*cursor & 0xf0) == 0xe0) {
        if (length < 3) return -1;
        if ((cursor[1] & 0xc0) != 0x80 || (cursor[2] & 0xc0) != 0x80) return -3;
        result = static_cast<unsigned long>(*cursor++ & 0xf) << 12;
        result |= static_cast<unsigned long>(*cursor++ & 0x3f) << 6;
        result |= *cursor++ & 0x3f;
        if (result < 0x800) return -4;
        if (IsSurrogate(result)) return -2;
        consumed = 3;
    } else if ((*cursor & 0xf8) == 0xf0 && *cursor < 0xf5) {
        if (length < 4) return -1;
        if ((cursor[1] & 0xc0) != 0x80 || (cursor[2] & 0xc0) != 0x80 || (cursor[3] & 0xc0) != 0x80) return -3;
        result = static_cast<unsigned long>(*cursor++ & 0x7) << 18;
        result |= static_cast<unsigned long>(*cursor++ & 0x3f) << 12;
        result |= static_cast<unsigned long>(*cursor++ & 0x3f) << 6;
        result |= *cursor++ & 0x3f;
        if (result < 0x10000) return -4;
        if (result > UnicodeMax) return -2;
        consumed = 4;
    } else {
        return -2;
    }
    value = result;
    return consumed;
}

int Utf8Putc(unsigned char* text, int length, unsigned long value) {
    if (value < 0x80) {
        if (text) {
            if (length < 1) return -1;
            text[0] = static_cast<unsigned char>(value);
        }
        return 1;
    }
    if (value < 0x800) {
        if (text) {
            if (length < 2) return -1;
            text[0] = static_cast<unsigned char>(((value >> 6) & 0x1f) | 0xc0);
            text[1] = static_cast<unsigned char>((value & 0x3f) | 0x80);
        }
        return 2;
    }
    if (value < 0x10000) {
        if (IsSurrogate(value)) return -2;
        if (text) {
            if (length < 3) return -1;
            text[0] = static_cast<unsigned char>(((value >> 12) & 0xf) | 0xe0);
            text[1] = static_cast<unsigned char>(((value >> 6) & 0x3f) | 0x80);
            text[2] = static_cast<unsigned char>((value & 0x3f) | 0x80);
        }
        return 3;
    }
    if (value <= UnicodeMax) {
        if (text) {
            if (length < 4) return -1;
            text[0] = static_cast<unsigned char>(((value >> 18) & 0x7) | 0xf0);
            text[1] = static_cast<unsigned char>(((value >> 12) & 0x3f) | 0x80);
            text[2] = static_cast<unsigned char>(((value >> 6) & 0x3f) | 0x80);
            text[3] = static_cast<unsigned char>((value & 0x3f) | 0x80);
        }
        return 4;
    }
    return -2;
}

bool IsPrintable(unsigned long value) {
    if (value > 0x7f) return false;
    const auto character = static_cast<int>(value);
    if (character >= 'a' && character <= 'z') return true;
    if (character >= 'A' && character <= 'Z') return true;
    if (character >= '0' && character <= '9') return true;
    return character == ' ' || std::strchr("'()+,-./:=?", character) != nullptr;
}

template <typename Visitor>
int Traverse(const unsigned char* text, int length, Format format, Visitor&& visit) {
    while (length > 0) {
        unsigned long value = 0;
        switch (format) {
        case Format::Ascii:
            value = *text++;
            --length;
            break;
        case Format::Bmp:
            value = static_cast<unsigned long>(text[0]) << 8;
            value |= text[1];
            text += 2;
            length -= 2;
            if (IsSurrogate(value)) return -1;
            break;
        case Format::Universal:
            value = static_cast<unsigned long>(text[0]) << 24;
            value |= static_cast<unsigned long>(text[1]) << 16;
            value |= static_cast<unsigned long>(text[2]) << 8;
            value |= text[3];
            text += 4;
            length -= 4;
            if (value > UnicodeMax || IsSurrogate(value)) return -1;
            break;
        case Format::Utf8: {
            const int consumed = Utf8Getc(text, length, value);
            if (consumed < 0) return -1;
            text += consumed;
            length -= consumed;
            break;
        }
        }
        if (!visit(value)) return -1;
    }
    return 1;
}

const StringType* FindStringType(int nid) {
    for (const auto& entry : StringTypes) {
        if (entry.nid == nid) return &entry;
    }
    return nullptr;
}

int PrintableType(const unsigned char* bytes, int length) {
    if (length <= 0) length = -1;
    if (!bytes) return Asn1PrintableString;
    bool ia5 = false;
    bool t61 = false;
    while (*bytes && length-- != 0) {
        const int character = *bytes++;
        if (!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == ' ' || character == '\'' ||
              character == '(' || character == ')' || character == '+' || character == ',' ||
              character == '-' || character == '.' || character == '/' || character == ':' ||
              character == '=' || character == '?'))
            ia5 = true;
        if (character & 0x80) t61 = true;
    }
    if (t61) return Asn1T61String;
    if (ia5) return Asn1Ia5String;
    return Asn1PrintableString;
}

bool SetByNid(Asn1String* value, int nid, const unsigned char* bytes, int len, int type) {
    Format input;
    switch (type) {
    case MbstringAsc:
        input = Format::Ascii;
        break;
    case MbstringBmp:
        input = Format::Bmp;
        break;
    case MbstringUniversal:
        input = Format::Universal;
        break;
    case MbstringUtf8:
        input = Format::Utf8;
        break;
    default:
        PutAsn1Error(UnknownFormat);
        return false;
    }
    if (len < 0) {
        const std::size_t size = std::strlen(reinterpret_cast<const char*>(bytes));
        if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            PutAsn1Error(AllocationFailure);
            return false;
        }
        len = static_cast<int>(size);
    }
    int characters = 0;
    switch (input) {
    case Format::Bmp:
        if ((len & 1) != 0) {
            PutAsn1Error(InvalidBmpStringLength);
            return false;
        }
        characters = len >> 1;
        break;
    case Format::Universal:
        if ((len & 3) != 0) {
            PutAsn1Error(InvalidUniversalStringLength);
            return false;
        }
        characters = len >> 2;
        break;
    case Format::Utf8:
        if (Traverse(bytes, len, input, [&characters](unsigned long) {
                ++characters;
                return true;
            }) < 0) {
            PutAsn1Error(InvalidUtf8String);
            return false;
        }
        break;
    case Format::Ascii:
        characters = len;
        break;
    }
    unsigned long mask = DirectoryType & GlobalMask;
    long minimum = 0;
    long maximum = 0;
    if (const auto* entry = FindStringType(nid)) {
        minimum = entry->minimum;
        maximum = entry->maximum;
        mask = entry->keepMask ? entry->mask : entry->mask & GlobalMask;
    }
    if (minimum > 0 && characters < minimum) {
        PutAsn1Error(StringTooShort);
        return false;
    }
    if (maximum > 0 && characters > maximum) {
        PutAsn1Error(StringTooLong);
        return false;
    }
    if (mask == 0) mask = DirectoryType;
    if (Traverse(bytes, len, input, [&mask](unsigned long character) {
            if ((mask & BitPrintableString) != 0 && !IsPrintable(character)) mask &= ~BitPrintableString;
            if ((mask & BitIa5String) != 0 && character > 0x7f) mask &= ~BitIa5String;
            if ((mask & BitT61String) != 0 && character > 0xff) mask &= ~BitT61String;
            if ((mask & BitBmpString) != 0 && character > 0xffff) mask &= ~BitBmpString;
            return mask != 0;
        }) < 0) {
        PutAsn1Error(IllegalCharacters);
        return false;
    }
    int outputType;
    Format output;
    if ((mask & BitPrintableString) != 0) {
        outputType = Asn1PrintableString;
        output = Format::Ascii;
    } else if ((mask & BitIa5String) != 0) {
        outputType = Asn1Ia5String;
        output = Format::Ascii;
    } else if ((mask & BitT61String) != 0) {
        outputType = Asn1T61String;
        output = Format::Ascii;
    } else if ((mask & BitBmpString) != 0) {
        outputType = Asn1BmpString;
        output = Format::Bmp;
    } else if ((mask & BitUniversalString) != 0) {
        outputType = Asn1UniversalString;
        output = Format::Universal;
    } else {
        outputType = Asn1Utf8String;
        output = Format::Utf8;
    }
    value->type = outputType;
    if (input == output) return ASN1_STRING_set_nid_postfix(value, bytes, len) != 0;
    int outputLength = 0;
    if (output == Format::Utf8) {
        if (Traverse(bytes, len, input, [&outputLength](unsigned long character) {
                const int size = Utf8Putc(nullptr, 0, character);
                if (size < 0 || outputLength > std::numeric_limits<int>::max() - size) return false;
                outputLength += size;
                return true;
            }) < 0) {
            PutAsn1Error(IllegalCharacters);
            return false;
        }
    } else {
        const std::int64_t width = output == Format::Bmp ? 2 : 4;
        const std::int64_t total = static_cast<std::int64_t>(characters) * width;
        if (total > std::numeric_limits<int>::max()) {
            PutAsn1Error(AllocationFailure);
            return false;
        }
        outputLength = static_cast<int>(total);
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(outputLength) + 1));
    if (!data) {
        PutAsn1Error(AllocationFailure);
        return false;
    }
    value->length = outputLength;
    value->data = data;
    data[outputLength] = 0;
    auto* cursor = data;
    auto* const end = data + outputLength;
    Traverse(bytes, len, input, [&cursor, end, output](unsigned long character) {
        switch (output) {
        case Format::Ascii:
            *cursor++ = static_cast<unsigned char>(character);
            break;
        case Format::Bmp:
            *cursor++ = static_cast<unsigned char>(character >> 8);
            *cursor++ = static_cast<unsigned char>(character);
            break;
        case Format::Universal:
            *cursor++ = static_cast<unsigned char>(character >> 24);
            *cursor++ = static_cast<unsigned char>(character >> 16);
            *cursor++ = static_cast<unsigned char>(character >> 8);
            *cursor++ = static_cast<unsigned char>(character);
            break;
        case Format::Utf8:
            cursor += Utf8Putc(cursor, static_cast<int>(end - cursor), character);
            break;
        }
        return true;
    });
    return true;
}

DataResult SetData(Asn1String* value, int type, const unsigned char* bytes, int len) {
    int length = len;
    if (length < 0) {
        const std::size_t size = std::strlen(reinterpret_cast<const char*>(bytes));
        if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            PutAsn1Error(AllocationFailure);
            return DataResult::Failed;
        }
        length = static_cast<int>(size);
    }
    if (!ASN1_STRING_set_nid_postfix(value, bytes, length)) return DataResult::Failed;
    if (type == Asn1Undefined) return DataResult::Ok;
    if (type == Asn1AppChoose) {
        value->type = PrintableType(bytes, length);
        return DataResult::Ok;
    }
    if (type == Asn1Boolean || type == Asn1Null || type == Asn1ObjectType) return DataResult::Unsupported;
    value->type = type;
    return DataResult::Ok;
}

class Number {
public:
    explicit Number(std::uint64_t value) : small_(value), limbs_{static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32)} {}

    void MultiplyAdd(std::uint32_t factor, std::uint32_t addend) {
        if (!big_ && small_ >= BigNumberThreshold) big_ = true;
        std::uint64_t carry = addend;
        for (auto& limb : limbs_) {
            const std::uint64_t product = static_cast<std::uint64_t>(limb) * factor + carry;
            limb = static_cast<std::uint32_t>(product);
            carry = product >> 32;
        }
        if (carry) limbs_.push_back(static_cast<std::uint32_t>(carry));
        if (!big_) small_ = small_ * factor + addend;
    }

    bool Big() const { return big_; }

    std::uint64_t Small() const { return small_; }

    int Bits() const {
        for (std::size_t index = limbs_.size(); index-- > 0;) {
            if (limbs_[index] != 0) return static_cast<int>(index * 32) + 32 - std::countl_zero(limbs_[index]);
        }
        return 0;
    }

    unsigned char DivideBy128() {
        std::uint64_t remainder = 0;
        for (std::size_t index = limbs_.size(); index-- > 0;) {
            const std::uint64_t current = (remainder << 32) | limbs_[index];
            limbs_[index] = static_cast<std::uint32_t>(current / 128);
            remainder = current % 128;
        }
        return static_cast<unsigned char>(remainder);
    }

private:
    std::uint64_t small_;
    bool big_ = false;
    std::vector<std::uint32_t> limbs_;
};

std::vector<unsigned char> TextContent(const char* text) {
    std::vector<unsigned char> content;
    std::size_t remaining = std::strlen(text);
    if (remaining == 0) return content;
    const char* next = text;
    int character = static_cast<signed char>(*next++);
    if (character < '0' || character > '2') {
        PutAsn1Error(FirstNumberTooLarge);
        return {};
    }
    --remaining;
    const int first = character - '0';
    if (remaining == 0) {
        PutAsn1Error(MissingSecondNumber);
        return {};
    }
    character = static_cast<signed char>(*next++);
    --remaining;
    while (remaining > 0) {
        if (character != '.' && character != ' ') {
            PutAsn1Error(InvalidSeparator);
            return {};
        }
        Number number(0);
        while (remaining > 0) {
            --remaining;
            character = static_cast<signed char>(*next++);
            if (character == ' ' || character == '.') break;
            if (character < '0' || character > '9') {
                PutAsn1Error(InvalidDigit);
                return {};
            }
            number.MultiplyAdd(10, static_cast<std::uint32_t>(character - '0'));
        }
        if (content.empty()) {
            if (first < 2 && number.Small() >= 40) {
                PutAsn1Error(SecondNumberTooLarge);
                return {};
            }
            number.MultiplyAdd(1, static_cast<std::uint32_t>(first * 40));
        }
        std::vector<unsigned char> groups;
        if (number.Big()) {
            for (int count = (number.Bits() + 6) / 7; count > 0; --count) groups.push_back(number.DivideBy128());
        } else {
            std::uint64_t value = number.Small();
            do {
                groups.push_back(static_cast<unsigned char>(value & 0x7f));
                value >>= 7;
            } while (value != 0);
        }
        if (groups.empty()) {
            PutAsn1Error(InvalidDigit);
            return {};
        }
        for (std::size_t index = groups.size(); index-- > 1;) content.push_back(groups[index] | 0x80);
        content.push_back(groups[0]);
    }
    return content;
}

const LibreSslAsn1Object* TableObject(int nid) {
    if (nid < 0 || nid >= LibreSsl3::ObjectCount) return nullptr;
    if (nid < LibreSslObjectCount) {
        const auto& object = LibreSslObjects_nid_no_patch[nid];
        if (nid != 0 && object.nid == 0) return nullptr;
        return &object;
    }
    return LibreSsl3::ObjectByNid(nid);
}

int LookupName(const char* field) {
    for (int nid = 0; nid < LibreSsl3::ObjectCount; ++nid) {
        const auto* object = TableObject(nid);
        if (object && object->shortName && std::strcmp(object->shortName, field) == 0) return object->nid;
    }
    for (int nid = 0; nid < LibreSsl3::ObjectCount; ++nid) {
        const auto* object = TableObject(nid);
        if (object && object->longName && std::strcmp(object->longName, field) == 0) return object->nid;
    }
    return 0;
}

struct ResolvedObject {
    const LibreSslAsn1Object* object = nullptr;
    int nid = 0;
    bool owned = false;
};

void ReleaseObject(const ResolvedObject& resolved) {
    if (!resolved.owned || !resolved.object) return;
    ASN1_OBJECT_free_nid_postfix(const_cast<Asn1Object_nid_no_patch*>(
        reinterpret_cast<const Asn1Object_nid_no_patch*>(resolved.object)));
}

Asn1Object_nid_no_patch* NumericObject(const std::vector<unsigned char>& content) {
    if (content.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        PutAsn1Error(AllocationFailure);
        return nullptr;
    }
    auto* object = static_cast<Asn1Object_nid_no_patch*>(calloc_nid_postfix(1, sizeof(Asn1Object_nid_no_patch)));
    if (!object) {
        PutAsn1Error(AllocationFailure);
        return nullptr;
    }
    unsigned char* data;
    try {
        data = static_cast<unsigned char*>(malloc_nid_postfix(content.size()));
    } catch (...) {
        free_nid_postfix(object);
        throw;
    }
    if (!data) {
        free_nid_postfix(object);
        PutAsn1Error(AllocationFailure);
        return nullptr;
    }
    std::memcpy(data, content.data(), content.size());
    object->nid = 0;
    object->length = static_cast<std::int32_t>(content.size());
    object->data = data;
    object->flags = ObjectDynamic | DynamicData;
    return object;
}

ResolvedObject ResolveField(const char* field) {
    ResolvedObject resolved;
    int nid = OBJ_sn2nid_nid_postfix(field);
    const LibreSslAsn1Object* object = nullptr;
    if (nid != 0) {
        object = LibreSslAddedObjectByNid_nid_no_patch(nid);
        if (!object) object = TableObject(nid);
    } else {
        nid = LookupName(field);
        if (nid != 0) object = TableObject(nid);
    }
    if (object) {
        resolved.object = object;
        resolved.nid = LibreSsl3::ObjectNid(object);
        return resolved;
    }
    std::vector<unsigned char> content = TextContent(field);
    if (content.empty()) {
        PutX509Error(InvalidFieldName);
        return resolved;
    }
    auto* numeric = NumericObject(content);
    if (!numeric) return resolved;
    resolved.object = reinterpret_cast<const LibreSslAsn1Object*>(numeric);
    resolved.owned = true;
    try {
        resolved.nid = LibreSsl3::ObjectNid(resolved.object);
    } catch (...) {
        ReleaseObject(resolved);
        throw;
    }
    return resolved;
}

void FreeEntry(LibreSslX509NameEntry* entry) {
    if (!entry) return;
    ASN1_OCTET_STRING_free_nid_postfix(static_cast<Asn1String*>(entry->value));
    ASN1_OBJECT_free_nid_postfix(static_cast<Asn1Object_nid_no_patch*>(entry->object));
    free_nid_postfix(entry);
}

class EntryOwner {
public:
    explicit EntryOwner(LibreSslX509NameEntry* entry) : entry_(entry) {}
    EntryOwner(const EntryOwner&) = delete;
    EntryOwner& operator=(const EntryOwner&) = delete;
    ~EntryOwner() { FreeEntry(entry_); }
    void Release() { entry_ = nullptr; }

private:
    LibreSslX509NameEntry* entry_;
};

LibreSslX509NameEntry* CreateEntry(const ResolvedObject& resolved, int type, const unsigned char* bytes, int len) {
    auto* entry = static_cast<LibreSslX509NameEntry*>(calloc_nid_postfix(1, sizeof(LibreSslX509NameEntry)));
    if (!entry) {
        PutAsn1Error(AllocationFailure);
        return nullptr;
    }
    EntryOwner owner(entry);
    auto* value = static_cast<Asn1String*>(calloc_nid_postfix(1, sizeof(Asn1String)));
    if (!value) {
        PutAsn1Error(AllocationFailure);
        return nullptr;
    }
    value->type = Asn1Undefined;
    value->flags = StringFlagMstring;
    entry->value = value;
    bool success;
    bool unsupported = false;
    if (type > 0 && (type & MbstringFlag) != 0) {
        success = SetByNid(value, resolved.nid, bytes, len, type);
    } else {
        const DataResult result = SetData(value, type, bytes, len);
        success = result == DataResult::Ok;
        unsupported = result == DataResult::Unsupported;
    }
    if (!success || unsupported) {
        if (unsupported) NotImplemented_nid_no_patch(__func__);
        return nullptr;
    }
    entry->object = const_cast<LibreSslAsn1Object*>(resolved.object);
    owner.Release();
    return entry;
}

void ReleaseCanonical(LibreSslX509Name* name) {
    free_nid_postfix(name->canonEnc);
    name->canonEnc = nullptr;
    name->canonEnclen = 0;
}

LibreSslX509NameEntry* EntryAt(LibreSslStack* stack, int index, const char* function) {
    if (!stack->data) throw std::invalid_argument(std::string(function) + ": null entry array");
    auto* entry = reinterpret_cast<LibreSslX509NameEntry*>(stack->data[index]);
    if (!entry) throw std::invalid_argument(std::string(function) + ": null name entry");
    return entry;
}

int Increment(int set, const char* function) {
    if (set == std::numeric_limits<int>::max())
        throw std::overflow_error(std::string(function) + ": entry set number overflows");
    return set + 1;
}

bool InsertEntry(LibreSslStack* stack, LibreSslX509NameEntry* entry, int loc, const char* function) {
    if (stack->numAlloc <= 0 || stack->numAlloc < stack->num)
        throw std::invalid_argument(std::string(function) + ": invalid entry array capacity");
    if (!sk_push_nid_postfix(stack, entry)) return false;
    if (loc < stack->num - 1) {
        if (!stack->data) throw std::invalid_argument(std::string(function) + ": null entry array");
        std::memmove(&stack->data[loc + 1], &stack->data[loc],
                     static_cast<std::size_t>(stack->num - 1 - loc) * sizeof(char*));
        stack->data[loc] = reinterpret_cast<char*>(entry);
    }
    return true;
}

int AddEntry(LibreSslX509Name* name, LibreSslX509NameEntry* entry, int loc, int set, const char* function) {
    LibreSslStack* const stack = name->entries;
    if (!stack) throw std::invalid_argument(std::string(function) + ": null entry stack");
    const int count = stack->num;
    if (count < 0) throw std::invalid_argument(std::string(function) + ": negative entry count");
    if (loc > count) loc = count;
    else if (loc < 0) loc = count;
    bool increment = set == 0;
    name->modified = 1;
    ReleaseCanonical(name);
    if (set == -1) {
        if (loc == 0) {
            set = 0;
            increment = true;
        } else {
            set = EntryAt(stack, loc - 1, function)->set;
        }
    } else if (loc >= count) {
        set = loc != 0 ? Increment(EntryAt(stack, loc - 1, function)->set, function) : 0;
    } else {
        set = EntryAt(stack, loc, function)->set;
    }
    if (increment) {
        for (int index = loc + 1; index < count; ++index) Increment(EntryAt(stack, index, function)->set, function);
    }
    entry->set = set;
    if (!InsertEntry(stack, entry, loc, function)) {
        PutX509Error(AllocationFailure);
        return 0;
    }
    if (increment) {
        const int total = stack->num;
        for (int index = loc + 1; index < total; ++index) ++EntryAt(stack, index, function)->set;
    }
    return 1;
}

}

extern "C" {

int APS5_VABI X509_NAME_add_entry_by_txt_nid_postfix(LibreSslX509Name* name, const char* field, int type,
    const unsigned char* bytes, int len, int loc, int set) {
    if (!name) return 0;
    if (!field) throw std::invalid_argument(std::string(__func__) + ": null field name");
    ResolvedObject resolved = ResolveField(field);
    if (!resolved.object) return 0;
    if (!bytes && len != 0) {
        ReleaseObject(resolved);
        return 0;
    }
    LibreSslX509NameEntry* entry;
    try {
        entry = CreateEntry(resolved, type, bytes, len);
    } catch (...) {
        ReleaseObject(resolved);
        throw;
    }
    if (!entry) {
        ReleaseObject(resolved);
        return 0;
    }
    EntryOwner owner(entry);
    const int result = AddEntry(name, entry, loc, set, __func__);
    if (result) owner.Release();
    return result;
}

}
