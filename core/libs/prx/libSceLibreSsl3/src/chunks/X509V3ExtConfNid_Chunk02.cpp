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
bool ParseAsn1(std::string_view text, void* conf, Asn1Value& result) {
    Format format = Format::Ascii;
    std::size_t start = 0;
    for (;;) {
        const std::size_t comma = text.find(',', start);
        const std::size_t end = comma == std::string_view::npos ? text.size() : comma;
        std::size_t begin = start;
        std::size_t trimmed = end;
        while (begin < end && IsSpace(text[begin])) ++begin;
        while (trimmed > begin && IsSpace(text[trimmed - 1])) --trimmed;
        if (begin == trimmed) return false;
        const std::string_view element = text.substr(begin, trimmed - begin);
        const std::size_t colon = element.find(':');
        const std::string_view name = colon == std::string_view::npos ? element : element.substr(0, colon);
        const std::string_view value = colon == std::string_view::npos ? std::string_view() : element.substr(colon + 1);
        SimpleType type;
        if (LookupSimpleType(name, type)) {
            if (colon == std::string_view::npos && (comma != std::string_view::npos || trimmed != end)) return false;
            result.type = type;
            result.format = format;
            result.text.assign(value);
            return true;
        }
        if (name == "FORM" || name == "FORMAT") {
            if (colon == std::string_view::npos) return false;
            if (value.rfind("ASCII", 0) == 0) {
                format = Format::Ascii;
            } else if (value.rfind("UTF8", 0) == 0) {
                format = Format::Utf8;
            } else if (value.rfind("HEX", 0) == 0) {
                format = Format::Hex;
            } else if (value.rfind("BITLIST", 0) == 0) {
                format = Format::BitList;
            } else {
                return false;
            }
        } else if (name == "IMP" || name == "IMPLICIT" || name == "EXP" || name == "EXPLICIT" ||
                   name == "SEQWRAP" || name == "SETWRAP" || name == "BITWRAP" || name == "OCTWRAP") {
            if (colon == std::string_view::npos) return false;
            NotImplemented_nid_no_patch(__func__);
        } else if (IsUnsupportedTag(name)) {
            if ((name == "SEQ" || name == "SEQUENCE" || name == "SET") && !conf) return false;
            NotImplemented_nid_no_patch(__func__);
        } else {
            return false;
        }
        if (comma == std::string_view::npos) return false;
        start = comma + 1;
    }
}

bool ParseIntegerLiteral(std::string_view text, bool& negative, std::vector<unsigned char>& magnitude) {
    if (text.empty()) return false;
    if (text.front() == '-') {
        negative = true;
        text.remove_prefix(1);
    }
    if (text.empty()) return false;
    bool hexadecimal = false;
    if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        hexadecimal = true;
        text.remove_prefix(2);
    }
    if (text.empty()) return false;
    if (text.front() == '-') {
        negative = true;
        text.remove_prefix(1);
    }
    if (text.empty()) return false;
    for (const char digit : text) {
        if (hexadecimal ? HexDigit(digit) < 0 : (digit < '0' || digit > '9')) return false;
    }
    magnitude = hexadecimal ? HexBytes(text) : DecimalBytes(text);
    if (magnitude.size() == 1 && magnitude[0] == 0) negative = false;
    return true;
}

std::vector<unsigned char> IntegerContent(bool negative, const std::vector<unsigned char>& magnitude) {
    if (magnitude.empty() || (magnitude.size() == 1 && magnitude[0] == 0)) return {0};
    std::vector<unsigned char> content = magnitude;
    if (!negative) {
        if ((content[0] & 0x80) != 0) content.insert(content.begin(), 0);
        return content;
    }
    unsigned int carry = 1;
    for (std::size_t index = content.size(); index != 0; --index) {
        const unsigned int sum =
            static_cast<unsigned int>(static_cast<unsigned char>(~content[index - 1])) + carry;
        content[index - 1] = static_cast<unsigned char>(sum & 0xff);
        carry = sum >> 8;
    }
    if ((content[0] & 0x80) == 0) content.insert(content.begin(), 0xff);
    while (content.size() > 1 && content[0] == 0xff && (content[1] & 0x80) != 0)
        content.erase(content.begin());
    return content;
}

bool EncodeString(const Asn1Value& value, const std::string_view text, std::vector<unsigned char>& der) {
    switch (value.type) {
    case SimpleType::Boolean: {
        if (value.format != Format::Ascii) return false;
        unsigned char encoded = 0;
        if (text == "TRUE" || text == "true" || text == "Y" || text == "y" || text == "YES" || text == "yes") {
            encoded = 0xff;
        } else if (text == "FALSE" || text == "false" || text == "N" || text == "n" || text == "NO" || text == "no") {
            encoded = 0x00;
        } else {
            PutError(InvalidBooleanString);
            return false;
        }
        const std::array<unsigned char, 1> content{encoded};
        AppendTlv(der, BooleanTag, content);
        return true;
    }
    case SimpleType::Integer:
    case SimpleType::Enumerated: {
        if (value.format != Format::Ascii) return false;
        bool negative = false;
        std::vector<unsigned char> magnitude;
        if (!ParseIntegerLiteral(text, negative, magnitude)) return false;
        const std::vector<unsigned char> content = IntegerContent(negative, magnitude);
        AppendTlv(der, value.type == SimpleType::Integer ? IntegerTag : EnumeratedTag, content);
        return true;
    }
    case SimpleType::Null: {
        if (!text.empty()) return false;
        AppendTlv(der, NullTag, std::span<const unsigned char>());
        return true;
    }
    case SimpleType::Object: {
        if (value.format != Format::Ascii) return false;
        std::vector<unsigned char> content;
        if (!EncodeObject(text, content)) return false;
        AppendTlv(der, ObjectTag, content);
        return true;
    }
    case SimpleType::OctetString:
    case SimpleType::BitString: {
        std::vector<unsigned char> bytes;
        if (value.format == Format::Hex) {
            if (!ParseHex(text, bytes)) return false;
        } else if (value.format == Format::Ascii) {
            bytes.assign(text.begin(), text.end());
        } else if (value.format == Format::BitList && value.type == SimpleType::BitString) {
            if (text.empty()) return false;
            unsigned long long bit = 0;
            for (const char digit : text) {
                if (digit < '0' || digit > '9') return false;
                if (bit > (std::numeric_limits<unsigned long long>::max() - 9) / 10) return false;
                bit = bit * 10 + static_cast<unsigned long long>(digit - '0');
            }
            if (bit > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max() / 8)) return false;
            const std::size_t size = static_cast<std::size_t>(bit) / 8 + 1;
            bytes.assign(size, 0);
            bytes[static_cast<std::size_t>(bit) / 8] |=
                static_cast<unsigned char>(0x80 >> (static_cast<std::size_t>(bit) % 8));
        } else {
            return false;
        }
        if (value.type == SimpleType::OctetString) {
            AppendTlv(der, OctetStringTag, bytes);
        } else {
            const std::vector<unsigned char> content = BitStringContent(bytes);
            AppendTlv(der, BitStringTag, content);
        }
        return true;
    }
    case SimpleType::Ia5String: {
        if (value.format != Format::Ascii && value.format != Format::Utf8) return false;
        for (const char character : text) {
            if (static_cast<unsigned char>(character) >= 0x80) return false;
        }
        AppendTlv(der, Ia5StringTag, std::span<const unsigned char>(
            reinterpret_cast<const unsigned char*>(text.data()), text.size()));
        return true;
    }
    case SimpleType::Utf8String: {
        if (value.format == Format::Ascii) {
            const std::vector<unsigned char> content = Latin1ToUtf8(text);
            AppendTlv(der, Utf8StringTag, content);
            return true;
        }
        if (value.format != Format::Utf8 || !IsValidUtf8(text)) return false;
        AppendTlv(der, Utf8StringTag, std::span<const unsigned char>(
            reinterpret_cast<const unsigned char*>(text.data()), text.size()));
        return true;
    }
    }
    return false;
}

bool ConsumeCritical(std::string_view& text) {
    if (text.size() < 9 || text.substr(0, 9) != "critical,") return false;
    text.remove_prefix(9);
    while (!text.empty() && IsSpace(text.front())) text.remove_prefix(1);
    return true;
}

int ConsumeGeneric(std::string_view& text) {
    int type = 0;
    if (text.size() >= 4 && text.substr(0, 4) == "DER:") {
        text.remove_prefix(4);
        type = 1;
    } else if (text.size() >= 5 && text.substr(0, 5) == "ASN1:") {
        text.remove_prefix(5);
        type = 2;
    } else {
        return 0;
    }
    while (!text.empty() && IsSpace(text.front())) text.remove_prefix(1);
    return type;
}

const LibreSslAsn1Object* NidObject(int nid) {
    if (nid <= 0) return nullptr;
    return LibreSsl3::ObjectByNid(nid);
}

LibreSsl3X509Extension* BuildExtension(int nid, bool critical, const std::vector<unsigned char>& der) {
    const LibreSslAsn1Object* object = NidObject(nid);
    if (!object) return nullptr;
    if (der.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) return nullptr;
    auto* value = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!value) return nullptr;
    auto* data = static_cast<unsigned char*>(calloc_nid_postfix(der.size() + 1, 1));
    if (!data) {
        free_nid_postfix(value);
        return nullptr;
    }
    if (!der.empty()) std::memcpy(data, der.data(), der.size());
    auto* extension =
        static_cast<LibreSsl3X509Extension*>(calloc_nid_postfix(1, sizeof(LibreSsl3X509Extension)));
    if (!extension) {
        ASN1_OCTET_STRING_free_nid_postfix(value);
        return nullptr;
    }
    extension->object = object;
    extension->critical = critical ? 1 : 0;
    extension->value = value;
    return extension;
}

bool IsStandardExtension(int nid) {
    constexpr std::array<int, 42> Nids = {
        71, 72, 73, 74, 75, 76, 77, 78, 82, 83, 84, 85, 86, 87, 88, 89, 90, 103, 126, 140, 141,
        142, 143, 177, 290, 291, 366, 367, 368, 369, 370, 371, 398, 401, 430, 663, 666, 747, 748,
        770, 771, 857,
    };
    for (const int candidate : Nids) {
        if (candidate == nid) return true;
    }
    return false;
}

LibreSsl3X509Extension* PlainExtension(void* conf, int nid, bool critical, std::string_view text) {
    if (nid == 0) {
        PutError(UnknownExtensionName);
        return nullptr;
    }
    if (!IsStandardExtension(nid)) {
        PutError(UnknownExtension);
        return nullptr;
    }
    std::vector<unsigned char> der;
    switch (nid) {
    case 71:
        if (!EncodeBitNames(conf, text, NsCertTypeNames, std::size(NsCertTypeNames), der)) return nullptr;
        break;
    case 83:
        if (!EncodeBitNames(conf, text, KeyUsageNames, std::size(KeyUsageNames), der)) return nullptr;
        break;
    case 126:
        if (!EncodeExtendedKeyUsage(conf, text, der)) return nullptr;
        break;
    default:
        NotImplemented_nid_no_patch(__func__);
    }
    return BuildExtension(nid, critical, der);
}

}

extern "C" LibreSsl3X509Extension* APS5_VABI X509V3_EXT_conf_nid_nid_postfix(void* conf,
    LibreSsl3X509V3Context* ctx, int ext_nid, const char* value) {
    (void)ctx;
    if (!value) return nullptr;
    std::string_view text(value);
    const bool critical = ConsumeCritical(text);
    const int generic = ConsumeGeneric(text);
    if (generic == 1) {
        std::vector<unsigned char> der;
        if (!ParseHex(text, der)) {
            PutError(ExtensionValueError);
            return nullptr;
        }
        if (!NidObject(ext_nid)) {
            PutError(ExtensionNameError);
            return nullptr;
        }
        return BuildExtension(ext_nid, critical, der);
    }
    if (generic == 2) {
        Asn1Value parsed;
        std::vector<unsigned char> der;
        if (!ParseAsn1(text, conf, parsed) || !EncodeString(parsed, parsed.text, der)) {
            PutError(ExtensionValueError);
            return nullptr;
        }
        if (!NidObject(ext_nid)) {
            PutError(ExtensionNameError);
            return nullptr;
        }
        return BuildExtension(ext_nid, critical, der);
    }
    return PlainExtension(conf, ext_nid, critical, text);
}
