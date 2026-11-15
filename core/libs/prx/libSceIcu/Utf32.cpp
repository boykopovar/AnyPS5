#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/GuestAccess.hpp"
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
    if (Icu::ReadGuest(status) > 0) return nullptr;
    if (!ValidArguments(source, sourceLength, destination, capacity, substitution)) {
        Icu::WriteGuest(status, Icu::IllegalArgument);
        return nullptr;
    }
    if (substitutions) Icu::WriteGuest(substitutions, std::int32_t{0});
    std::int64_t length = sourceLength;
    if (length < 0) length = Icu::GuestStringLength(source);
    std::int64_t written = 0;
    std::int64_t overflow = 0;
    std::int32_t replaced = 0;
    const auto put = [&](char16_t unit) {
        if (written < capacity) Icu::WriteGuest(destination, unit, written++);
        else ++overflow;
    };
    for (std::int64_t index = 0; index < length; ++index) {
        auto value = Icu::ReadGuest(source, index);
        for (;;) {
            if (Basic(value)) {
                put(static_cast<char16_t>(value));
            } else if (value >= 0x10000 && value <= 0x10ffff) {
                if (written + 2 <= capacity) {
                    Icu::WriteGuest(destination, static_cast<char16_t>(0xd7c0 + (value >> 10)), written++);
                    Icu::WriteGuest(destination, static_cast<char16_t>(0xdc00 + (value & 0x3ff)), written++);
                } else {
                    overflow += 2;
                }
            } else if ((value = substitution) < 0) {
                Icu::WriteGuest(status, Icu::InvalidCharacter);
                return nullptr;
            } else {
                ++replaced;
                continue;
            }
            break;
        }
    }
    const auto required = Icu::Checked(written + overflow);
    if (destinationLength) Icu::WriteGuest(destinationLength, required);
    if (substitutions) Icu::WriteGuest(substitutions, replaced);
    Icu::TerminateGuest(destination, capacity, required, status);
    return destination;
}

char16_t* APS5_VABI u_strFromUTF32_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                               const std::int32_t* source, std::int32_t sourceLength, std::int32_t* status) {
    return u_strFromUTF32WithSub_nid_postfix(destination, capacity, destinationLength, source, sourceLength, NoSubstitution, nullptr, status);
}


std::int32_t* APS5_VABI u_strToUTF32WithSub_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                                        const char16_t* source, std::int32_t sourceLength, std::int32_t substitution,
                                                        std::int32_t* substitutions, std::int32_t* status) {
    if (Icu::ReadGuest(status) > 0) return nullptr;
    if (!ValidArguments(source, sourceLength, destination, capacity, substitution)) {
        Icu::WriteGuest(status, Icu::IllegalArgument);
        return nullptr;
    }
    if (substitutions) Icu::WriteGuest(substitutions, std::int32_t{0});
    std::int64_t length = sourceLength;
    if (length < 0) length = Icu::GuestStringLength(source);
    std::int64_t written = 0;
    std::int64_t overflow = 0;
    std::int32_t replaced = 0;
    for (std::int64_t index = 0; index < length; ++index) {
        std::int32_t value = Icu::ReadGuest(source, index);
        if (Icu::Surrogate(value)) {
            const auto next = value < 0xdc00 && index + 1 < length ? Icu::ReadGuest(source, index + 1) : char16_t{};
            if (next >= 0xdc00 && next <= 0xdfff) {
                value = 0x10000 + ((value - 0xd800) << 10) + (next - 0xdc00);
                ++index;
            } else if ((value = substitution) < 0) {
                Icu::WriteGuest(status, Icu::InvalidCharacter);
                return nullptr;
            } else {
                ++replaced;
            }
        }
        if (written < capacity) Icu::WriteGuest(destination, value, written++);
        else ++overflow;
    }
    const auto required = Icu::Checked(written + overflow);
    if (destinationLength) Icu::WriteGuest(destinationLength, required);
    if (substitutions) Icu::WriteGuest(substitutions, replaced);
    Icu::TerminateGuest(destination, capacity, required, status);
    return destination;
}

std::int32_t* APS5_VABI u_strToUTF32_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                                 const char16_t* source, std::int32_t sourceLength, std::int32_t* status) {
    return u_strToUTF32WithSub_nid_postfix(destination, capacity, destinationLength, source, sourceLength, NoSubstitution, nullptr, status);
}

}
