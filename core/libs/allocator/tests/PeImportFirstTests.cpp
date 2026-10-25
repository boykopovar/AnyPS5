#include "allocator/PeImportFirst.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t PeOffset = 0x40;
constexpr std::size_t OptionalOffset = PeOffset + 24;
constexpr std::size_t OptionalBytes = 240;
constexpr std::size_t SectionTable = OptionalOffset + OptionalBytes;
constexpr std::uint32_t SectionRva = 0x1000;
constexpr std::size_t SectionOffset = 0x200;
constexpr std::size_t SectionBytes = 0x200;
constexpr std::size_t DescriptorBytes = 20;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void put16(std::vector<unsigned char>& bytes, std::size_t offset, std::uint16_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void put32(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::uint32_t get32(const std::vector<unsigned char>& bytes, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::vector<unsigned char> image(const std::vector<std::string>& dlls, std::uint16_t magic = 0x20b) {
    std::vector<unsigned char> bytes(SectionOffset + SectionBytes, 0);
    bytes[0] = 'M';
    bytes[1] = 'Z';
    put32(bytes, 0x3c, PeOffset);
    put32(bytes, PeOffset, 0x00004550u);
    put16(bytes, PeOffset + 4, 0x8664);
    put16(bytes, PeOffset + 6, 1);
    put16(bytes, PeOffset + 20, OptionalBytes);
    put16(bytes, OptionalOffset, magic);
    put32(bytes, OptionalOffset + 108, 16);
    put32(bytes, OptionalOffset + 112 + 8, SectionRva);
    put32(bytes, OptionalOffset + 112 + 12, static_cast<std::uint32_t>((dlls.size() + 1) * DescriptorBytes));
    std::memcpy(bytes.data() + SectionTable, ".idata", 6);
    put32(bytes, SectionTable + 8, SectionBytes);
    put32(bytes, SectionTable + 12, SectionRva);
    put32(bytes, SectionTable + 16, SectionBytes);
    put32(bytes, SectionTable + 20, SectionOffset);
    auto name = (dlls.size() + 1) * DescriptorBytes;
    for (std::size_t index = 0; index < dlls.size(); ++index) {
        const auto descriptor = SectionOffset + index * DescriptorBytes;
        put32(bytes, descriptor, static_cast<std::uint32_t>(SectionRva + 0x100 + index * 0x10));
        put32(bytes, descriptor + 4, static_cast<std::uint32_t>(index + 1));
        put32(bytes, descriptor + 12, static_cast<std::uint32_t>(SectionRva + name));
        put32(bytes, descriptor + 16, static_cast<std::uint32_t>(SectionRva + 0x180 + index * 0x10));
        std::memcpy(bytes.data() + SectionOffset + name, dlls[index].c_str(), dlls[index].size() + 1);
        name += dlls[index].size() + 1;
    }
    return bytes;
}

std::string importName(const std::vector<unsigned char>& bytes, std::size_t index) {
    const auto rva = get32(bytes, SectionOffset + index * DescriptorBytes + 12);
    return reinterpret_cast<const char*>(bytes.data() + SectionOffset + (rva - SectionRva));
}

std::vector<unsigned char> descriptor(const std::vector<unsigned char>& bytes, std::size_t index) {
    const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(SectionOffset + index * DescriptorBytes);
    return {first, first + static_cast<std::ptrdiff_t>(DescriptorBytes)};
}

void writeFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(output), "cannot write " + path.string());
}

std::vector<unsigned char> readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "cannot read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void movesTheNamedImportFirst() {
    const auto original = image({"kernel32.dll", "ucrtbase.dll", "mimalloc-redirect.dll", "advapi32.dll"});
    auto moved = original;
    Allocator::MoveImportFirst(moved, "MIMALLOC-REDIRECT.dll");
    require(importName(moved, 0) == "mimalloc-redirect.dll" && importName(moved, 1) == "kernel32.dll" && importName(moved, 2) == "ucrtbase.dll" && importName(moved, 3) == "advapi32.dll",
        "the named import did not move first, or the others changed their order");
    require(descriptor(moved, 0) == descriptor(original, 2) && descriptor(moved, 1) == descriptor(original, 0) && descriptor(moved, 2) == descriptor(original, 1) && descriptor(moved, 3) == descriptor(original, 3),
        "a descriptor did not move whole");
    const auto directoryEnd = SectionOffset + 4 * DescriptorBytes;
    require(std::equal(moved.begin(), moved.begin() + SectionOffset, original.begin()) && std::equal(moved.begin() + directoryEnd, moved.end(), original.begin() + directoryEnd),
        "bytes outside the import descriptors changed");
    auto unchanged = original;
    Allocator::MoveImportFirst(unchanged, "kernel32.dll");
    require(unchanged == original, "moving the first import changed the image");
}

void movesTheImportInAFile(const std::filesystem::path& directory) {
    const auto path = directory / "moved.dll";
    writeFile(path, image({"kernel32.dll", "mimalloc-redirect.dll"}));
    Allocator::MoveImportFirstInFile(path, "mimalloc-redirect.dll");
    const auto moved = readFile(path);
    require(importName(moved, 0) == "mimalloc-redirect.dll" && importName(moved, 1) == "kernel32.dll", "the file's import did not move first");
}

void failsWithoutTouchingTheFile(const std::filesystem::path& directory, const std::string& what, const std::vector<unsigned char>& bytes, const std::string& expected) {
    const auto path = directory / (what + ".dll");
    writeFile(path, bytes);
    try {
        Allocator::MoveImportFirstInFile(path, "mimalloc-redirect.dll");
    } catch (const std::exception& error) {
        require(std::string(error.what()).find(expected) != std::string::npos, what + ": failed with another error: " + error.what());
        require(readFile(path) == bytes, what + ": the file changed although the move failed");
        return;
    }
    throw std::runtime_error(what + ": the move did not fail");
}

}

int main() {
    auto directory = std::filesystem::temp_directory_path() / std::filesystem::path(u8"aps5-pe-import-first-a\u00e7\u00e3o-\u65e5\u672c-");
    directory += std::to_string(std::random_device{}());
    try {
        std::filesystem::create_directories(directory);
        movesTheNamedImportFirst();
        movesTheImportInAFile(directory);
        failsWithoutTouchingTheFile(directory, "missing-import", image({"kernel32.dll", "ucrtbase.dll"}), "does not import mimalloc-redirect.dll");
        auto truncated = image({"kernel32.dll", "mimalloc-redirect.dll", "ucrtbase.dll"});
        truncated.resize(SectionOffset + DescriptorBytes + DescriptorBytes / 2);
        failsWithoutTouchingTheFile(directory, "truncated-directory", truncated, "truncated import directory");
        failsWithoutTouchingTheFile(directory, "pe32", image({"kernel32.dll", "mimalloc-redirect.dll"}, 0x10b), "not a PE32+ image");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
        return 1;
    }
    std::filesystem::remove_all(directory);
    std::cout << "pe import first tests passed\n";
    return 0;
}
