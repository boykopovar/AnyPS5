#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace {

std::string CopyLibcxxMessage(const void* message) {
    constexpr std::size_t ShortDataOffset = 1;
    constexpr std::size_t LongSizeOffset = 8;
    constexpr std::size_t LongDataOffset = 16;
    std::array<char, 24> descriptor{};
    if (!GuestCopyIn_nid_no_patch(descriptor.data(), message, descriptor.size())) {
        throw std::invalid_argument("libc++ runtime_error: unreadable string");
    }

    const auto flags = static_cast<unsigned char>(descriptor[0]);
    if ((flags & 1u) == 0u) {
        const auto size = static_cast<std::size_t>(flags >> 1u);
        if (size >= descriptor.size() - ShortDataOffset || descriptor[ShortDataOffset + size] != '\0') {
            throw std::invalid_argument("libc++ runtime_error: invalid short string");
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
        throw std::invalid_argument("libc++ runtime_error: invalid long string");
    }

    const auto* data = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address));
    const int error = CopyGuestString_nid_no_patch(data, static_cast<std::size_t>(size) + 1, text);
    if (error == ENOMEM) {
        throw std::bad_alloc();
    }
    if (error != 0) {
        throw std::invalid_argument("libc++ runtime_error: unreadable or unterminated message");
    }
    return text;
}

}

extern "C" void APS5_VABI _ZNSt13runtime_errorC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
    LibcException::ExceptionObject* self, const void* message) {
    _ZNSt13runtime_errorC1EPKc_nid_postfix(self, CopyLibcxxMessage(message).c_str());
}

extern "C" void APS5_VABI _ZNSt13runtime_errorC2ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
    LibcException::ExceptionObject* self, const void* message) {
    _ZNSt13runtime_errorC2EPKc_nid_postfix(self, CopyLibcxxMessage(message).c_str());
}
