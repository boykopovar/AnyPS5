#pragma once

#include "prx/libc/include/General.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

static inline std::string CopyLibcxxStringArgument(const void* message) {
    constexpr std::size_t ShortDataOffset = 1;
    constexpr std::size_t LongSizeOffset = 8;
    constexpr std::size_t LongDataOffset = 16;
    std::array<char, 24> descriptor{};
    if (!GuestCopyIn_nid_no_patch(descriptor.data(), message, descriptor.size())) {
        throw std::invalid_argument("libc++ string argument: unreadable string");
    }

    const auto flags = static_cast<unsigned char>(descriptor[0]);
    if ((flags & 1u) == 0u) {
        const auto size = static_cast<std::size_t>(flags >> 1u);
        if (size >= descriptor.size() - ShortDataOffset || descriptor[ShortDataOffset + size] != '\0') {
            throw std::invalid_argument("libc++ string argument: invalid short string");
        }
        return std::string(descriptor.data() + ShortDataOffset);
    }

    std::uint64_t size = 0;
    std::uint64_t address = 0;
    std::memcpy(&size, descriptor.data() + LongSizeOffset, sizeof(size));
    std::memcpy(&address, descriptor.data() + LongDataOffset, sizeof(address));
    const auto maxAddress = std::numeric_limits<std::uintptr_t>::max();
    std::string text;
    if (address == 0 || address > maxAddress || size > maxAddress - address || size >= text.max_size()) {
        throw std::invalid_argument("libc++ string argument: invalid long string");
    }

    const auto* data = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address));
    const int error = CopyGuestString_nid_no_patch(data, static_cast<std::size_t>(size) + 1, text);
    if (error == ENOMEM) {
        throw std::bad_alloc();
    }
    if (error != 0) {
        throw std::invalid_argument("libc++ string argument: unreadable or unterminated message");
    }
    return text;
}
