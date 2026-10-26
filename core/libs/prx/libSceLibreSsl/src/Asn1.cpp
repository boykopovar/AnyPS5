#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace {

static_assert(sizeof(Asn1String) == 24);
static_assert(offsetof(Asn1String, data) == 8);
static_assert(offsetof(Asn1String, flags) == 16);

int CharacterWidth(int type) {
    switch (type) {
    case 12: return 0;
    case 19:
    case 20:
    case 22:
    case 23:
    case 24:
    case 26: return 1;
    case 28: return 4;
    case 30: return 2;
    default: return -1;
    }
}

bool ReadCharacter(std::span<const unsigned char>& input, int width, std::uint32_t& value) {
    value = 0;
    if (width != 0) {
        if (input.size() < static_cast<std::size_t>(width)) return false;
        for (int index = 0; index < width; ++index) value = (value << 8) | input[index];
        input = input.subspan(width);
    } else {
        const unsigned char lead = input.front();
        const int bytes = lead < 0x80 ? 1 : lead >= 0xc2 && lead <= 0xdf ? 2
            : lead >= 0xe0 && lead <= 0xef ? 3 : lead >= 0xf0 && lead <= 0xf4 ? 4 : 0;
        if (bytes == 0 || input.size() < static_cast<std::size_t>(bytes)) return false;
        value = lead & (0x7f >> (bytes == 1 ? 0 : bytes));
        for (int index = 1; index < bytes; ++index) {
            if ((input[index] & 0xc0) != 0x80) return false;
            value = (value << 6) | (input[index] & 0x3f);
        }
        constexpr std::uint32_t minimum[] = {0, 0, 0x80, 0x800, 0x10000};
        if (value < minimum[bytes]) return false;
        input = input.subspan(bytes);
    }
    return value <= 0x10ffff && (value < 0xd800 || value > 0xdfff);
}

void AppendCharacter(std::vector<unsigned char>& output, std::uint32_t value) {
    const int bytes = value < 0x80 ? 1 : value < 0x800 ? 2 : value < 0x10000 ? 3 : 4;
    if (output.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max() - bytes))
        NotImplemented_nid_no_patch("ASN1_STRING_to_UTF8: output length overflow");
    if (bytes == 1) {
        output.push_back(static_cast<unsigned char>(value));
        return;
    }
    constexpr unsigned char prefix[] = {0, 0, 0xc0, 0xe0, 0xf0};
    output.push_back(static_cast<unsigned char>(prefix[bytes] | (value >> (6 * (bytes - 1)))));
    for (int index = bytes - 2; index >= 0; --index)
        output.push_back(static_cast<unsigned char>(0x80 | ((value >> (6 * index)) & 0x3f)));
}

} // namespace

extern "C" {

std::int64_t APS5_VABI ASN1_INTEGER_get_nid_postfix(const Asn1String* value) {
    if (!value) return 0;
    const auto& integer = *value;
    if (integer.type != 2 && integer.type != 0x102) return -1;
    if (integer.length > 8) return -1;
    if (integer.length <= 0 || !integer.data) return 0;
    std::uint64_t magnitude = 0;
    for (int index = 0; index < integer.length; ++index) magnitude = (magnitude << 8) | integer.data[index];
    const bool negative = integer.type == 0x102;
    const auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + negative;
    if (magnitude > limit)
        NotImplemented_nid_no_patch(__func__);
    if (negative && magnitude != 0) return -static_cast<std::int64_t>(magnitude - 1) - 1;
    return static_cast<std::int64_t>(magnitude);
}

unsigned char* APS5_VABI ASN1_STRING_data_nid_postfix(Asn1String* value) {
    if (!value) NotImplemented_nid_no_patch(__func__);
    return value->data;
}

int APS5_VABI ASN1_STRING_length_nid_postfix(const Asn1String* value) {
    if (!value) NotImplemented_nid_no_patch(__func__);
    return value->length;
}

int APS5_VABI ASN1_STRING_type_nid_postfix(Asn1String* value) {
    if (!value) NotImplemented_nid_no_patch(__func__);
    return value->type;
}

int APS5_VABI ASN1_STRING_to_UTF8_nid_postfix(unsigned char** output, Asn1String* value) {
    if (!value) return -1;
    const auto& string = *value;
    const int width = CharacterWidth(string.type);
    if (width < 0) return -1;
    if (!output || (!string.data && string.length != 0)) NotImplemented_nid_no_patch(__func__);
    std::size_t length = string.length < 0 ? std::strlen(reinterpret_cast<const char*>(string.data))
        : static_cast<std::size_t>(string.length);
    if (length > static_cast<std::size_t>(std::numeric_limits<int>::max())) NotImplemented_nid_no_patch(__func__);
    std::span<const unsigned char> input(string.data, length);
    std::vector<unsigned char> converted;
    while (!input.empty()) {
        std::uint32_t character;
        if (!ReadCharacter(input, width, character)) NotImplemented_nid_no_patch(__func__);
        AppendCharacter(converted, character);
    }
    auto* result = static_cast<unsigned char*>(malloc_nid_postfix(converted.size() + 1));
    if (!result) NotImplemented_nid_no_patch(__func__);
    if (!converted.empty()) std::memcpy(result, converted.data(), converted.size());
    result[converted.size()] = 0;
    *output = result;
    return static_cast<int>(converted.size());
}

}
