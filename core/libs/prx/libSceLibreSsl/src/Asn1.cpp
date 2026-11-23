#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

static_assert(sizeof(Asn1String) == 24);
static_assert(offsetof(Asn1String, data) == 8);
static_assert(offsetof(Asn1String, flags) == 16);

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int IllegalCharacters = 124;
constexpr int InvalidBmpStringLength = 129;
constexpr int InvalidUniversalStringLength = 133;
constexpr int InvalidUtf8String = 134;

constexpr std::uintptr_t GuestChunkBytes = 4096;

template <typename T>
T CopyGuestInPreservingErrno(const T* source, const char* function) {
    T destination{};
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return destination;
}

std::vector<unsigned char> CopyGuestBytesInPreservingErrno(const unsigned char* source, int length, const char* function) {
    std::vector<unsigned char> bytes(length > 0 ? static_cast<std::size_t>(length) : 0);
    const bool ok = GuestCopyIn_nid_no_patch(bytes.data(), source, bytes.size());
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return bytes;
}

std::vector<unsigned char> CopyGuestTerminatedInPreservingErrno(const unsigned char* source, std::size_t limit, const char* function) {
    std::vector<unsigned char> bytes;
    unsigned char chunk[GuestChunkBytes];
    auto address = reinterpret_cast<std::uintptr_t>(source);
    for (;;) {
        const std::size_t count = GuestChunkBytes - address % GuestChunkBytes;
        const bool ok = GuestCopyIn_nid_no_patch(chunk, reinterpret_cast<const void*>(address), count);
        if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        const auto* end = static_cast<const unsigned char*>(std::memchr(chunk, 0, count));
        bytes.insert(bytes.end(), static_cast<const unsigned char*>(chunk), end ? end : chunk + count);
        if (end || bytes.size() > limit) return bytes;
        address += count;
    }
}

Asn1String CopyGuestStringPreservingErrno(const Asn1String* value, std::vector<unsigned char>& bytes, const char* function) {
    Asn1String copy = CopyGuestInPreservingErrno(value, function);
    if (copy.length > 0 && copy.data) {
        bytes = CopyGuestBytesInPreservingErrno(copy.data, copy.length, function);
        copy.data = bytes.data();
    }
    return copy;
}

int Asn1Error(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
    return -1;
}

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

int CharacterBytes(std::uint32_t value) {
    return value < 0x80 ? 1 : value < 0x800 ? 2 : value < 0x10000 ? 3 : 4;
}

void WriteCharacter(unsigned char*& output, std::uint32_t value) {
    const int bytes = CharacterBytes(value);
    if (bytes == 1) {
        *output++ = static_cast<unsigned char>(value);
        return;
    }
    constexpr unsigned char prefix[] = {0, 0, 0xc0, 0xe0, 0xf0};
    *output++ = static_cast<unsigned char>(prefix[bytes] | (value >> (6 * (bytes - 1))));
    for (int index = bytes - 2; index >= 0; --index)
        *output++ = static_cast<unsigned char>(0x80 | ((value >> (6 * index)) & 0x3f));
}

int TwoDigits(const char* text) {
    return (text[0] - '0') * 10 + (text[1] - '0');
}

bool AllDigits(const char* text, int count) {
    for (int index = 0; index < count; ++index)
        if (text[index] < '0' || text[index] > '9') return false;
    return true;
}

int PrintTime(LibreSsl::Bio* bio, const Asn1String& time) {
    static constexpr const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const bool generalized = time.type == 24;
    const int length = time.length;
    const char* text = reinterpret_cast<const char*>(time.data);
    const int digits = generalized ? 12 : 10;
    if ((time.type != 23 && !generalized) || length < digits || !AllDigits(text, digits)) return -1;
    const int fields = generalized ? 4 : 2;
    const int year = generalized ? TwoDigits(text) * 100 + TwoDigits(text + 2) : TwoDigits(text) + (TwoDigits(text) < 50 ? 2000 : 1900);
    const int month = TwoDigits(text + fields);
    if (month < 1 || month > 12) return -1;
    const int day = TwoDigits(text + fields + 2);
    const int hour = TwoDigits(text + fields + 4);
    const int minute = TwoDigits(text + fields + 6);
    int second = 0;
    int fraction = 0;
    if (length >= digits + 2 && AllDigits(text + digits, 2)) {
        second = TwoDigits(text + digits);
        if (generalized && length >= 15 && text[14] == '.') {
            fraction = 1;
            while (14 + fraction < length && text[14 + fraction] >= '0' && text[14 + fraction] <= '9') ++fraction;
        }
    }
    const char* zone = text[length - 1] == 'Z' ? " GMT" : "";
    const char* decimals = fraction ? text + 14 : "";
    const char* format = "%s %2d %02d:%02d:%02d%.*s %d%s";
    const int size = std::snprintf(nullptr, 0, format, months[month - 1], day, hour, minute, second, fraction, decimals, year, zone);
    std::string output(static_cast<std::size_t>(size) + 1, char{});
    std::snprintf(output.data(), output.size(), format, months[month - 1], day, hour, minute, second, fraction, decimals, year, zone);
    LibreSslBioWrite_nid_no_patch(bio, output.data(), size);
    return 1;
}

} // namespace

extern "C" {

int APS5_VABI ASN1_TIME_print_nid_postfix(LibreSsl::Bio* bio, const Asn1String* time) {
    if (!time) APS5_INVALID_ARG_EX;
    std::vector<unsigned char> bytes;
    const Asn1String copy = CopyGuestStringPreservingErrno(time, bytes, __func__);
    if (copy.length > 0 && !copy.data) APS5_INVALID_ARG_EX;
    if (PrintTime(bio, copy) == 1) return 1;
    LibreSslBioWrite_nid_no_patch(bio, "Bad time value", 14);
    return 0;
}

std::int64_t APS5_VABI ASN1_INTEGER_get_nid_postfix(const Asn1String* value) {
    if (!value) return 0;
    const Asn1String integer = CopyGuestInPreservingErrno(value, __func__);
    if (integer.type != 2 && integer.type != 0x102) return -1;
    if (integer.length > 8) return -1;
    if (integer.length <= 0 || !integer.data) return 0;
    const std::vector<unsigned char> bytes = CopyGuestBytesInPreservingErrno(integer.data, integer.length, __func__);
    std::uint64_t magnitude = 0;
    for (int index = 0; index < integer.length; ++index) magnitude = (magnitude << 8) | bytes[static_cast<std::size_t>(index)];
    const bool negative = integer.type == 0x102;
    const auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + negative;
    if (magnitude > limit)
        throw std::invalid_argument(std::string(__func__) + ": unrepresentable signed integer");
    if (negative && magnitude != 0) return -static_cast<std::int64_t>(magnitude - 1) - 1;
    return static_cast<std::int64_t>(magnitude);
}

unsigned char* APS5_VABI ASN1_STRING_data_nid_postfix(Asn1String* value) {
    if (!value) APS5_INVALID_ARG_EX;
    return CopyGuestInPreservingErrno(value, __func__).data;
}

int APS5_VABI ASN1_STRING_length_nid_postfix(const Asn1String* value) {
    if (!value) APS5_INVALID_ARG_EX;
    return CopyGuestInPreservingErrno(value, __func__).length;
}

int APS5_VABI ASN1_STRING_print_nid_postfix(LibreSsl::Bio* bio, const Asn1String* value) {
    if (!value) return 0;
    std::vector<unsigned char> bytes;
    const Asn1String copy = CopyGuestStringPreservingErrno(value, bytes, __func__);
    if (copy.length > 0 && !copy.data) APS5_INVALID_ARG_EX;
    char buffer[80];
    int used = 0;
    for (int index = 0; index < copy.length; ++index) {
        const unsigned char character = copy.data[index];
        const bool printable = (character >= ' ' && character <= '~') || character == '\n' || character == '\r';
        buffer[used++] = printable ? static_cast<char>(character) : '.';
        if (used == sizeof(buffer)) {
            if (LibreSslBioWrite_nid_no_patch(bio, buffer, used) <= 0) return 0;
            used = 0;
        }
    }
    if (used > 0 && LibreSslBioWrite_nid_no_patch(bio, buffer, used) <= 0) return 0;
    return 1;
}

int APS5_VABI ASN1_STRING_type_nid_postfix(Asn1String* value) {
    if (!value) APS5_INVALID_ARG_EX;
    return CopyGuestInPreservingErrno(value, __func__).type;
}

int APS5_VABI ASN1_STRING_to_UTF8_nid_postfix(unsigned char** output, Asn1String* value) {
    if (!value) return -1;
    const Asn1String string = CopyGuestInPreservingErrno(value, __func__);
    const int width = CharacterWidth(string.type);
    if (width < 0) return -1;
    if (!string.data && string.length < 0) throw std::invalid_argument("ASN1_STRING_to_UTF8: null terminated-string data");
    std::vector<unsigned char> bytes;
    if (string.length < 0)
        bytes = CopyGuestTerminatedInPreservingErrno(string.data, static_cast<std::size_t>(std::numeric_limits<int>::max()), __func__);
    const std::size_t length = string.length < 0 ? bytes.size() : static_cast<std::size_t>(string.length);
    if (length > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::length_error("ASN1_STRING_to_UTF8: input length overflow");
    if (width == 2 && length % 2 != 0) return Asn1Error(InvalidBmpStringLength);
    if (width == 4 && length % 4 != 0) return Asn1Error(InvalidUniversalStringLength);
    if (!string.data && length != 0) throw std::invalid_argument("ASN1_STRING_to_UTF8: null nonempty string data");
    if (string.length > 0) bytes = CopyGuestBytesInPreservingErrno(string.data, string.length, __func__);
    const std::span<const unsigned char> characters(bytes.data(), length);
    std::size_t size = 0;
    for (auto input = characters; !input.empty();) {
        std::uint32_t character;
        if (!ReadCharacter(input, width, character)) return Asn1Error(width == 0 ? InvalidUtf8String : IllegalCharacters);
        size += static_cast<std::size_t>(CharacterBytes(character));
        if (size > static_cast<std::size_t>(std::numeric_limits<int>::max() - 1)) throw std::length_error("ASN1_STRING_to_UTF8: output length overflow");
    }
    if (!output) throw std::invalid_argument("ASN1_STRING_to_UTF8: null output");
    auto* result = static_cast<unsigned char*>(malloc_nid_postfix(size + 1));
    if (!result) {
        if (width == 0) Asn1Error(MallocFailure);
        return Asn1Error(MallocFailure);
    }
    unsigned char* cursor = result;
    for (auto input = characters; !input.empty();) {
        std::uint32_t character;
        ReadCharacter(input, width, character);
        WriteCharacter(cursor, character);
    }
    *cursor = 0;
    const int outputErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(output, &result, sizeof(result));
    if (!written) free_nid_postfix(result);
    errno = outputErrno;
    if (!written) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return static_cast<int>(size);
}

int APS5_VABI i2a_ASN1_OBJECT_nid_postfix(LibreSsl::Bio* bio, const LibreSslAsn1Object* object) {
    if (!object || !CopyGuestInPreservingErrno(object, __func__).data) return LibreSslBioWrite_nid_no_patch(bio, "NULL", 4);
    int capacity = 256;
    const auto release = [&capacity](char* buffer) {
        std::memset(buffer, 0, capacity);
        free_nid_postfix(buffer);
    };
    std::unique_ptr<char, decltype(release)> text(static_cast<char*>(malloc_nid_postfix(capacity)), release);
    if (!text) return -1;
    int length = i2t_ASN1_OBJECT_nid_postfix(text.get(), capacity, object);
    if (length > capacity - 1) {
        text.reset();
        capacity = length + 1;
        text.reset(static_cast<char*>(malloc_nid_postfix(capacity)));
        if (!text) return -1;
        length = i2t_ASN1_OBJECT_nid_postfix(text.get(), capacity, object);
    }
    const int written = length <= 0 ? LibreSslBioWrite_nid_no_patch(bio, "<INVALID>", 9) : LibreSslBioWrite_nid_no_patch(bio, text.get(), length);
    text.reset();
    return written;
}

}
