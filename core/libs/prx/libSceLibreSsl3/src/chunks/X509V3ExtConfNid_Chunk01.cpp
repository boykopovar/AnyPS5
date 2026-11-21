#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int X509V3Library = 34;
constexpr int AnyFunction = 0xfff;

constexpr int ExtensionNameError = 115;
constexpr int ExtensionValueError = 116;
constexpr int IllegalHexDigit = 113;
constexpr int InvalidBooleanString = 104;
constexpr int InvalidExtensionString = 105;
constexpr int InvalidObjectIdentifier = 110;
constexpr int OddNumberOfDigits = 112;
constexpr int UnknownBitStringArgument = 111;
constexpr int UnknownExtension = 129;
constexpr int UnknownExtensionName = 130;

constexpr std::int32_t OctetStringType = 4;

constexpr unsigned char BooleanTag = 0x01;
constexpr unsigned char IntegerTag = 0x02;
constexpr unsigned char BitStringTag = 0x03;
constexpr unsigned char OctetStringTag = 0x04;
constexpr unsigned char NullTag = 0x05;
constexpr unsigned char ObjectTag = 0x06;
constexpr unsigned char EnumeratedTag = 0x0a;
constexpr unsigned char Utf8StringTag = 0x0c;
constexpr unsigned char Ia5StringTag = 0x16;
constexpr unsigned char SequenceTag = 0x30;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(X509V3Library, AnyFunction, reason);
}

bool IsSpace(char value) {
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

int HexDigit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool ParseHex(std::string_view text, std::vector<unsigned char>& bytes) {
    std::size_t index = 0;
    while (index < text.size()) {
        const char high = text[index++];
        if (high == ':') continue;
        if (index == text.size()) {
            PutError(OddNumberOfDigits);
            return false;
        }
        const char low = text[index++];
        const int highValue = HexDigit(high);
        const int lowValue = HexDigit(low);
        if (highValue < 0 || lowValue < 0) {
            PutError(IllegalHexDigit);
            return false;
        }
        bytes.push_back(static_cast<unsigned char>((highValue << 4) | lowValue));
    }
    return true;
}

void AppendLength(std::vector<unsigned char>& output, std::size_t length) {
    if (length < 0x80) {
        output.push_back(static_cast<unsigned char>(length));
        return;
    }
    std::array<unsigned char, sizeof(std::size_t)> encoded{};
    std::size_t count = 0;
    for (std::size_t remaining = length; remaining != 0; remaining >>= 8)
        encoded[count++] = static_cast<unsigned char>(remaining & 0xff);
    output.push_back(static_cast<unsigned char>(0x80 | count));
    while (count != 0) output.push_back(encoded[--count]);
}

void AppendTlv(std::vector<unsigned char>& output, unsigned char tag, std::span<const unsigned char> content) {
    output.push_back(tag);
    AppendLength(output, content.size());
    output.insert(output.end(), content.begin(), content.end());
}

bool IsZeroDecimal(std::string_view digits) {
    for (const char digit : digits) {
        if (digit != '0') return false;
    }
    return true;
}

std::string StripZeroes(std::string_view digits) {
    std::size_t index = 0;
    while (index + 1 < digits.size() && digits[index] == '0') ++index;
    if (digits.empty()) return "0";
    return std::string(digits.substr(index));
}

std::string AddDecimal(std::string digits, unsigned int add) {
    std::string normalized = StripZeroes(digits);
    unsigned int carry = add;
    for (std::size_t index = normalized.size(); index != 0 && carry != 0; --index) {
        const unsigned int value = static_cast<unsigned int>(normalized[index - 1] - '0') + carry;
        normalized[index - 1] = static_cast<char>('0' + value % 10);
        carry = value / 10;
    }
    while (carry != 0) {
        normalized.insert(normalized.begin(), static_cast<char>('0' + carry % 10));
        carry /= 10;
    }
    return normalized;
}

bool DecimalAtLeast(std::string_view digits, unsigned int limit) {
    const std::string normalized = StripZeroes(digits);
    const std::string bound = std::to_string(limit);
    if (normalized.size() != bound.size()) return normalized.size() > bound.size();
    return normalized >= bound;
}

std::vector<unsigned char> DecimalBytes(std::string_view digits) {
    std::string normalized = StripZeroes(digits);
    std::vector<unsigned char> reversed;
    while (!IsZeroDecimal(normalized)) {
        unsigned int remainder = 0;
        for (char& digit : normalized) {
            const unsigned int value = remainder * 10 + static_cast<unsigned int>(digit - '0');
            digit = static_cast<char>('0' + value / 256);
            remainder = value % 256;
        }
        reversed.push_back(static_cast<unsigned char>(remainder));
    }
    return std::vector<unsigned char>(reversed.rbegin(), reversed.rend());
}

std::vector<unsigned char> HexBytes(std::string_view digits) {
    std::size_t index = 0;
    while (index < digits.size() && digits[index] == '0') ++index;
    if (index == digits.size()) return {0};
    std::vector<unsigned char> bytes;
    if ((digits.size() - index) % 2 != 0) bytes.push_back(static_cast<unsigned char>(HexDigit(digits[index++])));
    while (index < digits.size()) {
        const int high = HexDigit(digits[index++]);
        const int low = HexDigit(digits[index++]);
        bytes.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return bytes;
}

std::vector<unsigned char> Base128(std::string digits) {
    std::string normalized = StripZeroes(digits);
    std::vector<unsigned char> reversed;
    while (!IsZeroDecimal(normalized)) {
        unsigned int remainder = 0;
        for (char& digit : normalized) {
            const unsigned int value = remainder * 10 + static_cast<unsigned int>(digit - '0');
            digit = static_cast<char>('0' + value / 128);
            remainder = value % 128;
        }
        reversed.push_back(static_cast<unsigned char>(remainder));
    }
    if (reversed.empty()) reversed.push_back(0);
    std::vector<unsigned char> encoded(reversed.rbegin(), reversed.rend());
    for (std::size_t index = 0; index + 1 < encoded.size(); ++index) encoded[index] |= 0x80;
    return encoded;
}

bool EncodeNumericObject(std::string_view text, std::vector<unsigned char>& content) {
    if (text.size() < 2) return false;
    if (text[0] < '0' || text[0] > '2') return false;
    const int first = text[0] - '0';
    std::size_t index = 1;
    char current = text[index++];
    std::size_t remaining = text.size() - index;
    for (;;) {
        if (remaining == 0) break;
        if (current != '.' && current != ' ') return false;
        std::string arc;
        for (;;) {
            if (remaining == 0) break;
            --remaining;
            current = text[index++];
            if (current == '.' || current == ' ') break;
            if (current < '0' || current > '9') return false;
            arc.push_back(current);
        }
        if (content.empty()) {
            if (first < 2 && DecimalAtLeast(arc, 40)) return false;
            arc = AddDecimal(arc, static_cast<unsigned int>(first * 40));
        }
        const std::vector<unsigned char> encoded = Base128(arc);
        content.insert(content.end(), encoded.begin(), encoded.end());
    }
    return !content.empty();
}

const LibreSslAsn1Object* ObjectByName(std::string_view text) {
    if (text.empty()) return nullptr;
    for (int nid = 0; nid < LibreSslObjectCount; ++nid) {
        const LibreSslAsn1Object& object = LibreSslObjects_nid_no_patch[nid];
        if (object.shortName && text == object.shortName) return &object;
        if (object.longName && text == object.longName) return &object;
    }
    for (int nid = LibreSslObjectCount; nid < LibreSsl3::ObjectCount; ++nid) {
        const LibreSslAsn1Object* object = LibreSsl3::ObjectByNid(nid);
        if (!object) continue;
        if (object->shortName && text == object->shortName) return object;
        if (object->longName && text == object->longName) return object;
    }
    const std::string name(text);
    const int nid = OBJ_sn2nid_nid_postfix(name.c_str());
    if (nid == 0) return nullptr;
    return LibreSsl3::ObjectByNid(nid);
}

bool EncodeObject(std::string_view text, std::vector<unsigned char>& content) {
    if (const LibreSslAsn1Object* object = ObjectByName(text)) {
        if (object->length < 0) return false;
        if (object->length > 0 && !object->data) return false;
        if (object->length == 0) {
            content.clear();
            return true;
        }
        content.assign(object->data, object->data + object->length);
        return true;
    }
    return EncodeNumericObject(text, content);
}

bool IsValidUtf8(std::string_view text) {
    std::size_t index = 0;
    while (index < text.size()) {
        const unsigned char byte = static_cast<unsigned char>(text[index]);
        if (byte < 0x80) {
            ++index;
            continue;
        }
        std::size_t length = 0;
        unsigned int code = 0;
        unsigned int minimum = 0;
        if ((byte & 0xe0) == 0xc0) {
            length = 2;
            code = byte & 0x1f;
            minimum = 0x80;
        } else if ((byte & 0xf0) == 0xe0) {
            length = 3;
            code = byte & 0x0f;
            minimum = 0x800;
        } else if ((byte & 0xf8) == 0xf0) {
            length = 4;
            code = byte & 0x07;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (index + length > text.size()) return false;
        for (std::size_t offset = 1; offset < length; ++offset) {
            const unsigned char continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xc0) != 0x80) return false;
            code = (code << 6) | (continuation & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
        index += length;
    }
    return true;
}

std::vector<unsigned char> Latin1ToUtf8(std::string_view text) {
    std::vector<unsigned char> output;
    output.reserve(text.size());
    for (const char character : text) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte < 0x80) {
            output.push_back(byte);
        } else {
            output.push_back(static_cast<unsigned char>(0xc0 | (byte >> 6)));
            output.push_back(static_cast<unsigned char>(0x80 | (byte & 0x3f)));
        }
    }
    return output;
}

std::vector<unsigned char> BitStringContent(const std::vector<unsigned char>& bytes) {
    std::size_t length = bytes.size();
    while (length != 0 && bytes[length - 1] == 0) --length;
    unsigned int unused = 0;
    if (length != 0) {
        const unsigned char last = bytes[length - 1];
        while (((last >> unused) & 0x01) == 0 && unused < 7) ++unused;
    }
    std::vector<unsigned char> content;
    content.reserve(length + 1);
    content.push_back(static_cast<unsigned char>(unused));
    content.insert(content.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(length));
    if (length != 0) content.back() &= static_cast<unsigned char>(0xff << unused);
    return content;
}

struct BitName {
    int bit;
    const char* shortName;
    const char* longName;
};

constexpr BitName NsCertTypeNames[] = {
    {0, "client", "SSL Client"},
    {1, "server", "SSL Server"},
    {2, "email", "S/MIME"},
    {3, "objsign", "Object Signing"},
    {4, "reserved", "Unused"},
    {5, "sslCA", "SSL CA"},
    {6, "emailCA", "S/MIME CA"},
    {7, "objCA", "Object Signing CA"},
};

constexpr BitName KeyUsageNames[] = {
    {0, "digitalSignature", "Digital Signature"},
    {1, "nonRepudiation", "Non Repudiation"},
    {2, "keyEncipherment", "Key Encipherment"},
    {3, "dataEncipherment", "Data Encipherment"},
    {4, "keyAgreement", "Key Agreement"},
    {5, "keyCertSign", "Certificate Sign"},
    {6, "cRLSign", "CRL Sign"},
    {7, "encipherOnly", "Encipher Only"},
    {8, "decipherOnly", "Decipher Only"},
};

struct ListEntry {
    std::string name;
    std::optional<std::string> value;
};

bool StripSpaces(std::string_view text, std::string& result) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && IsSpace(text[begin])) ++begin;
    while (end > begin && IsSpace(text[end - 1])) --end;
    if (begin == end) return false;
    result.assign(text.substr(begin, end - begin));
    return true;
}

bool ParseList(std::string_view text, std::vector<ListEntry>& entries) {
    std::size_t limit = 0;
    while (limit < text.size() && text[limit] != '\r' && text[limit] != '\n') ++limit;
    text = text.substr(0, limit);
    std::string name;
    std::string value;
    bool inValue = false;
    std::size_t start = 0;
    for (std::size_t position = 0; position < text.size(); ++position) {
        const char current = text[position];
        if (!inValue) {
            if (current != ':' && current != ',') continue;
            if (!StripSpaces(text.substr(start, position - start), name)) return false;
            start = position + 1;
            if (current == ':') {
                inValue = true;
            } else {
                entries.push_back({name, std::nullopt});
            }
        } else if (current == ',') {
            if (!StripSpaces(text.substr(start, position - start), value)) return false;
            entries.push_back({name, value});
            name.clear();
            value.clear();
            inValue = false;
            start = position + 1;
        }
    }
    if (inValue) {
        if (!StripSpaces(text.substr(start), value)) return false;
        entries.push_back({name, value});
    } else {
        if (!StripSpaces(text.substr(start), name)) return false;
        entries.push_back({name, std::nullopt});
    }
    return true;
}

bool ListValue(void* conf, std::string_view text, std::vector<ListEntry>& entries) {
    if (!text.empty() && text[0] == '@') {
        if (conf) NotImplemented_nid_no_patch(__func__);
        PutError(InvalidExtensionString);
        return false;
    }
    if (!ParseList(text, entries)) {
        PutError(InvalidExtensionString);
        return false;
    }
    return true;
}

bool EncodeBitNames(void* conf, std::string_view text, const BitName* names, std::size_t count,
    std::vector<unsigned char>& der) {
    std::vector<ListEntry> entries;
    if (!ListValue(conf, text, entries)) return false;
    std::vector<int> bits;
    int highest = -1;
    for (const ListEntry& entry : entries) {
        bool found = false;
        for (std::size_t index = 0; index < count; ++index) {
            if (entry.name == names[index].shortName || entry.name == names[index].longName) {
                if (names[index].bit > highest) highest = names[index].bit;
                bits.push_back(names[index].bit);
                found = true;
                break;
            }
        }
        if (!found) {
            PutError(UnknownBitStringArgument);
            return false;
        }
    }
    std::vector<unsigned char> bytes;
    if (highest >= 0) {
        bytes.assign(static_cast<std::size_t>(highest) / 8 + 1, 0);
        for (const int bit : bits)
            bytes[static_cast<std::size_t>(bit) / 8] |= static_cast<unsigned char>(0x80 >> (bit % 8));
    }
    const std::vector<unsigned char> content = BitStringContent(bytes);
    AppendTlv(der, BitStringTag, content);
    return true;
}

bool EncodeExtendedKeyUsage(void* conf, std::string_view text, std::vector<unsigned char>& der) {
    std::vector<ListEntry> entries;
    if (!ListValue(conf, text, entries)) return false;
    std::vector<unsigned char> content;
    for (const ListEntry& entry : entries) {
        const std::string& objectName = entry.value ? *entry.value : entry.name;
        std::vector<unsigned char> object;
        if (!EncodeObject(objectName, object)) {
            PutError(InvalidObjectIdentifier);
            return false;
        }
        AppendTlv(content, ObjectTag, object);
    }
    AppendTlv(der, SequenceTag, content);
    return true;
}

enum class Format {
    Ascii,
    Utf8,
    Hex,
    BitList,
};

enum class SimpleType {
    Boolean,
    Integer,
    Enumerated,
    Null,
    Object,
    OctetString,
    BitString,
    Ia5String,
    Utf8String,
};

struct Asn1Value {
    SimpleType type = SimpleType::Null;
    Format format = Format::Ascii;
    std::string text;
};

bool LookupSimpleType(std::string_view name, SimpleType& type) {
    if (name == "BOOL" || name == "BOOLEAN") {
        type = SimpleType::Boolean;
    } else if (name == "INT" || name == "INTEGER") {
        type = SimpleType::Integer;
    } else if (name == "ENUM" || name == "ENUMERATED") {
        type = SimpleType::Enumerated;
    } else if (name == "NULL") {
        type = SimpleType::Null;
    } else if (name == "OID" || name == "OBJECT") {
        type = SimpleType::Object;
    } else if (name == "OCT" || name == "OCTETSTRING") {
        type = SimpleType::OctetString;
    } else if (name == "BITSTR" || name == "BITSTRING") {
        type = SimpleType::BitString;
    } else if (name == "IA5" || name == "IA5STRING") {
        type = SimpleType::Ia5String;
    } else if (name == "UTF8" || name == "UTF8String") {
        type = SimpleType::Utf8String;
    } else {
        return false;
    }
    return true;
}

bool IsUnsupportedTag(std::string_view name) {
    constexpr std::string_view Names[] = {
        "UTCTIME", "UTC", "GENERALIZEDTIME", "GENTIME", "UNIVERSALSTRING", "UNIV",
        "BMP", "BMPSTRING", "VISIBLESTRING", "VISIBLE", "PRINTABLESTRING", "PRINTABLE",
        "T61", "T61STRING", "TELETEXSTRING", "GeneralString", "GENSTR", "NUMERIC",
        "NUMERICSTRING", "SEQUENCE", "SEQ", "SET",
    };
    for (const std::string_view candidate : Names) {
        if (candidate == name) return true;
    }
    return false;
}

}
