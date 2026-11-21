#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestWindow.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <climits>
#include <stdexcept>

namespace {

constexpr std::size_t PendingOffset = 0xa;
constexpr std::int8_t LowSurrogatePending = 0x10;

void StorePending(unsigned char* state, std::int8_t pending) {
    const auto stored = static_cast<std::int16_t>(pending);
    std::memcpy(state + PendingOffset, &stored, sizeof(stored));
}

std::size_t EncodeUtf8(char* destination, std::uint64_t value) {
    if (value < 0x80) {
        destination[0] = static_cast<char>(value);
        return 1;
    }
    int extra = 5;
    unsigned lead = 0xfc;
    int shift = 30;
    if (value < 0x800) { extra = 1; lead = 0xc0; shift = 6; }
    else if (value < 0x10000) { extra = 2; lead = 0xe0; shift = 12; }
    else if (value < 0x200000) { extra = 3; lead = 0xf0; shift = 18; }
    else if (value < 0x4000000) { extra = 4; lead = 0xf8; shift = 24; }
    destination[0] = static_cast<char>((value >> shift) | lead);
    for (int index = 1; index <= extra; ++index)
        destination[index] = static_cast<char>(((value >> (shift - 6 * index)) & 0x3f) | 0x80);
    return static_cast<std::size_t>(extra + 1);
}

bool DecodeUtf8Byte(unsigned char byte, std::uint64_t& value, std::int8_t& pending) {
    if (pending > 0) {
        if ((byte & 0xc0) != 0x80) return false;
        value = (value << 6) | (byte & 0x3f);
        --pending;
        return true;
    }
    if (byte < 0x80) value = byte;
    else if ((byte & 0xe0) == 0xc0) { value = byte & 0x1f; pending = 1; }
    else if ((byte & 0xf0) == 0xe0) { value = byte & 0x0f; pending = 2; }
    else if ((byte & 0xf8) == 0xf0) { value = byte & 0x07; pending = 3; }
    else if (byte >= 0xfc) { value = byte & 0x03; pending = 5; }
    else if ((byte & 0xfc) == 0xf8) { value = byte & 0x03; pending = 4; }
    else return false;
    return true;
}

constexpr auto Invalid = static_cast<std::size_t>(-1);
constexpr auto Incomplete = static_cast<std::size_t>(-2);
constexpr std::size_t StateSize = 16;
constexpr std::size_t LocaleMaxLength = 3;

std::size_t LocaleToWide(std::uint16_t* destination, const char* source, std::size_t count, unsigned char* state) {
    if (!source) {
        std::memset(state, 0, StateSize);
        return 0;
    }
    if (!count) return Incomplete;
    std::uint64_t value = 0;
    std::memcpy(&value, state, sizeof(value));
    auto pending = static_cast<std::int8_t>(state[PendingOffset]);
    for (std::size_t consumed = 0; consumed < count;) {
        const auto byte = static_cast<unsigned char>(source[consumed++]);
        if ((pending == 0 && byte >= 0xf0) || !DecodeUtf8Byte(byte, value, pending)) {
            errno = 86;
            return Invalid;
        }
        if (pending == 0) {
            if (destination) *destination = static_cast<std::uint16_t>(value);
            StorePending(state, 0);
            return value == 0 ? 0 : consumed;
        }
    }
    std::memcpy(state, &value, sizeof(value));
    StorePending(state, pending);
    return Incomplete;
}

std::size_t LocaleStringToWide(std::uint16_t* destination, const char** source, std::size_t capacity, unsigned char* state) {
    const char* cursor = *source;
    std::size_t converted = 0;
    while (!destination || converted < capacity) {
        std::uint16_t value = 0;
        const std::size_t length = LocaleToWide(&value, cursor, LocaleMaxLength, state);
        if (length == Invalid || length == Incomplete) {
            if (destination) *source = cursor;
            errno = 86;
            return Invalid;
        }
        if (destination) destination[converted] = value;
        if (length == 0) {
            if (destination) *source = nullptr;
            return converted;
        }
        cursor += length;
        ++converted;
    }
    *source = cursor;
    return converted;
}

int ToInt(std::size_t result) {
    return result == Invalid || result == Incomplete ? -1 : static_cast<int>(result);
}

struct GuestShiftState {
    std::uint64_t value;
    std::int8_t pending;
};

GuestShiftState ReadShiftState(const void* state, const char* message) {
    std::array<unsigned char, PendingOffset + 1> bytes;
    GuestCopyInOrThrow(bytes.data(), state, bytes.size(), message);
    GuestShiftState result{};
    std::memcpy(&result.value, bytes.data(), sizeof(result.value));
    result.pending = static_cast<std::int8_t>(bytes[PendingOffset]);
    return result;
}

void WriteShiftValue(void* state, std::uint64_t value, const char* message) {
    GuestCopyOutOrThrow(state, &value, sizeof(value), message);
}

void WriteShiftPending(void* state, std::int8_t pending, const char* message) {
    const auto stored = static_cast<std::int16_t>(pending);
    GuestCopyOutOrThrow(static_cast<unsigned char*>(state) + PendingOffset, &stored, sizeof(stored), message);
}

template<typename TValue>
void WriteDestination(TValue* destination, TValue value, const char* message) {
    if (destination) GuestCopyOutOrThrow(destination, &value, sizeof(value), message);
}

}

extern "C" {

int __mb_cur_max_nid_postfix = 1;

int APS5_VABI ___mb_cur_max_nid_postfix() {
    return 1;
}

int APS5_VABI _Getmbcurmax_nid_postfix() { return static_cast<int>(LocaleMaxLength); }

int APS5_VABI mbsinit_nid_postfix(const void* state) {
    if (!state) return 1;
    std::int16_t pending = 0;
    std::memcpy(&pending, static_cast<const unsigned char*>(state) + PendingOffset, sizeof(pending));
    return pending == 0;
}

std::size_t APS5_VABI mbrtowc_nid_postfix(std::uint16_t* destination, const char* source, std::size_t count, void* state) {
    thread_local unsigned char internal[StateSize]{};
    if (!source) {
        destination = nullptr;
        source = "";
    }
    return LocaleToWide(destination, source, count, state ? static_cast<unsigned char*>(state) : internal);
}

std::size_t APS5_VABI mbrlen_nid_postfix(const char* source, std::size_t count, void* state) {
    thread_local unsigned char internal[StateSize]{};
    return LocaleToWide(nullptr, source, count, state ? static_cast<unsigned char*>(state) : internal);
}

int APS5_VABI mbtowc_nid_postfix(std::uint16_t* destination, const char* source, std::size_t count) {
    thread_local unsigned char internal[StateSize]{};
    return ToInt(LocaleToWide(destination, source, count < LocaleMaxLength ? count : LocaleMaxLength, internal));
}

std::size_t APS5_VABI wcrtomb_nid_postfix(char* destination, std::uint16_t value, void*) {
    if (!destination) return 1;
    return EncodeUtf8(destination, value);
}

std::size_t APS5_VABI _Mbrtowc_nid_postfix(std::uint16_t* destination, const char* source, std::size_t count, void* state) {
    return mbrtowc_nid_postfix(destination, source, count, state);
}

std::size_t APS5_VABI _Wcrtomb_nid_postfix(char* destination, std::uint16_t value, void* state) {
    return wcrtomb_nid_postfix(destination, value, state);
}

int APS5_VABI mblen_nid_postfix(const char* source, std::size_t count) {
    unsigned char state[StateSize]{};
    return ToInt(LocaleToWide(nullptr, source, count, state));
}

int APS5_VABI wctomb_nid_postfix(char* destination, std::uint16_t value) {
    if (!destination) return 0;
    return static_cast<int>(EncodeUtf8(destination, value));
}

std::size_t APS5_VABI c32rtomb_nid_postfix(char* destination, std::uint32_t value, void*) {
    char scratch[8];
    if (!destination) {
        destination = scratch;
        value = 0;
    }
    return EncodeUtf8(destination, value);
}

std::size_t APS5_VABI c16rtomb_nid_postfix(char* destination, std::uint16_t unit, void* state) {
    if (!state) throw std::invalid_argument("c16rtomb: null conversion state");
    char scratch[8];
    if (!destination) {
        destination = scratch;
        unit = 0;
    }
    auto* const bytes = static_cast<unsigned char*>(state);
    std::uint64_t value = unit;
    if (bytes[PendingOffset] != 0) {
        if (static_cast<std::uint16_t>(unit + 0x2000) < 0xfc00) return static_cast<std::size_t>(-1);
        std::uint64_t high = 0;
        std::memcpy(&high, bytes, sizeof(high));
        value = high | (unit & 0x3ffu);
        StorePending(bytes, 0);
    } else if (static_cast<std::uint16_t>(unit + 0x2400) >= 0xfc00) {
        const std::uint64_t high = ((static_cast<std::uint64_t>(unit) << 10) + 0x10000) & 0x1ffc00;
        std::memcpy(bytes, &high, sizeof(high));
        StorePending(bytes, 1);
        return 0;
    }
    return EncodeUtf8(destination, value);
}

std::size_t APS5_VABI mbrtoc32_nid_postfix(std::uint32_t* destination, const char* source, std::size_t count, void* state) {
    if (!state) throw std::invalid_argument("mbrtoc32: null conversion state");
    if (!source) {
        destination = nullptr;
        source = "";
        count = 1;
    }
    auto [value, pending] = ReadShiftState(state, "mbrtoc32: unreadable conversion state");
    GuestSourceWindow<char, 8> input(source, count, "mbrtoc32: unreadable source");
    for (std::size_t consumed = 0; consumed < count;) {
        const auto byte = static_cast<unsigned char>(input[consumed++]);
        if (!DecodeUtf8Byte(byte, value, pending)) {
            errno = 86;
            return static_cast<std::size_t>(-1);
        }
        if (pending == 0) {
            WriteDestination(destination, static_cast<std::uint32_t>(value), "mbrtoc32: unwritable destination");
            WriteShiftPending(state, 0, "mbrtoc32: unwritable conversion state");
            return value == 0 ? 0 : consumed;
        }
    }
    WriteShiftValue(state, value, "mbrtoc32: unwritable conversion state");
    WriteShiftPending(state, pending, "mbrtoc32: unwritable conversion state");
    return static_cast<std::size_t>(-2);
}

std::size_t APS5_VABI mbrtoc16_nid_postfix(std::uint16_t* destination, const char* source, std::size_t count, void* state) {
    if (!state) throw std::invalid_argument("mbrtoc16: null conversion state");
    if (!source) {
        destination = nullptr;
        source = "";
        count = 1;
    }
    auto [value, pending] = ReadShiftState(state, "mbrtoc16: unreadable conversion state");
    if (pending == LowSurrogatePending) {
        WriteDestination(destination, static_cast<std::uint16_t>((value & 0x3ff) | 0xdc00), "mbrtoc16: unwritable destination");
        WriteShiftPending(state, 0, "mbrtoc16: unwritable conversion state");
        return static_cast<std::size_t>(-3);
    }
    GuestSourceWindow<char, 8> input(source, count, "mbrtoc16: unreadable source");
    for (std::size_t consumed = 0; consumed < count;) {
        const auto byte = static_cast<unsigned char>(input[consumed++]);
        if (!DecodeUtf8Byte(byte, value, pending)) {
            errno = 86;
            return static_cast<std::size_t>(-1);
        }
        if (pending != 0) continue;
        if (value >= 0x110000) {
            errno = 86;
            return static_cast<std::size_t>(-1);
        }
        if (value >= 0x10000) {
            WriteDestination(destination, static_cast<std::uint16_t>(((value >> 10) + 0x3fc0) | 0xd800), "mbrtoc16: unwritable destination");
            WriteShiftPending(state, LowSurrogatePending, "mbrtoc16: unwritable conversion state");
            return consumed;
        }
        WriteDestination(destination, static_cast<std::uint16_t>(value), "mbrtoc16: unwritable destination");
        WriteShiftPending(state, 0, "mbrtoc16: unwritable conversion state");
        return value == 0 ? 0 : consumed;
    }
    WriteShiftValue(state, value, "mbrtoc16: unwritable conversion state");
    WriteShiftPending(state, pending, "mbrtoc16: unwritable conversion state");
    return static_cast<std::size_t>(-2);
}

std::size_t APS5_VABI wcsrtombs_nid_postfix(char* destination, const std::uint16_t** source, std::size_t capacity, void*) {
    const std::uint16_t* wide = nullptr;
    GuestCopyInOrThrow(&wide, source, sizeof(wide), "wcsrtombs: unreadable source pointer");
    GuestSourceWindow<std::uint16_t> input(wide, SIZE_MAX, "wcsrtombs: unreadable source");
    GuestDestinationWindow<char> output(destination, "wcsrtombs: unwritable destination");
    const auto store = [&](const std::uint16_t* next) {
        output.Flush();
        GuestCopyOutOrThrow(source, &next, sizeof(next), "wcsrtombs: unwritable source pointer");
    };
    std::size_t written = 0;
    for (std::size_t index = 0;; ++index) {
        const std::uint16_t unit = input[index];
        char bytes[LocaleMaxLength];
        const std::size_t length = EncodeUtf8(bytes, unit);
        if (destination) {
            if (length > capacity - written) {
                store(wide + index);
                return written;
            }
            for (std::size_t byte = 0; byte < length; ++byte) output.Put(bytes[byte]);
        }
        if (unit == 0) {
            if (destination) store(nullptr);
            return written;
        }
        written += length;
    }
}

static int WideToLocaleChecked(
    std::size_t* result, char* destination, std::size_t capacity, const std::uint16_t** source, std::size_t limit,
    void* state, bool stopAtLimit, const char* badArgument, const char* bufferTooSmall
) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    constexpr int GuestEilseq = 86;
    constexpr auto Failed = static_cast<std::size_t>(-1);
    constexpr std::size_t RsizeMax = SIZE_MAX >> 3;
    const auto violation = [](const char* message, int error) {
        SonyInvokeConstraintHandler(message, nullptr, error);
        return error;
    };
    if (!result || !source || !state || !*source || (destination && (capacity == 0 || capacity > RsizeMax || limit > RsizeMax))) {
        if (destination && capacity != 0 && capacity <= RsizeMax) destination[0] = '\0';
        if (result) *result = Failed;
        return violation(badArgument, GuestEinval);
    }
    if (!destination && capacity) {
        *result = Failed;
        return violation(badArgument, GuestEinval);
    }
    const auto* wide = *source;
    char bytes[MB_LEN_MAX];
    if (!destination) {
        std::size_t total = 0;
        for (;; ++wide) {
            const auto length = wcrtomb_nid_postfix(bytes, *wide, state);
            if (length == Failed) {
                *result = Failed;
                return GuestEilseq;
            }
            if (length && bytes[length - 1] == '\0') {
                *result = total + length - 1;
                return 0;
            }
            total += length;
        }
    }
    std::size_t written = 0;
    while (limit) {
        const auto length = wcrtomb_nid_postfix(bytes, *wide, state);
        if (length == Failed) {
            destination[written] = '\0';
            *result = Failed;
            return GuestEilseq;
        }
        if (length > limit && stopAtLimit) break;
        if (length > limit || length > capacity) {
            *source = wide;
            destination[0] = '\0';
            *result = Failed;
            return violation(bufferTooSmall, GuestErange);
        }
        std::memcpy(destination + written, bytes, length);
        written += length;
        if (length && destination[written - 1] == '\0') {
            *source = nullptr;
            *result = written - 1;
            return 0;
        }
        ++wide;
        capacity -= length;
        if (!capacity) {
            *source = wide;
            destination[0] = '\0';
            *result = Failed;
            return violation(bufferTooSmall, GuestErange);
        }
        limit -= length;
    }
    *source = wide;
    destination[written] = '\0';
    *result = written;
    return 0;
}

int APS5_VABI wcsrtombs_s_nid_postfix(
    std::size_t* result, char* destination, std::size_t capacity, const std::uint16_t** source, std::size_t limit,
    void* state
) {
    return WideToLocaleChecked(result, destination, capacity, source, limit, state, false, "wcsrtombs_s: bad argument", "wcstombs_s: buffer too small");
}

int WideToMultibyteChecked_nid_no_patch(
    std::size_t* result, char* destination, std::size_t capacity, const std::uint16_t* source, std::size_t limit
) {
    std::array<std::uint64_t, 16> state{};
    return WideToLocaleChecked(result, destination, capacity, &source, limit, state.data(), limit < capacity, "wcstombs_s: bad argument", "wcstombs_s: buffer overflow");
}

int APS5_VABI wcstombs_s_nid_postfix(
    std::size_t* result, char* destination, std::size_t capacity, const std::uint16_t* source, std::size_t limit
) {
    return WideToMultibyteChecked_nid_no_patch(result, destination, capacity, source, limit);
}

std::size_t APS5_VABI mbsrtowcs_nid_postfix(std::uint16_t* destination, const char** source, std::size_t capacity, void* state) {
    thread_local unsigned char internal[StateSize]{};
    if (destination) *destination = 0;
    return LocaleStringToWide(destination, source, capacity, state ? static_cast<unsigned char*>(state) : internal);
}

std::size_t APS5_VABI mbstowcs_nid_postfix(std::uint16_t* dest, const char* src, std::size_t size) {
    if (!src) return 0;
    unsigned char state[StateSize]{};
    return LocaleStringToWide(dest, &src, size, state);
}

}
