#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr unsigned char BooleanType = 1;
constexpr unsigned char IntegerType = 2;
constexpr unsigned char BitStringType = 3;
constexpr unsigned char OctetStringType = 4;
constexpr unsigned char NullType = 5;
constexpr unsigned char ObjectType = 6;

constexpr std::int64_t BitsLeftFlag = 0x08;
constexpr std::int64_t NegativeFlag = 0x100;

template<typename T>
T CopyIn(const T* guest, const char* function) {
    T value{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, guest, sizeof(value));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": unreadable structure");
    return value;
}

void AppendGuestBytes(std::vector<unsigned char>& out, const unsigned char* data, int length, const char* function) {
    if (length < 0) throw std::invalid_argument(std::string(function) + ": negative length");
    if (length == 0) return;
    if (!data) throw std::invalid_argument(std::string(function) + ": null data with a length");
    const std::size_t start = out.size();
    out.resize(start + static_cast<std::size_t>(length));
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(out.data() + start, data, static_cast<std::size_t>(length));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": unreadable bytes");
}

void AppendLength(std::vector<unsigned char>& out, std::size_t length) {
    if (length < 128) {
        out.push_back(static_cast<unsigned char>(length));
        return;
    }
    unsigned char bytes[sizeof(std::size_t)]{};
    std::size_t count = 0;
    while (length != 0) {
        bytes[sizeof(bytes) - ++count] = static_cast<unsigned char>(length);
        length >>= 8;
    }
    out.push_back(static_cast<unsigned char>(0x80 | count));
    out.insert(out.end(), bytes + sizeof(bytes) - count, bytes + sizeof(bytes));
}

void Append(std::vector<unsigned char>& out, unsigned char tag, const unsigned char* data, std::size_t length) {
    out.push_back(tag);
    AppendLength(out, length);
    out.insert(out.end(), data, data + length);
}

void AppendObject(std::vector<unsigned char>& out, const LibreSslAsn1Object* guestObject) {
    if (!guestObject) throw std::invalid_argument("i2d_X509_PUBKEY: null algorithm object");
    const LibreSslAsn1Object object = CopyIn(guestObject, "i2d_X509_PUBKEY");
    if (!object.data || object.length <= 0)
        NotImplemented_nid_no_patch("i2d_X509_PUBKEY: an algorithm without an encoded object");
    std::vector<unsigned char> content;
    AppendGuestBytes(content, object.data, object.length, "i2d_X509_PUBKEY");
    Append(out, ObjectType, content.data(), content.size());
}

std::vector<unsigned char> BitStringContent(const Asn1String& bitsGuest, const char* function) {
    const Asn1String bits = CopyIn(&bitsGuest, function);
    std::vector<unsigned char> data;
    AppendGuestBytes(data, bits.data, bits.length, function);
    std::size_t used = data.size();
    int unused = 0;
    if (bits.length > 0 && (bits.flags & BitsLeftFlag) != 0) {
        unused = static_cast<int>(bits.flags & 0x07);
    } else {
        while (used > 0 && data[used - 1] == 0) --used;
        if (used > 0) {
            const unsigned char last = data[used - 1];
            while (((last >> unused) & 1) == 0) ++unused;
        }
    }
    std::vector<unsigned char> content;
    content.reserve(data.size() + 1);
    content.push_back(static_cast<unsigned char>(unused));
    content.insert(content.end(), data.begin(), data.begin() + static_cast<std::ptrdiff_t>(used));
    if (used > 0) content.back() &= static_cast<unsigned char>(0xff << unused);
    content.resize(data.size() + 1, 0);
    return content;
}

void AppendInteger(std::vector<unsigned char>& out, const Asn1String* guestValue) {
    const Asn1String value = CopyIn(guestValue, "i2d_X509_PUBKEY");
    std::vector<unsigned char> magnitude;
    AppendGuestBytes(magnitude, value.data, value.length, "i2d_X509_PUBKEY");
    while (!magnitude.empty() && magnitude.front() == 0) magnitude.erase(magnitude.begin());
    std::vector<unsigned char> content;
    const bool negative = (value.flags & NegativeFlag) != 0;
    if (magnitude.empty()) {
        content.push_back(0);
    } else if (!negative) {
        if ((magnitude.front() & 0x80) != 0) content.push_back(0);
        content.insert(content.end(), magnitude.begin(), magnitude.end());
    } else {
        content = magnitude;
        for (auto& byte : content) byte = static_cast<unsigned char>(~byte);
        for (std::size_t index = content.size(); index-- > 0;) {
            if (++content[index] != 0) break;
        }
        while (content.size() > 1 && content.front() == 0xff && (content[1] & 0x80) != 0)
            content.erase(content.begin());
        if ((content.front() & 0x80) == 0) content.insert(content.begin(), 0xff);
    }
    Append(out, IntegerType, content.data(), content.size());
}

void AppendType(std::vector<unsigned char>& out, const LibreSslAsn1TypeValue* guestValue) {
    if (!guestValue) return;
    const LibreSslAsn1TypeValue value = CopyIn(guestValue, "i2d_X509_PUBKEY");
    switch (value.type) {
    case NullType:
        Append(out, NullType, nullptr, 0);
        return;
    case ObjectType:
        AppendObject(out, static_cast<const LibreSslAsn1Object*>(value.value.pointer));
        return;
    case BooleanType: {
        const auto number = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(value.value.pointer));
        if (number == -1) return;
        const unsigned char content = static_cast<unsigned char>(number);
        Append(out, BooleanType, &content, 1);
        return;
    }
    case IntegerType:
        if (!value.value.pointer) throw std::invalid_argument("i2d_X509_PUBKEY: null integer parameter");
        AppendInteger(out, static_cast<const Asn1String*>(value.value.pointer));
        return;
    case BitStringType:
        if (!value.value.pointer) throw std::invalid_argument("i2d_X509_PUBKEY: null bit string parameter");
        {
            const std::vector<unsigned char> bits = BitStringContent(*static_cast<const Asn1String*>(value.value.pointer),
                                                                     "i2d_X509_PUBKEY");
            Append(out, BitStringType, bits.data(), bits.size());
        }
        return;
    case OctetStringType: {
        const Asn1String string = CopyIn(static_cast<const Asn1String*>(value.value.pointer), "i2d_X509_PUBKEY");
        std::vector<unsigned char> content;
        AppendGuestBytes(content, string.data, string.length, "i2d_X509_PUBKEY");
        Append(out, OctetStringType, content.data(), content.size());
        return;
    }
    default:
        NotImplemented_nid_no_patch("i2d_X509_PUBKEY: an unsupported algorithm parameter");
    }
}

}

extern "C" {

int APS5_VABI i2d_X509_PUBKEY_nid_postfix(const LibreSslX509Pubkey* guestKey, unsigned char** output) {
    if (!guestKey) return 0;
    const LibreSslX509Pubkey key = CopyIn(guestKey, "i2d_X509_PUBKEY");
    if (!key.algor) throw std::invalid_argument("i2d_X509_PUBKEY: null algorithm");
    if (!key.publicKey) throw std::invalid_argument("i2d_X509_PUBKEY: null public key");
    const LibreSslX509Algor algor = CopyIn(key.algor, "i2d_X509_PUBKEY");
    std::vector<unsigned char> algorithm;
    AppendObject(algorithm, static_cast<const LibreSslAsn1Object*>(algor.algorithm));
    AppendType(algorithm, static_cast<const LibreSslAsn1TypeValue*>(algor.parameter));
    std::vector<unsigned char> spki{0x30};
    AppendLength(spki, algorithm.size());
    spki.insert(spki.end(), algorithm.begin(), algorithm.end());
    const std::vector<unsigned char> bits = BitStringContent(*key.publicKey, "i2d_X509_PUBKEY");
    Append(spki, BitStringType, bits.data(), bits.size());
    std::vector<unsigned char> encoded{0x30};
    AppendLength(encoded, spki.size());
    encoded.insert(encoded.end(), spki.begin(), spki.end());
    const int length = static_cast<int>(encoded.size());
    if (!output) return length;
    unsigned char* target = nullptr;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&target, output, sizeof(target));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument("i2d_X509_PUBKEY: unreadable output pointer");
    unsigned char* next = nullptr;
    if (target) {
        if (!GuestCopyOut_nid_no_patch(target, encoded.data(), encoded.size()))
            throw std::invalid_argument("i2d_X509_PUBKEY: unwritable output buffer");
        next = target + encoded.size();
    } else {
        auto* allocated = static_cast<unsigned char*>(malloc_nid_postfix(encoded.size()));
        if (!allocated) return -1;
        std::memcpy(allocated, encoded.data(), encoded.size());
        next = allocated;
    }
    if (!GuestCopyOut_nid_no_patch(output, &next, sizeof(next)))
        throw std::invalid_argument("i2d_X509_PUBKEY: unwritable output pointer");
    return length;
}

}
