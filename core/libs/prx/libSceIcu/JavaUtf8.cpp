#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstdint>
#include <cstring>

namespace {

bool Continuation(std::uint8_t byte) {
    return byte >= 0x80 && byte <= 0xbf;
}

bool ThreeByteSecond(std::uint8_t lead, std::uint8_t byte) {
    if (lead == 0xe0) return byte >= 0xa0 && byte <= 0xbf;
    if (lead == 0xed) return byte >= 0x80 && byte <= 0x9f;
    return Continuation(byte);
}

bool FourByteSecond(std::uint8_t lead, std::uint8_t byte) {
    if (lead == 0xf0) return byte >= 0x90 && byte <= 0xbf;
    if (lead == 0xf4) return byte >= 0x80 && byte <= 0x8f;
    return lead >= 0xf1 && lead <= 0xf3 && Continuation(byte);
}

std::int64_t Skipped(const std::uint8_t* source, std::int64_t lead, std::int64_t length) {
    const auto first = source[lead];
    const auto at = [&](std::int64_t offset) { return lead + offset < length ? source[lead + offset] : 0; };
    if (first >= 0xf0 && first <= 0xf4) {
        if (lead + 1 >= length || !FourByteSecond(first, source[lead + 1])) return 1;
        if (!Continuation(at(2))) return 2;
        if (!Continuation(at(3))) return 3;
        return 4;
    }
    if (first >= 0xe0 && first <= 0xef) {
        if (lead + 1 >= length || !ThreeByteSecond(first, source[lead + 1])) return 1;
        return 2;
    }
    return 1;
}

int Encoded(char16_t unit) {
    return unit != 0 && unit <= 0x7f ? 1 : unit <= 0x7ff ? 2 : 3;
}

}

extern "C" {

char16_t* APS5_VABI u_strFromJavaModifiedUTF8WithSub_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                                                 const char* text, std::int32_t sourceLength, std::int32_t substitution,
                                                                 std::int32_t* substitutions, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if ((!text && sourceLength != 0) || sourceLength < -1 || (!destination && capacity != 0) || capacity < 0 ||
        substitution > 0x10ffff || Icu::Surrogate(substitution)) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    if (substitutions) *substitutions = 0;
    const auto* source = reinterpret_cast<const std::uint8_t*>(text);
    const std::int64_t length = sourceLength < 0 ? static_cast<std::int64_t>(std::strlen(text)) : sourceLength;
    std::int64_t written = 0;
    std::int64_t extra = 0;
    std::int32_t replaced = 0;
    for (std::int64_t index = 0; index < length;) {
        const auto lead = source[index];
        const bool writing = written < capacity;
        char16_t unit = lead;
        std::int64_t used = 1;
        bool valid = true;
        if (lead >= 0xc0 && lead <= 0xdf && index + 1 < length && Continuation(source[index + 1])) {
            unit = static_cast<char16_t>(((lead & 0x1f) << 6) | (source[index + 1] & 0x3f));
            used = 2;
        } else if (lead >= 0xe0 && lead <= 0xef && index + 2 < length && Continuation(source[index + 1]) && Continuation(source[index + 2])) {
            unit = static_cast<char16_t>(((lead & 0x0f) << 12) | ((source[index + 1] & 0x3f) << 6) | (source[index + 2] & 0x3f));
            used = 3;
        } else if (lead >= 0x80) {
            valid = false;
        }
        if (valid) {
            index += used;
            if (writing) destination[written++] = unit;
            else ++extra;
            continue;
        }
        if (substitution < 0) {
            *status = Icu::InvalidCharacter;
            return nullptr;
        }
        index += Skipped(source, index, length);
        ++replaced;
        if (!writing) {
            ++extra;
        } else if (substitution <= 0xffff) {
            destination[written++] = static_cast<char16_t>(substitution);
        } else {
            destination[written++] = static_cast<char16_t>(0xd7c0 + (substitution >> 10));
            if (written < capacity) destination[written++] = static_cast<char16_t>(0xdc00 + (substitution & 0x3ff));
            else ++extra;
        }
    }
    const auto required = Icu::Checked(written + extra);
    if (substitutions) *substitutions = replaced;
    if (destinationLength) *destinationLength = required;
    Icu::Terminate(destination, capacity, required, status);
    return destination;
}


char* APS5_VABI u_strToJavaModifiedUTF8_nid_postfix(char* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                                    const char16_t* source, std::int32_t sourceLength, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if ((!source && sourceLength != 0) || sourceLength < -1 || (!destination && capacity != 0) || capacity < 0) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    std::int64_t length = sourceLength;
    if (length < 0) for (length = 0; source[length] != 0; ++length) {}
    std::int64_t written = 0;
    std::int64_t extra = 0;
    bool full = false;
    for (std::int64_t index = 0; index < length; ++index) {
        const auto unit = source[index];
        const auto size = Encoded(unit);
        if (!full && capacity - written >= size) {
            if (size == 1) {
                destination[written++] = static_cast<char>(unit);
            } else if (size == 2) {
                destination[written++] = static_cast<char>(0xc0 | (unit >> 6));
                destination[written++] = static_cast<char>(0x80 | (unit & 0x3f));
            } else {
                destination[written++] = static_cast<char>(0xe0 | (unit >> 12));
                destination[written++] = static_cast<char>(0x80 | ((unit >> 6) & 0x3f));
                destination[written++] = static_cast<char>(0x80 | (unit & 0x3f));
            }
        } else {
            full = true;
            extra += size;
        }
    }
    const auto required = Icu::Checked(written + extra);
    if (destinationLength) *destinationLength = required;
    Icu::Terminate(destination, capacity, required, status);
    return destination;
}

}
