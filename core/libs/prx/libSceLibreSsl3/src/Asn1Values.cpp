#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int64_t BorrowedData = 0x10;
constexpr std::size_t ChunkBytes = 4096;

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

Asn1String ReadString(const Asn1String* string, const char* message) {
    if (!string) throw std::invalid_argument("ASN.1 value requires an object");
    Asn1String copy{};
    if (!GuestCopyIn_nid_no_patch(&copy, string, sizeof(copy)))
        throw std::invalid_argument(message);
    return copy;
}

void ZeroGuestBytes(unsigned char* data, std::size_t length, const char* message) {
    static constexpr std::array<unsigned char, ChunkBytes> zeros{};
    for (std::size_t offset = 0; offset < length; offset += zeros.size()) {
        if (!GuestCopyOut_nid_no_patch(data + offset, zeros.data(), std::min(zeros.size(), length - offset)))
            throw std::invalid_argument(message);
    }
}

void ClearData(Asn1String* guestString, Asn1String& string, bool respectBorrowed) {
    unsigned char* const oldData = string.data;
    const bool releaseData = oldData && (!respectBorrowed || !(string.flags & BorrowedData));
    if (releaseData) {
        if (string.length < 0)
            throw std::invalid_argument("ASN.1 value has a negative owned data length");
        ZeroGuestBytes(oldData, static_cast<std::size_t>(string.length), "ASN.1 value has unwritable owned data");
    }
    string.data = nullptr;
    string.length = 0;
    string.flags &= ~BorrowedData;
    if (!GuestCopyOut_nid_no_patch(guestString, &string, sizeof(string)))
        throw std::invalid_argument("ASN.1 value object is unwritable");
    if (releaseData) free_nid_postfix(oldData);
}

bool ScanGuestStringLength(const void* data, int& outLength) {
    constexpr std::size_t MaxBytes = static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1;
    std::array<unsigned char, ChunkBytes> buffer{};
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::size_t total = 0;
    while (total < MaxBytes) {
        const auto address = reinterpret_cast<std::uintptr_t>(bytes) + total;
        const std::size_t pageRemaining = ChunkBytes - (address & (ChunkBytes - 1));
        const std::size_t chunk = std::min(pageRemaining, MaxBytes - total);
        if (!GuestCopyIn_nid_no_patch(buffer.data(), bytes + total, chunk)) return false;
        if (const void* nul = std::memchr(buffer.data(), '\0', chunk)) {
            total += static_cast<std::size_t>(static_cast<const unsigned char*>(nul) - buffer.data());
            outLength = static_cast<int>(total);
            return true;
        }
        total += chunk;
    }
    return false;
}

} // namespace

extern "C" {

struct GuestBufferGuard {
    unsigned char* pointer = nullptr;
    ~GuestBufferGuard() {
        if (pointer) free_nid_postfix(pointer);
    }
    unsigned char* Release() {
        unsigned char* out = pointer;
        pointer = nullptr;
        return out;
    }
};

int APS5_VABI ASN1_STRING_set_nid_postfix(Asn1String* string, const void* data, int length) {
    ErrnoGuard errnoGuard;
    if (length == -1) {
        if (!data || !ScanGuestStringLength(data, length)) return 0;
    }
    Asn1String value = ReadString(string, "ASN1_STRING_set: unreadable object");
    if (data && length > 0 && value.data && value.length >= 0 && !(value.flags & BorrowedData)) {
        const auto source = reinterpret_cast<std::uintptr_t>(data);
        const auto old = reinterpret_cast<std::uintptr_t>(value.data);
        if ((source >= old && source - old <= static_cast<std::uintptr_t>(value.length)) ||
            (old > source && old - source < static_cast<std::uintptr_t>(length)))
            throw std::invalid_argument("ASN1_STRING_set: source aliases the data being released");
    }
    ClearData(string, value, true);
    if (length < 0 || length == std::numeric_limits<int>::max()) return 0;
    GuestBufferGuard output{static_cast<unsigned char*>(calloc_nid_postfix(static_cast<std::size_t>(length) + 1, 1))};
    if (!output.pointer) {
        LibreSslPutError_nid_no_patch(13, 0xfff, 65);
        return 0;
    }
    if (data && length && !GuestCopyIn_nid_no_patch(output.pointer, data, static_cast<std::size_t>(length))) {
        return 0;
    }
    value.data = output.pointer;
    value.length = length;
    if (!GuestCopyOut_nid_no_patch(string, &value, sizeof(value))) {
        throw std::invalid_argument("ASN1_STRING_set: unwritable object");
    }
    output.Release();
    return 1;
}

int APS5_VABI ASN1_INTEGER_set_nid_postfix(Asn1String* integer, std::int64_t value) {
    ErrnoGuard errnoGuard;
    Asn1String output = ReadString(integer, "ASN1_INTEGER_set: unreadable object");
    ClearData(integer, output, false);
    output = {0, value < 0 ? 0x102 : 2, nullptr, 0};
    if (!GuestCopyOut_nid_no_patch(integer, &output, sizeof(output)))
        throw std::invalid_argument("ASN1_INTEGER_set: unwritable object");
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
    if (!GuestCopyOut_nid_no_patch(data, encoded.data() + start, length)) {
        free_nid_postfix(data);
        throw std::invalid_argument("ASN1_INTEGER_set: unwritable data");
    }
    output.data = data;
    output.length = static_cast<int>(length);
    if (!GuestCopyOut_nid_no_patch(integer, &output, sizeof(output))) {
        free_nid_postfix(data);
        throw std::invalid_argument("ASN1_INTEGER_set: unwritable object");
    }
    return 1;
}

int APS5_VABI ASN1_OCTET_STRING_cmp_nid_postfix(const Asn1String* first, const Asn1String* second) {
    ErrnoGuard errnoGuard;
    if (!first || !second) return -1;
    const Asn1String left = ReadString(first, "ASN1_OCTET_STRING_cmp: unreadable object");
    const Asn1String right = ReadString(second, "ASN1_OCTET_STRING_cmp: unreadable object");
    if (left.length < 0 || right.length < 0)
        throw std::invalid_argument("ASN1_OCTET_STRING_cmp: negative length");
    if (left.length != right.length) return left.length - right.length;
    if (left.length) {
        if (!left.data || !right.data)
            throw std::invalid_argument("ASN1_OCTET_STRING_cmp: null nonempty data");
        std::array<unsigned char, ChunkBytes> leftChunk{};
        std::array<unsigned char, ChunkBytes> rightChunk{};
        const std::size_t totalLength = static_cast<std::size_t>(left.length);
        for (std::size_t offset = 0; offset < totalLength; offset += leftChunk.size()) {
            const std::size_t chunk = std::min(leftChunk.size(), totalLength - offset);
            if (!GuestCopyIn_nid_no_patch(leftChunk.data(), left.data + offset, chunk) ||
                !GuestCopyIn_nid_no_patch(rightChunk.data(), right.data + offset, chunk))
                throw std::invalid_argument("ASN1_OCTET_STRING_cmp: unreadable data");
            const int order = std::memcmp(leftChunk.data(), rightChunk.data(), chunk);
            if (order) return order;
        }
    }
    const auto typeDifference = static_cast<std::int64_t>(left.type) - right.type;
    if (typeDifference < std::numeric_limits<int>::min() || typeDifference > std::numeric_limits<int>::max())
        throw std::overflow_error("ASN1_OCTET_STRING_cmp: type difference overflow");
    return static_cast<int>(typeDifference);
}

void APS5_VABI ASN1_OCTET_STRING_free_nid_postfix(Asn1String* string) {
    if (!string) return;
    ErrnoGuard errnoGuard;
    const Asn1String fields = ReadString(string, (std::string(__func__) + ": unreadable string").c_str());
    if (fields.data && !(fields.flags & BorrowedData)) {
        if (fields.length < 0) throw std::invalid_argument(std::string(__func__) + ": negative owned data length");
        ZeroGuestBytes(fields.data, static_cast<std::size_t>(fields.length), (std::string(__func__) + ": unwritable data").c_str());
        free_nid_postfix(fields.data);
    }
    free_nid_postfix(string);
}

}
