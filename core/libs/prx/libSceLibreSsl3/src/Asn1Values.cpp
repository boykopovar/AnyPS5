#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::int64_t BorrowedData = 0x10;

void ClearData(Asn1String& string, bool respectBorrowed) {
    if (string.data && (!respectBorrowed || !(string.flags & BorrowedData))) {
        if (string.length < 0)
            throw std::invalid_argument("ASN.1 value has a negative owned data length");
        auto* bytes = static_cast<volatile unsigned char*>(string.data);
        for (int index = 0; index < string.length; ++index) bytes[index] = 0;
        free_nid_postfix(string.data);
    }
    string.data = nullptr;
    string.length = 0;
    string.flags &= ~BorrowedData;
}

Asn1String& Checked(Asn1String* string) {
    if (!string) throw std::invalid_argument("ASN.1 value requires an object");
    return *string;
}

} // namespace

extern "C" {

int APS5_VABI ASN1_STRING_set_nid_postfix(Asn1String* string, const void* data, int length) {
    if (length == -1) {
        if (!data) return 0;
        const auto size = std::strlen(static_cast<const char*>(data));
        if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) return 0;
        length = static_cast<int>(size);
    }
    auto& value = Checked(string);
    if (data && length > 0 && value.data && value.length > 0 && !(value.flags & BorrowedData)) {
        const auto source = reinterpret_cast<std::uintptr_t>(data);
        const auto old = reinterpret_cast<std::uintptr_t>(value.data);
        if ((source >= old && source - old < static_cast<std::uintptr_t>(value.length)) ||
            (old > source && old - source < static_cast<std::uintptr_t>(length)))
            throw std::invalid_argument("ASN1_STRING_set: source aliases the data being released");
    }
    ClearData(value, true);
    if (length < 0 || length == std::numeric_limits<int>::max()) return 0;
    auto* output = static_cast<unsigned char*>(calloc_nid_postfix(static_cast<std::size_t>(length) + 1, 1));
    if (!output) {
        LibreSslPutError_nid_no_patch(13, 0xfff, 65);
        return 0;
    }
    if (data && length) std::memcpy(output, data, static_cast<std::size_t>(length));
    value.data = output;
    value.length = length;
    return 1;
}

int APS5_VABI ASN1_INTEGER_set_nid_postfix(Asn1String* integer, std::int64_t value) {
    auto& output = Checked(integer);
    ClearData(output, false);
    output = {0, value < 0 ? 0x102 : 2, nullptr, 0};
    const auto bits = static_cast<std::uint64_t>(value);
    const auto magnitude = value < 0 ? std::uint64_t{0} - bits : bits;
    std::array<unsigned char, 8> encoded{};
    std::size_t start = encoded.size() - 1;
    for (auto remaining = magnitude; remaining > 0xff; remaining >>= 8) --start;
    const auto length = encoded.size() - start;
    for (std::size_t index = 0; index < length; ++index)
        encoded[start + index] = static_cast<unsigned char>(magnitude >> ((length - index - 1) * 8));
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(encoded.size()));
    if (!data) return 0;
    std::memcpy(data, encoded.data() + start, length);
    output.data = data;
    output.length = static_cast<int>(length);
    return 1;
}

int APS5_VABI ASN1_OCTET_STRING_cmp_nid_postfix(const Asn1String* first, const Asn1String* second) {
    if (!first || !second) return -1;
    if (first->length < 0 || second->length < 0)
        throw std::invalid_argument("ASN1_OCTET_STRING_cmp: negative length");
    if (first->length != second->length) return first->length - second->length;
    if (first->length) {
        if (!first->data || !second->data)
            throw std::invalid_argument("ASN1_OCTET_STRING_cmp: null nonempty data");
        const int order = std::memcmp(first->data, second->data, static_cast<std::size_t>(first->length));
        if (order) return order;
    }
    const auto typeDifference = static_cast<std::int64_t>(first->type) - second->type;
    if (typeDifference < std::numeric_limits<int>::min() || typeDifference > std::numeric_limits<int>::max())
        throw std::overflow_error("ASN1_OCTET_STRING_cmp: type difference overflow");
    return static_cast<int>(typeDifference);
}

void APS5_VABI ASN1_OCTET_STRING_free_nid_postfix(Asn1String* string) {
    if (!string) return;
    ClearData(*string, true);
    free_nid_postfix(string);
}

}
