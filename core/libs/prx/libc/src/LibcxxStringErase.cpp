#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstring>

extern "C" void* APS5_VABI _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5eraseEmm_nid_postfix(
    void* string, std::uint64_t position, std::uint64_t count) {
    std::array<char, 24> descriptor{};
    if (!GuestCopyIn_nid_no_patch(descriptor.data(), string, descriptor.size())) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::erase: unreadable string");
    }

    const auto flags = static_cast<unsigned char>(descriptor[0]);
    const bool isShort = (flags & 1u) == 0u;
    std::uint64_t size = flags >> 1u;
    std::uint64_t address = 0;
    if (isShort) {
        if (size >= descriptor.size() - 1) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::erase: invalid short size");
        }
    } else {
        std::memcpy(&size, descriptor.data() + 8, sizeof(size));
        std::memcpy(&address, descriptor.data() + 16, sizeof(address));
        if (address == 0 || address > UINTPTR_MAX || size >= UINTPTR_MAX - address) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::erase: invalid long string");
        }
    }
    if (position > size) {
        _ZSt14_Xout_of_rangePKc_nid_postfix("basic_string");
    }

    const auto removed = std::min(count, size - position);
    if (removed == 0) return string;
    const auto newSize = size - removed;
    const auto suffix = newSize - position;
    if (isShort) {
        auto* data = descriptor.data() + 1;
        std::memmove(data + position, data + position + removed, suffix);
        data[newSize] = '\0';
        descriptor[0] = static_cast<char>(newSize << 1u);
        if (!GuestCopyOut_nid_no_patch(string, descriptor.data(), newSize + 2)) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::erase: unwritable short string");
        }
        return string;
    }

    auto* data = reinterpret_cast<char*>(static_cast<std::uintptr_t>(address));
    std::array<char, 4096> block{};
    for (std::uint64_t copied = 0; copied < suffix;) {
        const auto bytes = std::min<std::uint64_t>(block.size(), suffix - copied);
        const auto offset = position + copied;
        if (!GuestCopyIn_nid_no_patch(block.data(), data + offset + removed, bytes) ||
            !GuestCopyOut_nid_no_patch(data + offset, block.data(), bytes)) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::erase: inaccessible long string");
        }
        copied += bytes;
    }
    constexpr char terminator = '\0';
    if (!GuestCopyOut_nid_no_patch(data + newSize, &terminator, sizeof(terminator)) ||
        !GuestCopyOut_nid_no_patch(static_cast<char*>(string) + 8, &newSize, sizeof(newSize))) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("basic_string::erase: unwritable long string");
    }
    return string;
}
