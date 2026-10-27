#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>


namespace {


static_assert(sizeof(Asn1String) == 24);
static_assert(offsetof(Asn1String, data) == 8);
static_assert(offsetof(Asn1String, flags) == 16);

unsigned char ReadByte(std::span<const unsigned char>& input) {
    if (input.empty()) NotImplemented_nid_no_patch("d2i_ASN1_INTEGER: truncated header");
    const auto byte = input.front();
    input = input.subspan(1);
    return byte;
}

std::span<const unsigned char> ReadInteger(std::span<const unsigned char>& input) {
    const auto identifier = ReadByte(input);
    if ((identifier & 0xe0) != 0) NotImplemented_nid_no_patch("d2i_ASN1_INTEGER: nonprimitive integer");
    std::uint32_t tag = identifier & 0x1f;
    if (tag == 0x1f) {
        tag = 0;
        unsigned char byte;
        do {
            byte = ReadByte(input);
            if (tag > (std::numeric_limits<std::uint32_t>::max() >> 7))
                NotImplemented_nid_no_patch("d2i_ASN1_INTEGER: tag overflow");
            tag = (tag << 7) | (byte & 0x7f);
        } while (byte & 0x80);
    }
    if (tag != 2) NotImplemented_nid_no_patch("d2i_ASN1_INTEGER: wrong tag");
    const auto lengthByte = ReadByte(input);
    std::size_t length = lengthByte;
    if (lengthByte >= 0x80) {
        const auto bytes = lengthByte & 0x7f;
        if (bytes == 0 || bytes == 0x7f) NotImplemented_nid_no_patch("d2i_ASN1_INTEGER: invalid length");
        length = 0;
        for (int index = 0; index < bytes; ++index) {
            if (length > (std::numeric_limits<std::size_t>::max() >> 8))
                NotImplemented_nid_no_patch("d2i_ASN1_INTEGER: length overflow");
            length = (length << 8) | ReadByte(input);
        }
    }
    if (length == 0 || length > input.size() || length > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        NotImplemented_nid_no_patch("d2i_ASN1_INTEGER: invalid content length");
    const auto content = input.first(length);
    input = input.subspan(length);
    return content;
}

void FreeInteger(void* value) {
    if (!value) return;
    auto& integer = *static_cast<Asn1String*>(value);
    if (!(integer.flags & 0x10)) {
        if (integer.data) {
            if (integer.length < 0) NotImplemented_nid_no_patch(__func__);
            auto* bytes = static_cast<volatile unsigned char*>(integer.data);
            for (int index = 0; index < integer.length; ++index) bytes[index] = 0;
            free_nid_postfix(integer.data);
        }
    }
    integer.flags &= ~std::int64_t{0x10};
    integer.data = nullptr;
    integer.length = 0;
    free_nid_postfix(value);
}


} // namespace

extern "C" {

void* APS5_VABI d2i_ASN1_INTEGER_nid_postfix(void** output, const unsigned char** input, std::int64_t length) {
    if (length < 0) return nullptr;
    if (!input || !*input) NotImplemented_nid_no_patch(__func__);
    std::span<const unsigned char> remaining(*input, static_cast<std::size_t>(length));
    auto content = ReadInteger(remaining);
    const bool negative = (content.front() & 0x80) != 0;
    const unsigned char padding = negative ? 0xff : 0;
    if (content.size() > 1 && content.front() == padding) {
        bool keepPadding = negative;
        for (std::size_t index = 1; index < content.size(); ++index)
            keepPadding = keepPadding && content[index] == 0;
        if (!keepPadding) {
            content = content.subspan(1);
            if (((content.front() & 0x80) != 0) == negative)
                NotImplemented_nid_no_patch("d2i_ASN1_INTEGER: redundant sign padding");
        }
    }
    if (output) {
        FreeInteger(*output);
        *output = nullptr;
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(content.size()));
    if (!data) NotImplemented_nid_no_patch("d2i_ASN1_INTEGER: allocation failure");
    std::memcpy(data, content.data(), content.size());
    if (negative) {
        unsigned int carry = 1;
        for (std::size_t index = content.size(); index != 0; --index) {
            const unsigned int digit = (data[index - 1] ^ 0xff) + carry;
            data[index - 1] = static_cast<unsigned char>(digit);
            carry = digit >> 8;
        }
    }
    auto* integer = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!integer) {
        auto* bytes = static_cast<volatile unsigned char*>(data);
        for (std::size_t index = 0; index < content.size(); ++index) bytes[index] = 0;
        free_nid_postfix(data);
        NotImplemented_nid_no_patch("d2i_ASN1_INTEGER: allocation failure");
    }
    *integer = {static_cast<std::int32_t>(content.size()), negative ? 0x102 : 2, data, 0};
    if (output) *output = integer;
    *input += static_cast<std::size_t>(length) - remaining.size();
    return integer;
}

}
