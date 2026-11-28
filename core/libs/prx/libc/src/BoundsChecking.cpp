#include "prx/libc/include/Api.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <cstring>
#include <string>
#include <vector>

#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
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
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {
        violation("bsearch_s: bad argument", guestEinval);
        return nullptr;
    }
    if (count == 0) return nullptr;
    if (!key || !base || !compare) {
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
    char bytes[MB_LEN_MAX];
    const int length = wctomb_nid_postfix(destination ? bytes : nullptr, wide);
    if (destination && length > 0 && static_cast<std::size_t>(length) > capacity) return violation("wctomb_s: buffer too small", guestErange);
    if (destination && length > 0 && !GuestCopyOut_nid_no_patch(destination, bytes, static_cast<std::size_t>(length)))
        return violation("wctomb_s: bad argument", guestEinval);
    if (!GuestCopyOut_nid_no_patch(status, &length, sizeof(length))) return violation("wctomb_s: bad argument", guestEinval);
    return destination && length < 0 ? guestEilseq : 0;
}

int APS5_VABI wcrtomb_s_nid_postfix(std::size_t* result, char* destination, std::size_t capacity, std::uint16_t wide, void* state) {
    const char terminator = '\0';
    if (!result || !state) {
        if (destination && usableSize(capacity)) GuestCopyOut_nid_no_patch(destination, &terminator, sizeof(terminator));
        if (result) GuestCopyOut_nid_no_patch(result, &failed, sizeof(failed));
        return violation("wcrtomb_s: bad argument", guestEinval);
    }
    if ((!destination && capacity) || (destination && !usableSize(capacity))) {
        GuestCopyOut_nid_no_patch(result, &failed, sizeof(failed));
        return violation("wcrtomb_s: bad argument", guestEinval);
    }
    char bytes[MB_LEN_MAX];
    const std::size_t length = wcrtomb_nid_postfix(bytes, destination ? wide : 0, state);
    if (destination && length != failed && length > capacity) {
        GuestCopyOut_nid_no_patch(result, &failed, sizeof(failed));
        GuestCopyOut_nid_no_patch(destination, &terminator, sizeof(terminator));
        return violation("wcrtomb_s: buffer too small", guestErange);
    }
    if (destination && length != failed && !GuestCopyOut_nid_no_patch(destination, bytes, length)) {
        GuestCopyOut_nid_no_patch(result, &failed, sizeof(failed));
        return violation("wcrtomb_s: bad argument", guestEinval);
    }
    if (!GuestCopyOut_nid_no_patch(result, &length, sizeof(length))) return violation("wcrtomb_s: bad argument", guestEinval);
    return length == failed ? guestEilseq : 0;
}

int APS5_VABI mbstowcs_s_nid_postfix(std::size_t* result, std::uint16_t* destination, std::size_t capacity, const char* source, std::size_t limit) {
    const std::uint16_t terminator = 0;
    const auto fault = [&] {
        GuestCopyOut_nid_no_patch(result, &failed, sizeof(failed));
        return violation("mbstowcs_s: bad argument", guestEinval);
    };
    const auto put = [&](std::size_t index, std::uint16_t value) { return GuestCopyOut_nid_no_patch(destination + index, &value, sizeof(value)); };
    const auto report = [&](std::size_t value) { return GuestCopyOut_nid_no_patch(result, &value, sizeof(value)); };
    if (!result || !source || (!destination && capacity) || (destination && (!usableSize(capacity) || limit > rsizeMax))) {
        if (result) report(failed);
        if (destination && usableSize(capacity)) put(0, terminator);
        return violation("mbstowcs_s: bad argument", guestEinval);
    }
    alignas(8) unsigned char state[guestStateSize]{};
    if (!destination) {
        std::size_t count = 0;
        for (;; ++count) {
            std::uint16_t wide = 0;
            const std::size_t length = mbrtowc_nid_postfix(&wide, source, INT_MAX, state);
            if (convertFailed(length)) return report(failed) ? guestEilseq : fault();
            if (length == 0 && wide == 0) break;
            source += length;
        }
        return report(count) ? 0 : fault();
    }
    const std::size_t total = limit < capacity ? limit : capacity;
    std::vector<std::uint16_t> converted;
    const auto store = [&] { return GuestCopyOut_nid_no_patch(destination, converted.data(), converted.size() * sizeof(converted[0])); };
    while (converted.size() < total) {
        std::uint16_t wide = 0;
        const std::size_t length = mbrtowc_nid_postfix(&wide, source, INT_MAX, state);
        if (convertFailed(length)) {
            converted.push_back(terminator);
            return report(failed) && store() ? guestEilseq : fault();
        }
        converted.push_back(wide);
        if (length == 0 && wide == 0) return report(converted.size() - 1) && store() ? 0 : fault();
        source += length;
    }
    if (converted.size() == capacity) {
        converted[0] = terminator;
        if (!report(failed) || !store()) return fault();
        return violation("mbstowcs_s: buffer overflow", guestErange);
    }
    const std::size_t index = converted.size();
    converted.push_back(terminator);
    return report(index) && store() ? 0 : fault();
}

int APS5_VABI mbsrtowcs_s_nid_postfix(std::size_t* result, std::uint16_t* destination, std::size_t capacity, const char** source, std::size_t limit, void* state) {
    const std::uint16_t terminator = 0;
    const auto fault = [&] {
        GuestCopyOut_nid_no_patch(result, &failed, sizeof(failed));
        return violation("mbsrtowcs_s: bad argument", guestEinval);
    };
    const auto put = [&](std::size_t index, std::uint16_t value) { return GuestCopyOut_nid_no_patch(destination + index, &value, sizeof(value)); };
    const auto report = [&](std::size_t value) { return GuestCopyOut_nid_no_patch(result, &value, sizeof(value)); };
    const auto advance = [&](const char* value) { return GuestCopyOut_nid_no_patch(source, &value, sizeof(value)); };
    const char* cursor = nullptr;
    if (source && !GuestCopyIn_nid_no_patch(&cursor, source, sizeof(cursor))) return fault();
    if (!result || !source || !cursor || !state || (!destination && capacity) || (destination && (!usableSize(capacity) || limit > rsizeMax))) {
        if (destination && usableSize(capacity)) put(0, terminator);
        if (result) report(failed);
        return violation("mbsrtowcs_s: bad argument", guestEinval);
    }
    if (!destination) {
        std::size_t count = 0;
        for (;; ++count) {
            std::uint16_t wide = 0;
            const std::size_t length = mbrtowc_nid_postfix(&wide, cursor, INT_MAX, state);
            if (convertFailed(length)) return report(failed) ? guestEilseq : fault();
            if (length == 0 && wide == 0) break;
            cursor += length;
        }
        return report(count) ? 0 : fault();
    }
    const std::size_t total = limit < capacity ? limit : capacity;
    std::vector<std::uint16_t> converted;
    const auto store = [&] { return GuestCopyOut_nid_no_patch(destination, converted.data(), converted.size() * sizeof(converted[0])); };
    while (converted.size() < total) {
        std::uint16_t wide = 0;
        const std::size_t length = mbrtowc_nid_postfix(&wide, cursor, INT_MAX, state);
        if (convertFailed(length)) {
            converted.push_back(terminator);
            return advance(cursor) && store() && report(failed) ? guestEilseq : fault();
        }
        converted.push_back(wide);
        if (length == 0 && wide == 0) return store() && advance(nullptr) && report(converted.size() - 1) ? 0 : fault();
        cursor += length;
    }
    if (total != 0 && !advance(cursor)) return fault();
    if (converted.size() == capacity) {
        converted[0] = terminator;
        if (!store() || !report(failed)) return fault();
        return violation("mbsrtowcs_s: buffer too small", guestErange);
    }
    const std::size_t index = converted.size();
    converted.push_back(terminator);
    return store() && report(index) ? 0 : fault();
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
    const char terminator = '\0';
    const auto unwritable = [&](int pending) {
        violation("gets_s: bad argument", guestEinval);
        while (pending != guestEof && pending != '\n') pending = getchar_nid_postfix();
        return nullptr;
    };
    std::string line;
    int value = getchar_nid_postfix();
    for (std::size_t left = size - 1; left != 0 && value != guestEof && value != '\n'; --left) {
        line.push_back(static_cast<char>(value));
        value = getchar_nid_postfix();
    }
    if (value == '\n' || (value == guestEof && !line.empty())) {
        if (!GuestCopyOut_nid_no_patch(buffer, line.c_str(), line.size() + 1)) return unwritable(guestEof);
        return buffer;
    }
    if (value != guestEof) {
        std::string kept = line.empty() ? std::string(1, terminator) : line;
        kept[0] = terminator;
        if (!GuestCopyOut_nid_no_patch(buffer, kept.data(), kept.size())) return unwritable(value);
        violation("gets_s: short buffer", guestErange);
        while (value != guestEof && value != '\n') value = getchar_nid_postfix();
        return nullptr;
    }
    if (!GuestCopyOut_nid_no_patch(buffer, &terminator, sizeof(terminator))) return unwritable(guestEof);
    return nullptr;
}

}
