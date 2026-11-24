#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

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
        std::string text;
        int error = CopyGuestString_nid_no_patch(static_cast<const char*>(data), 65536, text);
        if (error == 63) error = CopyGuestString_nid_no_patch(static_cast<const char*>(data), 1u << 20, text);
        if (error) return 0;
        length = static_cast<int>(text.size());
    }
    auto& value = Checked(string);
    if (data && length > 0 && value.data && value.length >= 0 && !(value.flags & BorrowedData)) {
        const auto source = reinterpret_cast<std::uintptr_t>(data);
        const auto old = reinterpret_cast<std::uintptr_t>(value.data);
        if ((source >= old && source - old <= static_cast<std::uintptr_t>(value.length)) ||
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
    if (data && length && !GuestCopyIn_nid_no_patch(output, data, static_cast<std::size_t>(length))) {
        free_nid_postfix(output);
        return 0;
    }
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
    Asn1String left;
    Asn1String right;
    if (!GuestCopyIn_nid_no_patch(&left, first, sizeof(left)) || !GuestCopyIn_nid_no_patch(&right, second, sizeof(right)))
        throw std::invalid_argument("ASN1_OCTET_STRING_cmp: unreadable object");
    if (left.length < 0 || right.length < 0)
        throw std::invalid_argument("ASN1_OCTET_STRING_cmp: negative length");
    if (left.length != right.length) return left.length - right.length;
    if (left.length) {
        if (!left.data || !right.data)
            throw std::invalid_argument("ASN1_OCTET_STRING_cmp: null nonempty data");
        std::vector<unsigned char> a(static_cast<std::size_t>(left.length));
        std::vector<unsigned char> b(a.size());
        if (!GuestCopyIn_nid_no_patch(a.data(), left.data, a.size()) || !GuestCopyIn_nid_no_patch(b.data(), right.data, b.size()))
            throw std::invalid_argument("ASN1_OCTET_STRING_cmp: unreadable data");
        const int order = std::memcmp(a.data(), b.data(), a.size());
        if (order) return order;
    }
    const auto typeDifference = static_cast<std::int64_t>(left.type) - right.type;
    if (typeDifference < std::numeric_limits<int>::min() || typeDifference > std::numeric_limits<int>::max())
        throw std::overflow_error("ASN1_OCTET_STRING_cmp: type difference overflow");
    return static_cast<int>(typeDifference);
}

void APS5_VABI ASN1_OCTET_STRING_free_nid_postfix(Asn1String* string) {
    if (!string) return;
    const int savedErrno = errno;
    Asn1String fields{};
    bool copied = GuestCopyIn_nid_no_patch(&fields, string, sizeof(fields));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(__func__) + ": unreadable string");
    if (fields.data && !(fields.flags & BorrowedData)) {
        if (fields.length < 0) throw std::invalid_argument(std::string(__func__) + ": negative owned data length");
        static constexpr std::array<unsigned char, 4096> zeros{};
        for (std::size_t offset = 0, length = static_cast<std::size_t>(fields.length); offset < length; offset += zeros.size()) {
            copied = GuestCopyOut_nid_no_patch(fields.data + offset, zeros.data(), std::min(zeros.size(), length - offset));
            errno = savedErrno;
            if (!copied) throw std::invalid_argument(std::string(__func__) + ": unwritable data");
        }
        free_nid_postfix(fields.data);
    }
    free_nid_postfix(string);
}

}
