#include "prx/libc/include/Api.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/SonyCrt.hpp"

extern "C" {

extern FileStream* __stderrp_nid_postfix;
[[noreturn]] void abort_nid_postfix(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);
}

namespace {

constexpr int guestEinval = 22;
constexpr int guestErange = 34;
constexpr int guestEilseq = 86;
constexpr int guestEof = -1;
constexpr std::size_t failed = static_cast<std::size_t>(-1);
constexpr std::size_t rsizeMax = (std::size_t{1} << 61) - 1;
constexpr std::size_t guestStateSize = 128;

bool usableSize(std::size_t size) {
    return size != 0 && size <= rsizeMax;
}

bool convertFailed(std::size_t length) {
    return length >= failed - 1;
}

int violation(const char* message, int error) {
    SonyInvokeConstraintHandler(message, nullptr, error);
    return error;
}

}  // namespace

extern "C" {

void APS5_VABI abort_handler_s_nid_postfix(const char* message, void*, int) {
    fputs_nid_postfix(message, __stderrp_nid_postfix);
    fputc_nid_postfix('\n', __stderrp_nid_postfix);

    abort_nid_postfix(0, 0, 0, 0, 0, 0);
}

void* APS5_VABI bsearch_s_nid_postfix(const void* key, const void* base, std::size_t count, std::size_t size, GuestContextCompare compare, void* context) {
    if (count > rsizeMax || size > rsizeMax) {
        violation("bsearch_s: bad argument", guestEinval);
        return nullptr;
    }
    if (count == 0) return nullptr;
    if (!key || !base || !compare || size == 0) {
        violation("bsearch_s: bad argument", guestEinval);
        return nullptr;
    }
    const auto* low = static_cast<const unsigned char*>(base);
    for (std::size_t remaining = count; remaining != 0; remaining >>= 1) {
        const auto* probe = low + (remaining >> 1) * size;
        const int order = compare(key, probe, context);
        if (order == 0) return const_cast<unsigned char*>(probe);
        if (order > 0) {
            low = probe + size;
            --remaining;
        }
    }
    return nullptr;
}

std::uint16_t* APS5_VABI wcstok_s_nid_postfix(std::uint16_t* str, std::size_t* remaining, const std::uint16_t* delimiters, std::uint16_t** context) {
    if (!remaining || !context || !delimiters || *remaining > rsizeMax || (!str && !*context)) {
        violation("wcstok_s: bad argument", guestEinval);
        return nullptr;
    }
    if (!str) str = *context;
    const auto isDelimiter = [delimiters](std::uint16_t value) {
        for (const auto* delimiter = delimiters; *delimiter != 0; ++delimiter)
            if (*delimiter == value) return true;
        return false;
    };
    const std::size_t available = *remaining;
    std::size_t skipped = 0;
    if (available == 0) {
        violation("wcstok_s: missing token", guestEinval);
        return nullptr;
    }
    while (str[skipped] != 0 && isDelimiter(str[skipped])) {
        if (++skipped == available) {
            violation("wcstok_s: missing token", guestEinval);
            return nullptr;
        }
    }
    std::uint16_t* const token = str + skipped;
    const std::size_t left = available - skipped;
    *remaining = left;
    if (*token == 0) {
        *context = token;
        return nullptr;
    }
    std::size_t length = 0;
    while (token[length] != 0 && !isDelimiter(token[length])) {
        if (++length == left) {
            violation("wcstok_s: missing delimiter", guestEinval);
            return nullptr;
        }
    }
    std::uint16_t* next = token + length;
    *remaining = left - length;
    if (*next != 0) {
        *next++ = 0;
        if (left != length) *remaining = left - length - 1;
    }
    *context = next;
    return token;
}

int APS5_VABI wctomb_s_nid_postfix(int* status, char* destination, std::size_t capacity, std::uint16_t wide) {
    if (!status || (!destination && capacity) || (destination && capacity > rsizeMax))
        return violation("wctomb_s: bad argument", guestEinval);
    if (destination && static_cast<std::size_t>(_Getmbcurmax_nid_postfix()) > capacity) {
        char bytes[MB_LEN_MAX];
        const int length = wctomb_nid_postfix(bytes, wide);
        if (length > 0 && static_cast<std::size_t>(length) > capacity) return violation("wctomb_s: buffer too small", guestErange);
        if (length > 0) std::memcpy(destination, bytes, static_cast<std::size_t>(length));
        *status = length;
        return length < 0 ? guestEilseq : 0;
    }
    const int length = wctomb_nid_postfix(destination, wide);
    *status = length;
    return destination && length < 0 ? guestEilseq : 0;
}

int APS5_VABI wcrtomb_s_nid_postfix(std::size_t* result, char* destination, std::size_t capacity, std::uint16_t wide, void* state) {
    if (!result || !state) {
        if (destination && usableSize(capacity)) destination[0] = '\0';
        if (result) *result = failed;
        return violation("wcrtomb_s: bad argument", guestEinval);
    }
    if ((!destination && capacity) || (destination && !usableSize(capacity))) {
        *result = failed;
        return violation("wcrtomb_s: bad argument", guestEinval);
    }
    char bytes[MB_LEN_MAX];
    std::size_t length = 0;
    if (!destination) {
        length = wcrtomb_nid_postfix(bytes, 0, state);
    } else if (static_cast<std::size_t>(_Getmbcurmax_nid_postfix()) > capacity) {
        length = wcrtomb_nid_postfix(bytes, wide, state);
        if (length != failed && length > capacity) {
            *result = failed;
            destination[0] = '\0';
            return violation("wcrtomb_s: buffer too small", guestErange);
        }
        if (length != failed) std::memcpy(destination, bytes, length);
    } else {
        length = wcrtomb_nid_postfix(destination, wide, state);
    }
    *result = length;
    return length == failed ? guestEilseq : 0;
}

int APS5_VABI mbstowcs_s_nid_postfix(std::size_t* result, std::uint16_t* destination, std::size_t capacity, const char* source, std::size_t limit) {
    if (!result || !source || (!destination && capacity) || (destination && (!usableSize(capacity) || limit > rsizeMax))) {
        if (result) *result = failed;
        if (destination && usableSize(capacity)) destination[0] = 0;
        return violation("mbstowcs_s: bad argument", guestEinval);
    }
    alignas(8) unsigned char state[guestStateSize]{};
    if (!destination) {
        std::size_t count = 0;
        for (;; ++count) {
            std::uint16_t wide = 0;
            const std::size_t length = mbrtowc_nid_postfix(&wide, source, INT_MAX, state);
            if (convertFailed(length)) {
                *result = failed;
                return guestEilseq;
            }
            if (length == 0 && wide == 0) break;
            source += length;
        }
        *result = count;
        return 0;
    }
    const std::size_t total = limit < capacity ? limit : capacity;
    std::size_t index = 0;
    while (index < total) {
        const std::size_t length = mbrtowc_nid_postfix(destination + index, source, INT_MAX, state);
        if (convertFailed(length)) {
            *result = failed;
            destination[index] = 0;
            return guestEilseq;
        }
        if (length == 0 && destination[index] == 0) {
            *result = index;
            return 0;
        }
        source += length;
        ++index;
    }
    if (index == capacity) {
        *result = failed;
        destination[0] = 0;
        return violation("mbstowcs_s: buffer overflow", guestErange);
    }
    *result = index;
    destination[index] = 0;
    return 0;
}

int APS5_VABI mbsrtowcs_s_nid_postfix(std::size_t* result, std::uint16_t* destination, std::size_t capacity, const char** source, std::size_t limit, void* state) {
    if (!result || !source || !*source || !state || (!destination && capacity) || (destination && (!usableSize(capacity) || limit > rsizeMax))) {
        if (destination && usableSize(capacity)) destination[0] = 0;
        if (result) *result = failed;
        return violation("mbsrtowcs_s: bad argument", guestEinval);
    }
    const char* cursor = *source;
    if (!destination) {
        std::size_t count = 0;
        for (;; ++count) {
            std::uint16_t wide = 0;
            const std::size_t length = mbrtowc_nid_postfix(&wide, cursor, INT_MAX, state);
            if (convertFailed(length)) {
                *result = failed;
                return guestEilseq;
            }
            if (length == 0 && wide == 0) break;
            cursor += length;
        }
        *result = count;
        return 0;
    }
    const std::size_t total = limit < capacity ? limit : capacity;
    std::size_t index = 0;
    while (index < total) {
        const std::size_t length = mbrtowc_nid_postfix(destination + index, cursor, INT_MAX, state);
        if (convertFailed(length)) {
            *source = cursor;
            destination[index] = 0;
            *result = failed;
            return guestEilseq;
        }
        if (length == 0 && destination[index] == 0) {
            *source = nullptr;
            *result = index;
            return 0;
        }
        cursor += length;
        ++index;
    }
    if (total != 0) *source = cursor;
    if (index == capacity) {
        destination[0] = 0;
        *result = failed;
        return violation("mbsrtowcs_s: buffer too small", guestErange);
    }
    destination[index] = 0;
    *result = index;
    return 0;
}

char* APS5_VABI gets_s_nid_postfix(char* buffer, std::size_t size) {
    if (size == 0) {
        violation("gets_s: zero buffer size", guestEinval);
        return nullptr;
    }
    if (!buffer || size > rsizeMax) {
        violation("gets_s: bad argument", guestEinval);
        return nullptr;
    }
    char* cursor = buffer;
    int value = getchar_nid_postfix();
    for (std::size_t left = size - 1; left != 0 && value != guestEof && value != '\n'; --left) {
        *cursor++ = static_cast<char>(value);
        value = getchar_nid_postfix();
    }
    if (value == '\n' || (value == guestEof && cursor != buffer)) {
        *cursor = '\0';
        return buffer;
    }
    if (value != guestEof) {
        violation("gets_s: short buffer", guestErange);
        while (value != guestEof && value != '\n') value = getchar_nid_postfix();
    }
    buffer[0] = '\0';
    return nullptr;
}

}
