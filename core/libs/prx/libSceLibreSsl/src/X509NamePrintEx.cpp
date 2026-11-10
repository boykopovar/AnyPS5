#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t Escape2253 = 0x1;
constexpr std::uint64_t EscapeControl = 0x2;
constexpr std::uint64_t EscapeMsb = 0x4;
constexpr std::uint64_t EscapeQuote = 0x8;
constexpr std::uint64_t ConvertUtf8 = 0x10;
constexpr std::uint64_t IgnoreType = 0x20;
constexpr std::uint64_t ShowType = 0x40;
constexpr std::uint64_t DumpAll = 0x80;
constexpr std::uint64_t DumpUnknown = 0x100;
constexpr std::uint64_t DumpDer = 0x200;
constexpr unsigned FirstEscape2253 = 0x20;
constexpr unsigned LastEscape2253 = 0x40;
constexpr unsigned BackslashEscape = Escape2253 | FirstEscape2253 | LastEscape2253;
constexpr unsigned EscapeFlags = Escape2253 | EscapeQuote | EscapeControl | EscapeMsb;

constexpr std::uint64_t SeparatorMask = 0xfull << 16;
constexpr std::uint64_t SeparatorCommaPlus = 1ull << 16;
constexpr std::uint64_t SeparatorCommaPlusSpace = 2ull << 16;
constexpr std::uint64_t SeparatorSemicolonPlusSpace = 3ull << 16;
constexpr std::uint64_t SeparatorMultiline = 4ull << 16;
constexpr std::uint64_t ReverseOrder = 1ull << 20;
constexpr std::uint64_t FieldMask = 3ull << 21;
constexpr std::uint64_t FieldShortName = 0;
constexpr std::uint64_t FieldLongName = 1ull << 21;
constexpr std::uint64_t FieldOid = 2ull << 21;
constexpr std::uint64_t FieldNone = 3ull << 21;
constexpr std::uint64_t SpaceAroundEquals = 1ull << 23;
constexpr std::uint64_t DumpUnknownFields = 1ull << 24;
constexpr std::uint64_t FieldAlign = 1ull << 25;

constexpr int WidthMask = 0x7;
constexpr int ConvertToUtf8 = 0x8;
constexpr std::uint32_t UnicodeMax = 0x10ffff;
constexpr int FieldWidthLongName = 25;
constexpr int FieldWidthShortName = 10;
constexpr int X509Library = 11;
constexpr int AnyFunction = 0xfff;
constexpr int BufferLibraryReason = 7;
constexpr int TypeBoolean = 1;
constexpr int TypeInteger = 2;
constexpr int TypeBitString = 3;
constexpr int TypeNull = 5;
constexpr int TypeObject = 6;
constexpr int TypeEnumerated = 10;
constexpr int TypeSequence = 16;
constexpr int TypeSet = 17;
constexpr int TypeOther = -3;
constexpr int NoUndefinedNid = 0;
constexpr std::int64_t BitStringBitsLeft = 0x8;
constexpr std::int64_t BitStringBitsMask = 0x7;
constexpr std::int64_t Negative = 0x100;

constexpr signed char TagWidth[] = {
    -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1,
    -1, -1, 0, -1,
    -1, -1, -1, -1,
    -1, 1, 1,
    -1, 1, 1, 1,
    -1, 1, -1,
    4, -1, 2,
};

constexpr const char* TagNames[] = {
    "EOC", "BOOLEAN", "INTEGER", "BIT STRING", "OCTET STRING",
    "NULL", "OBJECT", "OBJECT DESCRIPTOR", "EXTERNAL", "REAL",
    "ENUMERATED", "<ASN1 11>", "UTF8STRING", "<ASN1 13>",
    "<ASN1 14>", "<ASN1 15>", "SEQUENCE", "SET",
    "NUMERICSTRING", "PRINTABLESTRING", "T61STRING",
    "VIDEOTEXSTRING", "IA5STRING", "UTCTIME", "GENERALIZEDTIME",
    "GRAPHICSTRING", "VISIBLESTRING", "GENERALSTRING",
    "UNIVERSALSTRING", "<ASN1 29>", "BMPSTRING",
};

struct Entry {
    LibreSslAsn1Object* object;
    Asn1String* value;
    std::int32_t set;
    std::int32_t size;
};

static_assert(sizeof(Entry) == sizeof(LibreSslX509NameEntry));
static_assert(offsetof(Entry, set) == offsetof(LibreSslX509NameEntry, set));

bool Put(LibreSsl::Bio* bio, const void* data, int length) {
    if (!bio) return true;
    return LibreSslBioWrite_nid_no_patch(bio, data, length) == length;
}

bool PutIndent(LibreSsl::Bio* bio, int count) {
    for (int index = 0; index < count; ++index) {
        if (!Put(bio, " ", 1)) return false;
    }
    return true;
}

constexpr unsigned char CharTypes[] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    120, 0, 1, 40, 0, 0, 0, 16, 16, 16, 0, 25, 25, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 9, 9, 16, 9, 16,
    0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 1, 0, 0, 0,
    0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 2
};

unsigned CharacterType(unsigned char character) {
    return CharTypes[character];
}

std::string HexDigits(std::uint64_t value, int width) {
    constexpr char Digits[] = "0123456789ABCDEF";
    std::string text(static_cast<std::size_t>(width), '0');
    for (int position = width - 1; position >= 0 && value != 0; --position) {
        text[static_cast<std::size_t>(position)] = Digits[value & 0xf];
        value >>= 4;
    }
    return text;
}

int EscapeCharacter(std::uint64_t character, unsigned char flags, bool* needQuotes, LibreSsl::Bio* bio) {
    if (character > 0xffffffffull) return -1;
    if (character > 0xffff) {
        const std::string text = "\\W" + HexDigits(character, 8);
        return Put(bio, text.data(), 10) ? 10 : -1;
    }
    if (character > 0xff) {
        const std::string text = "\\U" + HexDigits(character, 4);
        return Put(bio, text.data(), 6) ? 6 : -1;
    }
    const auto byte = static_cast<unsigned char>(character);
    unsigned selected;
    if (byte > 0x7f) selected = flags & EscapeMsb;
    else selected = CharacterType(byte) & flags;
    if (selected & BackslashEscape) {
        if (selected & EscapeQuote) {
            if (needQuotes) *needQuotes = true;
            return Put(bio, &byte, 1) ? 1 : -1;
        }
        if (!Put(bio, "\\", 1)) return -1;
        return Put(bio, &byte, 1) ? 2 : -1;
    }
    if (selected & (EscapeControl | EscapeMsb)) {
        const std::string text = "\\" + HexDigits(byte, 2);
        return Put(bio, text.data(), 3) ? 3 : -1;
    }
    if (byte == '\\' && (flags & EscapeFlags)) return Put(bio, "\\\\", 2) ? 2 : -1;
    return Put(bio, &byte, 1) ? 1 : -1;
}

int ReadUtf8(const unsigned char* text, int length, std::uint32_t& value) {
    if (length <= 0) return 0;
    const unsigned char first = text[0];
    if ((first & 0x80) == 0) {
        value = first & 0x7f;
        return 1;
    }
    if ((first & 0xe0) == 0xc0) {
        if (first < 0xc2) return -2;
        if (length < 2) return -1;
        if ((text[1] & 0xc0) != 0x80) return -3;
        value = (static_cast<std::uint32_t>(first & 0x1f) << 6) | (text[1] & 0x3f);
        if (value < 0x80) return -4;
        return 2;
    }
    if ((first & 0xf0) == 0xe0) {
        if (length < 3) return -1;
        if ((text[1] & 0xc0) != 0x80 || (text[2] & 0xc0) != 0x80) return -3;
        value = (static_cast<std::uint32_t>(first & 0xf) << 12) | (static_cast<std::uint32_t>(text[1] & 0x3f) << 6) | (text[2] & 0x3f);
        if (value < 0x800) return -4;
        if (value >= 0xd800 && value < 0xe000) return -2;
        return 3;
    }
    if ((first & 0xf8) == 0xf0 && first < 0xf5) {
        if (length < 4) return -1;
        if ((text[1] & 0xc0) != 0x80 || (text[2] & 0xc0) != 0x80 || (text[3] & 0xc0) != 0x80) return -3;
        value = (static_cast<std::uint32_t>(first & 0x7) << 18) | (static_cast<std::uint32_t>(text[1] & 0x3f) << 12) |
            (static_cast<std::uint32_t>(text[2] & 0x3f) << 6) | (text[3] & 0x3f);
        if (value < 0x10000) return -4;
        if (value > UnicodeMax) return -2;
        return 4;
    }
    return -2;
}

int WriteUtf8(unsigned char* output, std::uint32_t value) {
    if (value < 0x80) {
        output[0] = static_cast<unsigned char>(value);
        return 1;
    }
    if (value < 0x800) {
        output[0] = static_cast<unsigned char>(((value >> 6) & 0x1f) | 0xc0);
        output[1] = static_cast<unsigned char>((value & 0x3f) | 0x80);
        return 2;
    }
    if (value < 0x10000) {
        if (value >= 0xd800 && value < 0xe000) return -2;
        output[0] = static_cast<unsigned char>(((value >> 12) & 0xf) | 0xe0);
        output[1] = static_cast<unsigned char>(((value >> 6) & 0x3f) | 0x80);
        output[2] = static_cast<unsigned char>((value & 0x3f) | 0x80);
        return 3;
    }
    if (value <= UnicodeMax) {
        output[0] = static_cast<unsigned char>(((value >> 18) & 0x7) | 0xf0);
        output[1] = static_cast<unsigned char>(((value >> 12) & 0x3f) | 0x80);
        output[2] = static_cast<unsigned char>(((value >> 6) & 0x3f) | 0x80);
        output[3] = static_cast<unsigned char>((value & 0x3f) | 0x80);
        return 4;
    }
    return -2;
}

int PrintBuffer(const unsigned char* buffer, int length, int type, unsigned char flags, bool* needQuotes, LibreSsl::Bio* bio) {
    if (length == 0) return 0;
    const unsigned char* position = buffer;
    const unsigned char* end = buffer + length;
    int written = 0;
    while (position != end) {
        unsigned orFlags = 0;
        if (position == buffer && (flags & Escape2253)) orFlags = FirstEscape2253;
        std::uint32_t character;
        switch (type & WidthMask) {
        case 4:
            if (end - position < 4) return -1;
            character = static_cast<std::uint32_t>(position[0]) << 24 | static_cast<std::uint32_t>(position[1]) << 16 |
                static_cast<std::uint32_t>(position[2]) << 8 | position[3];
            position += 4;
            if (character > UnicodeMax || (character >= 0xd800 && character < 0xe000)) return -1;
            break;
        case 2:
            if (end - position < 2) return -1;
            character = static_cast<std::uint32_t>(position[0]) << 8 | position[1];
            position += 2;
            if (character >= 0xd800 && character < 0xe000) return -1;
            break;
        case 1:
            character = *position++;
            break;
        case 0: {
            const int consumed = ReadUtf8(position, static_cast<int>(end - position), character);
            if (consumed < 0) return -1;
            position += consumed;
            break;
        }
        default:
            return -1;
        }
        if (position == end && (flags & Escape2253)) orFlags = LastEscape2253;
        if (type & ConvertToUtf8) {
            unsigned char converted[6];
            const int converted_length = WriteUtf8(converted, character);
            if (converted_length < 0) return -1;
            for (int index = 0; index < converted_length; ++index) {
                const int count = EscapeCharacter(converted[index], static_cast<unsigned char>(flags | orFlags), needQuotes, bio);
                if (count < 0) return -1;
                written += count;
            }
        } else {
            const int count = EscapeCharacter(character, static_cast<unsigned char>(flags | orFlags), needQuotes, bio);
            if (count < 0) return -1;
            written += count;
        }
    }
    return written;
}

int HexDump(LibreSsl::Bio* bio, const unsigned char* buffer, int length) {
    constexpr char Digits[] = "0123456789ABCDEF";
    if (bio) {
        for (int index = 0; index < length; ++index) {
            const char pair[2] = {Digits[buffer[index] >> 4], Digits[buffer[index] & 0xf]};
            if (!Put(bio, pair, 2)) return -1;
        }
    }
    return length << 1;
}

std::vector<unsigned char> IntegerContent(const Asn1String& value) {
    if (value.length < 0) throw std::invalid_argument("X509_NAME_print_ex: negative integer length");
    if (value.length == 0) return {0};
    if (!value.data) throw std::invalid_argument("X509_NAME_print_ex: integer without contents");
    const bool negative = (value.type & Negative) != 0;
    const int first = value.data[0];
    int pad = 0;
    unsigned char padByte = 0;
    if (!negative && first > 0x7f) {
        pad = 1;
    } else if (negative) {
        if (first > 0x80) {
            pad = 1;
            padByte = 0xff;
        } else if (first == 0x80) {
            for (int index = 1; index < value.length; ++index) {
                if (value.data[index] != 0) {
                    pad = 1;
                    padByte = 0xff;
                    break;
                }
            }
        }
    }
    std::vector<unsigned char> content(static_cast<std::size_t>(pad + value.length));
    if (pad) content[0] = padByte;
    if (!negative) {
        if (value.length > 0) std::memcpy(content.data() + pad, value.data, static_cast<std::size_t>(value.length));
        return content;
    }
    int remaining = value.length;
    int index = value.length - 1;
    int output = pad + value.length - 1;
    while (remaining > 0 && value.data[index] == 0) {
        content[static_cast<std::size_t>(output--)] = 0;
        --index;
        --remaining;
    }
    if (remaining > 0) {
        content[static_cast<std::size_t>(output--)] = static_cast<unsigned char>((value.data[index--] ^ 0xff) + 1);
        --remaining;
    }
    while (remaining > 0) {
        content[static_cast<std::size_t>(output--)] = static_cast<unsigned char>(value.data[index--] ^ 0xff);
        --remaining;
    }
    return content;
}

std::vector<unsigned char> BitStringContent(const Asn1String& value) {
    int length = value.length;
    if (length < 0) throw std::invalid_argument("X509_NAME_print_ex: negative bit string length");
    int bits = 0;
    if (length > 0) {
        if (!value.data) throw std::invalid_argument("X509_NAME_print_ex: bit string without contents");
        if (value.flags & BitStringBitsLeft) {
            bits = static_cast<int>(value.flags & BitStringBitsMask);
        } else {
            while (length > 0 && value.data[length - 1] == 0) --length;
            if (length == 0) {
                return {0};
            }
            unsigned char last = value.data[length - 1];
            bits = last & 1 ? 0 : last & 2 ? 1 : last & 4 ? 2 : last & 8 ? 3 :
                last & 0x10 ? 4 : last & 0x20 ? 5 : last & 0x40 ? 6 : 7;
        }
    }
    std::vector<unsigned char> content(1 + static_cast<std::size_t>(length));
    content[0] = static_cast<unsigned char>(bits);
    if (length > 0) {
        std::memcpy(content.data() + 1, value.data, static_cast<std::size_t>(length));
        content.back() &= static_cast<unsigned char>(0xff << bits);
    }
    return content;
}

std::vector<unsigned char> StringContent(const Asn1String& value, const char* function) {
    if (value.length < 0 || (value.length > 0 && !value.data))
        throw std::invalid_argument(std::string(function) + ": invalid string contents");
    std::vector<unsigned char> content;
    if (value.length > 0) content.assign(value.data, value.data + value.length);
    return content;
}

std::vector<unsigned char> EncodeDer(const Asn1String& value, const char* function) {
    const int type = value.type;
    std::vector<unsigned char> content;
    bool includeHeader = true;
    switch (type) {
    case TypeBoolean: {
        const auto low = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&value));
        if (low == 0xffffffffu) return {};
        content = {static_cast<unsigned char>(low & 0xffu)};
        break;
    }
    case TypeObject:
        throw std::invalid_argument(std::string(function) + ": unsupported DER type");
    case TypeNull:
        break;
    case TypeInteger:
        content = IntegerContent(value);
        break;
    case TypeEnumerated:
        content = IntegerContent(value);
        break;
    case TypeBitString:
        content = BitStringContent(value);
        break;
    case TypeSequence:
    case TypeSet:
    case TypeOther:
        content = StringContent(value, function);
        includeHeader = false;
        break;
    default:
        content = StringContent(value, function);
        break;
    }
    if (content.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::length_error(std::string(function) + ": DER length overflow");
    std::vector<unsigned char> der;
    if (includeHeader) {
        if (type >= 31) {
            der.push_back(0x1f);
            int groups = 0;
            for (int remaining = type; remaining > 0; remaining >>= 7) ++groups;
            if (groups == 0) groups = 1;
            const std::size_t base = der.size();
            der.resize(base + static_cast<std::size_t>(groups));
            int remaining = type;
            for (int index = groups - 1; index >= 0; --index) {
                der[base + static_cast<std::size_t>(index)] = static_cast<unsigned char>(remaining & 0x7f);
                if (index != groups - 1) der[base + static_cast<std::size_t>(index)] |= 0x80;
                remaining >>= 7;
            }
        } else {
            der.push_back(static_cast<unsigned char>(type & 0x1f));
        }
        const std::size_t length = content.size();
        if (length <= 127) {
            der.push_back(static_cast<unsigned char>(length));
        } else {
            int bytes = 0;
            for (std::size_t remaining = length; remaining > 0; remaining >>= 8) ++bytes;
            der.push_back(static_cast<unsigned char>(bytes | 0x80));
            const std::size_t base = der.size();
            der.resize(base + static_cast<std::size_t>(bytes));
            std::size_t remaining = length;
            for (int index = bytes - 1; index >= 0; --index) {
                der[base + static_cast<std::size_t>(index)] = static_cast<unsigned char>(remaining & 0xff);
                remaining >>= 8;
            }
        }
    }
    der.insert(der.end(), content.begin(), content.end());
    return der;
}

int DumpString(std::uint64_t flags, LibreSsl::Bio* bio, const Asn1String& value, const char* function) {
    if (!Put(bio, "#", 1)) return -1;
    if (!(flags & DumpDer)) {
        const int count = HexDump(bio, value.data, value.length);
        return count < 0 ? -1 : count + 1;
    }
    const std::vector<unsigned char> der = EncodeDer(value, function);
    const int count = HexDump(bio, der.data(), static_cast<int>(der.size()));
    return count < 0 ? -1 : count + 1;
}

const char* TagName(int tag) {
    if (tag == 258 || tag == 266) tag &= ~0x100;
    if (tag < 0 || tag > 30) return "(unknown)";
    return TagNames[tag];
}

int PrintString(LibreSsl::Bio* bio, std::uint64_t flagsWide, const Asn1String& value, const char* function) {
    bool needQuotes = false;
    const auto flags = static_cast<unsigned char>(flagsWide & EscapeFlags);
    int type = value.type;
    int written = 0;
    if (value.length < 0 || (value.length > 0 && !value.data)) throw std::invalid_argument(std::string(function) + ": invalid string contents");
    if (flagsWide & ShowType) {
        const char* name = TagName(type);
        written += static_cast<int>(std::strlen(name));
        if (!Put(bio, name, written) || !Put(bio, ":", 1)) return -1;
        ++written;
    }
    if (flagsWide & DumpAll) {
        type = -1;
    } else if (flagsWide & IgnoreType) {
        type = 1;
    } else {
        if (type > 0 && type < 31) type = TagWidth[type];
        else type = -1;
        if (type == -1 && !(flagsWide & DumpUnknown)) type = 1;
    }
    if (type == -1) {
        const int count = DumpString(flagsWide, bio, value, function);
        if (count < 0) return -1;
        return written + count;
    }
    if (flagsWide & ConvertUtf8) {
        if (!type) type = 1;
        else type |= ConvertToUtf8;
    }
    const int count = PrintBuffer(value.data, value.length, type, flags, &needQuotes, nullptr);
    if (count < 0) return -1;
    written += count;
    if (needQuotes) written += 2;
    if (!bio) return written;
    if (needQuotes && !Put(bio, "\"", 1)) return -1;
    if (PrintBuffer(value.data, value.length, type, flags, nullptr, bio) < 0) return -1;
    if (needQuotes && !Put(bio, "\"", 1)) return -1;
    return written;
}

int PrintCompat(LibreSsl::Bio* bio, const LibreSslX509Name* name) {
    char* text = X509_NAME_oneline_nid_postfix(name, nullptr, 0);
    if (!text) return 0;
    if (*text == '\0') {
        free_nid_postfix(text);
        return 1;
    }
    const char* scan = text + 1;
    const char* start = scan;
    bool failed = false;
    for (;;) {
        const bool boundary = (scan[0] == '/' && scan[1] >= 'A' && scan[1] <= 'Z' &&
            (scan[2] == '=' || (scan[2] >= 'A' && scan[2] <= 'Z' && scan[3] == '='))) || scan[0] == '\0';
        if (boundary) {
            const int length = static_cast<int>(scan - start);
            if (LibreSslBioWrite_nid_no_patch(bio, start, length) != length) {
                failed = true;
                break;
            }
            start = scan + 1;
            if (scan[0] != '\0' && LibreSslBioWrite_nid_no_patch(bio, ", ", 2) != 2) {
                failed = true;
                break;
            }
        }
        if (scan[0] == '\0') break;
        ++scan;
    }
    if (failed) LibreSslPutError_nid_no_patch(X509Library, AnyFunction, BufferLibraryReason);
    free_nid_postfix(text);
    return failed ? 0 : 1;
}

}

extern "C" {

int APS5_VABI X509_NAME_print_ex_nid_postfix(LibreSsl::Bio* bio, const LibreSslX509Name* name, int indent, std::uint64_t flags) {
    if (flags == 0) return PrintCompat(bio, name);
    if (indent < 0) indent = 0;
    int written = indent;
    if (!PutIndent(bio, indent)) return -1;
    const char* separatorDn;
    const char* separatorMulti;
    switch (flags & SeparatorMask) {
    case SeparatorMultiline:
        separatorDn = "\n";
        separatorMulti = " + ";
        break;
    case SeparatorCommaPlus:
        separatorDn = ",";
        separatorMulti = "+";
        indent = 0;
        break;
    case SeparatorCommaPlusSpace:
        separatorDn = ", ";
        separatorMulti = " + ";
        indent = 0;
        break;
    case SeparatorSemicolonPlusSpace:
        separatorDn = "; ";
        separatorMulti = " + ";
        indent = 0;
        break;
    default:
        return -1;
    }
    const int separatorDnLength = static_cast<int>(std::strlen(separatorDn));
    const int separatorMultiLength = static_cast<int>(std::strlen(separatorMulti));
    const char* equals = (flags & SpaceAroundEquals) ? " = " : "=";
    const int equalsLength = static_cast<int>(std::strlen(equals));
    const std::uint64_t nameOption = flags & FieldMask;
    const LibreSslStack* entries = name ? name->entries : nullptr;
    const int count = entries ? entries->num : -1;
    int previous = -1;
    for (int index = 0; index < count; ++index) {
        const int position = (flags & ReverseOrder) ? count - index - 1 : index;
        if (!entries->data) throw std::invalid_argument("X509_NAME_print_ex: null entry array");
        const auto* entry = reinterpret_cast<const Entry*>(entries->data[position]);
        if (!entry || !entry->object || !entry->value) throw std::invalid_argument("X509_NAME_print_ex: null name entry, object or value");
        if (previous != -1) {
            if (previous == entry->set) {
                if (!Put(bio, separatorMulti, separatorMultiLength)) return -1;
                written += separatorMultiLength;
            } else {
                if (!Put(bio, separatorDn, separatorDnLength)) return -1;
                written += separatorDnLength;
                if (!PutIndent(bio, indent)) return -1;
                written += indent;
            }
        }
        previous = entry->set;
        const int nid = LibreSslObjectNid_nid_no_patch(entry->object);
        if (nameOption != FieldNone) {
            char numeric[80];
            const char* text;
            int fieldWidth;
            if (nameOption == FieldOid || nid == NoUndefinedNid) {
                LibreSslObjectNumericText_nid_no_patch(numeric, sizeof(numeric), entry->object);
                fieldWidth = 0;
                text = numeric;
            } else if (nameOption == FieldShortName) {
                fieldWidth = FieldWidthShortName;
                text = LibreSslObjectShortName_nid_no_patch(nid);
            } else if (nameOption == FieldLongName) {
                fieldWidth = FieldWidthLongName;
                text = LibreSslObjectLongName_nid_no_patch(nid);
            } else {
                fieldWidth = 0;
                text = "";
            }
            if (!text) throw std::invalid_argument("X509_NAME_print_ex: object has no name");
            const int textLength = static_cast<int>(std::strlen(text));
            if (!Put(bio, text, textLength)) return -1;
            if (textLength < fieldWidth && (flags & FieldAlign)) {
                if (!PutIndent(bio, fieldWidth - textLength)) return -1;
                written += fieldWidth - textLength;
            }
            if (!Put(bio, equals, equalsLength)) return -1;
            written += textLength + equalsLength;
        }
        const std::uint64_t extra = (nid == NoUndefinedNid && (flags & DumpUnknownFields)) ? DumpAll : 0;
        const int length = PrintString(bio, flags | extra, *entry->value, "X509_NAME_print_ex");
        if (length < 0) return -1;
        written += length;
    }
    return written;
}

}
