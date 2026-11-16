#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

constexpr std::int32_t NoSubstitution = -1;

bool Basic(std::int32_t value) {
    return (value >= 0 && value < 0xd800) || (value >= 0xe000 && value <= 0xffff);
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

template <typename Unit>
bool CopySource(const Unit* source, std::int32_t sourceLength, std::vector<Unit>& units) {
    if (sourceLength >= 0) {
        units.resize(static_cast<std::size_t>(sourceLength));
        return units.empty() || CopyIn(units.data(), source, units.size() * sizeof(Unit));
    }
    units.clear();
    for (std::size_t offset = 0;;) {
        Unit chunk[256];
        const auto address = reinterpret_cast<std::uintptr_t>(source + offset);
        const std::size_t count = (std::min)(std::size(chunk), (std::max)(std::size_t{1}, (0x1000 - address % 0x1000) / sizeof(Unit)));
        if (!CopyIn(chunk, source + offset, count * sizeof(Unit))) return false;
        for (std::size_t index = 0; index < count; ++index) {
            if (chunk[index] == 0) return true;
            units.push_back(chunk[index]);
        }
        offset += count;
    }
}

template <typename Unit>
bool Finish(Unit* destination, std::int32_t capacity, std::vector<Unit>& written, std::int32_t required, std::int32_t* destinationLength,
            std::int32_t* substitutions, std::int32_t replaced, std::int32_t* status) {
    if (destinationLength && !CopyOut(destinationLength, &required, sizeof(required))) return false;
    if (substitutions && !CopyOut(substitutions, &replaced, sizeof(replaced))) return false;
    const std::size_t units = written.size();
    written.push_back(0);
    Icu::Terminate(written.data(), capacity, required, status);
    const std::size_t total = (std::min)(static_cast<std::size_t>(capacity), units + (required < capacity ? 1 : 0));
    return total == 0 || CopyOut(destination, written.data(), total * sizeof(Unit));
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
    const std::int32_t none = 0;
    std::vector<std::int32_t> input;
    if ((substitutions && !CopyOut(substitutions, &none, sizeof(none))) || !CopySource(source, sourceLength, input)) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    std::vector<char16_t> output;
    std::int64_t overflow = 0;
    std::int32_t replaced = 0;
    const auto put = [&](char16_t unit) {
        if (static_cast<std::int64_t>(output.size()) < capacity) output.push_back(unit);
        else ++overflow;
    };
    for (const std::int32_t unit : input) {
        auto value = unit;
        for (;;) {
            if (Basic(value)) {
                put(static_cast<char16_t>(value));
            } else if (value >= 0x10000 && value <= 0x10ffff) {
                if (static_cast<std::int64_t>(output.size()) + 2 <= capacity) {
                    output.push_back(static_cast<char16_t>(0xd7c0 + (value >> 10)));
                    output.push_back(static_cast<char16_t>(0xdc00 + (value & 0x3ff)));
                } else {
                    overflow += 2;
                }
            } else if ((value = substitution) < 0) {
                *status = Icu::InvalidCharacter;
                if (!output.empty()) CopyOut(destination, output.data(), output.size() * sizeof(char16_t));
                return nullptr;
            } else {
                ++replaced;
                continue;
            }
            break;
        }
    }
    const auto required = Icu::Checked(static_cast<std::int64_t>(output.size()) + overflow);
    if (!Finish(destination, capacity, output, required, destinationLength, substitutions, replaced, status)) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
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
    const std::int32_t none = 0;
    std::vector<char16_t> input;
    if ((substitutions && !CopyOut(substitutions, &none, sizeof(none))) || !CopySource(source, sourceLength, input)) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    const auto length = static_cast<std::int64_t>(input.size());
    std::vector<std::int32_t> output;
    std::int64_t overflow = 0;
    std::int32_t replaced = 0;
    for (std::int64_t index = 0; index < length; ++index) {
        std::int32_t value = input[index];
        if (Icu::Surrogate(value)) {
            if (value < 0xdc00 && index + 1 < length && input[index + 1] >= 0xdc00 && input[index + 1] <= 0xdfff) {
                value = 0x10000 + ((value - 0xd800) << 10) + (input[index + 1] - 0xdc00);
                ++index;
            } else if ((value = substitution) < 0) {
                *status = Icu::InvalidCharacter;
                if (!output.empty()) CopyOut(destination, output.data(), output.size() * sizeof(std::int32_t));
                return nullptr;
            } else {
                ++replaced;
            }
        }
        if (static_cast<std::int64_t>(output.size()) < capacity) output.push_back(value);
        else ++overflow;
    }
    const auto required = Icu::Checked(static_cast<std::int64_t>(output.size()) + overflow);
    if (!Finish(destination, capacity, output, required, destinationLength, substitutions, replaced, status)) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    return destination;
}

std::int32_t* APS5_VABI u_strToUTF32_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                                                 const char16_t* source, std::int32_t sourceLength, std::int32_t* status) {
    return u_strToUTF32WithSub_nid_postfix(destination, capacity, destinationLength, source, sourceLength, NoSubstitution, nullptr, status);
}

}
