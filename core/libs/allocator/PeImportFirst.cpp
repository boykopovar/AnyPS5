#include "PeImportFirst.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace Allocator {

namespace {

std::uint32_t read32(const std::vector<unsigned char>& bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) throw std::runtime_error("truncated image");
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::uint16_t read16(const std::vector<unsigned char>& bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) throw std::runtime_error("truncated image");
    std::uint16_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

}

void MoveImportFirst(std::vector<unsigned char>& bytes, const std::string& dll) {
    const auto pe = read32(bytes, 0x3c);
    if (read32(bytes, pe) != 0x00004550u) throw std::runtime_error("not a PE image");
    const auto sections = read16(bytes, pe + 6);
    const auto optionalSize = read16(bytes, pe + 20);
    const auto optional = pe + 24;
    if (read16(bytes, optional) != 0x20bu) throw std::runtime_error("not a PE32+ image");
    const auto importRva = read32(bytes, optional + 112 + 8);
    if (importRva == 0) throw std::runtime_error("the image has no imports");
    const auto sectionTable = optional + optionalSize;
    const auto fileOffset = [&](std::uint32_t rva) -> std::size_t {
        for (std::uint16_t index = 0; index < sections; ++index) {
            const auto header = sectionTable + index * 40u;
            const auto virtualSize = read32(bytes, header + 8);
            const auto virtualAddress = read32(bytes, header + 12);
            const auto rawSize = read32(bytes, header + 16);
            const auto rawOffset = read32(bytes, header + 20);
            if (rva >= virtualAddress && rva < virtualAddress + std::max(virtualSize, rawSize)) {
                if (rva - virtualAddress >= rawSize) throw std::runtime_error("RVA outside the section's file data");
                return rawOffset + (rva - virtualAddress);
            }
        }
        throw std::runtime_error("RVA outside every section");
    };
    const auto directory = fileOffset(importRva);
    constexpr std::size_t DescriptorBytes = 20;
    std::vector<std::array<unsigned char, DescriptorBytes>> descriptors;
    std::size_t found = SIZE_MAX;
    for (std::size_t offset = directory;; offset += DescriptorBytes) {
        if (offset + DescriptorBytes > bytes.size()) throw std::runtime_error("truncated import directory");
        std::array<unsigned char, DescriptorBytes> descriptor{};
        std::memcpy(descriptor.data(), bytes.data() + offset, DescriptorBytes);
        if (std::all_of(descriptor.begin(), descriptor.end(), [](unsigned char value) { return value == 0; })) break;
        const auto nameOffset = fileOffset(read32(bytes, offset + 12));
        std::string name;
        for (auto at = nameOffset; at < bytes.size() && bytes[at] != 0; ++at) name.push_back(static_cast<char>(bytes[at]));
        if (lower(name) == lower(dll)) found = descriptors.size();
        descriptors.push_back(descriptor);
    }
    if (found == SIZE_MAX) throw std::runtime_error("the image does not import " + dll);
    std::rotate(descriptors.begin(), descriptors.begin() + static_cast<std::ptrdiff_t>(found), descriptors.begin() + static_cast<std::ptrdiff_t>(found) + 1);
    for (std::size_t index = 0; index < descriptors.size(); ++index) std::memcpy(bytes.data() + directory + index * DescriptorBytes, descriptors[index].data(), DescriptorBytes);
}

void MoveImportFirstInFile(const std::string& path, const std::string& dll) {
    std::vector<unsigned char> bytes;
    {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("cannot open " + path);
        bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    MoveImportFirst(bytes, dll);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output || !output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error("cannot write " + path);
}

}
