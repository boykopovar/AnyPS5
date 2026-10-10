#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

namespace RelinkerTests {

using Bytes = std::vector<std::uint8_t>;

inline constexpr std::uint64_t kCodeOffset = 0x4000;
inline constexpr std::uint64_t kCodeSize = 0x1000;
inline constexpr std::uint64_t kDynamicVaddr = 0x600;
inline constexpr std::uint16_t kSpareProgramHeaders = 3;
inline constexpr std::uint64_t kModuleDataOffset = 0x5000;
inline constexpr std::uint64_t kModuleDataVaddr = 0x2000;
inline constexpr std::uint64_t kModuleDataSize = 0x400;

template<typename TValue>
void Write(Bytes& bytes, const std::size_t offset, const TValue value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("ELF fixture write is out of bounds");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

template<typename TValue>
TValue Read(const Bytes& bytes, const std::size_t offset) {
    TValue value;
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("ELF read is out of bounds");
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

inline void WriteProgramHeader(Bytes& image, const std::uint16_t index, const std::uint32_t type, const std::uint32_t flags, const std::uint64_t offset, const std::uint64_t vaddr, const std::uint64_t size, const std::uint64_t align) {
    const std::size_t header = 64 + index * 56;
    Write<std::uint32_t>(image, header, type);
    Write<std::uint32_t>(image, header + 4, flags);
    Write<std::uint64_t>(image, header + 8, offset);
    Write<std::uint64_t>(image, header + 16, vaddr);
    Write<std::uint64_t>(image, header + 24, vaddr);
    Write<std::uint64_t>(image, header + 32, size);
    Write<std::uint64_t>(image, header + 40, size);
    Write<std::uint64_t>(image, header + 48, align);
}

inline Bytes MakeExecutable(const std::span<const std::uint8_t> code) {
    if (code.size() > kDynamicVaddr) throw std::runtime_error("ELF fixture code overlaps the dynamic section");
    Bytes image(0x8000);
    const std::uint8_t ident[16] = {0x7F, 'E', 'L', 'F', 2, 1, 1};
    std::memcpy(image.data(), ident, sizeof(ident));
    Write<std::uint16_t>(image, 16, 3);
    Write<std::uint16_t>(image, 18, 62);
    Write<std::uint32_t>(image, 20, 1);
    Write<std::uint64_t>(image, 24, 0);
    Write<std::uint64_t>(image, 32, 64);
    Write<std::uint16_t>(image, 52, 64);
    Write<std::uint16_t>(image, 54, 56);
    Write<std::uint16_t>(image, 56, 2 + kSpareProgramHeaders);
    Write<std::uint16_t>(image, 58, 64);
    const std::pair<std::int64_t, std::uint64_t> tags[] = {{5, 0x600}, {10, 1}, {6, 0x620}, {11, 24}, {7, 0x700}, {8, 0}, {9, 24}, {0, 0}};
    WriteProgramHeader(image, 0, 1, 5, kCodeOffset, 0, kCodeSize, 0x4000);
    WriteProgramHeader(image, 1, 2, 6, kCodeOffset + kDynamicVaddr, kDynamicVaddr, sizeof(tags) / sizeof(tags[0]) * 16, 8);
    for (std::uint16_t index = 2; index < 2 + kSpareProgramHeaders; ++index) WriteProgramHeader(image, index, 0x6FFFFF01, 0, 0, 0, 0, 1);
    std::fill(image.begin() + kCodeOffset, image.begin() + kCodeOffset + kDynamicVaddr, 0xCC);
    std::copy(code.begin(), code.end(), image.begin() + kCodeOffset);
    for (std::size_t index = 0; index < sizeof(tags) / sizeof(tags[0]); ++index) {
        Write<std::int64_t>(image, kCodeOffset + kDynamicVaddr + index * 16, tags[index].first);
        Write<std::uint64_t>(image, kCodeOffset + kDynamicVaddr + index * 16 + 8, tags[index].second);
    }
    return image;
}

inline Bytes MakeModule(const std::span<const std::uint8_t> code) {
    if (code.size() > kCodeSize) throw std::runtime_error("ELF fixture code exceeds the code segment");
    Bytes image(0x8000);
    const std::uint8_t ident[16] = {0x7F, 'E', 'L', 'F', 2, 1, 1};
    std::memcpy(image.data(), ident, sizeof(ident));
    Write<std::uint16_t>(image, 16, 3);
    Write<std::uint16_t>(image, 18, 62);
    Write<std::uint32_t>(image, 20, 1);
    Write<std::uint64_t>(image, 24, 0);
    Write<std::uint64_t>(image, 32, 64);
    Write<std::uint16_t>(image, 52, 64);
    Write<std::uint16_t>(image, 54, 56);
    Write<std::uint16_t>(image, 56, 3 + kSpareProgramHeaders);
    Write<std::uint16_t>(image, 58, 64);
    const std::pair<std::int64_t, std::uint64_t> tags[] = {
        {5, kModuleDataVaddr + 0x200}, {10, 1},
        {6, kModuleDataVaddr + 0x220}, {11, 24},
        {4, kModuleDataVaddr + 0x240},
        {7, kModuleDataVaddr + 0x300}, {8, 0}, {9, 24},
        {0, 0},
    };
    WriteProgramHeader(image, 0, 1, 5, kCodeOffset, 0, kCodeSize, 0x4000);
    WriteProgramHeader(image, 1, 1, 6, kModuleDataOffset, kModuleDataVaddr, kModuleDataSize, 8);
    WriteProgramHeader(image, 2, 2, 6, kModuleDataOffset, kModuleDataVaddr, sizeof(tags) / sizeof(tags[0]) * 16, 8);
    for (std::uint16_t index = 3; index < 3 + kSpareProgramHeaders; ++index) WriteProgramHeader(image, index, 0x6FFFFF01, 0, 0, 0, 0, 1);
    std::fill(image.begin() + kCodeOffset, image.begin() + kCodeOffset + kCodeSize, 0xCC);
    std::copy(code.begin(), code.end(), image.begin() + kCodeOffset);
    for (std::size_t index = 0; index < sizeof(tags) / sizeof(tags[0]); ++index) {
        Write<std::int64_t>(image, kModuleDataOffset + index * 16, tags[index].first);
        Write<std::uint64_t>(image, kModuleDataOffset + index * 16 + 8, tags[index].second);
    }
    Write<std::uint32_t>(image, kModuleDataOffset + 0x240, 1);
    Write<std::uint32_t>(image, kModuleDataOffset + 0x244, 1);
    return image;
}

class MappedImage {
public:
    explicit MappedImage(const Bytes& elf) {
        const auto phOffset = Read<std::uint64_t>(elf, 32);
        const auto phSize = Read<std::uint16_t>(elf, 54);
        const auto phCount = Read<std::uint16_t>(elf, 56);
        for (std::uint16_t index = 0; index < phCount; ++index) {
            const auto header = phOffset + index * phSize;
            if (Read<std::uint32_t>(elf, header) != 1) continue;
            size = std::max<std::size_t>(size, Read<std::uint64_t>(elf, header + 16) + Read<std::uint64_t>(elf, header + 40));
        }
        if (size == 0) throw std::runtime_error("ELF has no PT_LOAD segment");
        base = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) throw std::runtime_error("Cannot map executable memory for the ELF image");
        for (std::uint16_t index = 0; index < phCount; ++index) {
            const auto header = phOffset + index * phSize;
            if (Read<std::uint32_t>(elf, header) != 1) continue;
            const auto offset = Read<std::uint64_t>(elf, header + 8);
            const auto fileSize = Read<std::uint64_t>(elf, header + 32);
            if (offset > elf.size() || fileSize > elf.size() - offset) throw std::runtime_error("PT_LOAD exceeds the ELF file");
            std::memcpy(static_cast<std::uint8_t*>(base) + Read<std::uint64_t>(elf, header + 16), elf.data() + offset, fileSize);
        }
        if (mprotect(base, size, PROT_NONE) != 0) throw std::runtime_error("Cannot protect the ELF image");
        const auto page = static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
        for (std::uint16_t index = 0; index < phCount; ++index) {
            const auto header = phOffset + index * phSize;
            if (Read<std::uint32_t>(elf, header) != 1) continue;
            const auto flags = Read<std::uint32_t>(elf, header + 4);
            const auto vaddr = Read<std::uint64_t>(elf, header + 16);
            const auto start = vaddr / page * page;
            const auto end = (vaddr + Read<std::uint64_t>(elf, header + 40) + page - 1) / page * page;
            const int protection = ((flags & 4) != 0 ? PROT_READ : 0) | ((flags & 2) != 0 ? PROT_WRITE : 0) | ((flags & 1) != 0 ? PROT_EXEC : 0);
            if (mprotect(static_cast<std::uint8_t*>(base) + start, std::min<std::uint64_t>(end, size) - start, protection) != 0) throw std::runtime_error("Cannot apply PT_LOAD permissions");
        }
    }

    MappedImage(const MappedImage&) = delete;
    MappedImage& operator=(const MappedImage&) = delete;

    ~MappedImage() {
        munmap(base, size);
    }

    template<typename TFunction>
    TFunction* At(const std::uint64_t vaddr) const {
        if (vaddr >= size) throw std::runtime_error("Address is outside the mapped ELF image");
        return reinterpret_cast<TFunction*>(static_cast<std::uint8_t*>(base) + vaddr);
    }

private:
    void* base = nullptr;
    std::size_t size = 0;
};

} // namespace RelinkerTests
