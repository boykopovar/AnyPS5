#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <string>

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidCharacter = 10;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NotTerminated = -124;

bool Surrogate(std::int32_t value) { return value >= 0xd800 && value <= 0xdfff; }

template<class TSource, class TDestination>
bool Validate(TDestination* destination, std::int32_t capacity, const TSource* source,
              std::int32_t length, std::int32_t substitution, std::int32_t* error) {
    if (!error) NotImplemented_nid_no_patch(__func__);
    if (*error > 0) return false;
    if ((!source && length != 0) || length < -1 || capacity < 0 || (!destination && capacity > 0) ||
        substitution > 0x10ffff || Surrogate(substitution)) {
        *error = IllegalArgument;
        return false;
    }
    return true;
}

template<class T>
std::int32_t Length(const T* source, std::int32_t length) {
    if (length >= 0) return length;
    const auto measured = std::char_traits<T>::length(source);
    if (measured > std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
    return static_cast<std::int32_t>(measured);
}

template<class T>
void Finish(T* destination, std::int32_t capacity, std::int64_t count, std::int32_t* length, std::int32_t* error) {
    if (count > std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
    if (length) *length = static_cast<std::int32_t>(count);
    if (count < capacity) {
        destination[count] = 0;
        if (*error == NotTerminated) *error = 0;
    } else {
        *error = count == capacity ? NotTerminated : BufferOverflow;
    }
}

std::int32_t Decode(const char* source, std::int32_t length, std::int32_t& position) {
    const auto lead = static_cast<std::uint8_t>(source[position++]);
    if (lead < 0x80) return lead;
    unsigned trailing = 0;
    std::uint32_t value = 0;
    if (lead >= 0xc2 && lead <= 0xdf) { trailing = 1; value = lead & 0x1f; }
    else if (lead >= 0xe0 && lead <= 0xef) { trailing = 2; value = lead & 0x0f; }
    else if (lead >= 0xf0 && lead <= 0xf4) { trailing = 3; value = lead & 7; }
    else return -1;
    for (unsigned index = 0; index < trailing; ++index) {
        if (position == length) return -1;
        const auto next = static_cast<std::uint8_t>(source[position]);
        if (next < 0x80 || next > 0xbf) return -1;
        if (index == 0 && ((lead == 0xe0 && next < 0xa0) || (lead == 0xed && next >= 0xa0) ||
                          (lead == 0xf0 && next < 0x90) || (lead == 0xf4 && next >= 0x90))) return -1;
        value = (value << 6) | (next & 0x3f);
        ++position;
    }
    return static_cast<std::int32_t>(value);
}

char16_t* FromUtf8(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                   const char* source, std::int32_t sourceLength, std::int32_t substitution,
                   std::int32_t* substitutions, std::int32_t* error) {
    if (!Validate(destination, capacity, source, sourceLength, substitution, error)) return nullptr;
    if (substitutions) *substitutions = 0;
    const auto length = Length(source, sourceLength);
    std::int64_t count = 0;
    std::int32_t replacements = 0;
    const auto emit = [&](char16_t unit) {
        if (count < capacity) destination[count] = unit;
        ++count;
    };
    for (std::int32_t position = 0; position < length;) {
        auto value = Decode(source, length, position);
        if (value < 0) {
            if (substitution < 0) { *error = InvalidCharacter; return nullptr; }
            value = substitution;
            ++replacements;
        }
        if (value <= 0xffff) emit(static_cast<char16_t>(value));
        else {
            emit(static_cast<char16_t>(0xd7c0 + (value >> 10)));
            emit(static_cast<char16_t>(0xdc00 + (value & 0x3ff)));
        }
    }
    if (substitutions) *substitutions = replacements;
    Finish(destination, capacity, count, destinationLength, error);
    return destination;
}

char* ToUtf8(char* destination, std::int32_t capacity, std::int32_t* destinationLength,
             const char16_t* source, std::int32_t sourceLength, std::int32_t substitution,
             std::int32_t* substitutions, std::int32_t* error) {
    if (!Validate(destination, capacity, source, sourceLength, substitution, error)) return nullptr;
    if (substitutions) *substitutions = 0;
    const auto length = Length(source, sourceLength);
    std::int64_t count = 0;
    std::int32_t replacements = 0;
    bool writing = true;
    for (std::int32_t position = 0; position < length;) {
        std::int32_t value = source[position++];
        if (Surrogate(value)) {
            if (value < 0xdc00 && position < length && source[position] >= 0xdc00 && source[position] <= 0xdfff) {
                value = 0x10000 + ((value - 0xd800) << 10) + source[position++] - 0xdc00;
            } else {
                if (substitution < 0) { *error = InvalidCharacter; return nullptr; }
                value = substitution;
                ++replacements;
            }
        }
        std::array<std::uint8_t, 4> encoded{};
        unsigned units;
        if (value < 0x80) { units = 1; encoded[0] = value; }
        else {
            units = value < 0x800 ? 2 : value < 0x10000 ? 3 : 4;
            auto remaining = static_cast<std::uint32_t>(value);
            for (unsigned index = units - 1; index > 0; --index) {
                encoded[index] = 0x80 | (remaining & 0x3f);
                remaining >>= 6;
            }
            encoded[0] = (units == 2 ? 0xc0 : units == 3 ? 0xe0 : 0xf0) | remaining;
        }
        if (writing && count + units <= capacity) {
            for (unsigned index = 0; index < units; ++index) destination[count + index] = static_cast<char>(encoded[index]);
        } else writing = false;
        count += units;
    }
    if (substitutions) *substitutions = replacements;
    Finish(destination, capacity, count, destinationLength, error);
    return destination;
}

} // namespace

extern "C" {

char16_t* APS5_VABI u_strFromUTF8_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
    const char* source, std::int32_t sourceLength, std::int32_t* error) {
    return FromUtf8(destination, capacity, destinationLength, source, sourceLength, -1, nullptr, error);
}

char* APS5_VABI u_strToUTF8_nid_postfix(char* destination, std::int32_t capacity, std::int32_t* destinationLength,
    const char16_t* source, std::int32_t sourceLength, std::int32_t* error) {
    return ToUtf8(destination, capacity, destinationLength, source, sourceLength, -1, nullptr, error);
}

char16_t* APS5_VABI u_strFromUTF8WithSub_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
    const char* source, std::int32_t sourceLength, std::int32_t substitution, std::int32_t* substitutions, std::int32_t* error) {
    return FromUtf8(destination, capacity, destinationLength, source, sourceLength, substitution, substitutions, error);
}

char* APS5_VABI u_strToUTF8WithSub_nid_postfix(char* destination, std::int32_t capacity, std::int32_t* destinationLength,
    const char16_t* source, std::int32_t sourceLength, std::int32_t substitution, std::int32_t* substitutions, std::int32_t* error) {
    return ToUtf8(destination, capacity, destinationLength, source, sourceLength, substitution, substitutions, error);
}

char16_t* APS5_VABI u_strFromUTF8Lenient_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
    const char* source, std::int32_t sourceLength, std::int32_t* error) {
    if (!Validate(destination, capacity, source, sourceLength, -1, error)) return nullptr;
    if (sourceLength >= 0 && capacity < sourceLength) {
        if (destinationLength) *destinationLength = sourceLength;
        *error = BufferOverflow;
        return nullptr;
    }
    const auto length = Length(source, sourceLength);
    std::int64_t count = 0;
    const auto emit = [&](char16_t unit) {
        if (count < capacity) destination[count] = unit;
        ++count;
    };
    for (std::int32_t position = 0; position < length;) {
        const std::uint32_t lead = static_cast<std::uint8_t>(source[position]);
        const unsigned units = lead < 0xc0 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        if (length - position < static_cast<std::int32_t>(units)) { emit(0xfffd); break; }
        std::uint32_t value = lead;
        for (unsigned index = 1; index < units; ++index) value = (value << 6) + static_cast<std::uint8_t>(source[position + index]);
        value -= units == 2 ? 0x3080 : units == 3 ? 0x2080 : units == 4 ? 0x3c82080 : 0;
        if (units < 4) emit(static_cast<char16_t>(value));
        else {
            emit(static_cast<char16_t>(0xd7c0 + (value >> 10)));
            emit(static_cast<char16_t>(0xdc00 + (value & 0x3ff)));
        }
        position += units;
    }
    Finish(destination, capacity, count, destinationLength, error);
    return destination;
}

}
