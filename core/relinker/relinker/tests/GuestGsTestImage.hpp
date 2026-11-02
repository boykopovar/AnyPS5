#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include "prx/libc/include/GuestGs.hpp"
#else
#include <sys/mman.h>
#endif

inline std::uint8_t* GuestGsAllocate(std::size_t size, bool executable) {
#ifdef _WIN32
    return static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE));
#else
    auto* result = mmap(nullptr, size, PROT_READ | PROT_WRITE | (executable ? PROT_EXEC : 0), MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return result == MAP_FAILED ? nullptr : static_cast<std::uint8_t*>(result);
#endif
}

inline std::uint8_t* GuestGsMapTestImage(const std::vector<std::uint8_t>& bytes) {
#ifdef _WIN32
    const auto read = [&]<typename TValue>(std::size_t offset) {
        TValue value;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    };
    const auto nt = read.operator()<std::uint32_t>(0x3c);
    const auto optional = nt + 24;
    auto* image = GuestGsAllocate(read.operator()<std::uint32_t>(optional + 56), true);
    if (!image) return nullptr;
    std::memcpy(image, bytes.data(), read.operator()<std::uint32_t>(optional + 60));
    const auto sections = optional + read.operator()<std::uint16_t>(nt + 20);
    for (unsigned index = 0; index < read.operator()<std::uint16_t>(nt + 6); ++index) {
        const auto section = sections + index * 40;
        std::memcpy(image + read.operator()<std::uint32_t>(section + 12), bytes.data() + read.operator()<std::uint32_t>(section + 20), read.operator()<std::uint32_t>(section + 16));
    }
    if (!GuestGsRegisterImage_nid_no_patch(image)) {
        VirtualFree(image, 0, MEM_RELEASE);
        return nullptr;
    }
    return image;
#else
    auto* image = GuestGsAllocate(bytes.size(), true);
    if (image) std::memcpy(image, bytes.data(), bytes.size());
    return image;
#endif
}
