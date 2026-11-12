#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstdint>

namespace {

constexpr std::int32_t NoSubstitution = -1;

bool Basic(std::int32_t value) {
    return (value >= 0 && value < 0xd800) || (value >= 0xe000 && value <= 0xffff);
}

bool ValidArguments(const void* source, std::int32_t sourceLength, const void* destination, std::int32_t capacity, std::int32_t substitution) {
    return !((!source && sourceLength != 0) || sourceLength < -1 || capacity < 0 || (!destination && capacity > 0) ||
             substitution > 0x10ffff || Icu::Surrogate(substitution));
}

}

extern "C" {

char16_t* APS5_VABI u_strFromUTF32WithSub_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                                      const std::int32_t* source, std::int32_t sourceLength, std::int32_t substitution,
                                                      std::int32_t* substitutions, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if (!ValidArguments(source, sourceLength, destination, capacity, substitution)) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    if (substitutions) *substitutions = 0;
    std::int64_t length = sourceLength;
    if (length < 0) for (length = 0; source[length] != 0; ++length) {}
    std::int64_t written = 0;
    std::int64_t overflow = 0;
    std::int32_t replaced = 0;
    const auto put = [&](char16_t unit) {
        if (written < capacity) destination[written++] = unit;
        else ++overflow;
    };
    for (std::int64_t index = 0; index < length; ++index) {
        auto value = source[index];
        for (;;) {
            if (Basic(value)) {
                put(static_cast<char16_t>(value));
            } else if (value >= 0x10000 && value <= 0x10ffff) {
                if (written + 2 <= capacity) {
                    destination[written++] = static_cast<char16_t>(0xd7c0 + (value >> 10));
                    destination[written++] = static_cast<char16_t>(0xdc00 + (value & 0x3ff));
                } else {
                    overflow += 2;
                }
            } else if ((value = substitution) < 0) {
                *status = Icu::InvalidCharacter;
                return nullptr;
            } else {
                ++replaced;
                continue;
            }
            break;
        }
    }
    const auto required = Icu::Checked(written + overflow);
    if (destinationLength) *destinationLength = required;
    if (substitutions) *substitutions = replaced;
    Icu::Terminate(destination, capacity, required, status);
    return destination;
}

char16_t* APS5_VABI u_strFromUTF32_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                               const std::int32_t* source, std::int32_t sourceLength, std::int32_t* status) {
    return u_strFromUTF32WithSub_nid_postfix(destination, capacity, destinationLength, source, sourceLength, NoSubstitution, nullptr, status);
}


std::int32_t* APS5_VABI u_strToUTF32WithSub_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                                        const char16_t* source, std::int32_t sourceLength, std::int32_t substitution,
                                                        std::int32_t* substitutions, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    if (!ValidArguments(source, sourceLength, destination, capacity, substitution)) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    if (substitutions) *substitutions = 0;
    std::int64_t length = sourceLength;
    if (length < 0) for (length = 0; source[length] != 0; ++length) {}
    std::int64_t written = 0;
    std::int64_t overflow = 0;
    std::int32_t replaced = 0;
    for (std::int64_t index = 0; index < length; ++index) {
        std::int32_t value = source[index];
        if (Icu::Surrogate(value)) {
            if (value < 0xdc00 && index + 1 < length && source[index + 1] >= 0xdc00 && source[index + 1] <= 0xdfff) {
                value = 0x10000 + ((value - 0xd800) << 10) + (source[index + 1] - 0xdc00);
                ++index;
            } else if ((value = substitution) < 0) {
                *status = Icu::InvalidCharacter;
                return nullptr;
            } else {
                ++replaced;
            }
        }
        if (written < capacity) destination[written++] = value;
        else ++overflow;
    }
    const auto required = Icu::Checked(written + overflow);
    if (destinationLength) *destinationLength = required;
    if (substitutions) *substitutions = replaced;
    Icu::Terminate(destination, capacity, required, status);
    return destination;
}

std::int32_t* APS5_VABI u_strToUTF32_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                                 const char16_t* source, std::int32_t sourceLength, std::int32_t* status) {
    return u_strToUTF32WithSub_nid_postfix(destination, capacity, destinationLength, source, sourceLength, NoSubstitution, nullptr, status);
}

}
