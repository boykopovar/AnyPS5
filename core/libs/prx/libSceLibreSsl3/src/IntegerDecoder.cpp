#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>


namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int NestedAsn1Error = 58;
constexpr int LengthMismatch = 62;
constexpr int MallocFailure = 65;
constexpr int BadObjectHeader = 102;
constexpr int TooLong = 155;
constexpr int WrongTag = 168;
constexpr int TypeNotPrimitive = 231;

static_assert(sizeof(Asn1String) == 24);
static_assert(offsetof(Asn1String, data) == 8);
static_assert(offsetof(Asn1String, flags) == 16);

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

void PutHeaderError(int reason) {
    PutError(reason);
    PutError(NestedAsn1Error);
}

bool ReadByte(std::span<const unsigned char>& input, unsigned char& byte) {
    if (input.empty()) return false;
    byte = input.front();
    input = input.subspan(1);
    return true;
}

bool ReadIdentifier(std::span<const unsigned char>& input, std::uint8_t& tagClass, bool& constructed, std::uint32_t& tag) {
    unsigned char identifier;
    if (!ReadByte(input, identifier)) return false;
    tagClass = identifier >> 6;
    constructed = (identifier & 0x20) != 0;
    tag = identifier & 0x1f;
    if (tag != 0x1f) return true;
    tag = 0;
    unsigned char byte;
    do {
        if (!ReadByte(input, byte)) return false;
        if (tag > (std::numeric_limits<std::uint32_t>::max() >> 7)) return false;
        tag = (tag << 7) | (byte & 0x7f);
    } while (byte & 0x80);
    return true;
}

bool ReadLength(std::span<const unsigned char>& input, bool& indefinite, std::size_t& length) {
    unsigned char lengthByte;
    if (!ReadByte(input, lengthByte)) return false;
    indefinite = lengthByte == 0x80;
    length = lengthByte < 0x80 ? lengthByte : 0;
    if (lengthByte <= 0x80) return true;
    const auto bytes = lengthByte & 0x7f;
    if (bytes == 0x7f) return false;
    for (int index = 0; index < bytes; ++index) {
        unsigned char byte;
        if (!ReadByte(input, byte)) return false;
        if (length > (std::numeric_limits<std::size_t>::max() >> 8)) return false;
        length = (length << 8) | byte;
    }
    return true;
}

std::optional<std::span<const unsigned char>> ReadInteger(std::span<const unsigned char>& input) {
    std::uint8_t tagClass;
    bool constructed;
    std::uint32_t tag;
    if (!ReadIdentifier(input, tagClass, constructed, tag)) {
        PutHeaderError(BadObjectHeader);
        return std::nullopt;
    }
    if (tag != 2 || tagClass != 0) {
        PutHeaderError(WrongTag);
        return std::nullopt;
    }
    bool indefinite;
    std::size_t length;
    if (!ReadLength(input, indefinite, length) || (indefinite && !constructed)) {
        PutHeaderError(BadObjectHeader);
        return std::nullopt;
    }
    if (!indefinite && length > input.size()) {
        PutHeaderError(TooLong);
        return std::nullopt;
    }
    if (constructed) {
        PutError(TypeNotPrimitive);
        return std::nullopt;
    }
    const auto content = input.first(length);
    input = input.subspan(length);
    return content;
}

void ClearAndFree(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
    free_nid_postfix(data);
}

void FreeInteger(void* value) {
    if (!value) return;
    auto& integer = *static_cast<Asn1String*>(value);
    if (!(integer.flags & 0x10)) {
        if (integer.data) {
            if (integer.length < 0) NotImplemented_nid_no_patch(__func__);
            ClearAndFree(integer.data, static_cast<std::size_t>(integer.length));
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
    if (!content || content->size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return nullptr;
    if (output) {
        FreeInteger(*output);
        *output = nullptr;
    }
    if (content->empty()) {
        PutError(LengthMismatch);
        return nullptr;
    }
    const bool negative = (content->front() & 0x80) != 0;
    const unsigned char padding = negative ? 0xff : 0;
    if (content->size() > 1 && content->front() == padding) {
        bool keepPadding = negative;
        for (std::size_t index = 1; index < content->size(); ++index)
            keepPadding = keepPadding && (*content)[index] == 0;
        if (!keepPadding) {
            content = content->subspan(1);
            if (((content->front() & 0x80) != 0) == negative) {
                PutError(LengthMismatch);
                return nullptr;
            }
        }
    }
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(content->size()));
    if (!data) return nullptr;
    std::memcpy(data, content->data(), content->size());
    if (negative) {
        unsigned int carry = 1;
        for (std::size_t index = content->size(); index != 0; --index) {
            const unsigned int digit = (data[index - 1] ^ 0xff) + carry;
            data[index - 1] = static_cast<unsigned char>(digit);
            carry = digit >> 8;
        }
    }
    auto* integer = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    if (!integer) {
        ClearAndFree(data, content->size());
        PutError(MallocFailure);
        PutError(MallocFailure);
        return nullptr;
    }
    *integer = {static_cast<std::int32_t>(content->size()), negative ? 0x102 : 2, data, 0};
    if (output) *output = integer;
    *input += static_cast<std::size_t>(length) - remaining.size();
    return integer;
}

}
